#include "ModelImport.h"
#include "MathLib/Maths.h"
#include "AssetLib/ModelAsset.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/MaterialAsset.h"
#include "UtilsLib/Color.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"
#include <vector>
#include <set>
#include "fbxsdk.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/json/json.h"

using namespace fbxsdk;

namespace
{
	typedef std::vector<std::string> TextureNameList;

	static constexpr uint kMaxNumTexCoords = 3;

	std::string makeAssetFilePath(const char* assetName, const AssetLib::AssetDef& rAssetDef)
	{
		std::string outFilename = Paths::GetSrcDataDir();
		outFilename += "/";
		outFilename += rAssetDef.GetFolder();
		outFilename += "/";
		outFilename += assetName;
		outFilename += ".";
		outFilename += rAssetDef.GetExt();

		Paths::CreateDirectoryTreeForFile(outFilename);

		return outFilename;
	}

	struct ImportedFbxVertex
	{
		Vec3	vPosition						= Vec3::kZero;
		Vec3	vNormal							= Vec3::kZero;
		Vec2	vTexCoords[kMaxNumTexCoords]	= { Vec2::kZero, Vec2::kZero, Vec2::kZero };
		Color32 color							= Color32(1, 1, 1, 1);
	};

	bool operator == (const ImportedFbxVertex& lhs, const ImportedFbxVertex& rhs)
	{
		return Vec3NearEqual(lhs.vPosition, rhs.vPosition)
			&& Vec3NearEqual(lhs.vNormal, rhs.vNormal)
			&& Vec2NearEqual(lhs.vTexCoords[0], rhs.vTexCoords[0])
			&& Vec2NearEqual(lhs.vTexCoords[1], rhs.vTexCoords[1])
			&& Vec2NearEqual(lhs.vTexCoords[2], rhs.vTexCoords[2])
			&& lhs.color == rhs.color;
	}

	struct ImportedFbxMesh
	{
		std::vector<ImportedFbxVertex> verts;
		std::vector<uint> indices;
		bool bHasTexCoord[kMaxNumTexCoords];
		std::string materialName;
	};

	struct ExportSubMesh
	{
		Vec3 m_vBoundsMin;
		Vec3 m_vBoundsMax;

		std::vector<Vec3> m_positions;
		std::vector<Vec3> m_normals;
		std::vector<Vec2> m_texcoords[kMaxNumTexCoords];
		std::vector<Color32> m_colors;
		std::vector<Vec3> m_tangents;
		std::vector<Vec3> m_bitangents;

		std::vector<uint> m_indices;
		std::string m_materialName;

		//////////////////////////////////////////////////////////////////////////
		bool Use16BitIndices() const
		{
			return GetVertexCount() <= std::numeric_limits<uint16>::max();
		}

		uint GetIndexFormatSize() const
		{
			return Use16BitIndices() ? sizeof(uint16) : sizeof(uint);
		}

		RdrIndexBufferFormat GetIndexBufferFormat() const
		{
			return Use16BitIndices() ? RdrIndexBufferFormat::R16_UINT : RdrIndexBufferFormat::R32_UINT;
		}

		uint GetVertexCount() const
		{
			return (uint)m_positions.size();
		}

		uint GetVertexStride() const
		{
			uint nBytes = 0;
			if (!m_positions.empty()) 
				nBytes += sizeof(Vec3);
			if (!m_normals.empty()) 
				nBytes += sizeof(Vec3);
			for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				if (!m_texcoords[nChannel].empty())
					nBytes += sizeof(Vec2);
			}
			if (!m_colors.empty())
				nBytes += sizeof(Color32);
			if (!m_tangents.empty())
				nBytes += sizeof(Vec3);
			if (!m_bitangents.empty())
				nBytes += sizeof(Vec3);
			return nBytes;
		}

