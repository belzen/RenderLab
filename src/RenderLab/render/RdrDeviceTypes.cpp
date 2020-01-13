#include "Precompiled.h"
#include "RdrDeviceTypes.h"
#include "Renderer.h"

namespace
{
	static FreeList<RdrPipelineState, 2048> s_pipelineStates;

	struct RdrResourceBlockInfo
	{
		int blockPixelSize;
		int blockByteSize;
	};

	RdrResourceBlockInfo s_resourceBlockInfo[]
	{
		{1, 1}, // UNKNOWN
		{1, 2}, // D16
		{1, 4}, // D24_UNORM_S8_UINT
		{1, 2}, // R16_UNORM
		{1, 2}, // R16_UINT
		{1, 2}, // R16_FLOAT
		{1, 4}, // R32_UINT
		{1, 4}, // R16G16_FLOAT
		{1, 1}, // R8_UNORM
		{4, 2}, // DXT1
		{4, 2}, // DXT1_sRGB
		{4, 4}, // DXT5
		{4, 4}, // DXT5_sRGB
		{4, 4}, // BC5_UNORM
		{1, 4}, // B8G8R8A8_UNORM
		{1, 4}, // B8G8R8A8_UNORM_sRGB
		{1, 4}, // R8G8B8A8_UNORM
		{1, 2}, // R8G8_UNORM
		{1, 8}, // R16G16B16A16_FLOAT
	};

	static_assert(ARRAY_SIZE(s_resourceBlockInfo) == (int)RdrResourceFormat::Count, "Missing resource block info!");
}

uint rdrGetResourceFormatSize(RdrResourceFormat eFormat)
{
	return s_resourceBlockInfo[(int)eFormat].blockByteSize;
}

int rdrGetTexturePitch(const int width, const RdrResourceFormat eFormat)
{
	const RdrResourceBlockInfo& rBlockInfo = s_resourceBlockInfo[(int)eFormat];
	int blockPixelsMinusOne = rBlockInfo.blockPixelSize - 1;
	int pitch = ((width + blockPixelsMinusOne) & ~blockPixelsMinusOne) * rBlockInfo.blockByteSize;
	return pitch;
}

int rdrGetTextureRows(const int height, const RdrResourceFormat eFormat)
{
	const RdrResourceBlockInfo& rBlockInfo = s_resourceBlockInfo[(int)eFormat];
	int blockPixelsMinusOne = rBlockInfo.blockPixelSize - 1;
	int rows = ((height + blockPixelsMinusOne) & ~blockPixelsMinusOne) / rBlockInfo.blockPixelSize;
	return rows;
}

void RdrDescriptors::MarkUsedThisFrame() const
{
	const_cast<RdrDescriptors*>(this)->m_nLastUsedFrameCode = g_pRenderer->GetContext()->GetFrameNum();
}

