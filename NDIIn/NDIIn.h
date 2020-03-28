//
//  NDIIn.h
//  NDIInTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include <string>
#include <vector>
#include "../third-parties/concurrentqueue.h"

#include "../third-parties/TOP_CPlusPlusBase.h"

#include <Processing.NDI.Lib.h>

class NDIIn : public TOP_CPlusPlusBase
{
public:
    NDIIn(const OP_NodeInfo *info);
    virtual ~NDIIn();

    virtual void getGeneralInfo(TOP_GeneralInfo *, const OP_Inputs*, void*) override;
    virtual bool getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void*) override;


    virtual void execute(TOP_OutputFormatSpecs*,
							const OP_Inputs*,
							TOP_Context* context,
							void* reserved1) override;

    virtual int32_t getNumInfoCHOPChans(void *reserved1) override;
    virtual void getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan *chan, void* reserved1) override;

    virtual bool getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override;
    virtual void getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries *entries,
									void *reserved1) override;

	virtual void setupParameters(OP_ParameterManager *manager, void *reserved1) override;
	virtual void pulsePressed(const char *name, void *reserved1) override;

	virtual void getErrorString(OP_String *error, void *reserved1) override;
	virtual void getWarningString(OP_String *warning, void *reserved1) override;

private:
	// Our finder
	NDIlib_find_instance_t _finder = nullptr;
	NDIlib_recv_instance_t _receiver = nullptr;
	
	NDIlib_video_frame_v2_t _videoFrame;

	struct {
		bool active;
		std::string sourceName = "";
		NDIlib_recv_bandwidth_e bandwidth;
		char additionalIPs[256] = {'\0'};
	} _params;

	struct {
		uint32_t sourcesCount;
		std::vector<std::string> sourcesNames;
		std::vector<std::string> sourcesAdresses;

		bool isErrored = false;
		std::string errorMessage;
		std::string warningMessage;
	} _state;
};
