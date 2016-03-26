#pragma once

enum RdrPassEnum
{
	kRdrPass_ZPrepass,
	kRdrPass_Opaque,
	kRdrPass_Alpha,
	kRdrPass_UI,

	kRdrPass_Count
};

enum RdrBucketType
{
	kRdrBucketType_Opaque,
	kRdrBucketType_Alpha,
	kRdrBucketType_UI,

	kRdrBucketType_Count
};

enum RdrShaderMode
{
	kRdrShaderMode_Normal,
	kRdrShaderMode_DepthOnly,
	kRdrShaderMode_CubeMapDepthOnly,

	kRdrShaderMode_Count
};

enum RdrTexCoordMode
{
	kRdrTexCoordMode_Wrap,
	kRdrTexCoordMode_Clamp,
	kRdrTexCoordMode_Mirror,

	kRdrTexCoordMode_Count
};

enum RdrResourceFormat
{
	kResourceFormat_D16,
	kResourceFormat_R16_UNORM,
	kResourceFormat_RG_F16,
	kResourceFormat_Count,
};

enum RdrComparisonFunc
{
	kComparisonFunc_Never,
	kComparisonFunc_Less,
	kComparisonFunc_Equal,
	kComparisonFunc_LessEqual,
	kComparisonFunc_Greater,
	kComparisonFunc_NotEqual,
	kComparisonFunc_GreaterEqual,
	kComparisonFunc_Always,

	kComparisonFunc_Count
};