bool RdrDescriptors::Release()
{
	if (--m_nRefCount > 0)
		return false;

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	switch (m_eDescType)
	{
	case RdrDescriptorType::Sampler:
		pRdrContext->GetSamplerHeap().FreeDescriptor(this);
		break;
	case RdrDescriptorType::SRV:
	case RdrDescriptorType::CBV:
	case RdrDescriptorType::UAV:
		pRdrContext->GetSrvHeap().FreeDescriptor(this);
		break;
	case RdrDescriptorType::RTV:
		pRdrContext->GetRtvHeap().FreeDescriptor(this);
		break;
	case RdrDescriptorType::DSV:
		pRdrContext->GetDsvHeap().FreeDescriptor(this);
		break;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// RdrPipelineState

void RdrPipelineState::MarkUsedThisFrame() const
{
	const_cast<RdrPipelineState*>(this)->m_nLastUsedFrameCode = g_pRenderer->GetContext()->GetFrameNum();
}

void RdrPipelineState::ReCreate()
{
	// Global depth bias to use for raster states that enable it.
	static constexpr float kSlopeScaledDepthBias = 3.f;
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	if (m_pDevPipelineState)
	{
		// Make a copy of the pipeline state so that we can queue it to be freed
		//	without breaking references to our original pointer.
		RdrPipelineState* pCopyState = s_pipelineStates.allocSafe();
		*pCopyState = *this;

		g_pRenderer->GetResourceCommandList().ReleasePipelineState(pCopyState, CREATE_BACKPOINTER(this));
	}

	if (m_bForGraphics)
	{
		D3D12_INPUT_ELEMENT_DESC d3dVertexDesc[32];
		Assert(m_inputLayoutElements.size() < _countof(d3dVertexDesc));

		for (uint i = 0; i < (uint)m_inputLayoutElements.size(); ++i)
		{
			RdrVertexInputElement& elem = m_inputLayoutElements[i];

			d3dVertexDesc[i].SemanticName = getD3DSemanticName(elem.semantic);
			d3dVertexDesc[i].SemanticIndex = elem.semanticIndex;
			d3dVertexDesc[i].AlignedByteOffset = elem.byteOffset;
			d3dVertexDesc[i].Format = getD3DVertexInputFormat(elem.format);
			d3dVertexDesc[i].InputSlotClass = getD3DVertexInputClass(elem.inputClass);
			d3dVertexDesc[i].InputSlot = elem.streamIndex;
			d3dVertexDesc[i].InstanceDataStepRate = elem.instanceDataStepRate;
		}

		// Describe and create the graphics pipeline state objects (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { d3dVertexDesc, (uint)m_inputLayoutElements.size() };
		psoDesc.pRootSignature = pRdrContext->GetGraphicsRootSignature().Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_pVertexShader->pCompiledData, m_pVertexShader->compiledSize);
		psoDesc.PS = m_pPixelShader ? CD3DX12_SHADER_BYTECODE(m_pPixelShader->pCompiledData, m_pPixelShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
		psoDesc.DS = m_pDomainShader ? CD3DX12_SHADER_BYTECODE(m_pDomainShader->pCompiledData, m_pDomainShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
		psoDesc.HS = m_pHullShader ? CD3DX12_SHADER_BYTECODE(m_pHullShader->pCompiledData, m_pHullShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
		psoDesc.DepthStencilState.DepthEnable = m_depthStencilState.bTestDepth;
		psoDesc.DepthStencilState.DepthWriteMask = m_depthStencilState.bWriteDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = getComparisonFuncD3d(m_depthStencilState.eDepthFunc);
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.DSVFormat = (psoDesc.DepthStencilState.DepthEnable ? getD3DDepthFormat(kDefaultDepthFormat) : DXGI_FORMAT_UNKNOWN);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = (m_rasterState.bEnableMSAA ? g_debugState.msaaLevel : 1);
		psoDesc.SampleDesc.Quality = 0;
		psoDesc.PrimitiveTopologyType = m_pDomainShader ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = (uint)m_rtvFormats.size();
		for (uint i = 0; i < (uint)m_rtvFormats.size(); ++i)
		{
			psoDesc.RTVFormats[i] = getD3DFormat(m_rtvFormats[i]);
		}

		// Rasterizer state
		{
			psoDesc.RasterizerState.AntialiasedLineEnable = !m_rasterState.bEnableMSAA;
			psoDesc.RasterizerState.MultisampleEnable = m_rasterState.bEnableMSAA;
			psoDesc.RasterizerState.CullMode = m_rasterState.bDoubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
			psoDesc.RasterizerState.SlopeScaledDepthBias = m_rasterState.bUseSlopeScaledDepthBias ? kSlopeScaledDepthBias : 0.f;
			psoDesc.RasterizerState.DepthBiasClamp = 0.f;
			psoDesc.RasterizerState.DepthClipEnable = true;
			psoDesc.RasterizerState.FrontCounterClockwise = false;
			psoDesc.RasterizerState.ForcedSampleCount = 0;
			psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
			if (m_rasterState.bWireframe)
			{
				psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
				psoDesc.RasterizerState.DepthBias = -2; // Wireframe gets a slight depth bias so z-fighting doesn't occur.
			}
			else
			{
				psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
				psoDesc.RasterizerState.DepthBias = 0;
			}
		}


		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.BlendState.IndependentBlendEnable = false;
		psoDesc.BlendState.AlphaToCoverageEnable = false;
		for (uint i = 0; i < ARRAY_SIZE(psoDesc.BlendState.RenderTarget); ++i)
		{
			switch (m_eBlendMode)
			{
			case RdrBlendMode::kOpaque:
				psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
				psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
				psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
				psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
				psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
				psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				break;
			case RdrBlendMode::kAlpha:
				psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
				psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
				psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_SRC_ALPHA;
				psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
				psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
				psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				break;
			case RdrBlendMode::kAdditive:
				psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
				psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
				psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
				psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
				psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				break;
			case RdrBlendMode::kSubtractive:
				psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
				psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
				psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
				psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
				psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
				psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
				psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
				break;
			}
		}

		ID3D12PipelineState* pDevPipelineState;
		pRdrContext->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pDevPipelineState));
		m_pDevPipelineState = pDevPipelineState;
	}
	else
	{
		// Describe and create the compute pipeline state objects (PSO).
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(m_pComputeShader->pCompiledData, m_pComputeShader->compiledSize);
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		psoDesc.NodeMask = 0;
		psoDesc.pRootSignature = pRdrContext->GetComputeRootSignature().Get();

		ID3D12PipelineState* pDevPipelineState;
		pRdrContext->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pDevPipelineState));
		m_pDevPipelineState = pDevPipelineState;
	}


	// Add pipeline state references to the shaders for reloading
	if (m_pVertexShader)
	{
		const_cast<RdrShader*>(m_pVertexShader)->refPipelineStates.insert(this);
	}
	if (m_pPixelShader)
	{
		const_cast<RdrShader*>(m_pPixelShader)->refPipelineStates.insert(this);
	}
	if (m_pHullShader)
	{
		const_cast<RdrShader*>(m_pHullShader)->refPipelineStates.insert(this);
	}
	if (m_pDomainShader)
	{
		const_cast<RdrShader*>(m_pDomainShader)->refPipelineStates.insert(this);
	}
	if (m_pComputeShader)
	{
		const_cast<RdrShader*>(m_pComputeShader)->refPipelineStates.insert(this);
	}
}

