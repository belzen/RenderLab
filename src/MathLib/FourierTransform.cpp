#include "FourierTransform.h"
#include "Maths.h"

// http://www.keithlantz.net/2011/11/ocean-simulation-part-two-using-the-fast-fourier-transform/
// TODO: Research the FFT and actually understand how it works.  This is basically a straight copy from the above URL.

namespace
{
	// Reverse bits in "i" given a max usable bit of maxBit - 1).
	// e.g. If:  
	//		i = 0x01, and
	//		maxBit = 7 (0x80 or 128), then
	//		res = 0x40
	int reverseBitRange(int i, int maxBit)
	{
		int res = 0;
		for (int j = 0; j < maxBit; j++)
		{
			res = (res << 1) + (i & 1);
			i >>= 1;
		}
		return res;
	}
}

FourierTransform::FourierTransform()
	: m_size(0)
	, m_sizeLog2(0)
	, m_reversed(nullptr)
	, m_w(nullptr)
{

}

void FourierTransform::Init(int size)
{
	m_size = size;
	m_sizeLog2 = (int)log2(m_size);

	m_reversed = new int[m_size];
	for (int i = 0; i < m_size; i++)
	{
		m_reversed[i] = reverseBitRange(i, m_sizeLog2);
	}

	int pow2 = 1;
	m_w = new Complex*[m_sizeLog2];
	for (int i = 0; i < m_sizeLog2; i++)
	{
		m_w[i] = new Complex[pow2];
		float mul = Maths::kTwoPi / (pow2 * 2);

		for (int j = 0; j < pow2; j++)
		{
			m_w[i][j] = Complex(cosf(mul * j), sinf(mul * j));
		}

		pow2 *= 2;
	}
}

FourierTransform::~FourierTransform()
{
	if (m_reversed)
	{
		delete[] m_reversed;
	}
	
	if (m_w)
	{
		for (int i = 0; i < m_sizeLog2; i++)
		{
			delete[] m_w[i];
		}
		delete[] m_w;
	}
}

void FourierTransform::Fast(Complex* input, Complex* output, int stride, int offset)
{
	Complex* c[2];
	c[0] = (Complex*)alloca(m_size * sizeof(Complex));
	c[1] = (Complex*)alloca(m_size * sizeof(Complex));
	memset(c[0], 0, m_size * sizeof(Complex));
	memset(c[1], 0, m_size * sizeof(Complex));

	int which = 0;

	for (int i = 0; i < m_size; i++)
		c[which][i] = input[m_reversed[i] * stride + offset];

	int loops = m_size >> 1;
	int size = 1 << 1;
	int size_over_2 = 1;
	int w_ = 0;
	for (int i = 1; i <= m_sizeLog2; i++)
	{
		which ^= 1;
		for (int j = 0; j < loops; j++)
		{
			for (int k = 0; k < size_over_2; k++)
			{
				c[which][size * j + k] = c[which ^ 1][size * j + k] +
					c[which ^ 1][size * j + size_over_2 + k] * m_w[w_][k];
			}

			for (int k = size_over_2; k < size; k++)
			{
				c[which][size * j + k] = c[which ^ 1][size * j - size_over_2 + k] -
					c[which ^ 1][size * j + k] * m_w[w_][k - size_over_2];
			}
		}
		loops >>= 1;
		size <<= 1;
		size_over_2 <<= 1;
		w_++;
	}

	for (int i = 0; i < m_size; i++)
		output[i * stride + offset] = c[which][i];
}
