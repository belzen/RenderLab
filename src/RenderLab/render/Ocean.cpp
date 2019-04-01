#include "Precompiled.h"
#include "Ocean.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "RdrAction.h"
#include "Renderer.h"

// References:
// Simulating Ocean Water https://people.cs.clemson.edu/~jtessen/papers_files/coursenotes2004.pdf
// http://www.keithlantz.net/2011/10/ocean-simulation-part-one-using-the-discrete-fourier-transform/
// http://malideveloper.arm.com/sample-code/ocean-rendering-with-fast-fourier-transform/

struct FourierGridCell
{
	Complex h0;
	Vec2 k;
	float dispersion;

	// Derivative values used for choppiness displacement
	float dx;
	float dz;
};

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Ocean, RdrShaderFlags::None };

	static const RdrVertexInputElement s_oceanVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::R_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
	};

	struct OceanVertex
	{
		Vec3 position;
		Vec3 normal;
		float jacobian;
	};

	//////////////////////////////////////////////////////////////////////////

	static const float kGravity = 9.8f;
	static const float kTimePeriod = 200.f; // Repeat time in seconds.

	Complex randomGaussian()
	{
		// http://www.design.caltech.edu/erik/Misc/Gaussian.html
		float x1, x2, w;

		do 
		{
			x1 = 2.f * randFloat() - 1.f;
			x2 = 2.f * randFloat() - 1.f;
			w = x1 * x1 + x2 * x2;
		} 
		while (w >= 1.f);

		w = sqrt((-2.f * log(w)) / w);

		return Complex(x1 * w, x2 * w);
	}

	float phillipsSpectrum(const Vec2& k, float A, const Vec2& windDir, float windSpeed)
	{
		// Phillips spectrum - Figure 40 from Tessendorf paper
		// P(k) = A * (exp(-1 / (_k * L)^2) / _k^4) * |k' * w|^2
		// A = wave height scalar
		// L = V^2 / g
		// V = Wind speed
		// w = Wind dir
		// _k = length(k)
		// k' = normalized k

		float kLen = Vec2Length(k);
		if (kLen < Maths::kEpsilon)
			return 0.f; // Avoid divide by zero

		float L = (windSpeed * windSpeed) / kGravity;
		float kLSqr = (kLen * L) * (kLen * L);

		float kDotWind = Vec2Dot(Vec2Normalize(k), windDir);

		// Spectrum results
		float res = kDotWind * kDotWind * (A * exp(-1.f / kLSqr)) / (kLen * kLen * kLen * kLen);

		// Suppress small waves
		const float kSupressionPercent = 0.001f;
		float l = L * kSupressionPercent;
		float waveSuppression = exp(-kLen * kLen * l * l);

		return res * waveSuppression;
	}

	float calcDispersion(const Vec2 k)
	{
		// Dispersion - Figure 35 from Tessendorf paper
		// w(k) = floor(w'(k) / w0) * w0
		// w'(k) = sqrt(g * |k|)
		// w0 = 2 * pi / T
		// T = repeat time
		float w0 = Maths::kTwoPi / kTimePeriod;
		float w = sqrt(kGravity * Vec2Length(k));
		return floor(w / w0) * w0;
	}
}

Ocean::Ocean()
	: m_initialized(false)
{

}

void Ocean::GenerateFourierGrid(int gridSize, float tileWorldSize, const Vec2& wind)
{
	// Destroy the old grid if we have one.
	if (m_grid)
	{
		delete[] m_grid;
	}

	// Allocate the new grid.
	m_tileSize = tileWorldSize;
	m_gridSize = gridSize;
	m_grid = new FourierGridCell[m_gridSize * m_gridSize];

	m_h = new Complex[m_gridSize * m_gridSize];
	m_hDx = new Complex[m_gridSize * m_gridSize];
	m_hDz = new Complex[m_gridSize * m_gridSize];
	m_hSlopeX = new Complex[m_gridSize * m_gridSize];
	m_hSlopeZ = new Complex[m_gridSize * m_gridSize];

	// Generate
	Vec2 windDir = Vec2Normalize(wind);
	float windSpeed = Vec2Length(wind);

	int gridHalfSize = m_gridSize / 2;
	float kScalar = Maths::kTwoPi / m_tileSize;

	for (int m = 0; m < m_gridSize; ++m)
	{
		for (int n = 0; n < m_gridSize; ++n)
		{
			int idx = (m * m_gridSize) + n;
			FourierGridCell& rCell = m_grid[idx];

			rCell.k.x = (float)((n > gridHalfSize) ? (n - m_gridSize) : n);
			rCell.k.y = (float)((m > gridHalfSize) ? (m - m_gridSize) : m);
			rCell.k *= kScalar;

			Complex gauss = randomGaussian();
			float phillips = sqrt(phillipsSpectrum(rCell.k, m_waveHeightScalar, windDir, windSpeed));
			rCell.h0 = gauss * phillips / sqrt(2.f);
			rCell.dispersion = calcDispersion(rCell.k);

			// Derivatives used for displacement/normals
			float len = Vec2Length(rCell.k);
			rCell.dx = rCell.k.x / (len + 0.00001f);
			rCell.dz = -rCell.k.y / (len + 0.00001f);
		}
	}
}