		uint GetNumVertexInputElements() const
		{
			uint nCount = 0;
			if (!m_positions.empty())
				++nCount;
			if (!m_normals.empty())
				++nCount;
			for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				if (!m_texcoords[nChannel].empty())
					++nCount;
			}
			if (!m_colors.empty())
				++nCount;
			if (!m_tangents.empty())
				++nCount;
			if (!m_bitangents.empty())
				++nCount;
			return nCount;
		}

		std::vector<RdrVertexInputElement> GetVertexInputElements() const
		{
			std::vector<RdrVertexInputElement> elements;
			uint nByteOffset = 0;
			if (!m_positions.empty())
			{
				elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
				nByteOffset += sizeof(float) * 3;
			}
			if (!m_normals.empty())
			{
				elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
				nByteOffset += sizeof(float) * 3;
			}
			for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				if (!m_texcoords[nChannel].empty())
				{
					elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Texcoord, nChannel, RdrVertexInputFormat::RG_F32, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
					nByteOffset += sizeof(float) * 2;
				}
			}
			if (!m_colors.empty())
			{
				elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_U8, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
				nByteOffset += sizeof(uint);
			}
			if (!m_tangents.empty())
			{
				elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
				nByteOffset += sizeof(float) * 3;
			}
			if (!m_bitangents.empty())
			{
				elements.push_back(RdrVertexInputElement{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, nByteOffset, RdrVertexInputClass::PerVertex, 0 });
				nByteOffset += sizeof(float) * 3;
			}

			return elements;
		}

		std::vector<uint16> MakeIndexBuffer16() const
		{
			std::vector<uint16> indices16;
			if (!Use16BitIndices())
			{
				Error("Invalid index format requested!");
				return indices16;
			}

			for (uint nIndex : m_indices)
			{
				indices16.push_back(nIndex);
			}

			return indices16;
		}

		std::vector<uint> MakeIndexBuffer32() const
		{
			if (Use16BitIndices())
			{
				Error("Invalid index format requested!");
				return m_indices;
			}

			return m_indices;
		}

		std::vector<uint8> MakeVertexBuffer() const
		{
			uint nByteOffset = 0;
			uint nVertexCount = (uint)m_positions.size();
			uint nVertexStride = GetVertexStride();

			std::vector<uint8> outBuffer;
			outBuffer.resize(nVertexStride * nVertexCount);

			for (uint iVert = 0; iVert < nVertexCount; ++iVert)
			{
				if (!m_positions.empty())
				{
					*(Vec3*)(outBuffer.data() + nByteOffset) = m_positions[iVert];
					nByteOffset += sizeof(m_positions[0]);
				}
				if (!m_normals.empty())
				{
					*(Vec3*)(outBuffer.data() + nByteOffset) = m_normals[iVert];
					nByteOffset += sizeof(m_normals[0]);
				}
				for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
				{
					if (!m_texcoords[nChannel].empty())
					{
						*(Vec2*)(outBuffer.data() + nByteOffset) = m_texcoords[nChannel][iVert];
						nByteOffset += sizeof(m_texcoords[nChannel][0]);
					}
				}
				if (!m_colors.empty())
				{
					*(Color32*)(outBuffer.data() + nByteOffset) = m_colors[iVert];
					nByteOffset += sizeof(m_colors[0]);
				}
				if (!m_tangents.empty())
				{
					*(Vec3*)(outBuffer.data() + nByteOffset) = m_tangents[iVert];
					nByteOffset += sizeof(m_tangents[0]);
				}
				if (!m_bitangents.empty())
				{
					*(Vec3*)(outBuffer.data() + nByteOffset) = m_bitangents[iVert];
					nByteOffset += sizeof(m_bitangents[0]);
				}
			}

			Assert(nByteOffset == nVertexStride * nVertexCount);
			return outBuffer;
		}
	};

	struct ExportMesh
	{
		std::vector<ExportSubMesh> submeshes;
	};

	struct ObjVertex
	{
		int iPos;
		int iUv;
		int iNorm;
	};

	struct ObjData
	{
		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
		std::vector<Vec2> texcoords;

		std::vector<ObjVertex> verts;
	};

	struct SceneMesh
	{
		std::string name;
		Vec3 position;
		Rotation rotation;
		Vec3 scale;
		std::string modelName;
		// donotcheckin - material swaps?
	};

	struct SceneLight
	{
		std::string name;
		LightType eType;
		Vec3 position;
		Rotation rotation;
		Color color;
		float fIntensity;
		float fInnerAngle;
		float fOuterAngle;
		float fRadius;
		float fPssmLambda;
		bool bCastShadows;
	};

	struct SceneData
	{
		std::string name;
		Vec3 cameraPosition;
		Rotation cameraRotation;
		std::vector<SceneMesh> meshes;
		std::vector<SceneLight> lights;

		std::set<std::string> writtenMeshes;
		std::set<std::string> writtenMaterials;

		Vec3 vVertexScale;
		uint nVertexPositionSwizzle[3];
		bool bReverseTriOrder;
	};

	void GenerateTangents(ExportMesh& mesh)
	{
		for (ExportSubMesh& submesh : mesh.submeshes)
		{
			submesh.m_tangents.resize(submesh.GetVertexCount());
			submesh.m_bitangents.resize(submesh.GetVertexCount());

			// TODO: Tangents/bitangents for shared verts
			for (uint i = 0; i < submesh.m_indices.size(); i += 3)
			{
				uint v0 = submesh.m_indices[i + 0];
				uint v1 = submesh.m_indices[i + 1];
				uint v2 = submesh.m_indices[i + 2];
				const Vec3& pos0 = submesh.m_positions[v0];
				const Vec3& pos1 = submesh.m_positions[v1];
				const Vec3& pos2 = submesh.m_positions[v2];
				const Vec2& uv0 = submesh.m_texcoords[0][v0];
				const Vec2& uv1 = submesh.m_texcoords[0][v1];
				const Vec2& uv2 = submesh.m_texcoords[0][v2];

				Vec3 edge1 = pos1 - pos0;
				Vec3 edge2 = pos2 - pos0;

				Vec2 uvEdge1 = uv1 - uv0;
				Vec2 uvEdge2 = uv2 - uv0;

				float r = 1.f / (uvEdge1.y * uvEdge2.x - uvEdge1.x * uvEdge2.y);

				Vec3 tangent = (edge1 * -uvEdge2.y + edge2 * uvEdge1.y) * r;
				Vec3 bitangent = (edge1 * -uvEdge2.x + edge2 * uvEdge1.x) * r;

				tangent = Vec3Normalize(tangent);
				bitangent = Vec3Normalize(bitangent);

				submesh.m_tangents[v0] = submesh.m_tangents[v1] = submesh.m_tangents[v2] = tangent;
				submesh.m_bitangents[v0] = submesh.m_bitangents[v1] = submesh.m_bitangents[v2] = bitangent;
			}
		}
	}

	void ConvertImportedFbxMeshToExportMesh(const std::vector<ImportedFbxMesh>& fbxMeshes, ExportMesh* pOutMesh)
	{
		for (const ImportedFbxMesh& fbxMesh : fbxMeshes)
		{
			uint nNumVerts = (uint)fbxMesh.verts.size();

			ExportSubMesh submesh;
			submesh.m_positions.resize(nNumVerts);
			submesh.m_normals.resize(nNumVerts);
			for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				if (fbxMesh.bHasTexCoord[nChannel])
					submesh.m_texcoords[nChannel].resize(nNumVerts);
			}
			submesh.m_colors.resize(nNumVerts);

			uint nNumIndices = (uint)fbxMesh.indices.size();
			submesh.m_indices.resize(nNumIndices);

			uint iDstVert = 0;
			submesh.m_vBoundsMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
			submesh.m_vBoundsMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (const ImportedFbxVertex& fbxVert : fbxMesh.verts)
			{
				submesh.m_vBoundsMax = Vec3Max(submesh.m_vBoundsMax, fbxVert.vPosition);
				submesh.m_vBoundsMin = Vec3Min(submesh.m_vBoundsMin, fbxVert.vPosition);

				submesh.m_positions[iDstVert] = fbxVert.vPosition;
				submesh.m_normals[iDstVert] = fbxVert.vNormal;
				submesh.m_colors[iDstVert] = fbxVert.color;
				for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
				{
					if (fbxMesh.bHasTexCoord[nChannel])
						submesh.m_texcoords[nChannel][iDstVert] = fbxVert.vTexCoords[nChannel];
				}

				++iDstVert;
			}

			submesh.m_indices = fbxMesh.indices;
			submesh.m_materialName = fbxMesh.materialName;

			pOutMesh->submeshes.push_back(submesh);
		}
	}

	bool WriteMeshAsset(ExportMesh& mesh, const std::string& dstFilename)
	{
		GenerateTangents(mesh);

		const AssetLib::AssetDef& rAssetDef = AssetLib::Model::GetAssetDef();

		std::ofstream dstFile(dstFilename, std::ios::binary);
		Assert(dstFile.is_open());

		// Write header
		AssetLib::BinFileHeader header;
		header.binUID = AssetLib::BinFileHeader::kUID;
		header.assetUID = rAssetDef.GetAssetUID();
		header.version = rAssetDef.GetBinVersion();
		header.srcHash = Hashing::SHA1();
		dstFile.write((char*)&header, sizeof(header));

		AssetLib::Model modelBin;
		modelBin.nTotalVertCount = 0;
		modelBin.nTotalIndexCount = 0;
		modelBin.nTotalInputElementCount = 0;
		modelBin.nVertexBufferSize = 0;
		modelBin.nIndexBufferSize = 0;
		modelBin.vBoundsMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
		modelBin.vBoundsMax = Vec3(-FLT_MIN, -FLT_MIN, -FLT_MIN);
		modelBin.nSubObjectCount = (uint)mesh.submeshes.size();

		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			modelBin.nTotalVertCount += submesh.GetVertexCount();
			modelBin.nTotalIndexCount += (uint)submesh.m_indices.size();
			modelBin.nVertexBufferSize += submesh.GetVertexStride() * submesh.GetVertexCount();
			modelBin.nIndexBufferSize += (uint)submesh.m_indices.size() * submesh.GetIndexFormatSize();

			modelBin.nTotalInputElementCount += !submesh.m_positions.empty();
			modelBin.nTotalInputElementCount += !submesh.m_normals.empty();
			for (uint nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				modelBin.nTotalInputElementCount += !submesh.m_texcoords[nChannel].empty();
			}
			modelBin.nTotalInputElementCount += !submesh.m_colors.empty();
			modelBin.nTotalInputElementCount += !submesh.m_tangents.empty();
			modelBin.nTotalInputElementCount += !submesh.m_bitangents.empty();

			for (const Vec3& vPos : submesh.m_positions)
			{
				modelBin.vBoundsMin = Vec3Min(modelBin.vBoundsMin, vPos);
				modelBin.vBoundsMax = Vec3Max(modelBin.vBoundsMax, vPos);
			}
		}

		modelBin.subobjects.offset = 0;
		modelBin.inputElements.offset = modelBin.subobjects.offset + modelBin.subobjects.CalcSize(modelBin.nSubObjectCount);
		modelBin.positions.offset = modelBin.inputElements.offset + modelBin.inputElements.CalcSize(modelBin.nTotalInputElementCount);
		modelBin.vertexBuffer.offset = modelBin.positions.offset + modelBin.positions.CalcSize(modelBin.nTotalVertCount);
		modelBin.indexBuffer.offset = modelBin.vertexBuffer.offset + modelBin.nVertexBufferSize;

		dstFile.write((char*)&modelBin, sizeof(AssetLib::Model));

		// Write submesh headers
		uint nTotalIndexBufferBytes = 0;
		uint nTotalInputElementCount = 0;
		uint nTotalVertexBufferBytes = 0;

		std::vector<AssetLib::Model::SubObject> allSubObjects;
		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			AssetLib::Model::SubObject subobject;
			subobject.nIndexCount = (uint)submesh.m_indices.size();
			subobject.eIndexFormat = submesh.GetIndexBufferFormat();
			subobject.nIndexStartByteOffset = nTotalIndexBufferBytes;
			nTotalIndexBufferBytes += subobject.nIndexCount * submesh.GetIndexFormatSize();

			subobject.nInputElementCount = submesh.GetNumVertexInputElements();
			subobject.nInputElementStart = nTotalInputElementCount;
			nTotalInputElementCount += subobject.nInputElementCount;

			subobject.nVertexCount = submesh.GetVertexCount();
			subobject.nVertexStartByteOffset = nTotalVertexBufferBytes;
			subobject.nVertexStride = submesh.GetVertexStride();
			nTotalVertexBufferBytes += subobject.nVertexCount * subobject.nVertexStride;

			strcpy_s(subobject.strMaterialName, ARRAY_SIZE(subobject.strMaterialName), submesh.m_materialName.c_str());

			subobject.vBoundsMin = submesh.m_vBoundsMin;
			subobject.vBoundsMax = submesh.m_vBoundsMax;

			allSubObjects.push_back(subobject);
		}
		dstFile.write((char*)allSubObjects.data(), sizeof(allSubObjects[0]) * allSubObjects.size());

		// Write input elements
		std::vector<RdrVertexInputElement> allInputElements;
		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			std::vector<RdrVertexInputElement> submeshElements = submesh.GetVertexInputElements();
			for (RdrVertexInputElement& elem : submeshElements)
			{
				allInputElements.push_back(elem);
			}
		}
		dstFile.write((char*)allInputElements.data(), sizeof(allInputElements[0]) * allInputElements.size());

		// Write positions
		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			dstFile.write((char*)submesh.m_positions.data(), sizeof(submesh.m_positions[0]) * submesh.m_positions.size());
		}

		// Write vertex buffers
		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			std::vector<uint8> vertexBuffer = submesh.MakeVertexBuffer();
			dstFile.write((char*)vertexBuffer.data(), vertexBuffer.size());
		}

		// Write index buffers
		for (const ExportSubMesh& submesh : mesh.submeshes)
		{
			if (submesh.Use16BitIndices())
			{
				std::vector<uint16> indexBuffer = submesh.MakeIndexBuffer16();
				dstFile.write((char*)indexBuffer.data(), sizeof(indexBuffer[0]) * indexBuffer.size());
			}
			else
			{
				std::vector<uint> indexBuffer = submesh.MakeIndexBuffer32();
				dstFile.write((char*)indexBuffer.data(), sizeof(indexBuffer[0]) * indexBuffer.size());
			}
		}

		return true;
	}

	enum class EMaterialType
	{
		Solid,
		Emissive
	};


	bool StringContains(const std::string& str, const std::string& strToFind)
	{
		uint i = 0;
		uint iOther = 0;
		uint strLen = (uint)str.length();
		uint strLenOther = (uint)strToFind.length();
		for (uint i = 0; i != strLen; ++i)
		{
			uint iOther;
			for (iOther = 0; iOther != strLenOther; ++iOther)
			{
				if (tolower(str[i + iOther]) == tolower(strToFind[iOther]))
					break;
			}

			if (iOther == strLenOther)
				return true;
		}

		return false;
	}

	void WriteMaterialAsset(EMaterialType eType, const TextureNameList& textures, const std::string& dstFilename)
	{
		Json::StyledStreamWriter jsonWriter;

		Json::Value jsonRoot(Json::objectValue);

		if (eType == EMaterialType::Solid)
		{
			jsonRoot["vertexShader"] = "model";
			jsonRoot["pixelShader"] = "p_model.hlsl";
			jsonRoot["needsLighting"] = true;
			jsonRoot["emissive"] = false;
		}
		else if (eType == EMaterialType::Emissive)
		{
			jsonRoot["vertexShader"] = "model";
			jsonRoot["pixelShader"] = "p_model.hlsl";
			jsonRoot["needsLighting"] = true;
			jsonRoot["emissive"] = true;
		}
		
		Json::Value jTextures(Json::arrayValue);
		for (int i = 0; i < (int)textures.size(); ++i)
		{
			jTextures[i] = textures[i];
		}
		jsonRoot["textures"] = jTextures;

		jsonRoot["normals_bc5"] = true;

		bool bAlphaCutout = StringContains(dstFilename, "foliage") || StringContains(dstFilename, "plant");
		jsonRoot["alphaCutout"] = bAlphaCutout;

		Assert(dstFilename.size() > 0);

		std::ofstream materialFile(dstFilename.c_str());
		jsonWriter.write(materialFile, jsonRoot);
	}
}

