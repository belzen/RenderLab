#include "Precompiled.h"
#include "Light.h"
#include "RdrContext.h"

LightList LightList::Create(RdrContext* pContext, const Light* aLights, int numLights)
{
	LightList list = { 0 };
	list.hResource = pContext->CreateStructuredBuffer(aLights, numLights, sizeof(Light));
	list.lightCount = numLights;
	return list;
}
