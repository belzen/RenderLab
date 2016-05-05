#include "Vec3.h"
#include "Matrix44.h"
#include "Vec4.h"
#include "Quaternion.h"
#include "IVec3.h"

const Matrix44 Matrix44::kIdentity({
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f
});

const Vec3 Vec3::kOrigin(0.f, 0.f, 0.f);
const Vec3 Vec3::kOne(1.f, 1.f, 1.f);
const Vec3 Vec3::kZero(0.f, 0.f, 0.f);
const Vec3 Vec3::kUnitX(1.f, 0.f, 0.f);
const Vec3 Vec3::kUnitY(0.f, 1.f, 0.f);
const Vec3 Vec3::kUnitZ(0.f, 0.f, 1.f);

const Quaternion Quaternion::kIdentity(0.f, 0.f, 0.f, 1.f);

const IVec3 IVec3::kZero(0, 0, 0);