bool ModelImport::ImportObj(const std::string& srcFilename, const std::string& dstFilename)
{
	std::ifstream srcFile(srcFilename);
	Assert(srcFile.is_open());

	// Build and write the main geo data
	ObjData obj;

	ExportMesh mesh;
	ExportSubMesh* pCurrSubMesh = nullptr;
	int nCurrMaterialStartVertex = 0;

	char line[1024];
	int lineNum = 0; // for debugging

	while (srcFile.getline(line, 1024))
	{
		++lineNum;

		char* context = nullptr;
		char* tok = strtok_s(line, " ", &context);

		if (strcmp(tok, "v") == 0)
		{
			// Position
			Vec3 pos;
			pos.x = (float)atof(strtok_s(nullptr, " ", &context));
			pos.y = (float)atof(strtok_s(nullptr, " ", &context));
			pos.z = (float)atof(strtok_s(nullptr, " ", &context));
			obj.positions.push_back(pos);
		}
		else if (strcmp(tok, "vn") == 0)
		{
			// Normal
			Vec3 norm;
			norm.x = (float)atof(strtok_s(nullptr, " ", &context));
			norm.y = (float)atof(strtok_s(nullptr, " ", &context));
			norm.z = (float)atof(strtok_s(nullptr, " ", &context));
			obj.normals.push_back(norm);
		}
		else if (strcmp(tok, "vt") == 0)
		{
			// Tex coord
			Vec2 uv;
			uv.x = (float)atof(strtok_s(nullptr, " ", &context));
			uv.y = (float)atof(strtok_s(nullptr, " ", &context));
			obj.texcoords.push_back(uv);
		}
		else if (strcmp(tok, "f") == 0)
		{
			// Face
			for (int i = 0; i < 3; ++i)
			{
				ObjVertex objVert;
				objVert.iPos = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				objVert.iUv = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				objVert.iNorm = atoi(strtok_s(nullptr, "/ ", &context)) - 1;

				// Find matching vert to re-use within the current material/submesh
				int reuseIndex = -1;
				for (int v = (int)obj.verts.size() - 1; v >= nCurrMaterialStartVertex; --v)
				{
					const ObjVertex& otherVert = obj.verts[v];
					if (otherVert.iPos == objVert.iPos && otherVert.iUv == objVert.iUv && otherVert.iNorm == objVert.iNorm)
					{
						reuseIndex = v - nCurrMaterialStartVertex;
						break;
					}
				}

				if (reuseIndex >= 0)
				{
					pCurrSubMesh->m_indices.push_back(reuseIndex);
				}
				else
				{
					pCurrSubMesh->m_indices.push_back((uint)pCurrSubMesh->m_positions.size());

					Color32 color(255, 255, 255, 255);
					pCurrSubMesh->m_positions.push_back(obj.positions[objVert.iPos]);
					pCurrSubMesh->m_texcoords[0].push_back(obj.texcoords[objVert.iUv]);
					pCurrSubMesh->m_normals.push_back(obj.normals[objVert.iNorm]);
					pCurrSubMesh->m_colors.push_back(color);

					obj.verts.push_back(objVert);
				}
			}
		}
		else if (strcmp(tok, "usemtl") == 0)
		{
			mesh.submeshes.emplace_back(ExportSubMesh());
			pCurrSubMesh = &mesh.submeshes.back();
			pCurrSubMesh->m_materialName = strtok_s(nullptr, " ", &context);

			nCurrMaterialStartVertex = (uint)obj.verts.size();
		}
	}

	return WriteMeshAsset(mesh, dstFilename);
}



