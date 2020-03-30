//
//  NDIInCHOP.cpp
//  NDIInCHOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>

#include "NDIInCHOP.h"

NDIInCHOP::NDIInCHOP(const OP_NodeInfo *) {
	if(!NDIlib_initialize()) {
		_state.isErrored = true;
		_state.errorMessage = "Could not initialized NDI. CPU may be unsupported.";
		return;
	}

	updateBuffer();

	_audioFrame.p_data = nullptr;
}

NDIInCHOP::~NDIInCHOP() {
	_feedMutex.lock();
	NDIlib_recv_destroy(_receiver);
	NDIlib_find_destroy(_finder);
	_receiver = nullptr;
	_finder = nullptr;
	_feedMutex.unlock();

	if(_pollBuffer.joinable())
		_pollBuffer.detach();

	NDIlib_destroy();
}

void NDIInCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo,
							 const OP_Inputs* inputs,
							 void *) {
	// Set up node info for TD
	ginfo->cookEveryFrameIfAsked = true;
	ginfo->timeslice = true;

	// Lock feed access while we check the current status
	std::unique_lock<std::mutex> lock(_feedMutex);

	// Retrieve all users parameters
	_params.active = inputs->getParInt("Active");
	const std::string sourceNamePar = inputs->getParString("Sourcename");
	char additionalIPsPar[256];
	strncpy(additionalIPsPar, inputs->getParString("Additionalips"), 256);
	std::string bandwidthParStr = inputs->getParString("Bandwidth");
	double bufferSizePar = inputs->getParDouble("Buffersize");

	// Is the node active ?
	if (!_params.active) {
		if (_receiver != nullptr) {
			stopReceiving();
		}

		if (_finder != nullptr) {
			NDIlib_find_destroy(_finder);
			_finder = nullptr;
		}

		return;
	}

	// Check if buffer size has changed
	const double bufferSizeDiff = std::abs(bufferSizePar - _params.bufferLength);
	if (bufferSizeDiff > std::numeric_limits<double>::epsilon()) {
		_params.bufferLength = bufferSizePar;

		std::unique_lock<std::mutex> buffersLock(_buffersMutex);
		updateBuffer();
	}

	// Check if the bandwidth has changed
	NDIlib_recv_bandwidth_e bandwidthPar;

	if (bandwidthParStr == "Low")
		bandwidthPar = NDIlib_recv_bandwidth_lowest;
	else  // if(bandwidthParStr == "high")
		bandwidthPar = NDIlib_recv_bandwidth_highest;

	if (bandwidthPar != _params.bandwidth && _receiver != nullptr) {
		// Requested source has changed, close current connection
		stopReceiving();
	}

	_params.bandwidth = bandwidthPar;

	// Check if specified additional lookup ips have changed
	if (strcmp(_params.additionalIPs, additionalIPsPar) != 0) {
		// Specified IP changed, close finder
		if (_finder != nullptr) {
			NDIlib_find_destroy(_finder);
			_finder = nullptr;
		}


#ifdef _WIN32
		strcpy_s(_params.additionalIPs, 256, additionalIPsPar);
#else
		strlcpy(_params.additionalIPs, additionalIPsPar, 256);
#endif
	}

	// Do we have a finder ?
	if (_finder == nullptr) {
		// No finder, create one
		NDIlib_find_create_t finderParams;
		finderParams.p_extra_ips = _params.additionalIPs;

		_finder = NDIlib_find_create2(&finderParams);
	}

	// Check if requested source changed
	if (sourceNamePar != _params.sourceName && _receiver != nullptr) {
		// Requested source has changed, close current connection
		stopReceiving();
	}

	_params.sourceName = sourceNamePar;

	// Check available sources
	const NDIlib_source_t* sources;
	sources = NDIlib_find_get_current_sources(_finder, &_state.sourcesCount);

	// Fill in the lists of sources
	_state.sourcesNames.clear();
	_state.sourcesAdresses.clear();
	_state.sourcesNames.reserve(_state.sourcesCount);
	_state.sourcesAdresses.reserve(_state.sourcesCount);

	for (uint32_t i = 0; i < _state.sourcesCount; ++i) {
		_state.sourcesNames.push_back(sources[i].p_ndi_name);
		_state.sourcesAdresses.push_back(sources[i].p_url_address);
	}

	// Are we connected to a source ?
	if (_receiver != nullptr) {
		// Yes, nothing else to do
		return;
	}

	// Check if one source match the requested one
	for (uint32_t i = 0; i < _state.sourcesCount; ++i) {
		if (sources[i].p_ndi_name != _params.sourceName)
			continue;

		for(RingBuffer * buff: _audioBuffers) {
			buff->clear();
		}

		// Connect to the source
		NDIlib_recv_create_v3_t receiverOptions;
		receiverOptions.bandwidth = _params.bandwidth;
		receiverOptions.source_to_connect_to = *(sources + i);

		_receiver = NDIlib_recv_create_v3(&receiverOptions);

		if (_receiver) {
			// We have a receiver
			_state.isErrored = false;
			_state.warningMessage = "";

			// we are connected, end here
			startPolling();
			return;
		}

		// An error occured, stop parsing here
		_state.isErrored = true;
		_state.errorMessage = "Could not connect to " + _params.sourceName + ".";
		return;
	}

	// Still no connection, put up a warning for the user
	_state.warningMessage = "Looking for source " + _params.sourceName + "...";
}

