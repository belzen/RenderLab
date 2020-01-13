#pragma once
#include "../Types.h"
#include "UtilsLib/Hash.h"

namespace AssetLib
{
	struct BinFileHeader
	{
		// UID at the start of all bin files to verify it's a known file type.
		// This value should never change.
		static const uint kUID = 0x29124e61;

		uint binUID;
		uint assetUID;
		int version;
		Hashing::SHA1 srcHash;
	};

	template<typename DataT>
	struct BinDataPtr
	{
		uint offset;
		DataT* ptr;
		void PatchPointer(char* pParentMem)
		{
			ptr = (DataT*)(pParentMem + offset);
		}

		static uint CalcSize(uint numElements)
		{
			return numElements * sizeof(DataT);
		}
	};
}