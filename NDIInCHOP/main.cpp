//
//  main.cpp
//  NDIInCHOP
//
//  Created by Valentin Dufois on 2020-03-26.
//  Copyright © 2020 Derivative. All rights reserved.
//

#include "NDIInCHOP.h"

extern "C" {
DLLEXPORT void FillCHOPPluginInfo(CHOP_PluginInfo *info) {
	// This must always be set to this constant
	info->apiVersion = CHOPCPlusPlusAPIVersion;;

	// The opType is the unique name for this TOP. It must start with a
	// capital A-Z character, and all the following characters must lower case
	// or numbers (a-z, 0-9)
	info->customOPInfo.opType->setString("Ndiinchop");

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

DLLEXPORT CHOP_CPlusPlusBase* CreateCHOPInstance(const OP_NodeInfo* info) {
	return new NDIInCHOP(info);
}

DLLEXPORT void DestroyCHOPInstance(CHOP_CPlusPlusBase* instance) {
	delete reinterpret_cast<NDIInCHOP *>(instance);
}
};