namespace
{
	void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
	{
		//The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
		pManager = FbxManager::Create();
		if (!pManager)
		{
			FBXSDK_printf("Error: Unable to create FBX Manager!\n");
			exit(1);
		}
		else FBXSDK_printf("Autodesk FBX SDK version %s\n", pManager->GetVersion());

		//Create an IOSettings object. This object holds all import/export settings.
		FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
		pManager->SetIOSettings(ios);

		//Load plugins from the executable directory (optional)
		FbxString lPath = FbxGetApplicationDirectory();
		pManager->LoadPluginsDirectory(lPath.Buffer());

		//Create an FBX scene. This object holds most objects imported/exported from/to files.
		pScene = FbxScene::Create(pManager, "My Scene");
		if (!pScene)
		{
			FBXSDK_printf("Error: Unable to create FBX scene!\n");
			exit(1);
		}
	}

	void DestroySdkObjects(FbxManager* pManager, bool pExitStatus)
	{
		//Delete the FBX Manager. All the objects that have been allocated using the FBX Manager and that haven't been explicitly destroyed are also automatically destroyed.
		if (pManager) pManager->Destroy();
		if (pExitStatus) FBXSDK_printf("Program Success!\n");
	}

	template<typename TFbxVector>
	Vec2 fbxConvertVector2(const TFbxVector& v)
	{
		return Vec2(float(v.mData[0]), float(v.mData[1]));
	}

	template<typename TFbxVector>
	Vec3 fbxConvertVector3(const TFbxVector& v)
	{
		return Vec3(float(v.mData[0]), float(v.mData[1]), float(v.mData[2]));
	}

	template<typename TFbxVector>
	Rotation fbxConvertRotation(const TFbxVector& v)
	{
		return Rotation(float(v.mData[0]), float(v.mData[1]), float(v.mData[2]));
	}

	Color fbxConvertColor(const FbxColor& c)
	{
		return Color((float)c.mRed, (float)c.mGreen, (float)c.mBlue, (float)c.mAlpha);
	}

	Color32 fbxConvertColor32(const FbxColor& c)
	{
		return Color32((uint8)(c.mRed * 255), (uint8)(c.mGreen * 255), (uint8)(c.mBlue * 255), (uint8)(c.mAlpha * 255));
	}

	Color32 fbxReadColorFromMesh(const FbxMesh* pMesh, int nControlPoint, int nVertexId, int nChannel)
	{
		Color32 color(255, 255, 255, 255);
		if (nChannel >= pMesh->GetElementVertexColorCount())
			return color;

		const FbxGeometryElementVertexColor* leVtxc = pMesh->GetElementVertexColor(nChannel);
		switch (leVtxc->GetMappingMode())
		{
		default:
			break;
		case FbxGeometryElement::eByControlPoint:
			switch (leVtxc->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
				color = fbxConvertColor32(leVtxc->GetDirectArray().GetAt(nControlPoint));
				break;
			case FbxGeometryElement::eIndexToDirect:
			{
				int id = leVtxc->GetIndexArray().GetAt(nControlPoint);
				color = fbxConvertColor32(leVtxc->GetDirectArray().GetAt(id));
				break;
			}
			break;
			default:
				Error("fbxReadColorFromMesh() - Unimplemented reference mode: %d", leVtxc->GetReferenceMode());
				break;
			}
			break;

		case FbxGeometryElement::eByPolygonVertex:
		{
			switch (leVtxc->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
				color = fbxConvertColor32(leVtxc->GetDirectArray().GetAt(nVertexId));
				break;
			case FbxGeometryElement::eIndexToDirect:
			{
				int id = leVtxc->GetIndexArray().GetAt(nVertexId);
				color = fbxConvertColor32(leVtxc->GetDirectArray().GetAt(id));
				break;
			}
			break;
			default:
				Error("fbxReadColorFromMesh() - Unimplemented reference mode: %d", leVtxc->GetReferenceMode());
				break;
			}
		}
		break;

		case FbxGeometryElement::eByPolygon: // doesn't make much sense for UVs
		case FbxGeometryElement::eAllSame:   // doesn't make much sense for UVs
		case FbxGeometryElement::eNone:       // doesn't make much sense for UVs
			Error("fbxReadColorFromMesh() - Unimplemented mapping mode: %d", leVtxc->GetReferenceMode());
			break;
		}

		return color;
	}

	Vec2 fbxReadTexCoordFromMesh(FbxMesh* pMesh, int nControlPoint, int nPolygonIndex, int nPositionInPolygon, int nChannel)
	{
		Vec2 vTexCoord;

		const FbxGeometryElementUV* leUV = pMesh->GetElementUV(nChannel);
		switch (leUV->GetMappingMode())
		{
		default:
			break;
		case FbxGeometryElement::eByControlPoint:
			switch (leUV->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
				vTexCoord = fbxConvertVector2(leUV->GetDirectArray().GetAt(nControlPoint));
				break;
			case FbxGeometryElement::eIndexToDirect:
			{
				int id = leUV->GetIndexArray().GetAt(nControlPoint);
				vTexCoord = fbxConvertVector2(leUV->GetDirectArray().GetAt(id));
				break;
			}
			default:
				Error("fbxReadTexCoordFromMesh() - Unimplemented reference mode: %d", leUV->GetReferenceMode());
				break;
			}
			break;

		case FbxGeometryElement::eByPolygonVertex:
		{
			int lTextureUVIndex = pMesh->GetTextureUVIndex(nPolygonIndex, nPositionInPolygon);
			switch (leUV->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
			case FbxGeometryElement::eIndexToDirect:
			{
				vTexCoord = fbxConvertVector2(leUV->GetDirectArray().GetAt(lTextureUVIndex));
				break;
			}
			break;
			default:
				Error("fbxReadTexCoordFromMesh() - Unimplemented reference mode: %d", leUV->GetReferenceMode());
				break;
			}
		}
		break;

		case FbxGeometryElement::eByPolygon: // doesn't make much sense for UVs
		case FbxGeometryElement::eAllSame:   // doesn't make much sense for UVs
		case FbxGeometryElement::eNone:       // doesn't make much sense for UVs
			Error("fbxReadTexCoordFromMesh() - Unimplemented mapping mode: %d", leUV->GetMappingMode());
			break;
		}

		return vTexCoord;
	}

