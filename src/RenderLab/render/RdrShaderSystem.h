#pragma once

#include "RdrShaders.h"
#include "RdrGeometry.h"

class RdrContext;

namespace RdrShaderSystem
{
	void Init(RdrContext* pRdrContext);

	const RdrShader* CreatePixelShaderFromFile(const char* filename, const char** aDefines, uint numDefines);

	void ReloadShader(const char* filename);
	void ReloadAllShaders();

	void SetGlobalShaderDefine(const char* define, bool enable);

	const RdrShader* GetVertexShader(const RdrVertexShader& shader);
	const RdrShader* GetGeometryShader(const RdrGeometryShader& shader);
	const RdrShader* GetHullShader(const RdrTessellationShader& shader);
	const RdrShader* GetDomainShader(const RdrTessellationShader& shader);

	const RdrShader* GetComputeShader(const RdrComputeShader eShader);
	const RdrPipelineState* GetComputeShaderPipelineState(const RdrComputeShader eShader);

	const RdrShader* GetWireframePixelShader();

	void FlipState();
	void ProcessCommands(RdrContext* pRdrContext);
}

