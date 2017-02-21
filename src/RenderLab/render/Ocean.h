#pragma once

#include "AssetLib/SceneAsset.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"
#include "RdrTessellationMaterial.h"
#include "RdrDrawOp.h"
#include "MathLib/FourierTransform.h"

class RdrAction;
class Camera;
struct FourierGridCell;

class Ocean
{
public:
	Ocean();

	// tileWorldSize - Size of an ocean tile in world units.
	// tileCounts - Number of tiles in each direction to build the ocean from.
	// fourierGridSize - Size of the fourier grid used on the tile.
	// waveHeightScalar - 
	// wind - Direction of the wind
	void Init(float tileWorldSize, UVec2 tileCounts, int fourierGridSize, float waveHeightScalar, const Vec2& wind);

	void Update();

	RdrDrawOpSet BuildDrawOps(RdrAction* pAction);

private:
	void GenerateFourierGrid(int gridSize, float tileWorldSize, const Vec2& wind);
	Vec2 GetDisplacement(int n, int m);
	
private:
	RdrMaterial m_material;

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrInputLayoutHandle m_hInputLayout;
	RdrGeoHandle m_hGeo;

	FourierTransform m_fft;
	FourierGridCell* m_grid;
	int m_gridSize;

	float m_tileSize;
	float m_waveHeightScalar;
	float m_timer;

	bool m_initialized;

	// Values used during Update().  These are written over every frame,
	// but allocating them here avoids unnecessary new/free every Update().
	Complex* m_h;
	Complex* m_hDx;
	Complex* m_hDz;
	Complex* m_hSlopeX;
	Complex* m_hSlopeZ;
};
