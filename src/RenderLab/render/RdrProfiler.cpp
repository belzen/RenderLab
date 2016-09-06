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
		m_pRdrContext->ReleaseQuery(m_disjointQueries[i]);

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
	TimestampQueries queries = { 0 };

	if (!m_queryPool.empty())
	{
		queries = m_queryPool.back();
		m_queryPool.pop_back();
	}

	queries.eSection = eSection;
	if (!queries.timestampStart.pTypeless)
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

	RdrQuery& rDisjointQuery = m_disjointQueries[m_currFrame];
	if (!rDisjointQuery.pTypeless)
	{
		rDisjointQuery = m_pRdrContext->CreateQuery(RdrQueryType::Disjoint);
	}

	m_pRdrContext->BeginQuery(rDisjointQuery);
	BeginSection(RdrProfileSection::Frame);

	Timer::Reset(m_hRenderThreadTimer);
}

void RdrProfiler::EndFrame()
{
	EndSection(); // End the main RdrProfileSection::Frame section
	assert(m_currStackDepth == 0);

	RdrQuery& rDisjointQuery = m_disjointQueries[m_currFrame];
	m_pRdrContext->EndQuery(rDisjointQuery);

	// The next frame in the list is also the oldest frame which we want to query from.
	m_currFrame = (m_currFrame + 1) % kFrameDelay;

	if (!m_disjointQueries[m_currFrame].pTypeless)
	{
		// Disjoint query hasn't been created yet meaning we're still in the first 5 frames of the app.
		return;
	}

	RdrQueryDisjointData disjointData = m_pRdrContext->GetDisjointQueryData(m_disjointQueries[m_currFrame]);

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

		m_timings[(int)rQueries.eSection] += 1000.f * (end - start) / (float)disjointData.frequency;

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