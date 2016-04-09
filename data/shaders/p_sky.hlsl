#include "vs_output.hlsli"

TextureCube texSkyDome : register(t0);
SamplerState sampSkyDome : register(s0);

float4 main(VsOutputSky input) : SV_TARGET
{
	return texSkyDome.Sample(sampSkyDome, input.direction);
}