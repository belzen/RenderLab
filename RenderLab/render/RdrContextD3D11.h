#pragma once
#include "RdrContext.h"

class RdrContextD3D11 : public RdrContext
{
private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;

};