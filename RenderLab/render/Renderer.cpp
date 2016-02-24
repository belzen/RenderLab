#include "Precompiled.h"
#include "Renderer.h"
#include <assert.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include "Model.h"
#include "Camera.h"
#include "WorldObject.h"
#include "Font.h"
#include <DXGIFormat.h>
#include "debug\DebugConsole.h"

#pragma comment (lib, "d3d11.lib")


namespace
{
	static bool s_debug = 1;
	static bool s_wireframe = 0;
	static int s_msaaLevel = 2;

	void setWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		s_wireframe = (args[0].val.num != 0.f);
	}
}

enum RdrReservedPsResourceSlots
{
	PS_RESOURCE_LIGHT_LIST = 16,
	PS_RESOURCE_TILE_LIGHT_IDS = 17,
};

struct RdrPass
{
	ID3D11DepthStencilState* pDepthStencilState;
	ID3D11BlendState* pBlendState;

	ID3D11RenderTargetView* pRenderTarget;
	ID3D11DepthStencilView* pDepthTarget;

	bool bEnabled;
	bool isOrtho;
};

struct RdrAction
{
	static RdrAction* Allocate();
	static void Release(RdrAction* pAction);

	///
	ID3D11RenderTargetView* pRenderTarget;

	std::vector<RdrDrawOp*> buckets[RBT_COUNT];
	RdrPass passes[RDRPASS_COUNT];

	Camera* pCamera;
};

static RdrAction s_actionPool[16];
static uint s_actionPoolSize = 16;

RdrAction* RdrAction::Allocate()
{
	assert(s_actionPoolSize > 0);
	return &s_actionPool[--s_actionPoolSize];
}

void RdrAction::Release(RdrAction* pAction)
{
	for (int i = 0; i < RBT_COUNT; ++i)
		pAction->buckets[i].clear();
	memset(pAction->passes, 0, sizeof(pAction->passes));
	pAction->pRenderTarget = nullptr;
	pAction->pCamera = nullptr;

	s_actionPool[s_actionPoolSize] = *pAction;
	++s_actionPoolSize;
}

//////////////////////////////////////////////////////

// Pass to bucket mappings
RdrBucketType s_passBuckets[] = {
	RBT_OPAQUE,	// RDRPASS_ZPREPASS
	RBT_OPAQUE,	// RDRPASS_OPAQUE
	RBT_ALPHA,	// RDRPASS_ALPHA
	RBT_UI,		// RDRPASS_UI
};
static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == RDRPASS_COUNT, "Missing pass -> bucket mappings");

// Event names for passes
static const wchar_t* s_passNames[] =
{
	L"Z-Prepass",		// RDRPASS_ZPREPASS,
	L"Opaque",			// RDRPASS_OPAQUE,
	L"Alpha",			// RDRPASS_ALPHA,
	L"UI",				// RDRPASS_UI,
};
static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == RDRPASS_COUNT, "Missing RdrPass names!");

//////////////////////////////////////////////////////

struct PerFrameConstantsVS
{
	Matrix44 view_mat;
	Matrix44 proj_mat;
};

struct PerObjectConstantsVS
{
	Matrix44 world_mat;
};

struct PerFrameConstantsPS
{
	Matrix44 inv_proj_mat;
	Vec3 camera_position;
	uint screenWidth;
};