	Vec3 fbxReadNormalFromMesh(const FbxMesh* pMesh, int nControlPoint, int nVertexId, int nChannel)
	{
		Vec3 vNormal(0, 1, 0);
		if (nChannel >= pMesh->GetElementNormalCount())
			return vNormal;

		const FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal(nChannel);
		if (leNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
		{
			switch (leNormal->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
				vNormal = fbxConvertVector3(leNormal->GetDirectArray().GetAt(nVertexId));
				break;
			case FbxGeometryElement::eIndexToDirect:
			{
				int id = leNormal->GetIndexArray().GetAt(nVertexId);
				vNormal = fbxConvertVector3(leNormal->GetDirectArray().GetAt(id));
				break;
			}
			default:
				Error("fbxReadNormalFromMesh() - Unimplemented reference mode: %d", leNormal->GetReferenceMode());
				break;
			}
		}
		else if(leNormal->GetMappingMode() == FbxGeometryElement::eByControlPoint)
		{
			switch (leNormal->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
				vNormal = fbxConvertVector3(leNormal->GetDirectArray().GetAt(nControlPoint));
				break;
			case FbxGeometryElement::eIndexToDirect:
			{
				int id = leNormal->GetIndexArray().GetAt(nControlPoint);
				vNormal = fbxConvertVector3(leNormal->GetDirectArray().GetAt(id));
				break;
			}
			default:
				Error("fbxReadNormalFromMesh() - Unimplemented reference mode: %d", leNormal->GetReferenceMode());
				break;
			}
		}
		else
		{
			Error("fbxReadNormalFromMesh() - Unimplemented mapping mode: %d", leNormal->GetMappingMode());
		}

		return vNormal;
	}

	int fbxReadMaterialIndexFromMesh(const FbxMesh* pMesh, int nPolygonIndex)
	{
		int iMaterial = -1;
		int numMaterialChannels = pMesh->GetElementMaterialCount();
		Assert(numMaterialChannels <= 1);
		for (int nChannel = 0; nChannel < numMaterialChannels; nChannel++)
		{
			const FbxGeometryElementMaterial* leMat = pMesh->GetElementMaterial(nChannel);
			if (leMat)
			{
				FbxGeometryElement::EMappingMode eMappingMode = leMat->GetMappingMode();
				switch (eMappingMode)
				{
				case FbxGeometryElement::eByPolygon:
					switch (leMat->GetReferenceMode())
					{
					case FbxGeometryElement::eIndex:
					case FbxGeometryElement::eIndexToDirect:
					{
						iMaterial = leMat->GetIndexArray().GetAt(nPolygonIndex);
					}
					break;
					default:
						Error("Unimplemented reference mode: %d", leMat->GetReferenceMode());
						break;
					}
					break;
				case FbxGeometryElement::eAllSame:
					iMaterial = leMat->GetIndexArray().GetAt(0);
					break;
				default:
					Error("Unimplemented mapping mode: %d", eMappingMode);
					break;
				}
			}
		}

		return iMaterial;
	}

	bool LoadScene(FbxManager* pManager, FbxDocument* pScene, const char* pFilename)
	{
		int lFileMajor, lFileMinor, lFileRevision;
		int lSDKMajor, lSDKMinor, lSDKRevision;
		bool lStatus;
		char lPassword[1024];

		// Get the file version number generate by the FBX SDK.
		FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);

		// Create an importer.
		FbxImporter* pImporter = FbxImporter::Create(pManager, "");
		FbxIOSettings* pIOSettings = pManager->GetIOSettings();

		// Initialize the importer by providing a filename.
		const bool lImportStatus = pImporter->Initialize(pFilename, -1, pManager->GetIOSettings());
		pImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

		if (!lImportStatus)
		{
			FbxString error = pImporter->GetStatus().GetErrorString();
			FBXSDK_printf("Call to FbxImporter::Initialize() failed.\n");
			FBXSDK_printf("Error returned: %s\n\n", error.Buffer());

			if (pImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
			{
				FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
				FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);
			}

			return false;
		}

		FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);

		if (pImporter->IsFBX())
		{
			FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);

