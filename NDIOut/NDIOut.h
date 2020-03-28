//
//  NDIOut.h
//  NDIOutTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include <string>
#include <future>

#include "../third-parties/concurrentqueue.h"
#include "../third-parties/TOP_CPlusPlusBase.h"

#include <Processing.NDI.Lib.h>

class NDIOut : public TOP_CPlusPlusBase
{
public:
    NDIOut(const OP_NodeInfo *info);
    virtual ~NDIOut();

    virtual void		getGeneralInfo(TOP_GeneralInfo *, const OP_Inputs*, void*) override;
    virtual bool		getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void*) override;


    virtual void		execute(TOP_OutputFormatSpecs*,
							const OP_Inputs*,
							TOP_Context* context,
							void* reserved1) override;


    virtual int32_t		getNumInfoCHOPChans(void *reserved1) override;
    virtual void		getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan *chan, void* reserved1) override;

    virtual bool		getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override;
    virtual void		getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries *entries,
									void *reserved1) override;

	virtual void		setupParameters(OP_ParameterManager *manager, void *reserved1) override;
	virtual void		pulsePressed(const char *name, void *reserved1) override;

	virtual void 	getErrorString(OP_String *error, void *reserved1) override;

private:
	// Our sender
	NDIlib_send_instance_t _feed = nullptr;

	struct {
		bool active;
		std::string sourceName = "";
		unsigned short fps = 60;

		int64_t groupsCookCount = 0;
		std::string groupsDATPath = "";
		std::string groups = "";
	} _params;

	NDIlib_send_create_t _feedSettings;
	NDIlib_metadata_frame_t _feedMetadata;

	bool _isErrored = false;
	std::string _errorMessage;

	OP_TOPInputDownloadOptions _GPUDownloadOptions;
	
	NDIlib_video_frame_v2_t _videoFrame;

	NDIlib_audio_frame_v2_t _audioFrame;

	// MARK: - Validations & updates

	std::string getGroups(const OP_Inputs* inputs);
};
