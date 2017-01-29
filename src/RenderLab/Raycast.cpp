#include "Precompiled.h"
#include "Raycast.h"
#include "components/ModelComponent.h"
#include "AssetLib/ModelAsset.h"

// Most of these come from Real-Time Rendering
//http://www.realtimerendering.com/intersections.html

bool raySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& sphereCenter, const float radius, float* pOutT)
{
	Vec3 l = sphereCenter - rayOrigin;
	float s = Vec3Dot(l, rayDir);
	float lenSqrL = Vec3Dot(l, l);
	if (s < 0 && lenSqrL > radius * radius)
		return false;

	float m2 = lenSqrL - s * s;
	if (m2 > radius * radius)
		return false;

	float q = sqrt(radius * radius - m2);
	if (lenSqrL > radius * radius)
		*pOutT = s - q;
	else
		*pOutT = s + q;

	return true;
}

bool rayTriangleIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& triPtA, const Vec3& triPtB, const Vec3& triPtC, float* pOutT)
{
	Vec3 e1 = triPtB - triPtA;
	Vec3 e2 = triPtC - triPtA;
	Vec3 q = Vec3Cross(rayDir, e2);
	float a = Vec3Dot(e1, q);
	if (a > -Maths::kEpsilon && a < Maths::kEpsilon)
		return false;

	float f = 1 / a;
	Vec3 s = rayOrigin - triPtA;
	float u = f * Vec3Dot(s, q);
	if (u < 0.f)
		return false;

	Vec3 r = Vec3Cross(s, e1);
	float v = f * Vec3Dot(rayDir, r);
	if (v < 0.f || u + v > 1.f)
		return false;

	*pOutT = f * Vec3Dot(e2, r);
	if (*pOutT > Maths::kEpsilon)
		return true;

	return false;
}

bool rayModelIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const ModelComponent* pModel, float* pOutT)
{
	Matrix44 mtxWorld = pModel->GetEntity()->GetTransform();
	Vec3 pos = pModel->GetEntity()->GetPosition();
	float radius = pModel->GetRadius();

	float sphereT;
	if (!raySphereIntersect(rayOrigin, rayDir, pos, radius, &sphereT))
		return false;

	float closestT = FLT_MAX;
	bool hit = false;

	const AssetLib::Model* pData = pModel->GetModelData()->GetSource();
	for (uint i = 0; i < pData->totalIndexCount; i += 3)
	{
		Vec3 triA = pData->positions.ptr[pData->indices.ptr[i + 0]];
		Vec3 triB = pData->positions.ptr[pData->indices.ptr[i + 1]];
		Vec3 triC = pData->positions.ptr[pData->indices.ptr[i + 2]];

		triA = Vec3TransformCoord(triA, mtxWorld);
		triB = Vec3TransformCoord(triB, mtxWorld);
		triC = Vec3TransformCoord(triC, mtxWorld);

		float t;
		if (rayTriangleIntersect(rayOrigin, rayDir, triA, triB, triC, &t) && t < closestT)
		{
			closestT = t;
			hit = true;
		}
	}

	*pOutT = closestT;
	return hit;
}

bool rayPlaneIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& planeNormal, const Vec3& planePt, float* pOutT)
{
	float ndu = Vec3Dot(planeNormal, rayDir);
	if (fabs(ndu) < 0.001f)
		return 0.f;

	Vec3 w = rayOrigin - planePt;
	float ndw = Vec3Dot(planeNormal, w);

	*pOutT = -ndw / ndu;
	return *pOutT > Maths::kEpsilon;
}

bool rayObbIntersect(const Vec3& rayOrigin, const Vec3& rayDir, const OBB& box, float* pOutT)
{
	float tMin = -FLT_MAX;
	float tMax = FLT_MAX;

	Vec3 p = box.center - rayOrigin;
	for (int i = 0; i < 3; ++i)
	{
		float e = Vec3Dot(box.axes[i], p);
		float f = Vec3Dot(box.axes[i], rayDir);
		if (fabs(f) > Maths::kEpsilon)
		{
			float t1 = (e + box.halfSize[i]) / f;
			float t2 = (e - box.halfSize[i]) / f;
			if (t1 > t2)
			{
				float t = t1;
				t1 = t2;
				t2 = t;
			}

			if (t1 > tMin)
				tMin = t1;

			if (t2 < tMax)
				tMax = t2;

			if (tMin > tMax)
				return false;

			if (tMax < 0)
				return false;
		}
		else if (-e - box.halfSize[i] > 0.f || -e + box.halfSize[i] < 0.f)
		{
			return false;
		}
	}

	*pOutT = (tMin > 0.f) ? tMin : tMax;
	return true;
}