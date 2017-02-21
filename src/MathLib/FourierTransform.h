#pragma once

#include <complex>

typedef std::complex<float> Complex;

class FourierTransform
{
public:
	FourierTransform();
	~FourierTransform();

	void Init(int size);

	void Fast(Complex* input, Complex* output, int stride, int offset);

private:
	int m_size;
	int m_sizeLog2;

	int* m_reversed;
	Complex** m_w;
};