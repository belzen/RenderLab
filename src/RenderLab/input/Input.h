#pragma once

enum Keys
{
	KEY_CANCEL = 0x03,

	KEY_BACKSPACE = 0x08,
	KEY_TABE = 0x09,
	KEY_CLEAR = 0x0C,
	KEY_ENTER = 0x0D,
	KEY_SHIFT = 0x10,
	KEY_CONTROL = 0x11,
	KEY_MENU = 0x12,
	KEY_PAUSE = 0x13,
	KEY_CAPITAL = 0x14,
	KEY_ESC = 0x1B,
	KEY_SPACE = 0x20,
	KEY_PRIOR = 0x21,
	KEY_NEXT = 0x22,
	KEY_END = 0x23,
	KEY_HOME = 0x24,
	KEY_LEFT = 0x25,
	KEY_UP = 0x26,
	KEY_RIGHT = 0x27,
	KEY_DOWN = 0x28,

	KEY_INSERT = 0x2D,
	KEY_DELETE = 0x2E,

	KEY_0 = 0x30,
	KEY_1 = 0x31,
	KEY_2 = 0x32,
	KEY_3 = 0x33,
	KEY_4 = 0x34,
	KEY_5 = 0x35,
	KEY_6 = 0x36,
	KEY_7 = 0x37,
	KEY_8 = 0x38,
	KEY_9 = 0x39,

	KEY_A = 0x41,
	KEY_B = 0x42,
	KEY_C = 0x43,
	KEY_D = 0x44,
	KEY_E = 0x45,
	KEY_F = 0x46,
	KEY_G = 0x47,
	KEY_H = 0x48,
	KEY_I = 0x49,
	KEY_J = 0x4A,
	KEY_K = 0x4B,
	KEY_L = 0x4C,
	KEY_M = 0x4D,
	KEY_N = 0x4E,
	KEY_O = 0x4F,
	KEY_P = 0x50,
	KEY_Q = 0x51,
	KEY_R = 0x52,
	KEY_S = 0x53,
	KEY_T = 0x54,
	KEY_U = 0x55,
	KEY_V = 0x56,
	KEY_W = 0x57,
	KEY_X = 0x58,
	KEY_Y = 0x59,
	KEY_Z = 0x5A,

	KEY_NUMPAD0 = 0x60,
	KEY_NUMPAD1 = 0x61,
	KEY_NUMPAD2 = 0x62,
	KEY_NUMPAD3 = 0x63,
	KEY_NUMPAD4 = 0x64,
	KEY_NUMPAD5 = 0x65,
	KEY_NUMPAD6 = 0x66,
	KEY_NUMPAD7 = 0x67,
	KEY_NUMPAD8 = 0x68,
	KEY_NUMPAD9 = 0x69,
	KEY_MULTIPLY = 0x6A,
	KEY_ADD = 0x6B,
	KEY_SEPARATOR = 0x6C,
	KEY_SUBTRACT = 0x6D,
	KEY_DECIMAL = 0x6E,
	KEY_DIVIDE = 0x6F,
	KEY_F1 = 0x70,
	KEY_F2 = 0x71,
	KEY_F3 = 0x72,
	KEY_F4 = 0x73,
	KEY_F5 = 0x74,
	KEY_F6 = 0x75,
	KEY_F7 = 0x76,
	KEY_F8 = 0x77,
	KEY_F9 = 0x78,
	KEY_F10 = 0x79,
	KEY_F11 = 0x7A,
	KEY_F12 = 0x7B,
	KEY_F13 = 0x7C,
	KEY_F14 = 0x7D,
	KEY_F15 = 0x7E,
	KEY_F16 = 0x7F,
	KEY_F17 = 0x80,
	KEY_F18 = 0x81,
	KEY_F19 = 0x82,
	KEY_F20 = 0x83,
	KEY_F21 = 0x84,
	KEY_F22 = 0x85,
	KEY_F23 = 0x86,
	KEY_F24 = 0x87,

	KEY_NUMLOCK = 0x90,
	KEY_SCROLL = 0x91,

	KEY_TILDE = 0xc0,
};

class InputManager;

class IInputContext
{
public:
	virtual void Update(const InputManager& rInputManager) = 0;

	virtual void GainedFocus() = 0;
	virtual void LostFocus() = 0;

	virtual void HandleChar(char c) = 0;
	virtual void HandleKeyDown(int key, bool down) = 0;
	virtual void HandleMouseDown(int button, bool down, int x, int y) = 0;
	virtual void HandleMouseMove(int x, int y, int dx, int dy) = 0;
};

class InputManager
{
public:
	InputManager();

	// Manage the input focus stack.
	void PushContext(IInputContext* pContext);
	void PopContext();
	IInputContext* GetActiveContext();

	void Reset();

	bool IsKeyDown(int key) const;
	void SetKeyDown(int key, bool down);
	void HandleChar(char c);

	bool IsMouseDown(int button) const;
	void SetMouseDown(int button, bool down, int x, int y);
	void GetMouseMove(int& x, int& y) const;

	void GetMousePos(int& x, int& y) const;
	void SetMousePos(int x, int y);

private:
	bool m_keyStates[255];
	bool m_mouseStates[5];
	int m_mouseX, m_mouseY;
	int m_mouseMoveX, m_mouseMoveY;
	std::stack<IInputContext*> m_inputFocusStack;
};