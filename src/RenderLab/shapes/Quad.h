#pragma once

struct Quad
{
	Vec3 topLeft;
	Vec3 bottomLeft;
	Vec3 topRight;
	Vec3 bottomRight;
};

inline void QuadTransform(Quad& rQuad, const Matrix44& mtx)
{
	rQuad.topLeft     = Vec3TransformCoord(rQuad.topLeft, mtx);
	rQuad.bottomLeft  = Vec3TransformCoord(rQuad.bottomLeft, mtx);
	rQuad.topRight    = Vec3TransformCoord(rQuad.topRight, mtx);
	rQuad.bottomRight = Vec3TransformCoord(rQuad.bottomRight, mtx);
}