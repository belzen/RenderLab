
SamplerState           g_samClamp         : register(s14);
SamplerComparisonState g_samShadowMaps    : register(s15);

Texture3D<float4> g_texVolumetricFogLut    : register(t12);
Texture2D<float4> g_texSkyTransmittanceLut : register(t13);
Texture2DArray    g_texShadowMaps          : register(t14);
TextureCubeArray  g_texShadowCubemaps      : register(t15);

StructuredBuffer<SpotLight>        g_bufSpotLights   : register(t16);
StructuredBuffer<PointLight>       g_bufPointLights  : register(t17);
Buffer<uint>                       g_bufLightIndices : register(t18);