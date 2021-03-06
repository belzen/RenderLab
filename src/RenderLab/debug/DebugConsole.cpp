#include "Precompiled.h"
#include "DebugConsole.h"
#include "render/RdrAction.h"
#include "render/Sprite.h"
#include "render/Font.h"
#include "UI.h"

namespace
{
	#define DEBUG_CMD_MAX_ARGS 4
	#define DEBUG_CMD_MAX_LEN 256
	#define DEBUG_FONT_SIZE 24.f

	struct DebugCommand
	{
		char name[256];
		DebugCommandArgType argTypes[DEBUG_CMD_MAX_ARGS];
		DebugCommandCallback func;
		int numArgs;
	};

	struct DebugCommandEntry
	{
		std::string command;
		RdrGeoHandle hTextGeo;
	};

	typedef std::map< std::string, DebugCommand, StringInvariantCompare > CommandMap;
	struct DebugConsoleData
	{
		DebugConsoleData()
			: historyIndex(-1)
			, currentCommandLen(0)
			, isActive(false)
		{
			currentCommand[0] = 0;
		}

		CommandMap commands;

		std::vector< DebugCommandEntry > commandHistory;
		int historyIndex;

		char currentCommand[DEBUG_CMD_MAX_LEN];
		int currentCommandLen;

		Sprite bgSprite;

		bool isActive;

	} s_debugConsole;

	class DebugInputContext : public IInputContext
	{
	public:
		void Update(const InputManager& rInputManager)
		{

		}

		void GainedFocus()
		{

		}

		void LostFocus()
		{

		}

		void HandleChar(char c)
		{
			const char kAsciiFirstReadableChar = 0x20; // Space
			const char kAsciiLastReadableChar = 0x7d; // Excluding tilde (0x7e)
			const char kAsciiBackspace = 0x08;
			const char kAsciiApostrophe = 0x60;

			if (c == kAsciiBackspace)
			{
				// Remove characters from the command.
				if (s_debugConsole.currentCommandLen > 0)
				{
					s_debugConsole.currentCommand[s_debugConsole.currentCommandLen - 1] = 0;
					s_debugConsole.currentCommandLen--;
				}
			}
			else if (c >= kAsciiFirstReadableChar && c < kAsciiLastReadableChar 
				&& c != kAsciiApostrophe)
			{
				// Add characters to the command.
				s_debugConsole.currentCommand[s_debugConsole.currentCommandLen] = c;
				s_debugConsole.currentCommandLen++;
				s_debugConsole.currentCommand[s_debugConsole.currentCommandLen] = 0;
			}
		}

		void HandleKeyDown(int key, bool down)
		{
			if (!down)
				return;
			
			if (key == KEY_UP)
			{
				if (s_debugConsole.historyIndex + 1 < (int)s_debugConsole.commandHistory.size())
				{
					++s_debugConsole.historyIndex;

					const DebugCommandEntry& cmd = s_debugConsole.commandHistory[s_debugConsole.commandHistory.size() - 1 - s_debugConsole.historyIndex];
					strcpy_s(s_debugConsole.currentCommand, cmd.command.c_str());
					s_debugConsole.currentCommandLen = (uint)cmd.command.length();
				}
			}
			else if (key == KEY_DOWN)
			{
				if (s_debugConsole.historyIndex == 0)
				{
					s_debugConsole.currentCommand[0] = 0;
					s_debugConsole.currentCommandLen = 0;
					s_debugConsole.historyIndex = -1;
				}
				else if (s_debugConsole.historyIndex > 0)
				{
					--s_debugConsole.historyIndex;

					const DebugCommandEntry& cmd = s_debugConsole.commandHistory[s_debugConsole.commandHistory.size() - 1 - s_debugConsole.historyIndex];
					strcpy_s(s_debugConsole.currentCommand, cmd.command.c_str());
					s_debugConsole.currentCommandLen = (uint)cmd.command.length();
				}
			}

			if (key == KEY_ENTER)
			{
				if (s_debugConsole.currentCommandLen != 0)
				{
					// Add to history
					DebugCommandEntry entry;
					entry.command = s_debugConsole.currentCommand;
					s_debugConsole.commandHistory.push_back(entry);

					// Parse and execute command.
					char* tok_context;
					char* argStrs[DEBUG_CMD_MAX_ARGS];
					int numArgs = 0;

					std::string cmdName = strtok_s(s_debugConsole.currentCommand, " ", &tok_context);
					do
					{
						argStrs[numArgs] = strtok_s(nullptr, " ", &tok_context);
						++numArgs;
					} while (argStrs[numArgs - 1] != nullptr && numArgs < DEBUG_CMD_MAX_ARGS);
					--numArgs;

					CommandMap::iterator cmdIter = s_debugConsole.commands.find(cmdName);

					if (cmdIter == s_debugConsole.commands.end())
					{
						// todo: output unknown command
					}
					else if (numArgs != cmdIter->second.numArgs)
					{
						// todo: output invalid args.
					}
					else
					{
						DebugCommandArg args[DEBUG_CMD_MAX_ARGS];
						for (int i = 0; i < numArgs; ++i)
						{
							DebugCommandArg& arg = args[i];
							arg.type = cmdIter->second.argTypes[i];
							
							switch (arg.type)
							{
							case DebugCommandArgType::Float:
								arg.val.fnum = (float)atof(argStrs[i]);
								break;
							case DebugCommandArgType::Integer:
								arg.val.inum = atoi(argStrs[i]);
								break;
							case DebugCommandArgType::String:
								arg.val.str = argStrs[i];
								break;
							}
						}

						cmdIter->second.func(args, numArgs - 1);
					}

					// Reset command state
					s_debugConsole.currentCommand[0] = 0;
					s_debugConsole.currentCommandLen = 0;
					s_debugConsole.historyIndex = -1;
				}
			}
		}

