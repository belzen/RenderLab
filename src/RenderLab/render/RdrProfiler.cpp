#include "Precompiled.h"
#include "RdrProfiler.h"
#include "RdrContext.h"

void RdrProfiler::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;
	m_hRenderThreadTimer = Timer::Create();
}

void RdrProfiler::Cleanup()
{
	for (int i = 0; i < kFrameDelay; ++i)
	{
		QueryList& rQueryList = m_frameQueries[i];
		int queryCount = (int)rQueryList.size();
		for (int n = 0; n < queryCount; ++n)
		{
			TimestampQueries& rQueries = rQueryList[n];
			m_pRdrContext->ReleaseQuery(rQueries.timestampEnd);
			m_pRdrContext->ReleaseQuery(rQueries.timestampStart);
		}
	}

	int queryCount = (int)m_queryPool.size();
	for (int n = 0; n < queryCount; ++n)
	{
		TimestampQueries& rQueries = m_queryPool[n];
		m_pRdrContext->ReleaseQuery(rQueries.timestampEnd);
		m_pRdrContext->ReleaseQuery(rQueries.timestampStart);
	}
}

void RdrProfiler::BeginSection(RdrProfileSection eSection)
{
	TimestampQueries queries = {};

	if (!m_queryPool.empty())
	{
		queries = m_queryPool.back();
		m_queryPool.pop_back();
	}

	queries.eSection = eSection;
	if (!queries.timestampStart.bIsValid)
	{
		queries.timestampStart = m_pRdrContext->CreateQuery(RdrQueryType::Timestamp);
		queries.timestampEnd = m_pRdrContext->CreateQuery(RdrQueryType::Timestamp);
	}

	m_pRdrContext->EndQuery(queries.timestampStart);

	m_activeQueryStack[m_currStackDepth] = queries;
	++m_currStackDepth;
}

void RdrProfiler::EndSection()
{
	TimestampQueries& rQueries = m_activeQueryStack[m_currStackDepth - 1];
	m_pRdrContext->EndQuery(rQueries.timestampEnd);
	--m_currStackDepth;

	m_frameQueries[m_currFrame].push_back(rQueries);
}

void RdrProfiler::BeginFrame()
{
	// Reset counters
	memset(m_counters, 0, sizeof(m_counters));

	BeginSection(RdrProfileSection::Frame);

	Timer::Reset(m_hRenderThreadTimer);
}

void RdrProfiler::EndFrame()
{
	EndSection(); // End the main RdrProfileSection::Frame section
	assert(m_currStackDepth == 0);

	// The next frame in the list is also the oldest frame which we want to query from.
	m_currFrame = (m_currFrame + 1) % kFrameDelay;

	uint64 timestampFrequency = m_pRdrContext->GetTimestampFrequency();

	// Clear previous timings
	memset(m_timings, 0, sizeof(m_timings));

	// Accumulate timing data for each section.
	QueryList& rQueryList = m_frameQueries[m_currFrame];
	int queryCount = (int)rQueryList.size();
	for (int i = 0; i < queryCount; ++i)
	{
		TimestampQueries& rQueries = rQueryList[i];
		uint64 start = m_pRdrContext->GetTimestampQueryData(rQueries.timestampStart);
		uint64 end = m_pRdrContext->GetTimestampQueryData(rQueries.timestampEnd);

		m_timings[(int)rQueries.eSection] += 1000.f * (end - start) / (float)timestampFrequency;

		// Return queries to the pool.
		m_queryPool.push_back(rQueries);
	}
	rQueryList.clear();

	// Update counters
	memcpy(m_prevCounters, m_counters, sizeof(m_prevCounters));

	m_renderThreadTime = Timer::GetElapsedSeconds(m_hRenderThreadTimer);
}

float RdrProfiler::GetSectionTime(RdrProfileSection eSection) const
{
	return m_timings[(int)eSection];
}

uint RdrProfiler::GetCounter(RdrProfileCounter eCounter) const
{
	return m_prevCounters[(int)eCounter];
}

float RdrProfiler::GetRenderThreadTime() const
{
	return m_renderThreadTime;
}