#pragma once

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

typedef uint RdrResourceHandle;
struct RdrResource
{
	union
	{
		ID3D11Texture2D* pTexture;
		ID3D11Buffer*    pBuffer;
		ID3D11Resource*  pResource;
	};

	// All optional.
	ID3D11ShaderResourceView*	pResourceView;
	ID3D11UnorderedAccessView*	pUnorderedAccessView;

	int width;
	int height;
};
