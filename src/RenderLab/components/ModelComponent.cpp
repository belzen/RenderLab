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

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Model, RdrShaderFlags::None };

	//todo - Get layout desc from ModelData instead of hardcoding here
	static const RdrVertexInputElement s_modelVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 40, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, 48, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, 60, RdrVertexInputClass::PerVertex, 0 }
	};
}


ModelComponent* ModelComponent::Create(IComponentAllocator* pAllocator, const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps)
{
	ModelComponent* pModel = pAllocator->AllocModelComponent();
	pModel->m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAY_SIZE(s_modelVertexDesc));
	pModel->SetModelData(modelAssetName, aMaterialSwaps, numMaterialSwaps);
	return pModel;
}

void ModelComponent::Release()
{
	if (m_hVsPerObjectConstantBuffer)
	{
		g_pRenderer->GetPreFrameCommandList().ReleaseConstantBuffer(m_hVsPerObjectConstantBuffer);
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
			if (m_pMaterials[i]->name == aMaterialSwaps[n].from)
			{
				m_pMaterials[i] = RdrMaterial::LoadFromFile(aMaterialSwaps[n].to);
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
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		memset(pConstants, 0, constantsSize);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);

		m_hVsPerObjectConstantBuffer = pAction->GetResCommandList().CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer, 
			pConstants, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	
		if (CanInstance())
		{
			if (!m_instancedDataId)
			{
				m_instancedDataId = RdrInstancedObjectDataBuffer::AllocEntry();
			}

			VsPerObject* pObjectData = RdrInstancedObjectDataBuffer::GetEntry(m_instancedDataId);
			pObjectData->mtxWorld = Matrix44Transpose(mtxWorld);
		}
		else
		{
			m_instancedDataId = 0;
		}

		m_lastTransformId = m_pEntity->GetTransformId();
	}

	uint numSubObjects = m_pModelData->GetNumSubObjects();
	RdrDrawOp* aDrawOps = RdrFrameMem::AllocDrawOps(numSubObjects);

	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);
		RdrDrawOp& rDrawOp = aDrawOps[i];

		rDrawOp.instanceDataId = m_instancedDataId;
		rDrawOp.hVsConstants = m_hVsPerObjectConstantBuffer;
		rDrawOp.pMaterial = m_pMaterials[i];

		rDrawOp.hInputLayout = m_hInputLayout;
		rDrawOp.vertexShader = kVertexShader;
		if (rSubObject.pMaterial->bAlphaCutout)
		{
			rDrawOp.vertexShader.flags |= RdrShaderFlags::AlphaCutout;
		}

		rDrawOp.hGeo = rSubObject.hGeo;
		rDrawOp.bHasAlpha = false;
	}

	return RdrDrawOpSet(aDrawOps, numSubObjects);
}
