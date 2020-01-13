#pragma once

void Warning(const char* msg, ...);
void Error(const char* msg, ...);
void AssertV(const char* strConditionText, bool bResult, const char* msg, ...);

#define Assert(condition) AssertV(#condition, (condition), nullptr)
#define AssertMsg(condition, msg, ...) AssertV(#condition, (condition), msg, __VA_ARGS__);