#include "v_output.hlsli"
#include "v_common.hlsli"


VsOutputSprite main(uint id : SV_VertexID)
{
	VsOutputSprite output = (VsOutputSprite)0;

	output.position.x = (id / 2) * 4.f - 1.f;
	output.position.y = (id % 2) * 4.f - 1.f;
	output.position.z = 0.f;
	output.position.w = 1.f;

	output.texcoords.x = (id / 2) * 2.f;
	output.texcoords.y = 1.f - (id % 2) * 2.f;
	
	return output;
}