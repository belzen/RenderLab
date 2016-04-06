#pragma once

#include <vector>
#include "RdrDeviceTypes.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "Camera.h"

struct RdrDrawOp;
class LightList;

#define MAX_ACTIONS_PER_FRAME 16
#define MAX_RENDER_TARGETS 6

struct RdrPass
{
	RdrRenderTargetViewHandle ahRenderTargets[MAX_RENDER_TARGETS];
	RdrDepthStencilViewHandle hDepthTarget;

	// If no camera is set, an ortho projection is used.
	const Camera* pCamera;
	Rect viewport;

	RdrShaderMode shaderMode;
	RdrDepthTestMode depthTestMode;
	bool bAlphaBlend;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
};

struct RdrLightParams
{
	RdrResourceHandle hLightListRes;
	RdrResourceHandle hShadowMapDataRes;
	RdrResourceHandle hShadowMapTexArray;
	RdrResourceHandle hShadowCubeMapTexArray;
	uint lightCount;
};
struct RdrAction
{
	void Reset();

	///
	LPCWSTR name;

	std::vector<RdrDrawOp*> buckets[kRdrBucketType_Count];
	RdrPass passes[kRdrPass_Count];

	Camera camera;

	RdrLightParams lightParams;

	bool bDoLightCulling;
	bool bIsCubemapCapture;
};

struct RdrConstantBufferUpdateCmd
{
	RdrResourceHandle hBuffer;
	Vec4 data[8];
};

struct RdrResourceUpdateCmd
{
	RdrResourceHandle hResource;
	void* pData;
	uint dataSize;
};

struct RdrShaderCreateCmd
{
	RdrShaderType type;
	RdrShaderHandle hShader;
	void* pCompiledData;
	uint dataSize;
};

struct RdrInputLayoutCreateCmd
{
	RdrInputLayoutHandle hInputLayout;
	RdrShaderHandle hShader;
	RdrVertexInputElement* pElements;
	uint numElements;
};

struct RdrGeometryCreateCmd
{
	RdrGeoHandle hGeo;
	const void* pVertData;
	uint vertStride;
	uint numVerts;
	const uint16* pIndexData;
	uint numIndices;
	Vec3 size;
};

struct RdrFrameState
{
	std::vector<RdrConstantBufferUpdateCmd> constantBufferUpdates;
	std::vector<RdrResourceUpdateCmd>       resourceUpdates;
	std::vector<RdrShaderCreateCmd>         shaderCreates;
	std::vector<RdrInputLayoutCreateCmd>    inputLayoutCreates;
	RdrAction actions[MAX_ACTIONS_PER_FRAME];
	uint      numActions;
};