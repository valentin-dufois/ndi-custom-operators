//
//  NDIInTOP.cpp
//  NDIInTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include "NDIInTOP.h"

#include "../Utils/fast_memcpy.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>
#include <random>
#include <chrono>

#include <iostream>

NDIInTOP::NDIInTOP(const OP_NodeInfo *) {
	if(!NDIlib_initialize()) {
		_state.isErrored = true;
		_state.errorMessage = "Could not initialized NDI. CPU may be unsupported.";
		return;
	}
}

NDIInTOP::~NDIInTOP() {
	NDIlib_recv_destroy(_receiver);
	NDIlib_find_destroy(_finder);
	NDIlib_destroy();
}

void NDIInTOP::getGeneralInfo(TOP_GeneralInfo * ginfo,
							const OP_Inputs * inputs,
							void *) {
	try {
		ginfo->cookEveryFrameIfAsked = true;
		ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;

		// get parameters
		_params.active = inputs->getParInt("Active");

		if (!_params.active) {
			if (_receiver != nullptr) {
				NDIlib_recv_destroy(_receiver);
				_receiver = nullptr;
			}

			if (_finder != nullptr) {
				NDIlib_find_destroy(_finder);
				_finder = nullptr;
			}

			return;
		}

		// Bandwidth
		std::string bandwidthParStr = inputs->getParString("Bandwidth");
		NDIlib_recv_bandwidth_e bandwidthPar;

		if (bandwidthParStr == "Low")
			bandwidthPar = NDIlib_recv_bandwidth_lowest;
		else  // if(bandwidthParStr == "high")
			bandwidthPar = NDIlib_recv_bandwidth_highest;

		// Check if requested bandwidth has changed
		if (bandwidthPar != _params.bandwidth && _receiver != nullptr) {
			// Requested source has changed, close current connection
			NDIlib_recv_destroy(_receiver);
			_receiver = nullptr;
		}

		_params.bandwidth = bandwidthPar;

		// Check if specified addition lookup ips changed
		char additionalIPsPar[256];
		strncpy(additionalIPsPar, inputs->getParString("Additionalips"), 256);
		if (strcmp(_params.additionalIPs, additionalIPsPar) != 0) {
			// Specified IP changed, close finder
			if (_finder != nullptr) {
				NDIlib_find_destroy(_finder);
				_finder = nullptr;
			}

			strlcpy(_params.additionalIPs, additionalIPsPar, 256);
		}

		// Do we have a finder ?
		if (_finder == nullptr) {
			NDIlib_find_create_t finderParams;
			finderParams.p_extra_ips = _params.additionalIPs;
			_finder = NDIlib_find_create2(&finderParams);
		}

		const std::string sourceNamePar = inputs->getParString("Sourcename");

		// Check if requested source changed
		if (sourceNamePar != _params.sourceName && _receiver != nullptr) {
			// Requested source has changed, close current connection
			NDIlib_recv_destroy(_receiver);
			_receiver = nullptr;
		}

		_params.sourceName = sourceNamePar;

		// Check available sources
		const NDIlib_source_t* sources = NDIlib_find_get_current_sources(_finder, &_state.sourcesCount);

		// Fill in the list of sources
		_state.sourcesNames.clear();
		_state.sourcesAdresses.clear();
		_state.sourcesNames.reserve(_state.sourcesCount);
		_state.sourcesAdresses.reserve(_state.sourcesCount);

		for (uint32_t i = 0; i < _state.sourcesCount; ++i) {
			_state.sourcesNames.push_back(sources[i].p_ndi_name);
			_state.sourcesAdresses.push_back(sources[i].p_url_address);
		}

		// Are we connected to a source ?
		if (_receiver == nullptr) {
			// No, check if one source match the requested one
			for (uint32_t i = 0; i < _state.sourcesCount; ++i) {
				if (std::string(sources[i].p_ndi_name) != _params.sourceName)
					continue;

				// Connect to the source
				NDIlib_recv_create_v3_t receiverOptions;
				receiverOptions.color_format = NDIlib_recv_color_format_BGRX_BGRA;
				receiverOptions.allow_video_fields = false;
				receiverOptions.bandwidth = _params.bandwidth;
				receiverOptions.source_to_connect_to = *(sources + i);

				_receiver = NDIlib_recv_create_v3(&receiverOptions);

				if (!_receiver) {
					_state.isErrored = true;
					_state.errorMessage = "Could not connect to source " + _params.sourceName + ".";
					return;
				}
				_state.isErrored = false;
				_state.warningMessage = "";

				// we are connected, end here
				break;
			}

			// Are we connected to a source now ?
			if (_receiver == nullptr) {
				// Still no connection, ends here
				_state.warningMessage = "Looking for source " + _params.sourceName + "...";
				return;
			}
		}

		// Get the latest frames
		NDIlib_frame_type_e frameType = NDIlib_frame_type_none;

		do {
			frameType = NDIlib_recv_capture_v2(_receiver, &_videoFrame, nullptr, nullptr, 10);
		} while (frameType != NDIlib_frame_type_none && frameType != NDIlib_frame_type_video);


		ginfo->clearBuffers = false;
	} catch (std::runtime_error &exc) {
		_state.isErrored = true;
		_state.errorMessage = "An error occured with NDI : " + std::string(exc.what());

		ginfo->clearBuffers = true;
	}
}