		void HandleMouseDown(int button, bool down, int x, int y) { }

		void HandleMouseMove(int x, int y, int dx, int dy) { }
	};

	DebugInputContext s_debugInputContext;
}

void DebugConsole::Init()
{
	s_debugConsole.isActive = false;

	Vec2 texcoords[4];
	texcoords[0] = Vec2(0.f, 0.f);
	texcoords[1] = Vec2(1.f, 0.f);
	texcoords[2] = Vec2(0.f, 1.f);
	texcoords[3] = Vec2(1.f, 1.f);

	s_debugConsole.bgSprite.Init(texcoords, "white");
}

void DebugConsole::RegisterCommand(const char* name, DebugCommandCallback func)
{
	DebugCommand cmd;
	strcpy_s(cmd.name, name);
	cmd.numArgs = 0;
	cmd.func = func;

	s_debugConsole.commands.insert(std::make_pair(std::string(cmd.name), cmd));
}

void DebugConsole::RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg)
{
	DebugCommand cmd;
	strcpy_s(cmd.name, name);
	cmd.argTypes[0] = arg;
	cmd.numArgs = 1;
	cmd.func = func;

	s_debugConsole.commands.insert(std::make_pair(std::string(cmd.name), cmd));
}

void DebugConsole::RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2)
{
	DebugCommand cmd;
	strcpy_s(cmd.name, name);
	cmd.argTypes[0] = arg1;
	cmd.argTypes[1] = arg2;
	cmd.numArgs = 2;
	cmd.func = func;

	s_debugConsole.commands.insert(std::make_pair(std::string(cmd.name), cmd));
}

void DebugConsole::RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2, DebugCommandArgType arg3)
{
	DebugCommand cmd;
	strcpy_s(cmd.name, name);
	cmd.argTypes[0] = arg1;
	cmd.argTypes[1] = arg2;
	cmd.numArgs = 3;
	cmd.func = func;

	s_debugConsole.commands.insert(std::make_pair(std::string(cmd.name), cmd));
}

void DebugConsole::RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2, DebugCommandArgType arg3, DebugCommandArgType arg4)
{
	DebugCommand cmd;
	strcpy_s(cmd.name, name);
	cmd.argTypes[0] = arg1;
	cmd.argTypes[1] = arg2;
	cmd.argTypes[2] = arg3;
	cmd.argTypes[3] = arg4;
	cmd.numArgs = 4;
	cmd.func = func;

	s_debugConsole.commands.insert(std::make_pair(std::string(cmd.name), cmd));
}

void DebugConsole::QueueDraw(RdrAction* pAction)
{
	if (!s_debugConsole.isActive)
		return;

	Vec2 size(pAction->GetViewport().width, 400.f);
	s_debugConsole.bgSprite.QueueDraw(pAction, Vec3::kZero, size, 0.25f);

	if (s_debugConsole.currentCommandLen > 0)
	{
		UI::Position pos(0.f, size.y - DEBUG_FONT_SIZE, 0.f, UI::AlignmentFlags::Default);
		Font::QueueDraw(pAction, pos, DEBUG_FONT_SIZE, s_debugConsole.currentCommand, Color::kWhite);
	}
}

void DebugConsole::ToggleActive(InputManager& rInputManager)
{
	if (s_debugConsole.isActive)
	{
		s_debugConsole.isActive = false;
		rInputManager.PopContext();
	}
	else
	{
		s_debugConsole.isActive = true;
		rInputManager.PushContext(&s_debugInputContext);
	}
}

