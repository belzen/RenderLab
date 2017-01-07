#include "Precompiled.h"
#include "ModelInstance.h"
#include "render/ModelData.h"
#include "render/RdrFrameMem.h"
#include "render/RdrDrawOp.h"
#include "render/RdrResourceSystem.h"
#include "render/RdrShaderSystem.h"
#include "render/RdrInstancedObjectDataBuffer.h"
#include "render/RdrFrameState.h"
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

	ModelInstanceFreeList s_modelInstances;
}

ModelInstance* ModelInstance::Create(const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps)
{
	ModelInstance* pModel = s_modelInstances.allocSafe();
	pModel->m_pModelData = ModelData::LoadFromFile(modelAssetName);
	pModel->m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAY_SIZE(s_modelVertexDesc));

	// Apply material swaps.
	uint numSubObjects = pModel->m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = pModel->m_pModelData->GetSubObject(i);
		pModel->m_pMaterials[i] = rSubObject.pMaterial;

		for (uint n = 0; n < numMaterialSwaps; ++n)
		{
			if (pModel->m_pMaterials[i]->name == aMaterialSwaps[n].from)
			{
				pModel->m_pMaterials[i] = RdrMaterial::LoadFromFile(aMaterialSwaps[n].to);
			}
		}
	}

	return pModel;
}

void ModelInstance::Release()
{
	// todo: refcount and free as necessary
	//memset(m_subObjects, 0, sizeof(m_subObjects));
	//s_models.release(this);
}

void ModelInstance::OnAttached(WorldObject* pObject)
{
}

void ModelInstance::OnDetached(WorldObject* pObject)
{
}

bool ModelInstance::CanInstance() const
{
	// TODO: Heuristic based on tri count.  Instancing seems to perform best on low-mid triangle counts.
	return true;
}

void ModelInstance::QueueDraw(RdrDrawBuckets* pDrawBuckets, const Matrix44& mtxWorld, bool transformChanged)
{
	if (transformChanged || !m_hVsPerObjectConstantBuffer)
	{
		uint constantsSize = sizeof(VsPerObject);
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		memset(pConstants, 0, constantsSize);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);

		m_hVsPerObjectConstantBuffer = g_pRenderer->GetActionCommandList()->CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer, 
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
	}

	uint numSubObjects = m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);

		RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();

		pDrawOp->instanceDataId = m_instancedDataId;
		pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
		pDrawOp->pMaterial = m_pMaterials[i];

		pDrawOp->hInputLayout = m_hInputLayout;
		pDrawOp->vertexShader = kVertexShader;
		if (rSubObject.pMaterial->bAlphaCutout)
		{
			pDrawOp->vertexShader.flags |= RdrShaderFlags::AlphaCutout;
		}

		pDrawOp->hGeo = rSubObject.hGeo;
		pDrawOp->bHasAlpha = false;

		pDrawBuckets->AddDrawOp(pDrawOp, RdrBucketType::Opaque);
	}
}
