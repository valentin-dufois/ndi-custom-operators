//
//  main.cpp
//  NDIOutTOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include "NDIOut.h"

extern "C"
{

DLLEXPORT
void
FillTOPPluginInfo(TOP_PluginInfo *info)
{
	// This must always be set to this constant
	info->apiVersion = TOPCPlusPlusAPIVersion;

	// Change this to change the executeMode behavior of this plugin.
	info->executeMode = TOP_ExecuteMode::CPUMemReadWrite;

	// The opType is the unique name for this TOP. It must start with a
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Ndiouttop");

	// The opLabel is the text that will show up in the OP Create Dialog
	info->customOPInfo.opLabel->setString("NDI Out");

	// Will be turned into a 3 letter icon on the nodes
	info->customOPInfo.opIcon->setString("NDI");

	// Information about the author of this OP
	info->customOPInfo.authorName->setString("Valentin Dufois");
	info->customOPInfo.authorEmail->setString("valentin@dufois.fr");

	// This TOP works with 1 input connected
	info->customOPInfo.minInputs = 1;
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new NDIOut(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (NDIOut*)instance;
}

};
