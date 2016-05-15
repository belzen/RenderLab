#include "Hash.h"
#include "Error.h"
#include <windows.h>
#include <winternl.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace
{
	BCRYPT_ALG_HANDLE s_algorithmSHA1 = 0;

	void initSHA1()
	{
		if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&s_algorithmSHA1, BCRYPT_SHA1_ALGORITHM, nullptr, 0)))
		{
			Error("Failed to create SHA1 algorithm provider");
		}
	}

}

SHA1HashState* SHA1Hash::Begin()
{
	if (!s_algorithmSHA1)
		initSHA1();

	BCRYPT_HASH_HANDLE hHash;
	if (!NT_SUCCESS(BCryptCreateHash(s_algorithmSHA1, &hHash, nullptr, 0, nullptr, 0, 0)))
		Error("Failed to create SHA1 hash");

	return hHash;
}

SHA1HashState* SHA1Hash::Begin(char* pData, uint dataSize)
{
	if (!s_algorithmSHA1)
		initSHA1();

	BCRYPT_HASH_HANDLE hHash;
	if (!NT_SUCCESS(BCryptCreateHash(s_algorithmSHA1, &hHash, nullptr, 0, nullptr, 0, 0)))
		Error("Failed to create SHA1 hash");

	Update(hHash, pData, dataSize);
	return hHash;
}

void SHA1Hash::Update(SHA1HashState* pState, char* pData, uint dataSize)
{
	if (!NT_SUCCESS(BCryptHashData(pState, (unsigned char*)pData, dataSize, 0)))
		Error("Failed to hash data for SHA1");
}

void SHA1Hash::Finish(SHA1HashState* pState, SHA1Hash& rOutHash)
{
	if (!NT_SUCCESS(BCryptFinishHash(pState, rOutHash.data, sizeof(rOutHash.data), 0)))
		Error("Failed to finalize SHA1 hash");
	BCryptDestroyHash(pState);
}

void SHA1Hash::Calculate(char* pData, uint dataSize, SHA1Hash& rOutHash)
{
	if (!s_algorithmSHA1)
		initSHA1();

	SHA1HashState* pState = Begin(pData, dataSize);
	Finish(pState, rOutHash);
}