#pragma once
#include "MathLib/Vec3.h"
#include "shapes/OBB.h"

class ModelComponent;

bool rayModelIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const ModelComponent* pModel, float* pOutT);
bool raySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& sphereCenter, const float radius, float* pOutT);
bool rayTriangleIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& triPtA, const Vec3& triPtB, const Vec3& triPtC, float* pOutT);
bool rayPlaneIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& planeNormal, const Vec3& planePt, float* pOutT);
bool rayObbIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const OBB& box, float* pOutT);
