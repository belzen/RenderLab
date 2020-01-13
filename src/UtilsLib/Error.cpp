#include "Error.h"
#include <vadefs.h>
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <debugapi.h>
#include <stdlib.h>

void Warning(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);
}

void Error(const char* msg, ...)
{
	char formattedMsg[1024] = { 0 };
	if (msg)
	{
		va_list args;
		va_start(args, msg);
		vprintf(msg, args);
		vsprintf_s(formattedMsg, msg, args);
		va_end(args);
	}

	strcat_s(formattedMsg, "\n\nPress retry to debug.");

	int result = MessageBoxA(NULL, formattedMsg, "Error!", MB_ABORTRETRYIGNORE);
	switch (result)
	{
	case IDABORT:
		exit(-101);
		break;
	case IDRETRY:
		DebugBreak();
		break;
	case IDIGNORE:
		break;
	}
}

void AssertV(const char* strConditionText, bool bResult, const char* msg, ...)
{
	if (bResult)
		return;

	char formattedMsg[1024] = { 0 };
	if (msg)
	{
		va_list args;
		va_start(args, msg);
		vprintf(msg, args);
		vsprintf_s(formattedMsg, msg, args);
		va_end(args);
	}

	strcat_s(formattedMsg, "\n");
	strcat_s(formattedMsg, strConditionText);
	strcat_s(formattedMsg, "\n\nPress retry to debug.");

	int result = MessageBoxA(NULL, formattedMsg, "Assert!", MB_ABORTRETRYIGNORE);
	switch (result)
	{
	case IDABORT:
		exit(-103);
		break;
	case IDRETRY:
		DebugBreak();
		break;
	case IDIGNORE:
		break;
	}
}