RdrPipelineState* RdrPipelineState::CreateGraphicsPipelineState(
	const RdrShader* pVertexShader, const RdrShader* pPixelShader,
	const RdrShader* pHullShader, const RdrShader* pDomainShader,
	const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
	const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
	const RdrBlendMode eBlendMode,
	const RdrRasterState& rasterState,
	const RdrDepthStencilState& depthStencilState)
{
	RdrPipelineState* pPipelineState = s_pipelineStates.allocSafe();
	*pPipelineState = RdrPipelineState();

	pPipelineState->m_pVertexShader = pVertexShader;
	pPipelineState->m_pPixelShader = pPixelShader;
	pPipelineState->m_pHullShader = pHullShader;
	pPipelineState->m_pDomainShader = pDomainShader;

	pPipelineState->m_inputLayoutElements.assign(pInputLayoutElements, pInputLayoutElements + nNumInputElements);
	pPipelineState->m_rtvFormats.assign(pRtvFormats, pRtvFormats + nNumRtvFormats);

	pPipelineState->m_eBlendMode = eBlendMode;
	pPipelineState->m_rasterState = rasterState;
	pPipelineState->m_depthStencilState = depthStencilState;
	pPipelineState->m_bForGraphics = true;

	pPipelineState->ReCreate();
	return pPipelineState;
}

RdrPipelineState* RdrPipelineState::CreateComputePipelineState(const RdrShader* pComputeShader)
{
	RdrPipelineState* pPipelineState = s_pipelineStates.allocSafe();
	*pPipelineState = RdrPipelineState();

	pPipelineState->m_pComputeShader = pComputeShader;
	pPipelineState->m_bForGraphics = false;

	pPipelineState->ReCreate();
	return pPipelineState;
}

void RdrPipelineState::Release()
{
	m_pDevPipelineState->Release();
	s_pipelineStates.releaseSafe(this);
}