bool NDIInCHOP::getOutputInfo(CHOP_OutputInfo * info,
							const OP_Inputs *,
							void *) {
	// Are we able to output something ?
	if(_state.isErrored || _receiver == nullptr || !_params.active) {
		info->numChannels = 0;
		return true;
	}

	info->numChannels = _audioFrame.no_channels;
	info->sampleRate = _audioFrame.sample_rate;

	// Sample count and start index are set automatically as we are outputting
	// a time slice

	return true;
}

void NDIInCHOP::getChannelName(int32_t index, OP_String * name,
							 const OP_Inputs *, void *) {
	name->setString(("chan" + std::to_string(index + 1)).c_str());
}

void NDIInCHOP::execute(CHOP_Output * output, const OP_Inputs *, void *) {
	if(output->numChannels == 0) {
		return;
	}

	const double buffSize = _params.bufferLength * _state.sampleRate * sizeof(float);

	// Are we in a state to send samples out ?
	if(_receiver	== nullptr || _audioBuffers[0]->getReadAvail()	== 0 || (
	   _audioBuffers.size() != 0 &&
	   _state.waitForBuffersFill &&
	   _audioBuffers[0]->getReadAvail() < buffSize)) {
		// Nop, fill output with silence
		memset(output->channels[0], 0, output->numSamples * output->numChannels * sizeof(float));

		return;
	}

	_state.isErrored = false;
	_state.waitForBuffersFill = false;

	std::unique_lock<std::mutex> lock(_buffersMutex);

	// Fill all channels
	for(int i = 0; i < output->numChannels; ++i) {
		// Fill the output
		_audioBuffers[i]->read((unsigned char *)output->channels[i], output->numSamples * sizeof(float));
	}
}

int32_t NDIInCHOP::getNumInfoCHOPChans(void *) {
	return 2;
}

void NDIInCHOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void *) {
	switch (index) {
		case 0:  // connected
			chan->name->setString("connected");
			chan->value = _receiver != nullptr;
			break;
		case 1:  // num_sources
			chan->name->setString("num_sources");
			chan->value = _state.sourcesCount;
			break;
	}
}

bool NDIInCHOP::getInfoDATSize(OP_InfoDATSize * infoSize, void *) {
	infoSize->rows = _state.sourcesCount + 1;
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void NDIInCHOP::getInfoDATEntries(int32_t index, int32_t,
								OP_InfoDATEntries * entries, void *) {
	if(index == 0) {
		entries->values[0]->setString("Sources");
		entries->values[1]->setString("Addresses");
		return;
	}

	entries->values[0]->setString(_state.sourcesNames[index - 1].c_str());
	entries->values[1]->setString(_state.sourcesAdresses[index - 1].c_str());
}

void NDIInCHOP::setupParameters(OP_ParameterManager * manager, void *) {
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

	OP_NumericParameter bufferSize;
	bufferSize.name = "Buffersize";
	bufferSize.label = "Buffer Size (s)";
	bufferSize.page = "NDI In";
	bufferSize.defaultValues[0] = .25;
	bufferSize.minValues[0] = 0;
	bufferSize.maxValues[0] = 10;
	bufferSize.clampMins[0] = true;
	bufferSize.clampMaxes[0] = true;
	bufferSize.minSliders[0] = 0;
	bufferSize.maxSliders[0] = 10;
	manager->appendFloat(bufferSize);
}

void NDIInCHOP::getErrorString(OP_String *error, void *) {
	if(_state.isErrored)
		error->setString(_state.errorMessage.c_str());
}

void NDIInCHOP::getWarningString(OP_String * warning, void *) {
	if(_state.warningMessage.size() != 0)
		warning->setString(_state.warningMessage.c_str());
}

void NDIInCHOP::pollLoop() {
	NDIlib_frame_type_e frameType = NDIlib_frame_type_none;

	while (_receiver != nullptr) {
		_feedMutex.lock();

		if(_audioFrame.p_data != NULL) {
			// Free the previous frame
			NDIlib_recv_free_audio_v2(_receiver, &_audioFrame);
			_audioFrame.p_data = nullptr;
		}

		// Poll the NDI feed
		frameType = NDIlib_recv_capture_v2(_receiver, nullptr, &_audioFrame, nullptr, 0);

		if(frameType != NDIlib_frame_type_audio) {
			_feedMutex.unlock();
			continue;
		}

		_buffersMutex.lock();

		// Check audio properties
		if(_audioFrame.no_channels != _state.channelCount ||
		   _audioFrame.sample_rate != _state.sampleRate) {
			_state.channelCount = _audioFrame.no_channels;
			_state.sampleRate = _audioFrame.sample_rate;

			updateBuffer();
		}

		unsigned char * channelPtr;
		const int stride = _audioFrame.channel_stride_in_bytes;

		for(int i = 0; i < _audioFrame.no_channels; ++i) {
			channelPtr = ((unsigned char *)_audioFrame.p_data) + stride * i;
			_audioBuffers[i]->write(channelPtr, stride);
		}

		_buffersMutex.unlock();
		_feedMutex.unlock();
	}
}
