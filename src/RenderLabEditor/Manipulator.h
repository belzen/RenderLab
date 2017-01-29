#pragma once

class Entity;
class RdrAction;
class Camera;

enum class ManipulatorMode
{
	kTranslation,
	kRotation,
	kScale
};

class Manipulator
{
public:
	Manipulator();
	void Init();
	void SetMode(ManipulatorMode mode);

	void Update(const Vec3& position, const Vec3& rayOrigin, const Vec3& rayDir, const Camera& rCamera);
	void QueueDraw(RdrAction* pAction);

	bool Begin(const Camera& rCamera, const Vec3& rayDirection, Entity* pTarget);
	void End();

private:
	Entity* m_pTarget;

	Entity* m_apTranslationHandles[3];
	Entity* m_apRotationHandles[3];
	Entity* m_apScaleHandles[3];

	Entity** m_apActiveHandles;

	int m_axis;
	ManipulatorMode m_mode;

	struct  
	{
		struct
		{
			Vec3 offset;
		} translation;

		struct Htrms
		{
			Vec3 original;
			Vec3 dragStartPos;
		} scale;
	} m_dragData;
};