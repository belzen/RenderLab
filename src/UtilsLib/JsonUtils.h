#pragma once
#include "../Types.h"
#include "Json/json.h"
#include "MathLib/Maths.h"
#include "StringCache.h"

inline void jsonReadString(const Json::Value& val, char* pOutStr, uint maxLen)
{
	if (val.isNull())
	{
		pOutStr[0] = 0;
	}
	else
	{
		strcpy_s(pOutStr, maxLen, val.asCString());
	}
}

inline void jsonReadCachedString(const Json::Value& val, CachedString* pOutStr)
{
	if (!val.isNull())
	{
		*pOutStr = val.asCString();
	}
}

inline Vec2 jsonReadVec2(const Json::Value& val)
{
	Vec2 vec;
	vec.x = val.get((uint)0, Json::Value(0.f)).asFloat();
	vec.y = val.get((uint)1, Json::Value(0.f)).asFloat();
	return vec;
}

inline Vec3 jsonReadVec3(const Json::Value& val)
{
	Vec3 vec;
	vec.x = val.get((uint)0, Json::Value(0.f)).asFloat();
	vec.y = val.get((uint)1, Json::Value(0.f)).asFloat();
	vec.z = val.get((uint)2, Json::Value(0.f)).asFloat();
	return vec;
}

inline Vec3 jsonReadScale(const Json::Value& val)
{
	Vec3 vec;
	vec.x = val.get((uint)0, Json::Value(1.f)).asFloat();
	vec.y = val.get((uint)1, Json::Value(1.f)).asFloat();
	vec.z = val.get((uint)2, Json::Value(1.f)).asFloat();
	return vec;
}

inline Rotation jsonReadRotation(const Json::Value& val)
{
	Rotation r;
	r.pitch = Maths::DegToRad(val.get((uint)0, Json::Value(0.f)).asFloat());
	r.yaw = Maths::DegToRad(val.get((uint)1, Json::Value(0.f)).asFloat());
	r.roll = Maths::DegToRad(val.get((uint)2, Json::Value(0.f)).asFloat());
	return r;
}