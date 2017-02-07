
float getLuminance(float3 color)
{
	// https://en.wikipedia.org/wiki/Relative_luminance
	const float3 kLumScalar = float3(0.2126f, 0.7152f, 0.0722f);
	return dot(color, kLumScalar);
}

// Reconstruct linear view depth from depth-buffer value.
float reconstructViewDepth(float depth, float nearDist, float farDist)
{
	float a = farDist / (farDist - nearDist);
	float b = (-farDist * nearDist) / (farDist - nearDist);
	return b / (depth - a);
}
