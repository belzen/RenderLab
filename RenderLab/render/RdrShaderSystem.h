#pragma once

#include <map>
#include <vector>
#include "RdrShaders.h"
#include "FreeList.h"
#include "RdrGeometry.h"

class RdrContext;

namespace RdrShaderSystem
{
	void Init(RdrContext* pRdrContext);

	RdrShaderHandle CreatePixelShaderFromFile(const char* filename);
	RdrInputLayoutHandle CreateInputLayout(const RdrVertexShader& vertexShader, const RdrVertexInputElement* aVertexElements, const uint numElements);

	void ReloadShader(const char* filename);

	const RdrShader* GetVertexShader(const RdrVertexShader shader);
	const RdrShader* GetGeometryShader(const RdrGeometryShader shader);
	const RdrShader* GetComputeShader(const RdrComputeShader eShader);
	const RdrShader* GetPixelShader(const RdrShaderHandle hShader);

	const RdrInputLayout* GetInputLayout(const RdrInputLayoutHandle hLayout);

	void FlipState();
	void ProcessCommands(RdrContext* pRdrContext);
}

