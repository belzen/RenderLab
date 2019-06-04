#include "Precompiled.h"
#include "ModelComponent.h"
#include "ComponentAllocator.h"
#include "AssetLib/SceneAsset.h"
#include "render/ModelData.h"
#include "render/RdrFrameMem.h"
#include "render/RdrDrawOp.h"
#include "render/RdrResourceSystem.h"
#include "render/RdrShaderSystem.h"
#include "render/RdrInstancedObjectDataBuffer.h"
#include "render/RdrAction.h"
#include "render/Renderer.h"

ModelComponent* ModelComponent::Create(IComponentAllocator* pAllocator, const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps)
{
	ModelComponent* pModel = pAllocator->AllocModelComponent();
	pModel->SetModelData(modelAssetName, aMaterialSwaps, numMaterialSwaps);
	return pModel;
}

void ModelComponent::Release()
{
	if (m_hVsPerObjectConstantBuffer)
	{
		g_pRenderer->GetResourceCommandList().ReleaseConstantBuffer(m_hVsPerObjectConstantBuffer, CREATE_BACKPOINTER(this));
		m_hVsPerObjectConstantBuffer = 0;
	}

	memset(m_pMaterials, 0, sizeof(m_pMaterials));

	m_pAllocator->ReleaseComponent(this);
}

void ModelComponent::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void ModelComponent::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}

bool ModelComponent::CanInstance() const
{
	// TODO: Heuristic based on tri count.  Instancing seems to perform best on low-mid triangle counts.
	return true;
}

void ModelComponent::SetModelData(const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps)
{
	m_pModelData = ModelData::LoadFromFile(modelAssetName);

	// Apply material swaps.
	uint numSubObjects = m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);
		m_pMaterials[i] = rSubObject.pMaterial;

		for (uint n = 0; n < numMaterialSwaps; ++n)
		{
			if (m_pMaterials[i]->GetName() == aMaterialSwaps[n].from)
			{
				m_pMaterials[i] = RdrMaterial::Create(aMaterialSwaps[n].to, m_pModelData->GetVertexElements(), m_pModelData->GetNumVertexElements());
			}
		}
	}
}

RdrDrawOpSet ModelComponent::BuildDrawOps(RdrAction* pAction)
{
	if (!m_hVsPerObjectConstantBuffer || m_lastTransformId != m_pEntity->GetTransformId())
	{
		Matrix44 mtxWorld = m_pEntity->GetTransform();

		uint constantsSize = sizeof(VsPerObject);
		VsPerObject* pVsPerObject = (VsPerObject*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pVsPerObject->mtxWorld = Matrix44Transpose(mtxWorld);

		m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer,
			pVsPerObject, constantsSize, RdrResourceAccessFlags::GpuRead, CREATE_BACKPOINTER(this));
	
		if (CanInstance())
		{
			if (!m_instancedDataId)
			{
				m_instancedDataId = RdrInstancedObjectDataBuffer::AllocEntry();
			}
		}
		else
		{
			m_instancedDataId = 0;
		}

		m_lastTransformId = m_pEntity->GetTransformId();
	}

	uint numSubObjects = m_pModelData->GetNumSubObjects();
	RdrDrawOp* aDrawOps = RdrFrameMem::AllocDrawOps(numSubObjects, CREATE_BACKPOINTER(this));

	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);
		RdrDrawOp& rDrawOp = aDrawOps[i];

		rDrawOp.instanceDataId = m_instancedDataId;
		rDrawOp.hVsConstants = m_hVsPerObjectConstantBuffer;
		rDrawOp.pMaterial = m_pMaterials[i];

		rDrawOp.hGeo = rSubObject.hGeo;
		rDrawOp.bHasAlpha = rDrawOp.pMaterial->HasAlpha();
	}

	return RdrDrawOpSet(aDrawOps, numSubObjects);
}
