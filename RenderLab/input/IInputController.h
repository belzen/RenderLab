#pragma once

// Interface for input targets/controllers.
class IInputController
{
public:
	virtual void Update(float dt) = 0;
};
