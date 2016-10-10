#pragma once
#include "../Types.h"
#include "Json/json.h"
#include "MathLib/Maths.h"

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

inline Quaternion jsonReadRotation(const Json::Value& val)
{
	Vec3 pitchYawRoll;
	pitchYawRoll.x = val.get((uint)0, Json::Value(0.f)).asFloat();
	pitchYawRoll.y = val.get((uint)1, Json::Value(0.f)).asFloat();
	pitchYawRoll.z = val.get((uint)2, Json::Value(0.f)).asFloat();

	return QuaternionPitchYawRoll(
		Maths::DegToRad(pitchYawRoll.x),
		Maths::DegToRad(pitchYawRoll.y),
		Maths::DegToRad(pitchYawRoll.z));
}

inline Vec3 jsonReadPitchYawRoll(const Json::Value& val)
{
	Vec3 pitchYawRoll;
	pitchYawRoll.x = Maths::DegToRad(val.get((uint)0, Json::Value(0.f)).asFloat());
	pitchYawRoll.y = Maths::DegToRad(val.get((uint)1, Json::Value(0.f)).asFloat());
	pitchYawRoll.z = Maths::DegToRad(val.get((uint)2, Json::Value(0.f)).asFloat());
	return pitchYawRoll;
}