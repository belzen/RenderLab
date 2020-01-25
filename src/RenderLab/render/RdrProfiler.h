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
	PsConstantBuffer,
	CsConstantBuffer,
	CsResource,
	CsSampler,
	CsUnorderedAccess,

	Count
};

class RdrProfilerCpuSection
{
public:
	RdrProfilerCpuSection(const char* strName);

	uint8 GetId() const { return m_nId; }

private:
	CachedString m_name;
	uint8 m_nId;
};

class RdrGpuProfiler
{
public:
	void Init(RdrContext* pRdrContext);

	void BeginSection(RdrProfileSection eSection);
	void EndSection();

	void BeginFrame();
	void EndFrame();

	void IncrementCounter(RdrProfileCounter eCounter);
	void AddCounter(RdrProfileCounter eCounter, int val);

	float GetSectionTime(RdrProfileSection eSection) const;
	uint GetCounter(RdrProfileCounter eCounter) const;

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

	float m_gpuTimings[(int)RdrProfileSection::Count];

	uint m_counters[(int)RdrProfileCounter::Count];
	uint m_prevCounters[(int)RdrProfileCounter::Count];

	RdrContext* m_pRdrContext;
	int m_currFrame;
};

inline void RdrGpuProfiler::IncrementCounter(RdrProfileCounter eCounter)
{
	m_counters[(int)eCounter]++;
}

inline void RdrGpuProfiler::AddCounter(RdrProfileCounter eCounter, int val)
{
	m_counters[(int)eCounter] += val;
}


class RdrCpuThreadProfiler
{
public:
	static RdrCpuThreadProfiler& GetThreadProfiler(DWORD threadId);
	static std::string MakeSectionName(uint64 nId);

	RdrCpuThreadProfiler();
	~RdrCpuThreadProfiler();

	void BeginSection(const RdrProfilerCpuSection& section);
	void EndSection(const RdrProfilerCpuSection& section);

	void BeginFrame();
	void EndFrame();

	float GetThreadTime() const;
	
	const std::map<uint64, float>& GetTimings() const;

private:
	uint64 MakeStackId() const;

	std::map<uint64, float> m_frameTimings[2];

	struct TimingBlock
	{
		uint8 nId;
		float fStartTime;
	};
	typedef std::vector<TimingBlock> TimingBlockList;
	TimingBlockList m_sectionStack;

	uint m_nCurrFrame;

	Timer::Handle m_hThreadTimer;
	float m_threadTime;
};

class RdrAutoProfilerCpuSection
{
public:
	RdrAutoProfilerCpuSection(RdrCpuThreadProfiler& profiler, const RdrProfilerCpuSection& section)
		: m_profiler(profiler)
		, m_section(section)
	{
		m_profiler.BeginSection(m_section);
	}

	~RdrAutoProfilerCpuSection()
	{
		m_profiler.EndSection(m_section);
	}

private:
	RdrCpuThreadProfiler& m_profiler;
	const RdrProfilerCpuSection& m_section;
};

#define RDR_PROFILER_CPU_SECTION(name) \
	static RdrProfilerCpuSection s_section(name); \
	RdrAutoProfilerCpuSection activeProfilerSection(RdrCpuThreadProfiler::GetThreadProfiler(GetCurrentThreadId()), s_section);
