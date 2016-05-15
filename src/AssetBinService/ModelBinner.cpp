#include "ModelBinner.h"
#include "MathLib/Maths.h"
#include "AssetLib/ModelAsset.h"
#include "UtilsLib/Color.h"
#include "UtilsLib/FileLoader.h"

namespace
{
	struct GeoData
	{
		std::vector<Vec3> positions;
		std::vector<Vec3> normals;
		std::vector<Vec2> texcoords;
		std::vector<Color> colors;
		std::vector<Vec3> tangents;
		std::vector<Vec3> bitangents;

		std::vector<uint16> indices;
		std::vector<AssetLib::Model::SubObject> subobjects;
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
		std::vector<uint> subobjectStartIndices;
		std::vector<std::string> materials;
	};
	
	bool binObjFile(const std::string& srcFilename, std::ofstream& dstFile)
	{
		GeoData geo;
		ObjData obj;

		std::ifstream srcFile(srcFilename);

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

					// Find matching vert to re-use.
					int reuseIndex = -1;
					for (int v = (int)obj.verts.size() - 1; v >= 0; --v)
					{
						const ObjVertex& otherVert = obj.verts[v];
						if (otherVert.iPos == objVert.iPos && otherVert.iUv == objVert.iUv && otherVert.iNorm == objVert.iNorm)
						{
							reuseIndex = v;
							break;
						}
					}

					if (reuseIndex >= 0)
					{
						geo.indices.push_back((uint16)reuseIndex);
					}
					else
					{
						geo.indices.push_back((uint16)geo.positions.size());

						Color color = { 1.f, 1.f, 1.f, 1.f };
						geo.positions.push_back(obj.positions[objVert.iPos]);
						geo.texcoords.push_back(obj.texcoords[objVert.iUv]);
						geo.normals.push_back(obj.normals[objVert.iNorm]);
						geo.colors.push_back(color);

						obj.verts.push_back(objVert);
					}
				}
			}
			else if (strcmp(tok, "usemtl") == 0)
			{
				std::string material = strtok_s(nullptr, " ", &context);
				obj.materials.push_back(material);

				obj.subobjectStartIndices.push_back((uint)geo.indices.size());
			}
		}

		obj.subobjectStartIndices.push_back((uint)geo.indices.size());
		geo.tangents.resize(geo.positions.size());
		geo.bitangents.resize(geo.positions.size());

		// TODO: Tangents/bitangents for shared verts
		for (uint i = 0; i < geo.indices.size(); i += 3)
		{
			uint v0 = geo.indices[i + 0];
			uint v1 = geo.indices[i + 1];
			uint v2 = geo.indices[i + 2];
			const Vec3& pos0 = geo.positions[v0];
			const Vec3& pos1 = geo.positions[v1];
			const Vec3& pos2 = geo.positions[v2];
			const Vec2& uv0 = geo.texcoords[v0];
			const Vec2& uv1 = geo.texcoords[v1];
			const Vec2& uv2 = geo.texcoords[v2];

			Vec3 edge1 = pos1 - pos0;
			Vec3 edge2 = pos2 - pos0;

			Vec2 uvEdge1 = uv1 - uv0;
			Vec2 uvEdge2 = uv2 - uv0;

			float r = 1.f / (uvEdge1.y * uvEdge2.x - uvEdge1.x * uvEdge2.y);

			Vec3 tangent = (edge1 * -uvEdge2.y + edge2 * uvEdge1.y) * r;
			Vec3 bitangent = (edge1 * -uvEdge2.x + edge2 * uvEdge1.x) * r;

			tangent = Vec3Normalize(tangent);
			bitangent = Vec3Normalize(bitangent);

			geo.tangents[v0] = geo.tangents[v1] = geo.tangents[v2] = tangent;
			geo.bitangents[v0] = geo.bitangents[v1] = geo.bitangents[v2] = bitangent;
		}

		// Calc radius
		Vec3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
		Vec3 vMax(-FLT_MIN, -FLT_MIN, -FLT_MIN);
		for (int i = (int)geo.positions.size() - 1; i >= 0; --i)
		{
			vMin.x = std::min(geo.positions[i].x, vMin.x);
			vMax.x = std::max(geo.positions[i].x, vMax.x);
			vMin.y = std::min(geo.positions[i].y, vMin.y);
			vMax.y = std::max(geo.positions[i].y, vMax.y);
			vMin.z = std::min(geo.positions[i].z, vMin.z);
			vMax.z = std::max(geo.positions[i].z, vMax.z);
		}

		Vec3 modelBoundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
		Vec3 modelBoundsMax(-FLT_MIN, -FLT_MIN, -FLT_MIN);
		uint totalVertCount = 0;
		for (int i = 0; i < obj.subobjectStartIndices.size() - 1; ++i)
		{
			AssetLib::Model::SubObject subobj;

			strcpy_s(subobj.materialName, obj.materials[i].c_str());
			subobj.indexCount = obj.subobjectStartIndices[i + 1] - obj.subobjectStartIndices[i];

			uint16 vertMin = -1;
			uint16 vertMax = 0;
			subobj.boundsMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
			subobj.boundsMax = Vec3(-FLT_MIN, -FLT_MIN, -FLT_MIN);
			for (uint tri = obj.subobjectStartIndices[i]; tri < obj.subobjectStartIndices[i + 1]; ++tri)
			{
				uint16 iVert = geo.indices[tri];
				vertMin = std::min(iVert, vertMin);
				vertMax = std::max(iVert, vertMax);

				subobj.boundsMin.x = std::min(geo.positions[iVert].x, subobj.boundsMin.x);
				subobj.boundsMax.x = std::max(geo.positions[iVert].x, subobj.boundsMax.x);
				subobj.boundsMin.y = std::min(geo.positions[iVert].y, subobj.boundsMin.y);
				subobj.boundsMax.y = std::max(geo.positions[iVert].y, subobj.boundsMax.y);
				subobj.boundsMin.z = std::min(geo.positions[iVert].z, subobj.boundsMin.z);
				subobj.boundsMax.z = std::max(geo.positions[iVert].z, subobj.boundsMax.z);
			}

			subobj.vertCount = (vertMax - vertMin) + 1;
			totalVertCount += subobj.vertCount;

			modelBoundsMin = Vec3Min(modelBoundsMin, subobj.boundsMin);
			modelBoundsMax = Vec3Max(modelBoundsMax, subobj.boundsMax);

			geo.subobjects.push_back(subobj);
		}

		assert(totalVertCount == (uint)geo.positions.size());

		AssetLib::Model modelBin;
		modelBin.totalVertCount = totalVertCount;
		modelBin.totalIndexCount = (uint)geo.indices.size();
		modelBin.boundsMin = modelBoundsMin;
		modelBin.boundsMax = modelBoundsMax;
		modelBin.subObjectCount = (uint)geo.subobjects.size();
		modelBin.subobjects.offset = 0;
		modelBin.positions.offset  = modelBin.subobjects.offset + modelBin.subobjects.CalcSize(modelBin.subObjectCount);
		modelBin.texcoords.offset  = modelBin.positions.offset + modelBin.positions.CalcSize(totalVertCount);
		modelBin.normals.offset    = modelBin.texcoords.offset + modelBin.texcoords.CalcSize(totalVertCount);
		modelBin.colors.offset     = modelBin.normals.offset + modelBin.normals.CalcSize(totalVertCount);
		modelBin.tangents.offset   = modelBin.colors.offset + modelBin.colors.CalcSize(totalVertCount);
		modelBin.bitangents.offset = modelBin.tangents.offset + modelBin.tangents.CalcSize(totalVertCount);
		modelBin.indices.offset    = modelBin.bitangents.offset + modelBin.bitangents.CalcSize(totalVertCount);

		dstFile.write((char*)&modelBin, sizeof(AssetLib::Model));
		dstFile.write((char*)geo.subobjects.data(), sizeof(geo.subobjects[0]) * geo.subobjects.size());
		dstFile.write((char*)geo.positions.data(), sizeof(geo.positions[0]) * geo.positions.size());
		dstFile.write((char*)geo.texcoords.data(), sizeof(geo.texcoords[0]) * geo.texcoords.size());
		dstFile.write((char*)geo.normals.data(), sizeof(geo.normals[0]) * geo.normals.size());
		dstFile.write((char*)geo.colors.data(), sizeof(geo.colors[0]) * geo.colors.size());
		dstFile.write((char*)geo.tangents.data(), sizeof(geo.tangents[0]) * geo.tangents.size());
		dstFile.write((char*)geo.bitangents.data(), sizeof(geo.bitangents[0]) * geo.bitangents.size());
		dstFile.write((char*)geo.indices.data(), sizeof(geo.indices[0]) * geo.indices.size());

		return true;
	}
}

AssetLib::AssetDef& ModelBinner::GetAssetDef() const
{
	return AssetLib::g_modelDef;
}

std::vector<std::string> ModelBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("obj");
	return types;
}

void ModelBinner::CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	SHA1Hash::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool ModelBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	size_t pos = srcFilename.find_last_of('.');
	const char* ext = srcFilename.c_str() + pos + 1;
	if (_stricmp(ext, "obj") == 0)
	{
		return binObjFile(srcFilename, dstFile);
	}
	else
	{
		printf("Unknown file type passed to GeoBinner: %s", srcFilename.c_str());
		return false;
	}
}