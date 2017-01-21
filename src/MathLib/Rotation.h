#pragma once

struct Quaternion;

struct Rotation
{
	static const Rotation kIdentity;

	Rotation();
	Rotation(float pitch, float yaw, float roll);

	static Rotation FromQuaternion(const Quaternion& q);

	float pitch;
	float yaw;
	float roll;
};

inline Rotation::Rotation()
	: pitch(0.f), yaw(0.f), roll(0.f)
{
}

inline Rotation::Rotation(float p, float y, float r)
	: pitch(p), yaw(y), roll(r)
{
}
