#pragma once

#include <map>
#include <vector>
#include "RdrShaders.h"
#include "FreeList.h"
#include "RdrGeometry.h"

class RdrContext;

typedef std::map<std::string, RdrShaderHandle> RdrShaderHandleMap; // todo: better map key for this and other caches
typedef FreeList<RdrShader, 1024> RdrShaderList;
typedef FreeList<RdrInputLayout, 1024> RdrInputLayoutList;

class RdrShaderSystem
{
public:
	void Init(RdrContext* pRdrContext);

	RdrShaderHandle CreatePixelShaderFromFile(const char* filename);
	RdrInputLayoutHandle CreateInputLayout(const RdrVertexShader& vertexShader, const RdrVertexInputElement* aVertexElements, const uint numElements);

	const RdrShader* GetVertexShader(const RdrVertexShader shader);
	const RdrShader* GetGeometryShader(const RdrGeometryShader shader);
	const RdrShader* GetComputeShader(const RdrComputeShader eShader);
	const RdrShader* GetPixelShader(const RdrShaderHandle hShader);

	const RdrInputLayout* GetInputLayout(const RdrInputLayoutHandle hLayout);

	void FlipState();
	void ProcessCommands();

private:
	struct CmdCreatePixelShader
	{
		RdrShaderHandle hShader;
		char* pShaderText;
		uint textLen;
	};

	struct CmdCreateInputLayout
	{
		RdrInputLayoutHandle hLayout;
		RdrVertexShader vertexShader;
		RdrVertexInputElement vertexElements[16];
		uint numElements;
	};

	struct FrameState
	{
		std::vector<CmdCreatePixelShader> pixelShaderCreates;
		std::vector<CmdCreateInputLayout> layoutCreates;
	};

private:
	RdrContext* m_pRdrContext;

	RdrShader m_vertexShaders[kRdrVertexShader_Count * kRdrShaderFlag_NumCombos];
	RdrShader m_geometryShaders[kRdrGeometryShader_Count * kRdrShaderFlag_NumCombos];
	RdrShader m_computeShaders[kRdrComputeShader_Count];

	RdrShaderHandleMap m_pixelShaderCache;
	RdrShaderList      m_pixelShaders;

	RdrInputLayoutList m_inputLayouts;

	FrameState m_states[2];
	uint       m_queueState;
};
