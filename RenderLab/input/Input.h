#pragma once

class IInputController;

enum Keys
{
	KEY_ESC = 0x1B,
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
};

namespace Input
{
	// Manage the input focus stack.
	void PushController(IInputController* pController);
	void PopController();
	IInputController* GetActiveController();

	void Reset();

	bool IsKeyDown(int key);
	void SetKeyDown(int key, bool down);

	bool IsMouseDown(int button);
	void SetMouseDown(int button, bool down, int x, int y);
	void GetMouseMove(int& x, int& y);

	void GetMousePos(int& x, int& y);
	void SetMousePos(int x, int y);
}