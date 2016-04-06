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

	RdrShaderHandle CreateShaderFromFile(RdrShaderType eType, const char* filename);
	RdrInputLayoutHandle CreateInputLayout(RdrShaderHandle hVertexShader, RdrVertexInputElement* aVertexElements, uint numElements);

	const RdrShader* GetShader(RdrShaderType eType, RdrShaderHandle hShader);
	const RdrInputLayout* GetInputLayout(RdrInputLayoutHandle hLayout);

	void FlipState();
	void ProcessCommands();

private:
	struct CmdCreateShader
	{
		RdrShaderHandle hShader;
		char* pShaderText;
		uint textLen;
		RdrShaderType eType;
	};

	struct CmdCreateInputLayout
	{
		RdrInputLayoutHandle hLayout;
		RdrShaderHandle hVertexShader;
		RdrVertexInputElement vertexElements[16];
		uint numElements;
	};

	struct FrameState
	{
		std::vector<CmdCreateShader> shaderCreates;
		std::vector<CmdCreateInputLayout> layoutCreates;
	};

private:
	RdrContext* m_pRdrContext;

	RdrShaderHandleMap m_shaderCaches[kRdrShaderType_Count];
	RdrShaderList      m_shaders[kRdrShaderType_Count];
	RdrInputLayoutList m_inputLayouts;

	FrameState m_states[2];
	uint       m_queueState;
};
