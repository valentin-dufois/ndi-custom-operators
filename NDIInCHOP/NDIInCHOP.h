//
//  NDIInCHOP.h
//  NDIInCHOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include <string>
#include <vector>
#include <mutex>
#include <thread> 
#include <functional>

#include "../third-parties/CHOP_CPlusPlusBase.h"
#include "../Utils/ringbuffer.hpp"

#include <Processing.NDI.Lib.h>

/// The NDI In CHOP connects to a NDI source and retrieves its audio feed.
class NDIInCHOP : public CHOP_CPlusPlusBase
{
public:
	NDIInCHOP(const OP_NodeInfo* info);
	virtual ~NDIInCHOP();

	virtual void	getGeneralInfo(CHOP_GeneralInfo*, const OP_Inputs*, void* ) override;
	virtual bool	getOutputInfo(CHOP_OutputInfo*, const OP_Inputs*, void*) override;
	virtual void	getChannelName(int32_t index, OP_String *name, const OP_Inputs*, void* reserved) override;

	virtual void	execute(CHOP_Output*,
								const OP_Inputs*,
								void* reserved) override;


	virtual int32_t getNumInfoCHOPChans(void* reserved1) override;
	virtual void	getInfoCHOPChan(int index,
										OP_InfoCHOPChan* chan,
										void* reserved1) override;

	virtual bool	getInfoDATSize(OP_InfoDATSize* infoSize, void* resereved1) override;
	virtual void	getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries, void* reserved1) override;

	virtual void	setupParameters(OP_ParameterManager* manager, void *reserved1) override;

	virtual void getErrorString(OP_String *error, void *reserved1) override;
	virtual void getWarningString(OP_String *warning, void *reserved1) override;
private:
	// Our finder
	NDIlib_find_instance_t _finder = nullptr;
	NDIlib_recv_instance_t _receiver = nullptr;

	std::mutex _feedMutex;

	NDIlib_audio_frame_v2_t _audioFrame;

	// We make one buffer for each channel
	std::vector<RingBuffer *> _audioBuffers;
	std::mutex _buffersMutex;

	std::thread _pollBuffer;

	struct {
		bool active;
		std::string sourceName = "";
		NDIlib_recv_bandwidth_e bandwidth;
		char additionalIPs[256] = {'\0'};
		double bufferLength = .25;
	} _params;

	struct {
		uint32_t sourcesCount;
		std::vector<std::string> sourcesNames;
		std::vector<std::string> sourcesAdresses;

		int channelCount = 2;
		int sampleRate = 44100;

		bool waitForBuffersFill = true;

		bool isErrored = false;
		std::string errorMessage;
		std::string warningMessage;
	} _state;

	inline void stopReceiving() {
		NDIlib_recv_destroy(_receiver);
		_receiver = nullptr;

		if(_pollBuffer.joinable())
			_pollBuffer.detach();
	}

	/// Clean and rebuild the audioBuffer with the appropriate size
	inline void updateBuffer() {
		// Clear existing buffers
		if(_audioBuffers.size() != 0) {
			for(RingBuffer * buff: _audioBuffers) {
				delete buff;
			}

			_audioBuffers.clear();
		}

		_audioBuffers.reserve(_state.channelCount);

		std::uint32_t bufferSize = int(_params.bufferLength * _state.sampleRate * 2 * sizeof(float));

		for(int i = 0; i < _state.channelCount; ++i) {
			_audioBuffers.push_back(new RingBuffer(bufferSize));
		}

		_state.waitForBuffersFill = true;
	}

	/// Starts the polling thread
	/// Does nothing if the polling thread is still active
	inline void startPolling() {
		if(_pollBuffer.joinable())
			return;

		_pollBuffer = std::thread(std::bind(&NDIInCHOP::pollLoop, this));
	}

	/// Continuously polls the receiver for new audio frames
	void pollLoop();
};