bool NDIInTOP::getOutputFormat(TOP_OutputFormat * format,
							 const OP_Inputs *, void *) {
	// Are we able to output something ?
	if(_state.isErrored || _receiver == nullptr || !_params.active) {
		return false;
	}

	// Yes, set parameters to 8bits RGBA
	format->redChannel = true;
	format->greenChannel = true;
	format->blueChannel = true;
	format->alphaChannel = true;
	format->bitsPerChannel = 8;
	format->width = _videoFrame.xres;
	format->height = _videoFrame.yres;

	return true;
}

void NDIInTOP::execute(TOP_OutputFormatSpecs * output,
					  const OP_Inputs *, TOP_Context *, void *) {
	// Default output is the first provided buffer
	output->newCPUPixelDataLocation = 0;

	// Can we receive ?
	if(_state.isErrored || _receiver == nullptr || !_params.active) {
		memset(output->cpuPixelData[0], 0, output->width * output->height);
		return;
	}

	if (_videoFrame.p_data == nullptr) {
		output->newCPUPixelDataLocation = -1;
		return;
	}

	// Is the received video frame supported ?
	// We check against the TD-provided buffers as they will have different resolutions than the requested ones if we exceed the TD licence limitation.
	if(_videoFrame.xres != output->width || _videoFrame.yres != output->height) {
		_state.isErrored = true;
		_state.errorMessage = "TouchDesigner does not support the received video resolution (" + std::to_string(_videoFrame.xres) + "x" + std::to_string(_videoFrame.yres) + ").";
		NDIlib_recv_free_video_v2(_receiver, &_videoFrame);
		_videoFrame.p_data = nullptr;
		return;
	}

	// We're good
	_state.isErrored = false;

	// Copy frame
	memcpy_fast(output->cpuPixelData[0], _videoFrame.p_data, _videoFrame.xres * _videoFrame.yres * 4);

	NDIlib_recv_free_video_v2(_receiver, &_videoFrame);
	_videoFrame.p_data = nullptr;
}

int32_t NDIInTOP::getNumInfoCHOPChans(void *) {
	// We return the number of channel we want to output to any Info CHOP
	// connected, num_sources
	return 3;
}

void NDIInTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan * chan, void *) {
	switch (index) {
		case 0:  // connected
			chan->name->setString("connected");
			chan->value = _receiver != nullptr;
			break;
		case 1:  // num_sources
			chan->name->setString("num_sources");
			chan->value = _state.sourcesCount;
			break;
		case 2:  // fps
			chan->name->setString("received_fps");
			if(_receiver == nullptr)
				chan->value = 0;
			else
				chan->value = _videoFrame.frame_rate_N / _videoFrame.frame_rate_D;
			break;
	}
}

bool	NDIInTOP::getInfoDATSize(OP_InfoDATSize * infoSize, void *) {
	infoSize->rows = _state.sourcesCount + 1;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void NDIInTOP::getInfoDATEntries(int32_t index, int32_t,
							   OP_InfoDATEntries * entries, void *) {
	if(index == 0) {
		entries->values[0]->setString("Sources");
		entries->values[1]->setString("Addresses");
		return;
	}

	entries->values[0]->setString(_state.sourcesNames[index - 1].c_str());
	entries->values[1]->setString(_state.sourcesAdresses[index - 1].c_str());
}


// Override these methods if you want to define specfic parameters
void NDIInTOP::setupParameters(OP_ParameterManager * manager, void *) {
	OP_NumericParameter activeToggle;
	activeToggle.name = "Active";
	activeToggle.label = "Active";
	activeToggle.page = "NDI In";
	activeToggle.defaultValues[0] = 1;
	manager->appendToggle(activeToggle);

	OP_StringParameter sourceName;
	sourceName.name = "Sourcename";
	sourceName.label = "Source name";
	sourceName.defaultValue = "";
	sourceName.page = "NDI In";
	manager->appendString(sourceName);

	OP_StringParameter additionalIPs;
	additionalIPs.name = "Additionalips";
	additionalIPs.label = "Additional Search IPs";
	additionalIPs.defaultValue = "";
	additionalIPs.page = "NDI In";
	manager->appendString(additionalIPs);

	OP_StringParameter bandwidth;
	bandwidth.name = "Bandwidth";
	bandwidth.label = "Bandwidth";
	bandwidth.page = "NDI In";
	const char * bandwidthValues[] = {"High", "Low"};
	manager->appendMenu(bandwidth, 2, bandwidthValues, bandwidthValues);
}

void NDIInTOP::getErrorString(OP_String * error, void *) {
	if(_state.isErrored)
		error->setString(_state.errorMessage.c_str());
}

void NDIInTOP::getWarningString(OP_String * warning, void *) {
	if(_state.warningMessage.size() != 0)
		warning->setString(_state.warningMessage.c_str());
}

