#include "v_output.hlsli"
#include "p_common.hlsli"

#define REINHARD 1
#define LOGMAP 2
#define FILMIC 3

#define TONEMAP_METHOD FILMIC

Texture2D texColor : register(t0);
SamplerState sampColor : register(s0);

Texture2D texBloom : register(t1);
SamplerState sampBloom : register(s1);

StructuredBuffer<ToneMapOutputParams> bufToneMapParams : register(t2);

#if TONEMAP_HISTOGRAM
cbuffer ToneMapHistogramParamsBuffer : register(c0)
{
	ToneMapHistogramParams cbHistogram;
};

Buffer<float> responseCurve : register(t3);
#endif

float3 applyFilmicTonemap(float3 color)
{
	// http://filmicgames.com/archives/75
	const float A = 0.15f;
	const float B = 0.5f;
	const float C = 0.1f;
	const float D = 0.2f;
	const float E = 0.02f;
	const float F = 0.3f;

	return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float4 main(VsOutputSprite input) : SV_TARGET
{
	float4 color = texColor.Sample(sampColor, input.texcoords);

	ToneMapOutputParams tonemap = bufToneMapParams[0];

	color.rgb += texBloom.Sample(sampBloom, input.texcoords).rgb;

#if TONEMAP_HISTOGRAM
	float logLum = log(getLuminance(color));
	uint bin = 64 * saturate(logLum - cbHistogram.logLuminanceMin) / (cbHistogram.logLuminanceMax - cbHistogram.logLuminanceMin);
	color.rgb *= (bin < 64) ? responseCurve[bin] : 1.f; // todo - bin should always be in a valid range because of saturate()

#else
	color.rgb *= tonemap.linearExposure;

	#if TONEMAP_METHOD == REINHARD
		color = color / (1.f + color);

	#elif TONEMAP_METHOD == LOGMAP
		//http://resources.mpi-inf.mpg.de/tmo/logmap/logmap.pdf
		float Lw = getLuminance(color.rgb);
		float Lwmax = tonemap.white;
		const float kBias = 0.85f;
		const float kBiasPow = log(kBias) / log(0.5f);
		// Ldmax = 100, factors out to 1
		float Ld = log(Lw + 1.f) / (log10(Lwmax + 1.f) * log(2.f + 8.f * pow(Lw / Lwmax, kBiasPow)));

		color.rgb *= Ld / Lw;

	#elif TONEMAP_METHOD == FILMIC
		const float kExposureBias = 2.f;
		color.rgb = applyFilmicTonemap(color.rgb * kExposureBias) / applyFilmicTonemap(tonemap.white);

	#endif
#endif

	color.rgb = pow(color.rgb, 1 / 2.2f);
	return color;
}