			// Set the import states. By default, the import states are always set to 
			// true. The code below shows how to change these states.
			pIOSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
			pIOSettings->SetBoolProp(IMP_FBX_TEXTURE, true);
			pIOSettings->SetBoolProp(IMP_FBX_LINK, true);
			pIOSettings->SetBoolProp(IMP_FBX_SHAPE, true);
			pIOSettings->SetBoolProp(IMP_FBX_GOBO, true);
			pIOSettings->SetBoolProp(IMP_FBX_ANIMATION, true);
			pIOSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
		}

		// Import the scene.
		lStatus = pImporter->Import(pScene);

		if (lStatus == false && pImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
		{
			FBXSDK_printf("Please enter password: ");

			lPassword[0] = '\0';

			FBXSDK_CRT_SECURE_NO_WARNING_BEGIN
				scanf("%s", lPassword);
			FBXSDK_CRT_SECURE_NO_WARNING_END

				FbxString lString(lPassword);

			pIOSettings->SetStringProp(IMP_FBX_PASSWORD, lString);
			pIOSettings->SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);

			lStatus = pImporter->Import(pScene);

			if (lStatus == false && pImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
			{
				FBXSDK_printf("\nPassword is wrong, import aborted.\n");
			}
		}

		// Destroy the importer.
		pImporter->Destroy();

		return lStatus;
	}

	SceneLight ImportLight(FbxNode* pNode)
	{
		FbxLight* pFbxLight = (FbxLight*)pNode->GetNodeAttribute();
		SceneLight sceneLight;

		if (!pFbxLight->CastLight.Get())
		{
			// Light doesn't cast light...?
			Warning("ImportLight() - Light does not cast any light...");
			return sceneLight;
		}

		sceneLight.name = pNode->GetName();

		FbxAMatrix xform = pNode->EvaluateGlobalTransform();
		sceneLight.position = fbxConvertVector3(xform.GetT());
		sceneLight.rotation = fbxConvertRotation(xform.GetR());

		sceneLight.color = fbxConvertColor(pFbxLight->Color.Get());
		sceneLight.fIntensity = (float)pFbxLight->Intensity.Get();
		sceneLight.fInnerAngle = (float)pFbxLight->InnerAngle.Get();
		sceneLight.fOuterAngle = (float)pFbxLight->OuterAngle.Get();
		sceneLight.fRadius;//donotcheckin = pLight->
		sceneLight.bCastShadows = pFbxLight->CastShadows.Get(); // todo: use
		// donotcheckin fog? DisplayDouble("        Default Fog: ", pLight->Fog.Get());
		sceneLight.fPssmLambda = 0.6f;//donotcheckin

		switch (pFbxLight->LightType.Get())
		{
		case FbxLight::eDirectional:
			sceneLight.eType = LightType::Directional;
			break;
		case FbxLight::ePoint:
			sceneLight.eType = LightType::Point;
			break;
		case FbxLight::eSpot:
			sceneLight.eType = LightType::Spot;
			break;
		case FbxLight::eArea:
			Error("ImportLight() - Unimplemented light type: Area");
			break;
		case FbxLight::eVolume:
			Error("ImportLight() - Unimplemented light type: Volume");
			break;
		}

		if (!(pFbxLight->FileName.Get().IsEmpty()))
		{
			// GOBO donotcheckin?
			/*
			DisplayString("    Gobo");

			DisplayString("        File Name: \"", lLight->FileName.Get().Buffer(), "\"");
			DisplayBool("        Ground Projection: ", lLight->DrawGroundProjection.Get());
			DisplayBool("        Volumetric Projection: ", lLight->DrawVolumetricLight.Get());
			DisplayBool("        Front Volumetric Projection: ", lLight->DrawFrontFacingVolumetricLight.Get());
			*/
		}

		return sceneLight;
	}

	TextureNameList ReadTextureNames(FbxProperty &prop)
	{
		std::vector<std::string> textures;

		int numLayeredTextures = prop.GetSrcObjectCount<FbxLayeredTexture>();
		if (numLayeredTextures > 0)
		{
			for (int j = 0; j < numLayeredTextures; ++j)
			{
				FbxLayeredTexture* pLayeredTexture = prop.GetSrcObject<FbxLayeredTexture>(j);
				int numTextures = pLayeredTexture->GetSrcObjectCount<FbxTexture>(); //donotcheckin what? why are there two #s of textures
				for (int j = 0; j < numTextures; ++j)
				{
					FbxTexture* pTexture = pLayeredTexture->GetSrcObject<FbxTexture>(j);
					textures.push_back(pTexture->GetName());
				}
			}
		}
		else
		{
			//no layered texture simply get on the property
			int numTextures = prop.GetSrcObjectCount<FbxTexture>();
			if (numTextures > 0)
			{
				for (int j = 0; j < numTextures; ++j)
				{
					FbxTexture* pTexture = prop.GetSrcObject<FbxTexture>(j);
					if (pTexture)
					{
						textures.push_back(pTexture->GetName());
					}
				}
			}
		}

		return textures;
	}

	bool ImportFbxMesh(FbxNode* pNode, SceneData& sceneData, const std::string& dstFilename)
	{
		FbxMesh* pMesh = (FbxMesh*)pNode->GetNodeAttribute();

		int lPolygonCount = pMesh->GetPolygonCount();
		FbxVector4* lControlPoints = pMesh->GetControlPoints();

		uint nNumMaterials = pNode->GetMaterialCount();

		std::vector<ImportedFbxMesh> importedMeshes;
		importedMeshes.resize(nNumMaterials);

		for (ImportedFbxMesh& importedMesh : importedMeshes)
		{
			importedMesh.verts.reserve(lPolygonCount * 3);
			importedMesh.indices.reserve(lPolygonCount * 3);
			for (int nChannel = 0; nChannel < kMaxNumTexCoords; ++nChannel)
			{
				importedMesh.bHasTexCoord[nChannel] = (pMesh->GetElementUVCount() > nChannel);
			}
		}

		int fbxVertexId = 0;
		for (int iPoly = 0; iPoly < lPolygonCount; iPoly++)
		{
			int lPolygonSize = pMesh->GetPolygonSize(iPoly);
			Assert(lPolygonSize == 3);

			for (int iPolyPoint = 0; iPolyPoint < lPolygonSize; iPolyPoint++, fbxVertexId++)
			{
				int lControlPointIndex = pMesh->GetPolygonVertex(iPoly, iPolyPoint);

				ImportedFbxVertex importedVert;
				importedVert.vPosition = fbxConvertVector3(pMesh->GetControlPointAt(lControlPointIndex));

				const uint* aSwizzle = sceneData.nVertexPositionSwizzle;
				importedVert.vPosition = Vec3(importedVert.vPosition[aSwizzle[0]], importedVert.vPosition[aSwizzle[1]], importedVert.vPosition[aSwizzle[2]]);
				importedVert.vPosition = importedVert.vPosition * sceneData.vVertexScale;

				//////////////////////////////////////////////////////////////////////////
				Assert(pMesh->GetElementVertexColorCount() <= 1);
				importedVert.color = fbxReadColorFromMesh(pMesh, lControlPointIndex, fbxVertexId, 0);

				//////////////////////////////////////////////////////////////////////////
				int numUvChannels = pMesh->GetElementUVCount();
				Assert(numUvChannels <= kMaxNumTexCoords);
				for (int nChannel = 0; nChannel < numUvChannels; ++nChannel)
				{
					importedVert.vTexCoords[nChannel] = fbxReadTexCoordFromMesh(pMesh, lControlPointIndex, iPoly, iPolyPoint, nChannel);
					importedVert.vTexCoords[nChannel].y = 1.f - importedVert.vTexCoords[nChannel].y;
				}

				//////////////////////////////////////////////////////////////////////////
				Assert(pMesh->GetElementNormalCount() == 1);
				importedVert.vNormal = fbxReadNormalFromMesh(pMesh, lControlPointIndex, fbxVertexId, 0);

				//////////////////////////////////////////////////////////////////////////
				int iMaterial = fbxReadMaterialIndexFromMesh(pMesh, iPoly);

				// Check for duplicate vertex
				ImportedFbxMesh& importedMesh = importedMeshes[iMaterial];
				uint nNumVerts = (uint)importedMesh.verts.size();
				uint nVertIndex = nNumVerts;
				for (uint iVert = 0; iVert < nNumVerts; ++iVert)
				{
					if (importedVert == importedMesh.verts[iVert])
					{
						nVertIndex = iVert;
						break;
					}
				}

				importedMesh.indices.push_back(nVertIndex);
				if (nVertIndex == nNumVerts)
				{
					// New vertex
					importedMesh.verts.push_back(importedVert);
				}
			}
		}

		// Reverse triangle ordering when source is using RH coord system
		if (sceneData.bReverseTriOrder)
		{
			for (ImportedFbxMesh& importedMesh : importedMeshes)
			{
				uint numIndices = (uint)importedMesh.indices.size();
				for (uint i = 0; i < numIndices; i += 3)
				{
					uint nTemp = importedMesh.indices[i];
					importedMesh.indices[i] = importedMesh.indices[i + 2];
					importedMesh.indices[i + 2] = nTemp;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Write out material assets
		for (uint iMaterial = 0; iMaterial < nNumMaterials; ++iMaterial)
		{
			FbxSurfaceMaterial* pMaterial = pNode->GetMaterial(iMaterial);
			const char* pMaterialName = pMaterial->GetName();

			if (sceneData.writtenMaterials.find(pMaterialName) != sceneData.writtenMaterials.end())
			{
				// Already wrote this material
				continue;
			}

			const FbxImplementation* pImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_HLSL);
			FbxString lImplemenationType = "HLSL";
			if (!pImplementation)
			{
				pImplementation = GetImplementation(pMaterial, FBXSDK_IMPLEMENTATION_CGFX);
				lImplemenationType = "CGFX";
			}
			if (pImplementation)
			{
				//Now we have a hardware shader, let's read it
				const FbxBindingTable* lRootTable = pImplementation->GetRootTable();
				FbxString lFileName = lRootTable->DescAbsoluteURL.Get();
				FbxString lTechniqueName = lRootTable->DescTAG.Get();

				const FbxBindingTable* lTable = pImplementation->GetRootTable();
				size_t lEntryNum = lTable->GetEntryCount();

				for (int iEntry = 0; iEntry < (int)lEntryNum; ++iEntry)
				{
					const FbxBindingTableEntry& lEntry = lTable->GetEntry(iEntry);
					const char* lEntrySrcType = lEntry.GetEntryType(true);
					FbxProperty lFbxProp;

					if (strcmp(FbxPropertyEntryView::sEntryType, lEntrySrcType) == 0)
					{
						lFbxProp = pMaterial->FindPropertyHierarchical(lEntry.GetSource());
						if (!lFbxProp.IsValid())
						{
							lFbxProp = pMaterial->RootProperty.FindHierarchical(lEntry.GetSource());
						}
					}
					else if (strcmp(FbxConstantEntryView::sEntryType, lEntrySrcType) == 0)
					{
						lFbxProp = pImplementation->GetConstants().FindHierarchical(lEntry.GetSource());
					}

					if (!lFbxProp.IsValid())
						continue;

					if (lFbxProp.GetSrcObjectCount<FbxTexture>() > 0)
					{
						//do what you want with the textures
						for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j)
						{
							FbxFileTexture *lTex = lFbxProp.GetSrcObject<FbxFileTexture>(j);
							FBXSDK_printf("           File Texture: %s\n", lTex->GetFileName());
						}
						for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j)
						{
							FbxLayeredTexture *lTex = lFbxProp.GetSrcObject<FbxLayeredTexture>(j);
							FBXSDK_printf("        Layered Texture: %s\n", lTex->GetName());
						}
						for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j)
						{
							FbxProceduralTexture *lTex = lFbxProp.GetSrcObject<FbxProceduralTexture>(j);
							FBXSDK_printf("     Procedural Texture: %s\n", lTex->GetName());
						}
					}
					else
					{
						FbxDataType lFbxType = lFbxProp.GetPropertyDataType();
						if (FbxBoolDT == lFbxType)
						{
							lFbxProp.Get<FbxBool>();
						}
						else if (FbxIntDT == lFbxType || FbxEnumDT == lFbxType)
						{
							lFbxProp.Get<FbxInt>();
						}
						else if (FbxFloatDT == lFbxType)
						{
							lFbxProp.Get<FbxFloat>();

						}
						else if (FbxDoubleDT == lFbxType)
						{
							lFbxProp.Get<FbxDouble>();
						}
						else if (FbxStringDT == lFbxType
							|| FbxUrlDT == lFbxType
							|| FbxXRefUrlDT == lFbxType)
						{
							lFbxProp.Get<FbxString>().Buffer();
						}
						else if (FbxDouble2DT == lFbxType)
						{
							FbxDouble2 lDouble2 = lFbxProp.Get<FbxDouble2>();
							FbxVector2 lVect;
							lVect[0] = lDouble2[0];
							lVect[1] = lDouble2[1];
						}
						else if (FbxDouble3DT == lFbxType || FbxColor3DT == lFbxType)
						{
							FbxDouble3 lDouble3 = lFbxProp.Get<FbxDouble3>();

							FbxVector4 lVect;
							lVect[0] = lDouble3[0];
							lVect[1] = lDouble3[1];
							lVect[2] = lDouble3[2];
						}
						else if (FbxDouble4DT == lFbxType || FbxColor4DT == lFbxType)
						{
							FbxDouble4 lDouble4 = lFbxProp.Get<FbxDouble4>();
							FbxVector4 lVect;
							lVect[0] = lDouble4[0];
							lVect[1] = lDouble4[1];
							lVect[2] = lDouble4[2];
							lVect[3] = lDouble4[3];
						}
						else if (FbxDouble4x4DT == lFbxType)
						{
							FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
							for (int j = 0; j < 4; ++j)
							{
								FbxVector4 lVect;
								lVect[0] = lDouble44[j][0];
								lVect[1] = lDouble44[j][1];
								lVect[2] = lDouble44[j][2];
								lVect[3] = lDouble44[j][3];
							}

						}
					}
				}
			}
			else
			{
				FbxProperty lProperty;
				//Diffuse Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
				TextureNameList diffuseTextures = std::move(ReadTextureNames(lProperty));

				//DiffuseFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
				TextureNameList diffuseFactorTextures = std::move(ReadTextureNames(lProperty));

				//Emissive Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissive);
				TextureNameList emissiveTextures = std::move(ReadTextureNames(lProperty));

				//EmissiveFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
				TextureNameList emissiveFactorTextures = std::move(ReadTextureNames(lProperty));

				//Ambient Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbient);
				TextureNameList ambientTextures = std::move(ReadTextureNames(lProperty));

				//AmbientFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbientFactor);
				TextureNameList ambientFactorTextures = std::move(ReadTextureNames(lProperty));

				//Specular Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecular);
				TextureNameList specularTextures = std::move(ReadTextureNames(lProperty));

				//SpecularFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
				TextureNameList specularFactorTextures = std::move(ReadTextureNames(lProperty));

				//Shininess Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
				TextureNameList shininessTextures = std::move(ReadTextureNames(lProperty));

				//Bump Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sBump);
				TextureNameList bumpTextures = std::move(ReadTextureNames(lProperty));

				//Normal Map Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
				TextureNameList normalMapTextures = std::move(ReadTextureNames(lProperty));

				//Transparent Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor);
				TextureNameList transparentTextures = std::move(ReadTextureNames(lProperty));

				//TransparencyFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
				TextureNameList transparencyFactorTextures = std::move(ReadTextureNames(lProperty));

				//Reflection Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sReflection);
				TextureNameList reflectionTextures = std::move(ReadTextureNames(lProperty));

				//ReflectionFactor Textures
				lProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sReflectionFactor);
				TextureNameList reflectionFactorTextures = std::move(ReadTextureNames(lProperty));

				Assert(reflectionFactorTextures.empty()
					&& reflectionTextures.empty()
					&& transparencyFactorTextures.empty()
					&& transparentTextures.empty()
					&& bumpTextures.empty()
					&& shininessTextures.empty()
					&& specularFactorTextures.empty()
					&& ambientFactorTextures.empty()
					&& ambientTextures.empty()
					&& emissiveFactorTextures.empty()
					&& diffuseFactorTextures.empty());

				Assert(diffuseTextures.size() == 1
					&& normalMapTextures.size() == 1);

				EMaterialType eMaterialType = EMaterialType::Solid;

				TextureNameList finalTextures;
				finalTextures.push_back(diffuseTextures[0]);
				finalTextures.push_back(normalMapTextures[0]);

				if (!emissiveTextures.empty())
				{
					eMaterialType = EMaterialType::Emissive;
					Assert(emissiveTextures.size() == 1);
					finalTextures.push_back(emissiveTextures[0]);
				}

				// Prepend texture names with the scene directory.
				for (std::string& texName : finalTextures)
				{
					texName = sceneData.name + "\\" + texName; // TODO: this doesn't allow for sharing textures between scenes.  Also how about auto-importing them?
				}
				// donotcheckin - handle specular textures somehow?? specularTextures.

				// donotcheckin - extract base props, but also all of its textures
				std::string materialFilename = sceneData.name + "\\" + pMaterialName;
				if (strlen(pMaterialName) == 0)
				{
					char strUniqueId[64];
					sprintf_s(strUniqueId, "%I64u", pNode->GetUniqueID());
					materialFilename += strUniqueId;
				}

				std::string dstFilename = makeAssetFilePath(materialFilename.c_str(), AssetLib::Material::GetAssetDef());
				WriteMaterialAsset(eMaterialType, finalTextures, dstFilename);

				importedMeshes[iMaterial].materialName = materialFilename;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		ExportMesh exportMesh;
		ConvertImportedFbxMeshToExportMesh(importedMeshes, &exportMesh);

		return WriteMeshAsset(exportMesh, dstFilename);
	}

	bool ImportFbxSceneMesh(FbxNode* pNode, SceneData& sceneData, SceneMesh& sceneMesh)
	{
		sceneMesh.name = pNode->GetName();

		FbxAMatrix xform = pNode->EvaluateGlobalTransform();
		sceneMesh.position = fbxConvertVector3(xform.GetT());
		sceneMesh.rotation = fbxConvertRotation(xform.GetR());
		sceneMesh.scale = fbxConvertVector3(xform.GetS());

		// Check if there is a geometric transform we need to apply.  (Not yet implemented).
		FbxVector4 vSrcGeoPos = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		FbxVector4 vSrcGeoScale = pNode->GetGeometricScaling(FbxNode::eSourcePivot);
		FbxVector4 vSrcGeoRotation = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
		FbxVector4 vDstGeoPos = pNode->GetGeometricTranslation(FbxNode::eDestinationPivot);
		FbxVector4 vDstGeoScale = pNode->GetGeometricScaling(FbxNode::eDestinationPivot);
		FbxVector4 vDstGeoRotation = pNode->GetGeometricRotation(FbxNode::eDestinationPivot);
		if (vSrcGeoPos.Compare(FbxVector4(0, 0, 0)) != 0 || vSrcGeoScale.Compare(FbxVector4(1,1,1,1)) != 0 || vSrcGeoRotation.Compare(FbxVector4(0,0,0,1)) != 0
			|| vDstGeoPos.Compare(FbxVector4(0, 0, 0)) != 0 || vDstGeoScale.Compare(FbxVector4(1, 1, 1, 1)) != 0 || vDstGeoRotation.Compare(FbxVector4(0, 0, 0, 1)) != 0)
		{
			Error("Geometric transformations not yet implemented!");
			return false;
		}

		FbxMesh* pMesh = (FbxMesh*)pNode->GetNodeAttribute();
		std::string meshFilename = sceneData.name + "\\" + pMesh->GetName();
		if (strlen(pMesh->GetName()) == 0)
		{
			char strUniqueId[64];
			sprintf_s(strUniqueId, "%I64u", pMesh->GetUniqueID());
			meshFilename += strUniqueId;
		}

		sceneMesh.modelName = meshFilename;

		if (sceneData.writtenMeshes.find(meshFilename) == sceneData.writtenMeshes.end())
		{
			std::string dstFilename = makeAssetFilePath(meshFilename.c_str(), AssetLib::Model::GetAssetDef());
			if (!ImportFbxMesh(pNode, sceneData, dstFilename))
			{
				return false;
			}

			sceneData.writtenMeshes.insert(meshFilename);
		}
		else
		{
			sceneMesh = sceneMesh;
		}

		return true;
	}

	bool TraverseFbxTree(FbxNode* pNode, const FbxString& strDefaultCamera, SceneData& sceneData)
	{
		FbxNodeAttribute::EType lAttributeType;
		int i;

		if (pNode->GetNodeAttribute() == NULL)
		{
			FBXSDK_printf("NULL Node Attribute\n\n");
		}
		else
		{
			lAttributeType = (pNode->GetNodeAttribute()->GetAttributeType());

			switch (lAttributeType)
			{
			default:
				break;
			case FbxNodeAttribute::eMarker:
				Error("Unsupported node attribute: Marker");
				break;

			case FbxNodeAttribute::eSkeleton:
				Error("Unsupported node attribute: Skeleton");
				break;

			case FbxNodeAttribute::eMesh:
				{
					SceneMesh sceneMesh;
					if (!ImportFbxSceneMesh(pNode, sceneData, sceneMesh))
						return false;
					sceneData.meshes.push_back(sceneMesh);
				}
				break;

			case FbxNodeAttribute::eNurbs:
				Error("Unsupported node attribute: Nurbs");
				break;

			case FbxNodeAttribute::ePatch:
				Error("Unsupported node attribute: Patch");
				break;

			case FbxNodeAttribute::eCamera:
			{
				FbxCamera* pCamera = (FbxCamera*)pNode->GetNodeAttribute();
				if (pCamera->GetName() == strDefaultCamera)
				{
					sceneData.cameraPosition = fbxConvertVector3(pCamera->Position.Get());
					sceneData.cameraRotation = Rotation();
#if 0
					FbxNode* pTarget = pNode->GetTarget();
					FbxNode* pTargetUp = pNode->GetTargetUp();
					if (pTargetNode)
					{
						DisplayString("        Camera Interest: ", (char *)pTargetNode->GetName());
					}
					else
					{
						Display3DVector("        Default Camera Interest Position: ", pCamera->InterestPosition.Get());
					}

					if (pTargetUpNode)
					{
						DisplayString("        Camera Up Target: ", (char *)pTargetUpNode->GetName());
					}
					else
					{
						Display3DVector("        Up Vector: ", pCamera->UpVector.Get());
					}

					DisplayDouble("        Roll: ", pCamera->Roll.Get());
#endif
				}
				break;
			}
			case FbxNodeAttribute::eLight:
				sceneData.lights.push_back(ImportLight(pNode));
				break;

			case FbxNodeAttribute::eLODGroup:
				Error("Unsupported node attribute: LODGroup");
				break;
			}

			/*
			DisplayUserProperties(pNode);
			DisplayTarget(pNode);
			DisplayPivotsAndLimits(pNode);
			DisplayTransformPropagation(pNode);
			DisplayGeometricTransform(pNode);
			*/
		}

		for (i = 0; i < pNode->GetChildCount(); i++)
		{
			if (!TraverseFbxTree(pNode->GetChild(i), strDefaultCamera, sceneData))
				return false;
		}

		return true;
	}
}

