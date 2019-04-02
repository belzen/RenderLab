#pragma once

#include "RdrDeviceTypes.h"
#include "UtilsLib/Timer.h"

class RdrContext;

enum class RdrProfileSection
{
	Frame,
	Shadows,
	ZPrepass,
	LightCulling,
	VolumetricFog,
	Opaque,
	Decal,
	Alpha,
	Sky,
	PostProcessing,
	UI,
	Editor,
	Wireframe,

	Count
};

enum class RdrProfileCounter
{
	DrawCall,
	Triangles,

	// State changes
	VertexShader,
	GeometryShader,
	HullShader,
	DomainShader,
	PixelShader,
	VertexBuffer,
	IndexBuffer,
	InputLayout,
	PrimitiveTopology,
	VsResource,
	PsResource,
	PsSamplers,
	DsResource,
	DsSamplers,
	VsConstantBuffer,
	DsConstantBuffer,
	GsConstantBuffer,
	PsConstantBuffer,
	CsConstantBuffer,
	CsResource,
	CsSampler,
	CsUnorderedAccess,

	Count
};

class RdrProfiler
{
public:
	void Init(RdrContext* pRdrContext);
	void Cleanup();

	void BeginSection(RdrProfileSection eSection);
	void EndSection();

	void BeginFrame();
	void EndFrame();

	void IncrementCounter(RdrProfileCounter eCounter);
	void AddCounter(RdrProfileCounter eCounter, int val);

	float GetSectionTime(RdrProfileSection eSection) const;
	uint GetCounter(RdrProfileCounter eCounter) const;

	float GetRenderThreadTime() const;

private:
	// Intel HD 530 takes 6 frames for timestamp data to be ready, seems pretty fishy...
	static const int kFrameDelay = 6;

	struct TimestampQueries
	{
		RdrQuery timestampStart;
		RdrQuery timestampEnd;
		RdrProfileSection eSection;
	};
	typedef std::vector<TimestampQueries> QueryList;

	QueryList m_frameQueries[kFrameDelay];

	TimestampQueries m_activeQueryStack[8];
	uint m_currStackDepth;

	float m_timings[(int)RdrProfileSection::Count];

	uint m_counters[(int)RdrProfileCounter::Count];
	uint m_prevCounters[(int)RdrProfileCounter::Count];

	Timer::Handle m_hRenderThreadTimer;
	float m_renderThreadTime;

	RdrContext* m_pRdrContext;
	int m_currFrame;
};

inline void RdrProfiler::IncrementCounter(RdrProfileCounter eCounter)
{
	m_counters[(int)eCounter]++;
}

inline void RdrProfiler::AddCounter(RdrProfileCounter eCounter, int val)
{
	m_counters[(int)eCounter] += val;
}
