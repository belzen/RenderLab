#include "Precompiled.h"
#include "Manipulator.h"
#include "Entity.h"
#include "components/ComponentAllocator.h"
#include "components/ModelComponent.h"
#include "render/RdrAction.h"
#include "Raycast.h"

namespace
{
	class EditorComponentAllocator : public IComponentAllocator
	{
	public:
		Light* AllocLight()
		{
			return m_lights.allocSafe();
		}

		ModelComponent* AllocModelComponent()
		{
			return m_models.allocSafe();
		}

		RigidBody* AllocRigidBody()
		{
			return m_rigidBodies.allocSafe();
		}

		SkyVolume* AllocSkyVolume()
		{
			return m_skyVolumes.allocSafe();
		}

		PostProcessVolume* AllocPostProcessVolume()
		{
			return m_postProcVolumes.allocSafe();
		}

		void ReleaseComponent(const Light* pComponent)
		{
			m_lights.releaseSafe(pComponent);
		}

		void ReleaseComponent(const ModelComponent* pComponent)
		{
			m_models.releaseSafe(pComponent);
		}

		void ReleaseComponent(const RigidBody* pComponent)
		{
			m_rigidBodies.releaseSafe(pComponent);
		}

		void ReleaseComponent(const SkyVolume* pComponent)
		{
			m_skyVolumes.releaseSafe(pComponent);
		}

		void ReleaseComponent(const PostProcessVolume* pComponent)
		{
			m_postProcVolumes.releaseSafe(pComponent);
		}

	private:
		FreeList<ModelComponent, 16> m_models;
		FreeList<RigidBody, 16> m_rigidBodies;
		FreeList<Light, 4> m_lights;
		FreeList<PostProcessVolume, 4> m_postProcVolumes;
		FreeList<SkyVolume, 4> m_skyVolumes;
	};

	EditorComponentAllocator s_editorComponentAllocator;


	Vec3 getPlaneNormal(const Vec3& cameraDir, int axis)
	{
		Vec3 planeNormal;
		if (axis == 0)
		{
			if (fabs(cameraDir.y) > 0.5f)
			{
				planeNormal = Vec3(0.f, 1.f, 0.f);
			}
			else
			{
				planeNormal = Vec3(0.f, 0.f, 1.f);
			}
		}
		else if (axis == 1)
		{
			if (fabs(cameraDir.x) > 0.5f)
			{
				planeNormal = Vec3(1.f, 0.f, 0.f);
			}
			else
			{
				planeNormal = Vec3(0.f, 0.f, 1.f);
			}
		}
		else if (axis == 2)
		{
			if (fabs(cameraDir.y) > 0.5f)
			{
				planeNormal = Vec3(0.f, 1.f, 0.f);
			}
			else
			{
				planeNormal = Vec3(1.f, 0.f, 0.f);
			}
		}

		return planeNormal;
	}
}


Manipulator::Manipulator()
{

}

void Manipulator::Init()
{
	const char* kMaterialNames[] = { "editor/red", "editor/green", "editor/blue" };

	AssetLib::MaterialSwap materialSwap;
	materialSwap.from = "white";

	const char* kTranslatorModelName = "editor/arrow";
	m_apTranslationHandles[0] = Entity::Create("TranslationHandleX", Vec3::kOrigin, Rotation(0.f, 0.f, -Maths::kHalfPi), Vec3::kOne);
	m_apTranslationHandles[1] = Entity::Create("TranslationHandleY", Vec3::kOrigin, Rotation::kIdentity, Vec3::kOne);
	m_apTranslationHandles[2] = Entity::Create("TranslationHandleZ", Vec3::kOrigin, Rotation(Maths::kHalfPi, 0.f, 0.f), Vec3::kOne);

	const char* kScaleModelName = "editor/scale";
	m_apScaleHandles[0] = Entity::Create("ScaleHandleX", Vec3::kOrigin, Rotation(0.f, 0.f, -Maths::kHalfPi), Vec3::kOne);
	m_apScaleHandles[1] = Entity::Create("ScaleHandleY", Vec3::kOrigin, Rotation::kIdentity, Vec3::kOne);
	m_apScaleHandles[2] = Entity::Create("ScaleHandleZ", Vec3::kOrigin, Rotation(Maths::kHalfPi, 0.f, 0.f), Vec3::kOne);

	const char* kRotationModelName = "editor/scale"; //todo
	m_apRotationHandles[0] = Entity::Create("RotationHandleX", Vec3::kOrigin, Rotation(0.f, 0.f, -Maths::kHalfPi), Vec3::kOne);
	m_apRotationHandles[1] = Entity::Create("RotationHandleY", Vec3::kOrigin, Rotation::kIdentity, Vec3::kOne);
	m_apRotationHandles[2] = Entity::Create("RotationHandleZ", Vec3::kOrigin, Rotation(Maths::kHalfPi, 0.f, 0.f), Vec3::kOne);

	for (int i = 0; i < 3; ++i)
	{
		materialSwap.to = kMaterialNames[i];
		m_apTranslationHandles[i]->AttachRenderable(ModelComponent::Create(&s_editorComponentAllocator, kTranslatorModelName, &materialSwap, 1));
		m_apScaleHandles[i]->AttachRenderable(ModelComponent::Create(&s_editorComponentAllocator, kScaleModelName, &materialSwap, 1));
		m_apRotationHandles[i]->AttachRenderable(ModelComponent::Create(&s_editorComponentAllocator, kRotationModelName, &materialSwap, 1));
	}

	SetMode(ManipulatorMode::kTranslation);
}