static ID3D11BlendState* getBlendState(ID3D11Device* pDevice, bool alpha)
{
	static ID3D11BlendState* s_pStates[2] = { 0 };

	if (!s_pStates[alpha])
	{
		D3D11_BLEND_DESC blend_desc = { 0 };
		blend_desc.AlphaToCoverageEnable = false;
		blend_desc.IndependentBlendEnable = false;

		if (alpha)
		{
			blend_desc.RenderTarget[0].BlendEnable = true;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else
		{
			blend_desc.RenderTarget[0].BlendEnable = false;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		ID3D11BlendState* pBlendState = nullptr;
		HRESULT hr = pDevice->CreateBlendState(&blend_desc, &pBlendState);
		assert(hr == S_OK);

		s_pStates[alpha] = pBlendState;
	}

	return s_pStates[alpha];
}

enum DepthTestMode
{
	kDepthTestMode_None,
	kDepthTestMode_Less,
	kDepthTestMode_Equal,
	kDepthTestMode_NumModes,
};

static ID3D11DepthStencilState* getDepthStencilState(ID3D11Device* pDevice, DepthTestMode depthTest)
{
	static ID3D11DepthStencilState* s_pStates[kDepthTestMode_NumModes] = { 0 };

	if (!s_pStates[depthTest])
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc;

		dsDesc.DepthEnable = (depthTest != kDepthTestMode_None);
		switch (depthTest)
		{
		case kDepthTestMode_Less:
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
			break;
		case kDepthTestMode_Equal:
			dsDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
			break;
		default:
			dsDesc.DepthFunc = D3D11_COMPARISON_NEVER;
			break;
		}
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

		dsDesc.StencilEnable = false;

		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		ID3D11DepthStencilState* pState;
		HRESULT hr = pDevice->CreateDepthStencilState(&dsDesc, &pState);
		assert(hr == S_OK);

		s_pStates[depthTest] = pState;
	}

	return s_pStates[depthTest];
}

namespace
{
	void DrawGeo(RdrDrawOp* pDrawOp, RdrContext* pContext, RdrPassEnum ePass)
	{
		ID3D11DeviceContext* pDevContext = pContext->m_pContext;
		RdrGeometry* pGeo = pContext->m_geo.get(pDrawOp->hGeo);
		UINT stride = pGeo->vertStride;
		UINT offset = 0;
		ID3D11Buffer* pConstantsBuffer = nullptr;
		HRESULT hr;

		if (pDrawOp->numConstants)
		{
			D3D11_BUFFER_DESC desc = { 0 };
			D3D11_SUBRESOURCE_DATA data = { 0 };

			desc.ByteWidth = pDrawOp->numConstants * sizeof(Vec4);
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;

			data.pSysMem = pDrawOp->constants;

			hr = pContext->m_pDevice->CreateBuffer(&desc, &data, &pConstantsBuffer);
			assert(hr == S_OK);

			pDevContext->VSSetConstantBuffers(1, 1, &pConstantsBuffer);
		}

		VertexShader* pVertexShader = pContext->m_vertexShaders.get(pDrawOp->hVertexShader);
		PixelShader* pPixelShader = pContext->m_pixelShaders.get(pDrawOp->hPixelShader);

		pDevContext->VSSetShader(pVertexShader->pShader, nullptr, 0);
		if (ePass == RDRPASS_ZPREPASS)
		{
			pDevContext->PSSetShader(nullptr, nullptr, 0);
		}
		else
		{
			pDevContext->PSSetShader(pPixelShader->pShader, nullptr, 0);

			for (uint i = 0; i < pDrawOp->texCount; ++i)
			{
				RdrTexture* pTex = pContext->m_textures.get(pDrawOp->hTextures[i]);
				pDevContext->PSSetShaderResources(i, 1, &pTex->pResourceView);
				pDevContext->PSSetSamplers(i, 1, &pTex->pSamplerState);
			}

			if (pDrawOp->needsLighting)
			{
				RdrTexture* pTex = pContext->m_textures.get(pContext->m_lights.hResource);
				pDevContext->PSSetShaderResources(PS_RESOURCE_LIGHT_LIST, 1, &pTex->pResourceView);

				pTex = pContext->m_textures.get(pContext->m_hTileLightIndices);
				pDevContext->PSSetShaderResources(PS_RESOURCE_TILE_LIGHT_IDS, 1, &pTex->pResourceView);
			}
		}

		pDevContext->IASetInputLayout(pVertexShader->pInputLayout);
		pDevContext->IASetVertexBuffers(0, 1, &pGeo->pVertexBuffer, &stride, &offset);
		pDevContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (pGeo->pIndexBuffer)
		{
			pDevContext->IASetIndexBuffer(pGeo->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
			pDevContext->DrawIndexed(pGeo->numIndices, 0, 0);
		}
		else
		{
			pDevContext->Draw(pGeo->numVerts, 0);
		}

		if (pConstantsBuffer)
			pConstantsBuffer->Release();


		ID3D11ShaderResourceView* pNullResourceView = nullptr;
		pDevContext->PSSetShaderResources(PS_RESOURCE_LIGHT_LIST, 1, &pNullResourceView);
		pDevContext->PSSetShaderResources(PS_RESOURCE_TILE_LIGHT_IDS, 1, &pNullResourceView);
	}

	void DispatchCompute(RdrDrawOp* pDrawOp, RdrContext* pContext)
	{
		ID3D11DeviceContext* pDevContext = pContext->m_pContext;
		ComputeShader* pComputeShader = pContext->m_computeShaders.get(pDrawOp->hComputeShader);
		ID3D11Buffer* pConstantsBuffer = nullptr;
		HRESULT hr;

		pDevContext->CSSetShader(pComputeShader->pShader, nullptr, 0);

		if (pDrawOp->numConstants)
		{
			D3D11_BUFFER_DESC desc = { 0 };
			D3D11_SUBRESOURCE_DATA data = { 0 };

			desc.ByteWidth = pDrawOp->numConstants * sizeof(Vec4);
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;

			data.pSysMem = pDrawOp->constants;

			hr = pContext->m_pDevice->CreateBuffer(&desc, &data, &pConstantsBuffer);
			assert(hr == S_OK);

			pDevContext->CSSetConstantBuffers(0, 1, &pConstantsBuffer);
		}

		for (uint i = 0; i < pDrawOp->texCount; ++i)
		{
			RdrTexture* pTex = pContext->m_textures.get(pDrawOp->hTextures[i]);
			pDevContext->CSSetShaderResources(i, 1, &pTex->pResourceView);
		}

		for (uint i = 0; i < pDrawOp->viewCount; ++i)
		{
			RdrTexture* pTex = pContext->m_textures.get(pDrawOp->hViews[i]);
			pDevContext->CSSetUnorderedAccessViews(i, 1, &pTex->pUnorderedAccessView, nullptr);
		}

		pDevContext->Dispatch(pDrawOp->computeThreads[0], pDrawOp->computeThreads[1], pDrawOp->computeThreads[2]);

		// Clear resources to avoid binding errors (input bound as output).  todo: don't do this
		for (uint i = 0; i < pDrawOp->texCount; ++i)
		{
			ID3D11ShaderResourceView* pResourceView = nullptr;
			pDevContext->CSSetShaderResources(i, 1, &pResourceView);
		}

		for (uint i = 0; i < pDrawOp->viewCount; ++i)
		{
			ID3D11UnorderedAccessView* pUnorderedAccessView = nullptr;
			pDevContext->CSSetUnorderedAccessViews(i, 1, &pUnorderedAccessView, nullptr);
		}

		if (pConstantsBuffer)
			pConstantsBuffer->Release();
	}
}


bool Renderer::Init(HWND hWnd, int width, int height)
{
	DebugConsole::RegisterCommand("wireframe", setWireframeEnabled, kDebugCommandArgType_Number);

	HRESULT hr;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SampleDesc.Count = s_msaaLevel;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		s_debug ? D3D11_CREATE_DEVICE_DEBUG : 0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&m_pSwapChain,
		&m_context.m_pDevice,
		nullptr,
		&m_context.m_pContext);

	if (hr != S_OK)
	{
		MessageBox(NULL, L"Failed to create D3D11 device", L"Failure", MB_OK);
		return false;
	}

	// Annotator
	m_context.m_pContext->QueryInterface(__uuidof(m_pAnnotator), (void**)&m_pAnnotator);

	// Render target
	ID3D11Texture2D* pBackBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

	D3D11_TEXTURE2D_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = bbDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	rtvDesc.Texture2D.MipSlice = 0;

	hr = m_context.m_pDevice->CreateRenderTargetView(pBackBuffer, &rtvDesc, &m_pPrimaryRenderTarget);
	assert(hr == S_OK);
	pBackBuffer->Release();

	// Depth stencil state
	D3D11_DEPTH_STENCIL_DESC dsDesc;
	dsDesc.DepthEnable = true;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

	dsDesc.StencilEnable = false;

	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	hr = m_context.m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStencilState);
	assert(hr == S_OK);

	// Depth Buffer
	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthTexDesc.Height = height;
	depthTexDesc.Width = width;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.MiscFlags = 0;
	depthTexDesc.SampleDesc.Count = s_msaaLevel;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;

	hr = m_context.m_pDevice->CreateTexture2D(&depthTexDesc, nullptr, &m_pDepthBuffer);
	assert(hr == S_OK);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = m_context.m_pDevice->CreateDepthStencilView(m_pDepthBuffer, &dsvDesc, &m_pDepthStencilView);
	assert(hr == S_OK);

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC resDesc;
		resDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		resDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

		hr = m_context.m_pDevice->CreateShaderResourceView(m_pDepthBuffer, &resDesc, &m_pDepthResourceView);
		assert(hr == S_OK);

		RdrTexture* pTex = m_context.m_textures.alloc();
		pTex->pTexture = m_pDepthBuffer;
		pTex->pResourceView = m_pDepthResourceView;
		m_hDepthTex = m_context.m_textures.getId(pTex);
	}

	// Rasterizer states
	{
		// Default
		D3D11_RASTERIZER_DESC rasterDesc;
		rasterDesc.AntialiasedLineEnable = false; // Only if MSAA is disabled.
		rasterDesc.MultisampleEnable = true;
		rasterDesc.CullMode = D3D11_CULL_BACK;
		rasterDesc.DepthBias = 0;
		rasterDesc.SlopeScaledDepthBias = 0.f;
		rasterDesc.DepthBiasClamp = 0.f;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.ScissorEnable = false;

		m_context.m_pDevice->CreateRasterizerState(&rasterDesc, &m_pRasterStateDefault);

		// Wireframe
		rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
		m_context.m_pDevice->CreateRasterizerState(&rasterDesc, &m_pRasterStateWireframe);
	}

	Resize(width, height);

	// Constant buffers
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.ByteWidth = sizeof(PerFrameConstantsVS);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		hr = m_context.m_pDevice->CreateBuffer(&desc, nullptr, &m_pPerFrameBufferVS);
		assert(hr == S_OK);
	}
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.ByteWidth = sizeof(PerFrameConstantsPS);
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		hr = m_context.m_pDevice->CreateBuffer(&desc, nullptr, &m_pPerFrameBufferPS);
		assert(hr == S_OK);
	}

	Font::Init(&m_context);

	return true;
}

