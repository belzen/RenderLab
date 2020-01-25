#include "Precompiled.h"
#include "RdrProfiler.h"
#include "RdrContext.h"

namespace
{
	static std::vector<CachedString> s_cpuSectionNames;
	static std::map<DWORD, RdrCpuThreadProfiler> s_cpuThreadProfilers;
}

RdrProfilerCpuSection::RdrProfilerCpuSection(const char* strName)
	: m_name(strName)
	, m_nId(0)
{
	for (uint i = 0; i < s_cpuSectionNames.size(); ++i)
	{
		if (s_cpuSectionNames[i] == m_name)
		{
			m_nId = i + 1;
			break;
		}
	}

	if (m_nId == 0)
	{
		Assert(s_cpuSectionNames.size() < 255);
		s_cpuSectionNames.push_back(strName);
		m_nId = (uint8)s_cpuSectionNames.size();
	}
}

void RdrGpuProfiler::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;
}

void RdrGpuProfiler::BeginSection(RdrProfileSection eSection)
{
	TimestampQueries queries = {};

	queries.eSection = eSection;
	queries.timestampStart = m_pRdrContext->InsertTimestampQuery();

	m_activeQueryStack[m_currStackDepth] = queries;
	++m_currStackDepth;
}

void RdrGpuProfiler::EndSection()
{
	TimestampQueries& rQueries = m_activeQueryStack[m_currStackDepth - 1];
	rQueries.timestampEnd = m_pRdrContext->InsertTimestampQuery();
	--m_currStackDepth;

	m_frameQueries[m_currFrame].push_back(rQueries);
}

void RdrGpuProfiler::BeginFrame()
{
	// Reset counters
	memset(m_counters, 0, sizeof(m_counters));

	BeginSection(RdrProfileSection::Frame);
}

void RdrGpuProfiler::EndFrame()
{
	EndSection(); // End the main RdrProfileSection::Frame section
	Assert(m_currStackDepth == 0);

	// The next frame in the list is also the oldest frame which we want to query from.
	m_currFrame = (m_currFrame + 1) % kFrameDelay;

	uint64 timestampFrequency = m_pRdrContext->GetTimestampFrequency();

	// Clear previous timings
	memset(m_gpuTimings, 0, sizeof(m_gpuTimings));

	// Accumulate timing data for each section.
	QueryList& rQueryList = m_frameQueries[m_currFrame];
	int queryCount = (int)rQueryList.size();
	for (int i = 0; i < queryCount; ++i)
	{
		TimestampQueries& rQueries = rQueryList[i];
		uint64 start = m_pRdrContext->GetTimestampQueryData(rQueries.timestampStart);
		uint64 end = m_pRdrContext->GetTimestampQueryData(rQueries.timestampEnd);

		m_gpuTimings[(int)rQueries.eSection] += 1000.f * (end - start) / (float)timestampFrequency;
	}
	rQueryList.clear();

	// Update counters
	memcpy(m_prevCounters, m_counters, sizeof(m_prevCounters));
}

float RdrGpuProfiler::GetSectionTime(RdrProfileSection eSection) const
{
	return m_gpuTimings[(int)eSection];
}

uint RdrGpuProfiler::GetCounter(RdrProfileCounter eCounter) const
{
	return m_prevCounters[(int)eCounter];
}

//////////////////////////////////////////////////////////////////////////
RdrCpuThreadProfiler& RdrCpuThreadProfiler::GetThreadProfiler(DWORD threadId)
{
	auto iter = s_cpuThreadProfilers.find(threadId);
	if (iter == s_cpuThreadProfilers.end())
	{
		s_cpuThreadProfilers[threadId] = RdrCpuThreadProfiler();
		return s_cpuThreadProfilers[threadId];
	}
	else
	{
		return iter->second;
	}
}

RdrCpuThreadProfiler::RdrCpuThreadProfiler()
{
	m_hThreadTimer = Timer::Create();
	m_nCurrFrame = 0;
}

RdrCpuThreadProfiler::~RdrCpuThreadProfiler()
{
	Timer::Release(m_hThreadTimer);
}

void RdrCpuThreadProfiler::BeginFrame()
{
	m_nCurrFrame = (m_nCurrFrame + 1) % ARRAYSIZE(m_frameTimings);

	// Reset timings
	m_frameTimings[m_nCurrFrame].clear();
	Timer::Reset(m_hThreadTimer);
}

void RdrCpuThreadProfiler::EndFrame()
{
	Assert(m_sectionStack.empty());
	m_threadTime = Timer::GetElapsedSeconds(m_hThreadTimer);
}

void RdrCpuThreadProfiler::BeginSection(const RdrProfilerCpuSection& section)
{
	Assert(m_sectionStack.size() <= 8);
	m_sectionStack.push_back(TimingBlock{ section.GetId(), Timer::GetElapsedSeconds(m_hThreadTimer) });
}

void RdrCpuThreadProfiler::EndSection(const RdrProfilerCpuSection& section)
{
	const TimingBlock& block = m_sectionStack.back();
	Assert(block.nId == section.GetId());

	float fTime = Timer::GetElapsedSeconds(m_hThreadTimer);
	uint64 nStackId = MakeStackId();

	auto iter = m_frameTimings[m_nCurrFrame].find(nStackId);
	if (iter == m_frameTimings[m_nCurrFrame].end())
	{
		m_frameTimings[m_nCurrFrame][nStackId] = fTime;
	}
	else
	{
		iter->second += fTime;
	}

	m_sectionStack.pop_back();
}

uint64 RdrCpuThreadProfiler::MakeStackId() const
{
	uint64 nStackId = 0;
	for (uint i = 0; i < m_sectionStack.size(); ++i)
	{
		nStackId |= ((uint64)m_sectionStack[i].nId) << (i * 8);
	}
	return nStackId;
}

float RdrCpuThreadProfiler::GetThreadTime() const
{
	return m_threadTime;
}

const std::map<uint64, float>& RdrCpuThreadProfiler::GetTimings() const
{
	return m_frameTimings[(m_nCurrFrame - 1) % ARRAYSIZE(m_frameTimings)];
}

std::string RdrCpuThreadProfiler::MakeSectionName(uint64 nFullId)
{
	std::string str;
	while (nFullId != 0)
	{
		uint8 nSectionId = (nFullId & 0xff);
		str += s_cpuSectionNames[nSectionId - 1].getString();
		str += ".";

		nFullId >>= 8;
	}

	return str;
}