void Manipulator::SetMode(ManipulatorMode mode)
{
	m_mode = mode;
	switch (mode)
	{
	case ManipulatorMode::kTranslation:
		m_apActiveHandles = m_apTranslationHandles;
		break;
	case ManipulatorMode::kRotation:
		m_apActiveHandles = m_apRotationHandles;
		break;
	case ManipulatorMode::kScale:
		m_apActiveHandles = m_apScaleHandles;
		break;
	}
}

void Manipulator::Update(const Vec3& position, const Vec3& rayOrigin, const Vec3& rayDirection, const Camera& rCamera)
{
	for (int i = 0; i < 3; ++i)
	{
		m_apActiveHandles[i]->SetPosition(position);
	}

	if (m_pTarget)
	{
		switch (m_mode)
		{
		case ManipulatorMode::kTranslation:
			{
				Vec3 planeNormal = getPlaneNormal(rCamera.GetDirection(), m_axis);
				Vec3 planePoint = m_pTarget->GetPosition();

				float t;
				rayPlaneIntersect(rayOrigin, rayDirection, planeNormal, planePoint, &t);

				Vec3 pos = m_pTarget->GetPosition();
				pos[m_axis] = (rayOrigin + rayDirection * t)[m_axis];
				m_pTarget->SetPosition(pos + m_dragData.translation.offset);
			}
			break;
		case ManipulatorMode::kScale:
			{
				Vec3 planeNormal = getPlaneNormal(rCamera.GetDirection(), m_axis);
				Vec3 planePoint = m_pTarget->GetPosition();

				float t;
				rayPlaneIntersect(rayOrigin, rayDirection, planeNormal, planePoint, &t);

				Vec3 pos = m_pTarget->GetPosition();
				Vec3 rayHitPos = (rayOrigin + rayDirection * t);
				Vec3 newScale = m_dragData.scale.original;
				newScale[m_axis] += (rayHitPos[m_axis] - m_dragData.scale.dragStartPos[m_axis]) * 0.5f;
				m_pTarget->SetScale(newScale);
			}
			break;
		case ManipulatorMode::kRotation:
			// todo
			break;
		}
	}
}

bool Manipulator::Begin(const Camera& rCamera, const Vec3& rayDirection, Entity* pTarget)
{
	Vec3 rayOrigin = rCamera.GetPosition();
	Entity* pTargetHandle = nullptr;
	float closestT = FLT_MAX;
	int iHandle = 0;
	for (int i = 0; i < 3; ++i)
	{
		OBB box;
		box.center = m_apActiveHandles[i]->GetPosition();
		box.center[i] += 1.4f;
		box.axes[0] = Vec3(1.f, 0.f, 0.f);
		box.axes[1] = Vec3(0.f, 1.f, 0.f);
		box.axes[2] = Vec3(0.f, 0.f, 1.f);
		box.halfSize = Vec3(0.2f, 0.2f, 0.2f);
		box.halfSize[i] = 1.4f;

		float t;
		if (rayObbIntersect(rayOrigin, rayDirection, box, &t) && t < closestT)
		{
			closestT = t;
			pTargetHandle = m_apActiveHandles[i];
			iHandle = i;
		}
	}

	if (pTargetHandle)
	{
		m_pTarget = pTarget;
		m_axis = iHandle;

		Vec3 axis = Vec3::kZero;
		axis[m_axis] = 1.f;

		Vec3 planeNormal = getPlaneNormal(rCamera.GetDirection(), m_axis);
		Vec3 targetPosition = m_pTarget->GetPosition();

		float t;
		rayPlaneIntersect(rayOrigin, rayDirection, planeNormal, targetPosition, &t);

		Vec3 rayHitPos = rayOrigin + rayDirection * t;

		switch (m_mode)
		{
		case ManipulatorMode::kTranslation:
			m_dragData.translation.offset = Vec3::kZero;
			m_dragData.translation.offset[m_axis] = -(rayHitPos - targetPosition)[m_axis];
			break;
		case ManipulatorMode::kScale:
			m_dragData.scale.original = pTarget->GetScale();
			m_dragData.scale.dragStartPos = Vec3::kZero;
			m_dragData.scale.dragStartPos[m_axis] = rayHitPos[m_axis];
			break;
		case ManipulatorMode::kRotation:
			// todo
			break;
		}

		return true;
	}

	return false;
}

void Manipulator::End()
{
	m_pTarget = nullptr;
}

void Manipulator::QueueDraw(RdrAction* pAction)
{
	// TODO: Scale handles to be constant size regardless of distance.
	for (int i = 0; i < 3; ++i)
	{
		Renderable* pRenderable = m_apActiveHandles[i]->GetRenderable();
		RdrDrawOpSet ops = pRenderable->BuildDrawOps(pAction);
		for (int n = 0; n < ops.numDrawOps; ++n)
		{
			pAction->AddDrawOp(&ops.aDrawOps[n], RdrBucketType::Editor);
		}
	}
}
