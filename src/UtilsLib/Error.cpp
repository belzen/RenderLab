#include "Error.h"
#include <vadefs.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

void Warning(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);
}

void Error(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);

	assert(false);
}