void Renderer::Cleanup()
{
	m_pSwapChain->Release();
	m_pPrimaryRenderTarget->Release();
	m_pDepthBuffer->Release();
	m_pPerFrameBufferVS->Release();
	m_pPerFrameBufferPS->Release();
	m_pRasterStateDefault->Release();
	m_pRasterStateWireframe->Release();
	m_pDepthStencilState->Release();
	m_pDepthStencilView->Release();
	m_context.m_pContext->Release();
	m_context.m_pDevice->Release();
}

void Renderer::Resize(int width, int height)
{
	if (!m_context.m_pContext)
		return;

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.f;
	viewport.MaxDepth = 1.f;

	m_context.m_pContext->RSSetViewports(1, &viewport);

	m_viewWidth = width;
	m_viewHeight = height;

	m_context.m_mainCamera.SetAspectRatio(width / (float)height);
}

const Camera* Renderer::GetCurrentCamera(void) const
{
	return m_pCurrentAction->pCamera;
}

void Renderer::DispatchLightCulling(Camera* pCamera)
{
	ID3D11DeviceContext* pDevContext = m_context.m_pContext;

	m_pAnnotator->BeginEvent(L"Light Culling");

	ID3D11RenderTargetView* pRenderTarget = nullptr;
	pDevContext->OMSetRenderTargets(1, &pRenderTarget, nullptr);
	pDevContext->OMSetDepthStencilState(getDepthStencilState(m_context.m_pDevice, kDepthTestMode_None), 1);
	pDevContext->OMSetBlendState(getBlendState(m_context.m_pDevice, false), nullptr, 0xffffffff);

	// todo: handle screen resizing
	const int kTilePixelSize = 16;
	int tileCountX = (m_viewWidth + (kTilePixelSize-1)) / kTilePixelSize;
	int tileCountY = (m_viewHeight + (kTilePixelSize - 1)) / kTilePixelSize;

	//////////////////////////////////////
	// Depth min max
	if (!m_hDepthMinMaxTex)
	{
		m_hDepthMinMaxShader = m_context.LoadComputeShader("c_tiled_depth_calc.hlsl");
		m_hDepthMinMaxTex = m_context.CreateTexture2D(tileCountX, tileCountY, DXGI_FORMAT_R16G16_FLOAT);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->hComputeShader = m_hDepthMinMaxShader;
	pDepthOp->computeThreads[0] = tileCountX;
	pDepthOp->computeThreads[1] = tileCountY;
	pDepthOp->computeThreads[2] = 1;
	pDepthOp->hTextures[0] = m_hDepthTex;
	pDepthOp->texCount = 1;
	pDepthOp->hViews[0] = m_hDepthMinMaxTex;
	pDepthOp->viewCount = 1;

	Matrix44 viewMtx;
	Matrix44 invProjMtx;
	pCamera->GetMatrices(viewMtx, invProjMtx);
	invProjMtx = Matrix44Inverse(invProjMtx);
	invProjMtx = Matrix44Transpose(invProjMtx);

	for (int i = 0; i < 4; ++i)
	{
		pDepthOp->constants[i].x = invProjMtx.m[i][0];
		pDepthOp->constants[i].y = invProjMtx.m[i][1];
		pDepthOp->constants[i].z = invProjMtx.m[i][2];
		pDepthOp->constants[i].w = invProjMtx.m[i][3];
	}
	pDepthOp->numConstants = 4;

	// Dispatch 
	DispatchCompute(pDepthOp, &m_context);
	RdrDrawOp::Release(pDepthOp);

	//////////////////////////////////////
	// Light culling
	if (!m_hLightCullShader)
	{
		const uint kMaxLightsPerTile = 128; // Sync with MAX_LIGHTS_PER_TILE in "light_inc.hlsli"
		m_hLightCullShader = m_context.LoadComputeShader("c_tiled_light_cull.hlsl");
		m_context.m_hTileLightIndices = m_context.CreateStructuredBuffer(nullptr, tileCountX * tileCountY * kMaxLightsPerTile, sizeof(uint));
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->hComputeShader = m_hLightCullShader;
	pCullOp->computeThreads[0] = tileCountX;
	pCullOp->computeThreads[1] = tileCountY;
	pCullOp->computeThreads[2] = 1;
	pCullOp->hTextures[0] = m_context.m_lights.hResource;
	pCullOp->hTextures[1] = m_hDepthMinMaxTex;
	pCullOp->texCount = 2;
	pCullOp->hViews[0] = m_context.m_hTileLightIndices;
	pCullOp->viewCount = 1;


	struct CullingParams // Sync with c_tiledlight_cull.hlsl
	{
		Vec3 cameraPos;
		float fovY;

		Vec3 cameraDir;
		float aspectRatio;

		float cameraNearDist;
		float cameraFarDist;

		uint lightCount;
		uint tileCountX;
		uint tileCountY;
	};

	CullingParams* pParams = reinterpret_cast<CullingParams*>(pCullOp->constants);
	pParams->cameraPos = pCamera->GetPosition();
	pParams->cameraDir = pCamera->GetDirection();
	pParams->cameraNearDist = pCamera->GetNearDist();
	pParams->cameraFarDist = pCamera->GetFarDist();
	pParams->fovY = pCamera->GetFieldOfViewY();
	pParams->aspectRatio = pCamera->GetAspectRatio();
	pParams->lightCount = m_context.m_lights.lightCount;
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	pCullOp->numConstants = (sizeof(CullingParams) / sizeof(Vec4)) + 1;

	// Dispatch 
	DispatchCompute(pCullOp, &m_context);
	RdrDrawOp::Release(pCullOp);

	m_pAnnotator->EndEvent();
}

void Renderer::BeginAction(Camera* pCamera, ID3D11RenderTargetView* pRenderTarget)
{
	m_pCurrentAction = RdrAction::Allocate();
	m_pCurrentAction->pRenderTarget = pRenderTarget ? pRenderTarget : m_pPrimaryRenderTarget;
	m_pCurrentAction->pCamera = pCamera;
	pCamera->UpdateFrustum();

	// Setup action passes
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].pBlendState = getBlendState(m_context.m_pDevice, false);
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].pDepthStencilState = getDepthStencilState(m_context.m_pDevice, kDepthTestMode_Less);
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].isOrtho = false;
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].bEnabled = true;
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].pDepthTarget = m_pDepthStencilView;
	m_pCurrentAction->passes[RDRPASS_ZPREPASS].pRenderTarget = m_pCurrentAction->pRenderTarget;

	m_pCurrentAction->passes[RDRPASS_OPAQUE].pBlendState = getBlendState(m_context.m_pDevice, false);
	m_pCurrentAction->passes[RDRPASS_OPAQUE].pDepthStencilState = getDepthStencilState(m_context.m_pDevice, kDepthTestMode_Equal);
	m_pCurrentAction->passes[RDRPASS_OPAQUE].isOrtho = false;
	m_pCurrentAction->passes[RDRPASS_OPAQUE].bEnabled = true;
	m_pCurrentAction->passes[RDRPASS_OPAQUE].pDepthTarget = m_pDepthStencilView;
	m_pCurrentAction->passes[RDRPASS_OPAQUE].pRenderTarget = m_pCurrentAction->pRenderTarget;

	m_pCurrentAction->passes[RDRPASS_ALPHA].pBlendState = getBlendState(m_context.m_pDevice, true);
	m_pCurrentAction->passes[RDRPASS_ALPHA].pDepthStencilState = getDepthStencilState(m_context.m_pDevice, kDepthTestMode_None);
	m_pCurrentAction->passes[RDRPASS_ALPHA].isOrtho = false;
	m_pCurrentAction->passes[RDRPASS_ALPHA].bEnabled = true;
	m_pCurrentAction->passes[RDRPASS_ALPHA].pDepthTarget = m_pDepthStencilView;
	m_pCurrentAction->passes[RDRPASS_ALPHA].pRenderTarget = m_pCurrentAction->pRenderTarget;

	m_pCurrentAction->passes[RDRPASS_UI].pBlendState = getBlendState(m_context.m_pDevice, true);
	m_pCurrentAction->passes[RDRPASS_UI].pDepthStencilState = getDepthStencilState(m_context.m_pDevice, kDepthTestMode_None);
	m_pCurrentAction->passes[RDRPASS_UI].isOrtho = true;
	m_pCurrentAction->passes[RDRPASS_UI].bEnabled = true;
	m_pCurrentAction->passes[RDRPASS_UI].pDepthTarget = m_pDepthStencilView;
	m_pCurrentAction->passes[RDRPASS_UI].pRenderTarget = m_pCurrentAction->pRenderTarget;
}