void Ocean::Init(float tileWorldSize, UVec2 tileCounts, int fourierGridSize, float waveHeightScalar, const Vec2& wind)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();

	m_initialized = true;
	m_timer = 0.f;
	m_waveHeightScalar = waveHeightScalar;

	m_fft.Init(fourierGridSize);
	GenerateFourierGrid(fourierGridSize, tileWorldSize, wind);

	int numVerts = m_gridSize * m_gridSize;
	OceanVertex* aVertices = (OceanVertex*)RdrFrameMem::AllocAligned(sizeof(OceanVertex) * numVerts, 16);

	const int numColumns = (m_gridSize - 1);
	const int numRowTris = numColumns * 2;
	const int numTriangles = (m_gridSize - 1) * numRowTris;
	uint16* aIndices = (uint16*)RdrFrameMem::AllocAligned(sizeof(uint16) * numTriangles * 3, 16);
	
	for (int m = 0; m < m_gridSize - 1; ++m)
	{
		for (int n = 0; n < m_gridSize - 1; ++n)
		{
			int idx = (numRowTris * m * 3) + (n * 2 * 3);
			aIndices[idx + 0] = (m * m_gridSize) + n;
			aIndices[idx + 1] = ((m + 1) * m_gridSize) + n;
			aIndices[idx + 2] = (m * m_gridSize) + n + 1;

			aIndices[idx + 3] = (m * m_gridSize) + n + 1;
			aIndices[idx + 4] = ((m + 1) * m_gridSize) + n;
			aIndices[idx + 5] = ((m + 1) * m_gridSize) + n + 1;
		}
	}

	m_hGeo = rResCommandList.CreateGeo(
		aVertices, sizeof(aVertices[0]), numVerts,
		aIndices, numTriangles * 3,
		RdrTopology::TriangleList, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f), this);

	// Setup pixel material
	memset(&m_material, 0, sizeof(m_material));
	m_material.bNeedsLighting = true;
	m_material.hConstants = rResCommandList.CreateConstantBuffer(nullptr, 16, RdrResourceAccessFlags::CpuRW_GpuRO, this);

	const RdrResourceFormat* pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene);
	uint nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = false;//donotcheckin - match g_debugState
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ocean.hlsl", nullptr, 0);
	m_material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader,
		s_oceanVertexDesc, ARRAY_SIZE(s_oceanVertexDesc), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kOpaque,
		rasterState,
		RdrDepthStencilState(true, true, RdrComparisonFunc::Equal));

	// Setup per-object constants
	Matrix44 mtxWorld = Matrix44Translation(Vec3(0.f, 5.f, 0.f));
	uint constantsSize = sizeof(VsPerObject);
	VsPerObject* pVsPerObject = (VsPerObject*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pVsPerObject->mtxWorld = Matrix44Transpose(mtxWorld);

	m_hVsPerObjectConstantBuffer = g_pRenderer->GetResourceCommandList().CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer,
		pVsPerObject, constantsSize, RdrResourceAccessFlags::CpuRW_GpuRO, this);
}

Vec2 Ocean::GetDisplacement(int n, int m)
{
	int idx = (m * m_gridSize) + n;
	return Vec2(m_hDx[idx].real(), m_hDz[idx].real());
}

