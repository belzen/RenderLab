#pragma once

#include "EntityComponent.h"
#include "render/RdrDrawOp.h"

class RdrAction;

class Renderable : public EntityComponent
{
public:
	virtual RdrDrawOpSet BuildDrawOps(RdrAction* pAction) = 0;

protected:
	virtual ~Renderable() {}
};
