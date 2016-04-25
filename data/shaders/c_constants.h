#ifndef SHADER_C_CONSTANTS_H
#define SHADER_C_CONSTANTS_H

#define BLUR_TEXELS_PER_THREAD 4
#define BLUR_THREAD_COUNT 64
#define BLUR_SIZE 9
#define BLUR_HALFSIZE 4

#define ADD_THREADS_X 32
#define ADD_THREADS_Y 16

struct ToneMapInputParams
{
	float white;
	float middleGrey;
	float bloomThreshold;
	float unused;
};

struct AddParams
{
	float width;
	float height;
	float2 unused;
};


#endif // SHADER_C_CONSTANTS_H