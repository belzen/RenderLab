#pragma once

#include "UtilsLib/Array.h"
#include "UtilsLib/StringCache.h"
#include "RdrShaders.h"
#include "RdrResource.h"


enum class RdrMaterialFlags
{
	None			= 0x0,
	NeedsLighting	= 0x1,
	AlphaCutout		= 0x2,
	HasAlpha		= 0x4,
};
ENUM_FLAGS(RdrMaterialFlags);

struct RdrTessellationMaterialData
{
	RdrConstantBufferHandle hDsConstants;
	Array<RdrResourceHandle, 2> ahResources;
	Array<RdrSamplerState, 2> aSamplers;

	const RdrDescriptorTable* pResourceDescriptorTable;
	const RdrDescriptorTable* pSamplerDescriptorTable;
};

class RdrMaterial
{
public:
	static const uint kMaxTextures = 12;

	// Retrieve a unique id for the material.
	static uint16 GetMaterialId(const RdrMaterial* pMaterial);
	static RdrMaterial* Create(const CachedString& materialName, const RdrVertexInputElement* pInputLayout, uint nNumInputElements);
	static void ReloadMaterial(const char* materialName);

public:
	RdrMaterial();
	~RdrMaterial();

	void Init(const CachedString& name, RdrMaterialFlags flags);
	void Cleanup();

	const CachedString& GetName() const { return m_name; }
	bool HasAlpha() const { return IsFlagSet(m_flags, RdrMaterialFlags::HasAlpha); }
	bool NeedsLighting() const { return IsFlagSet(m_flags, RdrMaterialFlags::NeedsLighting); }

	bool IsShaderStageActive(RdrShaderMode eMode, RdrShaderStageFlags eStageFlag) const
	{
		return (m_activeShaderStages[(uint)eMode] & eStageFlag) != RdrShaderStageFlags::None;
	}

	void CreatePipelineState(
		RdrShaderMode eMode,
		const RdrVertexShader& vertexShader, const RdrShader* pPixelShader,
		const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		const RdrBlendMode eBlendMode,
		const RdrRasterState& rasterState,
		const RdrDepthStencilState& depthStencilState);
	void CreateTessellationPipelineState(
		RdrShaderMode eMode,
		const RdrVertexShader& vertexShader, const RdrShader* pPixelShader,
		const RdrShader* pHullShader, const RdrShader* pDomainShader,
		const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		const RdrBlendMode eBlendMode,
		const RdrRasterState& rasterState,
		const RdrDepthStencilState& depthStencilState);

	void SetTextures(uint startSlot, uint count, const RdrResourceHandle* ahTextures);
	void SetSamplers(uint startSlot, uint count, const RdrSamplerState* aSamplers);
	void FillConstantBuffer(void* pData, uint nDataSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	void SetTessellationTextures(uint startSlot, uint count, const RdrResourceHandle* ahTextures);
	void SetTessellationSamplers(uint startSlot, uint count, const RdrSamplerState* aSamplers);
	void FillTessellationConstantBuffer(void* pData, uint nDataSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	const RdrTessellationMaterialData& GetTessellationData() const { return m_tessellation; }
	const RdrPipelineState& GetPipelineState(RdrShaderMode eMode) const { return m_hPipelineStates[(int)eMode]; }
	RdrShaderStageFlags GetActiveShaderStages(RdrShaderMode eMode) const { return m_activeShaderStages[(int)eMode]; }

	const RdrDescriptorTable* GetResourceDescriptorTable() const { return m_pResourceDescriptorTable; }
	const RdrDescriptorTable* GetSamplerDescriptorTable() const { return m_pSamplerDescriptorTable; }

	const RdrConstantBufferHandle& GetConstantBufferHandle() const { return m_hConstants; }

private:
	CachedString m_name;
	RdrMaterialFlags m_flags;

	RdrShaderStageFlags m_activeShaderStages[(int)RdrShaderMode::Count];
	RdrPipelineState m_hPipelineStates[(int)RdrShaderMode::Count];

	Array<RdrResourceHandle, kMaxTextures> m_ahTextures;
	Array<RdrSamplerState, 4> m_aSamplers;
	RdrConstantBufferHandle m_hConstants;

	const RdrDescriptorTable* m_pResourceDescriptorTable;
	const RdrDescriptorTable* m_pSamplerDescriptorTable;

	RdrTessellationMaterialData m_tessellation;
};