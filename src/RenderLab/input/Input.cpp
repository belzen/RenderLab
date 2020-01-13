#include "Precompiled.h"
#include "Input.h"
#include "debug\DebugConsole.h"

InputManager::InputManager()
{
	memset(m_keyStates, 0, sizeof(m_keyStates));
	memset(m_mouseStates, 0, sizeof(m_mouseStates));
}

void InputManager::PushContext(IInputContext* pController)
{
	if ( !m_inputFocusStack.empty() )
		m_inputFocusStack.top()->LostFocus();

	m_inputFocusStack.push(pController);
	m_inputFocusStack.top()->GainedFocus();
}

void InputManager::PopContext()
{
	Assert(m_inputFocusStack.size() > 1);
	m_inputFocusStack.top()->LostFocus();
	m_inputFocusStack.pop();
	m_inputFocusStack.top()->GainedFocus();
}

IInputContext* InputManager::GetActiveContext()
{
	return m_inputFocusStack.top();
}

void InputManager::Reset()
{
	m_mouseMoveX = 0;
	m_mouseMoveY = 0;
}

bool InputManager::IsKeyDown(int key) const
{
	return m_keyStates[key];
}

void InputManager::SetKeyDown(int key, bool down)
{
	bool changed = (m_keyStates[key] != down);
	m_keyStates[key] = down;

	IInputContext* pContext = m_inputFocusStack.top();
	if (changed && down && key == KEY_TILDE)
	{
		// ~ reserved for debug console.
		DebugConsole::ToggleActive(*this);
	}
	else if (changed)
	{
		pContext->HandleKeyDown(key, down);
	}
}

void InputManager::HandleChar(char c)
{
	IInputContext* pContext = m_inputFocusStack.top();
	pContext->HandleChar(c);
}

bool InputManager::IsMouseDown(int button) const
{
	return m_mouseStates[button];
}

void InputManager::SetMouseDown(int button, bool down, int x, int y)
{
	m_mouseStates[button] = down;
	m_mouseX = x;
	m_mouseY = y;
	m_mouseMoveX = 0;
	m_mouseMoveY = 0;

	m_inputFocusStack.top()->HandleMouseDown(button, down, x, y);
}

void InputManager::GetMouseMove(int& x, int& y) const
{
	x = m_mouseMoveX;
	y = m_mouseMoveY;
}

void InputManager::GetMousePos(int& x, int& y) const
{
	x = m_mouseX;
	y = m_mouseY;
}

void InputManager::SetMousePos(int x, int y)
{
	m_mouseMoveX = x - m_mouseX;
	m_mouseMoveY = y - m_mouseY;
	m_mouseX = x;
	m_mouseY = y;

	m_inputFocusStack.top()->HandleMouseMove(x, y, m_mouseMoveX, m_mouseMoveY);
}
