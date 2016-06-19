#pragma once

#include <map>
#include <string>
#include "RdrDeviceTypes.h"

class RdrContext;

enum class RdrProfileSection
{
	Frame,
	Shadows,
	ZPrepass,
	LightCulling,
	Opaque,
	Alpha,
	Sky,
	PostProcessing,
	UI,

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

	float GetSectionTime(RdrProfileSection eSection) const;

private:
	static const int kFrameDelay = 5;

	struct TimestampQueries
	{
		RdrQuery timestampStart;
		RdrQuery timestampEnd;
		RdrProfileSection eSection;
	};
	typedef std::vector<TimestampQueries> QueryList;

	QueryList m_queryPool;

	QueryList m_frameQueries[kFrameDelay];
	RdrQuery m_disjointQueries[kFrameDelay];

	TimestampQueries m_activeQueryStack[8];
	uint m_currStackDepth;

	float m_timings[(int)RdrProfileSection::Count];

	RdrContext* m_pRdrContext;
	int m_currFrame;
};