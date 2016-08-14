#pragma once

enum Keys
{
	KEY_BACKSPACE = 0x08,
	KEY_ENTER = 0x0D,
	KEY_ESC = 0x1B,
	KEY_SPACE = 0x20,

	KEY_LEFT = 0x25,
	KEY_UP = 0x26,
	KEY_RIGHT = 0x27,
	KEY_DOWN = 0x28,

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

	KEY_F11 = 0x7a,

	KEY_TILDE = 0xc0,
};

class InputManager;

class IInputContext
{
public:
	virtual void Update(const InputManager& rInputManager, float dt) = 0;

	virtual void GainedFocus() = 0;
	virtual void LostFocus() = 0;

	virtual void HandleKeyDown(int key, bool down) = 0;
	virtual void HandleMouseDown(int button, bool down, int x, int y) = 0;
	virtual void HandleMouseMove(int x, int y, int dx, int dy) = 0;

	// Whether to send key down repeat events to this context.
	// Mainly used by text input contexts.
	virtual bool WantsKeyDownRepeat() = 0;
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