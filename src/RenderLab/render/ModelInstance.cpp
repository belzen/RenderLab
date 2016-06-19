#include "Precompiled.h"
#include "ModelInstance.h"
#include "ModelData.h"
#include "RdrScratchMem.h"
#include "RdrDrawOp.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"

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

ModelInstance* ModelInstance::Create(const char* modelAssetName)
{
	ModelInstance* pModel = s_modelInstances.allocSafe();
	pModel->m_pModelData = ModelData::LoadFromFile(modelAssetName);
	memset(pModel->m_pSubObjectDrawOps, 0, sizeof(pModel->m_pSubObjectDrawOps));
	pModel->m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAY_SIZE(s_modelVertexDesc));

	return pModel;
}

void ModelInstance::Release()
{
	// todo: refcount and free as necessary
	//memset(m_subObjects, 0, sizeof(m_subObjects));
	//s_models.release(this);
}

void ModelInstance::PrepareDraw(const Matrix44& mtxWorld, bool transformChanged)
{
	if (transformChanged)
	{
		uint constantsSize = sizeof(Vec4) * 4;
		Vec4* pConstants = (Vec4*)RdrScratchMem::AllocAligned(constantsSize, 16);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);

		m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize, RdrCpuAccessFlags::None, RdrResourceUsage::Default);
	}

	uint numSubObjects = m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);

		if (transformChanged && m_pSubObjectDrawOps[i])
		{
			RdrDrawOp::QueueRelease(m_pSubObjectDrawOps[i]);
			m_pSubObjectDrawOps[i] = nullptr;
		}
		
		if (!m_pSubObjectDrawOps[i])
		{
			RdrDrawOp* pDrawOp = m_pSubObjectDrawOps[i] = RdrDrawOp::Allocate();
			pDrawOp->eType = RdrDrawOpType::Graphics;

			pDrawOp->graphics.hVsConstants = m_hVsPerObjectConstantBuffer;

			pDrawOp->graphics.pMaterial = rSubObject.pMaterial;

			pDrawOp->graphics.hInputLayout = m_hInputLayout;
			pDrawOp->graphics.vertexShader = kVertexShader;
			if (rSubObject.pMaterial->bAlphaCutout)
			{
				pDrawOp->graphics.vertexShader.flags |= RdrShaderFlags::AlphaCutout;
			}

			pDrawOp->graphics.hGeo = rSubObject.hGeo;
			pDrawOp->graphics.bHasAlpha = false;
		}
	}
}