#include "c_constants.h"

#if 0
Buffer<WaveParticle> g_bufWaveParticles : register(t0);

RWBuffer<float> g_outputHeightField : register(u0);

cbuffer WaveParticleParamsBuffer : register(b0);
{
	WaveParticleParams cbParams;
};
#endif

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
#if 0
	for (int i = 0; i < cbParams.numParticles; ++i)
	{
		float dt = Time::GlobalTime() - m_aParticles[i].spawnTime;
		aVertices[i].position = m_aParticles[i].spawnPosition + m_aParticles[i].direction * m_aParticles[i].velocity * dt;
		aVertices[i].amplitude = m_aParticles[i].amplitude;
	}
#endif
}
