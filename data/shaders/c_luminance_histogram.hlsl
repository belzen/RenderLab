#include "p_util.hlsli"
#include "p_constants.h"

// Source: http://tomhammersley.blogspot.com/2013/05/image-histogram-construction-in-dx11.html
// TODO: Finish this.  It doesn't currently work right now for a couple reasons:
//		1) Merged histogram is never cleared, so it accumulates each frame
//		2) The existing response curve implementation doesn't seem like it will work
//			- The response curve is a summation of the frequencies in the previous bins as an increasing 0 to 1 value
//				This means the HDR value is not going to be compressed into the 0-1 range for actually displaying on the monitor.
//				There seems to be a missing step of doing that HDR compression.

#define NUM_HISTOGRAM_BINS 64
#define HISTOGRAM_TILE_SIZE_X 32
#define HISTOGRAM_TILE_SIZE_Y 16

cbuffer ToneMapHistogramParamsBuffer : register(c0)
{
	ToneMapHistogramParams cbHistogram;
};

#if STEP_TILE

Texture2D<float4> texInput : register(t0);

RWBuffer<uint> tileHistograms : register(u0);

groupshared uint grp_histogram[NUM_HISTOGRAM_BINS];

[numthreads(HISTOGRAM_TILE_SIZE_X, HISTOGRAM_TILE_SIZE_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex )
{
	if (localIdx == 0)
	{
		for (uint i = 0; i < NUM_HISTOGRAM_BINS; ++i)
		{
			grp_histogram[i] = 0;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	float3 color = texInput.Load(int3(globalId.xy, 0));
	float logLum = log( getLuminance(color) );
	uint bin = NUM_HISTOGRAM_BINS * saturate(logLum - cbHistogram.logLuminanceMin) / (cbHistogram.logLuminanceMax - cbHistogram.logLuminanceMin);

	InterlockedAdd(grp_histogram[bin], 1);

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		uint2 inputSize;
		texInput.GetDimensions(inputSize.x, inputSize.y);

		uint tileStartIndex = groupId.y * ceil(inputSize.x / (float)HISTOGRAM_TILE_SIZE_X) + groupId.x;
		tileStartIndex *= NUM_HISTOGRAM_BINS;

		for (uint i = 0; i < NUM_HISTOGRAM_BINS; ++i)
		{
			tileHistograms[tileStartIndex + i] = grp_histogram[i];
		}
	}
}

#elif STEP_MERGE

Buffer<uint> tileHistograms : register(t0);
RWBuffer<uint> mergedHistogram : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	uint tileIndex = globalId.x;

	if (tileIndex < cbHistogram.tileCount)
	{
		tileIndex *= NUM_HISTOGRAM_BINS;

		for (uint i = 0; i < NUM_HISTOGRAM_BINS; ++i)
		{
			InterlockedAdd(mergedHistogram[i], tileHistograms[tileIndex + i]);
		}
	}
}

#else//if STEP_CURVE

Buffer<uint> mergedHistogram : register(t0);
RWBuffer<float> responseCurve : register(u0);

groupshared float grp_frequencyPerBin[NUM_HISTOGRAM_BINS];
groupshared float grp_initialFrequencyPerBin[NUM_HISTOGRAM_BINS];

[numthreads(1, 1, 1)]
void main(uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	uint i;
	float t = 0.f;
	for (i = 0; i < NUM_HISTOGRAM_BINS; ++i)
	{
		float freq = float(mergedHistogram[i]);
		grp_frequencyPerBin[i] = freq;
		grp_initialFrequencyPerBin[i] = freq;
		t += freq;
	}

	// TODO: This should be the output display range
	float recipDisplayRange = 1.f / (cbHistogram.logLuminanceMax - cbHistogram.logLuminanceMin);

	float deltaB = (cbHistogram.logLuminanceMax - cbHistogram.logLuminanceMin) / (float)NUM_HISTOGRAM_BINS;

	float tolerance = t * 0.025f;
	float trimmings = 0.f;

	uint loops = 0;
	do
	{
		t = 0.f;
		for (i = 0; i < NUM_HISTOGRAM_BINS; ++i)
		{
			t += grp_frequencyPerBin[i];
		}

		if (t < tolerance)
		{
			t = 0.f;
			for (uint n = 0; n < NUM_HISTOGRAM_BINS; ++n)
			{
				grp_frequencyPerBin[n] = grp_initialFrequencyPerBin[n];
				t += grp_frequencyPerBin[n];
			}
			break;
		}

		trimmings = 0.f;
		float ceiling = t * deltaB * recipDisplayRange;
		for (i = 0; i < NUM_HISTOGRAM_BINS; ++i)
		{
			if (grp_frequencyPerBin[i] > ceiling)
			{
				trimmings += grp_frequencyPerBin[i] - ceiling;
				grp_frequencyPerBin[i] = ceiling;
			}
		}

		t -= trimmings;
		++loops;
	} while (trimmings > tolerance && loops < 10);

	float recipT = 1.f / t;
	float sum = 0.f;
	for (i = 0; i < NUM_HISTOGRAM_BINS; ++i)
	{
		float probability = grp_frequencyPerBin[i] * recipT;
		sum += probability;
		responseCurve[i] = sum;
	}
}

#endif