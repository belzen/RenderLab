#pragma once
#include <string>

// Misc utility functions.

#define SAFE_DELETE(mem) if ( mem ) { delete mem; mem = nullptr; }

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0])) // TODO: Does this compile out in for loops?

#define ENUM_FLAGS(enumType) \
	inline enumType operator & (const enumType lhs, const enumType rhs) \
	{ \
		return (enumType)((int)lhs & (int)rhs); \
	} \
	\
	inline enumType operator | (const enumType lhs, const enumType rhs) \
	{ \
		return (enumType)((int)lhs | (int)rhs); \
	} \
	\
	inline enumType operator |= (enumType& lhs, const enumType rhs) \
	{ \
		lhs = static_cast<enumType>((int)lhs | (int)rhs); \
		return lhs; \
	}

template<typename TFlagsType>
bool IsFlagSet(TFlagsType setFlags, TFlagsType checkFlag)
{
	return (int)(setFlags & checkFlag) != 0;
}

struct StringInvariantCompare
{
	// Less than operator
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

// Random float between 0.0 and 1.0.
inline float randFloat()
{
	return rand() / (float)RAND_MAX;
}

// Random float between min and max
inline float randFloatRange(float min, float max)
{
	return min + (max - min) * randFloat();
}

inline float lerp(float start, float end, float blend)
{
	return start * (1.f - blend) + end * blend;
}