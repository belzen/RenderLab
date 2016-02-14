#include "Precompiled.h"
#include "Input.h"
#include "IInputController.h"
#include <memory.h>
#include <stack>

namespace
{
	bool s_keyStates[255] = { 0 };
	bool s_mouseStates[5] = { 0 };
	int s_mouseX, s_mouseY;
	int s_mouseMoveX, s_mouseMoveY;
	std::stack<IInputController*> s_inputFocusStack;
}

void Input::PushController(IInputController* pController)
{
	s_inputFocusStack.push(pController);
}

void Input::PopController()
{
	assert(s_inputFocusStack.size() > 1);
	s_inputFocusStack.pop();
}

IInputController* Input::GetActiveController()
{
	return s_inputFocusStack.top();
}


void Input::Reset()
{
	s_mouseMoveX = 0;
	s_mouseMoveY = 0;
}

bool Input::IsKeyDown(int key)
{
	return s_keyStates[key];
}

void Input::SetKeyDown(int key, bool down)
{
	s_keyStates[key] = down;
}

bool Input::IsMouseDown(int button)
{
	return s_mouseStates[button];
}

void Input::SetMouseDown(int button, bool down, int x, int y)
{
	s_mouseStates[button] = down;
	s_mouseX = x;
	s_mouseY = y;
	s_mouseMoveX = 0;
	s_mouseMoveY = 0;
}

void Input::GetMouseMove(int& x, int& y)
{
	x = s_mouseMoveX;
	y = s_mouseMoveY;
}

void Input::GetMousePos(int& x, int& y)
{
	x = s_mouseX;
	y = s_mouseY;
}

void Input::SetMousePos(int x, int y)
{
	s_mouseMoveX = x - s_mouseX;
	s_mouseMoveY = y - s_mouseY;
	s_mouseX = x;
	s_mouseY = y;
}
