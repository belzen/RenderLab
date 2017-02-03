#include "p_constants.h"
#include "v_output.hlsli"

Texture2D g_tex : register(t0);
SamplerState g_samClamp : register(s0);

// Generic texture copy to render target.
// Primarily used for debugging (copying debug textures over the backbuffer).
float4 main(VsOutputSprite input) : SV_TARGET
{
	return g_tex.Sample(g_samClamp, input.texcoords);
}