bool ModelImport::ImportFbx(const std::string& srcFilename, const std::string& dstFilename)
{
	FbxManager* pSdkManager = nullptr;
	FbxScene* pScene = nullptr;

	InitializeSdkObjects(pSdkManager, pScene);

	bool lResult = LoadScene(pSdkManager, pScene, srcFilename.c_str());

	// Global settings
	FbxGlobalSettings& globalSettings = pScene->GetGlobalSettings();
	
	FbxString strDefaultCamera = globalSettings.GetDefaultCamera();

	char sceneName[260];
	Paths::GetFilenameNoExtension(dstFilename.c_str(), sceneName, ARRAY_SIZE(sceneName));

	SceneData sceneData;
	sceneData.name = sceneName;
	sceneData.nVertexPositionSwizzle[0] = 0;
	sceneData.nVertexPositionSwizzle[1] = 1;
	sceneData.nVertexPositionSwizzle[2] = 2;
	sceneData.vVertexScale = Vec3(1,1,1);

	int nSign = 0;
	FbxAxisSystem origAxisSystem = globalSettings.GetAxisSystem();
	if (origAxisSystem.GetUpVector(nSign) == FbxAxisSystem::eZAxis)
	{
		sceneData.nVertexPositionSwizzle[1] = 2;
		sceneData.nVertexPositionSwizzle[2] = 1;
	}

	if (origAxisSystem.GetCoorSystem() == FbxAxisSystem::eRightHanded)
	{
		sceneData.bReverseTriOrder = true;
		sceneData.vVertexScale = Vec3(-1, 1, 1);
	}

	if (!TraverseFbxTree(pScene->GetRootNode(), strDefaultCamera, sceneData))
	{
		return false;
	}

	pScene->Destroy(true);
	DestroySdkObjects(pSdkManager, true);

	//////////////////////////////////////////////////////////////////////////
	// Write the scene file
	Json::StyledStreamWriter jsonWriter;

	Json::Value jSceneRoot(Json::objectValue);

	// Camera
	{
		Json::Value jCamera(Json::objectValue);

		Json::Value jPos(Json::arrayValue);
		jPos[0] = sceneData.cameraPosition.x;
		jPos[1] = sceneData.cameraPosition.y;
		jPos[2] = sceneData.cameraPosition.z;
		jCamera["position"] = jPos;

		Json::Value jRot(Json::arrayValue);
		jRot[0] = sceneData.cameraRotation.pitch;
		jRot[1] = sceneData.cameraRotation.yaw;
		jRot[2] = sceneData.cameraRotation.roll;
		jCamera["rotation"] = jRot;

		jSceneRoot["camera"] = jCamera;
	}

	jSceneRoot["sky"] = "cloudy";
	jSceneRoot["lights"] = Json::Value(Json::arrayValue);
	Json::Value& jSceneObjects = jSceneRoot["objects"] = Json::Value(Json::arrayValue);

	if (sceneData.lights.empty())
	{
		SceneLight sceneLight;
		sceneLight.name = "Default";
		sceneLight.position = Vec3(0,0,0);
		sceneLight.rotation = Rotation(45,0,0);
		sceneLight.color = Color(1, 1, 1, 1);
		sceneLight.fIntensity = 5.f;
		sceneLight.bCastShadows = true;
		sceneLight.fPssmLambda = 0.6f;
		sceneLight.eType = LightType::Directional;

		sceneData.lights.push_back(sceneLight);
	}

	// Add lights to scene
	for (const SceneLight& light : sceneData.lights)
	{
		Json::Value& jObj = jSceneObjects.append(Json::objectValue);
		{
			jObj["name"] = light.name;

			Json::Value& jPos = jObj["position"] = Json::arrayValue;
			jPos[0] = light.position.x; jPos[1] = light.position.y; jPos[2] = light.position.z;

			Json::Value& jRot = jObj["rotation"] = Json::arrayValue;
			jRot[0] = light.rotation.pitch; jRot[1] = light.rotation.yaw; jRot[2] = light.rotation.roll;

			Json::Value& jLight = jObj["light"] = Json::objectValue;
			{
				switch (light.eType)
				{
				case LightType::Directional:
					jLight["type"] = "directional";
					jLight["pssmLambda"] = light.fPssmLambda;
					break;
				case LightType::Spot:
					jLight["type"] = "spot";
					jLight["innerConeAngle"] = light.fInnerAngle;
					jLight["outerConeAngle"] = light.fOuterAngle;
					jLight["radius"] = light.fRadius;
					break;
				case LightType::Point:
					jLight["type"] = "point";
					jLight["radius"] = light.fRadius;
					break;
				case LightType::Environment:
				default:
					Error("Unsupported light type: %d", light.eType);
					return false;
				}

				Json::Value& jColor = jLight["color"] = Json::arrayValue;
				jColor[0] = light.color.r; jColor[1] = light.color.g; jColor[2] = light.color.b;

				jLight["intensity"] = light.fIntensity;
				jLight["castsShadows"] = light.bCastShadows;
			}
		}
	}

	// Add meshes to scene
	for (const SceneMesh& mesh : sceneData.meshes)
	{
		Json::Value& jObj = jSceneObjects.append(Json::objectValue);
		{
			jObj["name"] = mesh.name;

			Json::Value& jPos = jObj["position"] = Json::arrayValue;
			jPos[0] = mesh.position.x; jPos[1] = mesh.position.y; jPos[2] = mesh.position.z;

			Json::Value& jRot = jObj["rotation"] = Json::arrayValue;
			jRot[0] = mesh.rotation.pitch;
			jRot[1] = mesh.rotation.yaw;
			jRot[2] = mesh.rotation.roll;

			Json::Value& jScale = jObj["scale"] = Json::arrayValue;
			jScale[0] = mesh.scale.z; jScale[1] = mesh.scale.z; jScale[2] = mesh.scale.z;

			Json::Value& jModel = jObj["model"] = Json::objectValue;
			{
				jModel["name"] = mesh.modelName;
			}
		}
	}

	std::string dstSceneFilename = makeAssetFilePath(sceneName, AssetLib::Scene::GetAssetDef());
	std::ofstream sceneFile(dstSceneFilename.c_str());
	jsonWriter.write(sceneFile, jSceneRoot);

	return true;
}
