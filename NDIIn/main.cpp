//
//  main.cpp
//  NDIInTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include "NDIIn.h"

// Uncomment this if you want to run an example that fills the data using threading
//#define THREADING_EXAMPLE

// The threading example can run in two modes. One where the producer is continually
// producing new frames that the consumer (main thread) picks up when able.
// This more is useful for things such as external device input. This is the default
// mode.

// The second mode can be instead used by defining THREADING_SINGLED_PRODUCER.
// (I.e Uncommenting the below line)
// In this mode the main thread will signal to the producer thread to generate a new
// frame each time it consumes a frame.
// Assuming the producer will generate a new frame in time before execute() gets called
// again this gives a better 1:1 sync between producing and consuming frames.

//#define THREADING_SIGNALED_PRODUCER


// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
void
FillTOPPluginInfo(TOP_PluginInfo *info)
{
	// This must always be set to this constant
	info->apiVersion = TOPCPlusPlusAPIVersion;

	// Change this to change the executeMode behavior of this plugin.
	info->executeMode = TOP_ExecuteMode::CPUMemWriteOnly;

	// The opType is the unique name for this TOP. It must start with a
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Ndiintop");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("NDI In");

	// Will be turned into a 3 letter icon on the nodes
	info->customOPInfo.opIcon->setString("NDI");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Valentin Dufois");
	info->customOPInfo.authorEmail->setString("valentin@dufois.fr");

	// This TOP works with 1 input connected
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 0;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new NDIIn(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (NDIIn*)instance;
}

};