void Ocean::Update()
{
	if (!m_initialized)
		return;

	m_timer += Time::FrameTime();

	// Update geometry
	OceanVertex* aVertices = (OceanVertex*)RdrFrameMem::AllocAligned(sizeof(OceanVertex) * m_gridSize * m_gridSize, 16);
	
	// Generate h~ values for each point in the grid.
	for (int m = 0; m < m_gridSize; ++m)
	{
		for (int n = 0; n < m_gridSize; ++n)
		{
			// Figure 43 from Tessendorf paper
			// h~(k, t) = h0(k) * exp(i * w(k) * t)
			//		  + conj(h0(-k)) * exp(-i * w(k) * t)
			int idx = (m * m_gridSize) + n;
			const FourierGridCell& rCell = m_grid[idx];

			// Negative frequency
			int n2 = (n == 0) ? 0 : (m_gridSize - n);
			int m2 = (m == 0) ? 0 : (m_gridSize - m);
			int idxNeg = (m2 * m_gridSize) + n2;
			const FourierGridCell& rCellNeg = m_grid[idxNeg];

			// Calculate h~
			float c = cos(rCell.dispersion * m_timer);
			float s = sin(rCell.dispersion * m_timer);
			m_h[idx] = rCell.h0 * Complex(c, s) + rCellNeg.h0 * Complex(c, -s);

			m_hSlopeX[idx] = m_h[idx] * Complex(0, rCell.k.x);
			m_hSlopeZ[idx] = m_h[idx] * Complex(0, rCell.k.y);
			m_hDx[idx] = m_h[idx] * Complex(0, rCell.dx);
			m_hDz[idx] = m_h[idx] * Complex(0, rCell.dz);
		}
	}
	
	for (int m = 0; m < m_gridSize; m++)
	{
		m_fft.Fast(m_h, m_h, 1, m * m_gridSize);
		m_fft.Fast(m_hSlopeX, m_hSlopeX, 1, m * m_gridSize);
		m_fft.Fast(m_hSlopeZ, m_hSlopeZ, 1, m * m_gridSize);
		m_fft.Fast(m_hDx, m_hDx, 1, m * m_gridSize);
		m_fft.Fast(m_hDz, m_hDz, 1, m * m_gridSize);
	}

	for (int n = 0; n < m_gridSize; n++)
	{
		m_fft.Fast(m_h, m_h, m_gridSize, n);
		m_fft.Fast(m_hSlopeX, m_hSlopeX, m_gridSize, n);
		m_fft.Fast(m_hSlopeZ, m_hSlopeZ, m_gridSize, n);
		m_fft.Fast(m_hDx, m_hDx, m_gridSize, n);
		m_fft.Fast(m_hDz, m_hDz, m_gridSize, n);
	}

	int gridHalfSize = m_gridSize / 2;
	for (int m = 0; m < m_gridSize; ++m)
	{
		for (int n = 0; n < m_gridSize; ++n)
		{
			int idx = (m * m_gridSize) + n;

			aVertices[idx].position.x = ((n - gridHalfSize) * m_tileSize) / m_gridSize;
			aVertices[idx].position.y = m_h[idx].real(); 
			aVertices[idx].position.z = ((m - gridHalfSize) * m_tileSize) / m_gridSize;

			// Apply displacement to the XZ positions to get some choppiness to the waves.
			// This generally results in high peaks and flat valleys instead of simple heightfield points moving up and down.
			Vec2 displacement = Vec2(m_hDx[idx].real(), m_hDz[idx].real());
			aVertices[idx].position.x += displacement.x;
			aVertices[idx].position.z += displacement.y;

			// Normal
			Vec3 normal = Vec3(0.0f - m_hSlopeX[idx].real(), 1.0f, 0.0f - m_hSlopeZ[idx].real());
			aVertices[idx].normal = Vec3Normalize(normal);

			// Jacobian for turbulence coloring.
			const float lambda = 1.2f;
			Vec2 dDdx = 0.5f * lambda * (GetDisplacement(n, m + 1) - GetDisplacement(n, m - 1));
			Vec2 dDdy = 0.5f * lambda * (GetDisplacement(n, m + 1) - GetDisplacement(n, m - 1));
			aVertices[idx].jacobian = (1.f + dDdx.x) * (1.f + dDdy.y) - dDdx.y * dDdy.x;
		}
	}

	g_pRenderer->GetResourceCommandList().UpdateGeoVerts(m_hGeo, aVertices, this);
}

RdrDrawOpSet Ocean::BuildDrawOps(RdrAction* pAction)
{
	if (!m_initialized)
		return RdrDrawOpSet();

	//////////////////////////////////////////////////////////////////////////
	// Fill out the draw op
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
	pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->pMaterial = &m_material;

	return RdrDrawOpSet(pDrawOp, 1);
}
