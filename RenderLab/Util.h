#pragma once

#define SAFE_DELETE(mem) if ( mem ) { delete mem; }

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