void Renderer::EndAction()
{
	m_actions.push_back(m_pCurrentAction);
	m_pCurrentAction = nullptr;
}

void Renderer::AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket)
{
	m_pCurrentAction->buckets[bucket].push_back(pDrawOp);
}

void Renderer::SetPerFrameConstants(Camera* pCamera)
{
	ID3D11DeviceContext* pContext = m_context.m_pContext;

	// Per-frame VS buffer
	D3D11_MAPPED_SUBRESOURCE res = { 0 };
	HRESULT hr = pContext->Map(m_pPerFrameBufferVS, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	assert(hr == S_OK);

	PerFrameConstantsVS* vsConstants = (PerFrameConstantsVS*)res.pData;
	if (pCamera)
	{
		pCamera->GetMatrices(vsConstants->view_mat, vsConstants->proj_mat);
	}
	else
	{
		vsConstants->view_mat = Matrix44::kIdentity;
		vsConstants->proj_mat = DirectX::XMMatrixOrthographicLH((float)m_viewWidth, (float)m_viewHeight, 0.01f, 1000.f);
	}
	vsConstants->view_mat = Matrix44Transpose(vsConstants->view_mat);
	vsConstants->proj_mat = Matrix44Transpose(vsConstants->proj_mat);

	pContext->Unmap(m_pPerFrameBufferVS, 0);
	pContext->VSSetConstantBuffers(0, 1, &m_pPerFrameBufferVS);

	// Setup per-frame PS buffer.
	hr = pContext->Map(m_pPerFrameBufferPS, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	assert(hr == S_OK);

	PerFrameConstantsPS* psConstants = (PerFrameConstantsPS*)res.pData;
	psConstants->inv_proj_mat = Matrix44Inverse(vsConstants->proj_mat);
	psConstants->screenWidth = m_viewWidth;
	psConstants->camera_position = pCamera ? pCamera->GetPosition() : Vec3::kUnitZ;

	pContext->Unmap(m_pPerFrameBufferPS, 0);
	pContext->PSSetConstantBuffers(0, 1, &m_pPerFrameBufferPS);
}

void Renderer::DrawPass(RdrAction* pAction, RdrPassEnum ePass)
{
	RdrPass* pPass = &pAction->passes[ePass];
	ID3D11DeviceContext* pDevContext = m_context.m_pContext;

	m_pAnnotator->BeginEvent(s_passNames[ePass]);

	pDevContext->OMSetRenderTargets(1, &pPass->pRenderTarget, pPass->pDepthTarget);
	pDevContext->OMSetDepthStencilState(pPass->pDepthStencilState, 1);
	pDevContext->OMSetBlendState(pPass->pBlendState, nullptr, 0xffffffff);

	if (pPass->isOrtho)
		SetPerFrameConstants(nullptr);
	else
		SetPerFrameConstants(pAction->pCamera);


	std::vector<RdrDrawOp*>::iterator opIter = pAction->buckets[ s_passBuckets[ePass] ].begin();
	std::vector<RdrDrawOp*>::iterator opEndIter = pAction->buckets[ s_passBuckets[ePass] ].end();
	for ( ; opIter != opEndIter; ++opIter )
	{
		RdrDrawOp* pDrawOp = *opIter;
		if (pDrawOp->hComputeShader)
		{
			DispatchCompute(pDrawOp, &m_context);
		}
		else
		{
			DrawGeo(pDrawOp, &m_context, ePass);
		}
	}

	m_pAnnotator->EndEvent();
}
	
void Renderer::DrawFrame()
{
	float clearColor[4] = { 0.0f, 0.f, 0.f, 1.f };
	ID3D11DeviceContext* pContext = m_context.m_pContext;


	for (uint iAction = 0; iAction < m_actions.size(); ++iAction)
	{
		RdrAction* pAction = m_actions[iAction];

		pContext->RSSetState(s_wireframe ? m_pRasterStateWireframe : m_pRasterStateDefault);

		pContext->ClearRenderTargetView(pAction->pRenderTarget, clearColor);
		pContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

		// todo: sort buckets 

		DrawPass(pAction, RDRPASS_ZPREPASS);

		DispatchLightCulling(pAction->pCamera);

		DrawPass(pAction, RDRPASS_OPAQUE);
		DrawPass(pAction, RDRPASS_ALPHA);

		pContext->RSSetState(m_pRasterStateDefault); // UI should never be wireframe
		DrawPass(pAction, RDRPASS_UI);

		// Free draw ops.
		for (uint iBucket = 0; iBucket < RBT_COUNT; ++iBucket)
		{
			uint numOps = pAction->buckets[iBucket].size();
			std::vector<RdrDrawOp*>::iterator iter = pAction->buckets[iBucket].begin();
			std::vector<RdrDrawOp*>::iterator endIter = pAction->buckets[iBucket].end();
			for (; iter != endIter; ++iter)
			{
				RdrDrawOp* pDrawOp = *iter;
				if (pDrawOp->bFreeGeo)
				{
					RdrGeometry* pGeo = m_context.m_geo.get(pDrawOp->hGeo);
					pGeo->pVertexBuffer->Release();
					pGeo->pIndexBuffer->Release();
					m_context.m_geo.release(pGeo);
				}

				RdrDrawOp::Release(pDrawOp);
			}
			pAction->buckets[iBucket].clear();
		}

		// Return action to the pool.
		RdrAction::Release(pAction);
	}
	m_actions.clear();

	m_pSwapChain->Present(0, 0);
}
