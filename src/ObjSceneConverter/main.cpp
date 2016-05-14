// Super ugly tool to convert an OBJ file containing lots of 
// objects into a scene and individual model+obj files.
#include <windows.h>
#include <fstream>
#include <vector>
#include <assert.h>
#include "json.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

struct Vec2
{
	float x, y;
};
struct Vec3
{
	float x, y, z;
};
struct Vertex
{
	int pos;
	int uv;
	int norm;
};

struct SubObject
{
	std::string material;
	std::vector<Vertex> verts;
};

struct Object
{
	std::vector<SubObject> subobjects;
	std::string name;
};

void createDirectoryTreeForFile(const std::string& filename)
{
	int endPos = -1;
	while ((endPos = (int)filename.find_first_of("/\\", endPos + 1)) != std::string::npos)
	{
		std::string dir = filename.substr(0, endPos);
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}

int main(int argc, char** argv)
{
	float scale = 0.1f;
	float scaleTexV = -1.f;

	std::vector<Object> objects;
	std::ifstream inputFile(argv[1]);
	if (!inputFile.is_open())
	{
		printf("Failed to open %s", argv[1]);
		return -1;
	}

	if (argc != 3)
	{
		printf("Missing scene name");
		return -1;
	}
	std::string sceneName = argv[2];

	char line[1024];
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;

	Object* pActiveObj = nullptr;
	SubObject* pActiveSubObject = nullptr;

	int lineNum = 0;
	while (inputFile.getline(line, 1024))
	{
		++lineNum;

		char* context = nullptr;
		char* tok = strtok_s(line, " ", &context);
		if (!tok)
			continue;

		if (strcmp(tok, "v") == 0)
		{
			// Position
			Vec3 pos;
			pos.x = scale * (float)atof(strtok_s(nullptr, " ", &context));
			pos.y = scale * (float)atof(strtok_s(nullptr, " ", &context));
			pos.z = scale * (float)atof(strtok_s(nullptr, " ", &context));
			positions.push_back(pos);
		}
		else if (strcmp(tok, "vn") == 0)
		{
			// Normal
			Vec3 norm;
			norm.x = (float)atof(strtok_s(nullptr, " ", &context));
			norm.y = (float)atof(strtok_s(nullptr, " ", &context));
			norm.z = (float)atof(strtok_s(nullptr, " ", &context));
			normals.push_back(norm);
		}
		else if (strcmp(tok, "vt") == 0)
		{
			// Tex coord
			Vec2 uv;
			uv.x = (float)atof(strtok_s(nullptr, " ", &context));
			uv.y = (float)atof(strtok_s(nullptr, " ", &context)) * scaleTexV;
			texcoords.push_back(uv);
		}
		else if (strcmp(tok, "f") == 0)
		{
			// Face
			Vertex faceVerts[4];
			int numVerts = 0;

			while (context[0] != 0)
			{
				Vertex& rVert = faceVerts[numVerts];
				rVert.pos = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				rVert.uv = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				rVert.norm = atoi(strtok_s(nullptr, "/ ", &context)) - 1;

				++numVerts;
			}

			if (numVerts == 3)
			{
				pActiveSubObject->verts.push_back(faceVerts[0]);
				pActiveSubObject->verts.push_back(faceVerts[1]);
				pActiveSubObject->verts.push_back(faceVerts[2]);
			}
			else if (numVerts == 4)
			{
				pActiveSubObject->verts.push_back(faceVerts[0]);
				pActiveSubObject->verts.push_back(faceVerts[1]);
				pActiveSubObject->verts.push_back(faceVerts[2]);
				pActiveSubObject->verts.push_back(faceVerts[0]);
				pActiveSubObject->verts.push_back(faceVerts[2]);
				pActiveSubObject->verts.push_back(faceVerts[3]);
			}
			else
			{
				assert(false);
			}
		}
		else if (strcmp(tok, "g") == 0)
		{
			objects.push_back(Object());
			pActiveObj = &objects.back();

			pActiveObj->name = strtok_s(nullptr, " ", &context);
		}
		else if (strcmp(tok, "usemtl") == 0)
		{
			pActiveObj->subobjects.push_back(SubObject());
			pActiveSubObject = &pActiveObj->subobjects.back();

			pActiveSubObject->material = strtok_s(nullptr, " ", &context);
		}
	}

	Json::StyledStreamWriter jsonWriter;

	Json::Value jSceneRoot(Json::objectValue);

	// Camera
	{
		Json::Value jCamera(Json::objectValue);

		Json::Value jPos(Json::arrayValue);
		jPos[0] = 0.f;
		jPos[1] = 5.f;
		jPos[2] = 0.f;
		jCamera["position"] = jPos;

		Json::Value jRot(Json::arrayValue);
		jRot[0] = 0.f;
		jRot[1] = 0.f;
		jRot[2] = 0.f;
		jCamera["rotation"] = jRot;

		jSceneRoot["camera"] = jCamera;
	}

	jSceneRoot["sky"] = "cloudy";
	jSceneRoot["lights"] = Json::Value(Json::arrayValue);
	Json::Value& jSceneObjects = jSceneRoot["objects"] = Json::Value(Json::arrayValue);

	int numObjects = (int)objects.size();
	for (int i = 0; i < numObjects; ++i)
	{
		const Object& obj = objects[i];

		Vec3 minExtents = { FLT_MAX, FLT_MAX, FLT_MAX };
		Vec3 maxExtents = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		char str[256];

		for (unsigned int n = 0; n < obj.subobjects.size(); ++n)
		{
			const SubObject& subobj = obj.subobjects[n];
			for (unsigned int k = 0; k < subobj.verts.size(); ++k)
			{
				const Vertex& vert = subobj.verts[k];
				const Vec3& pos = positions[vert.pos];
				minExtents.x = min(minExtents.x, pos.x);
				minExtents.y = min(minExtents.y, pos.y);
				minExtents.z = min(minExtents.z, pos.z);
				maxExtents.x = max(maxExtents.x, pos.x);
				maxExtents.y = max(maxExtents.y, pos.y);
				maxExtents.z = max(maxExtents.z, pos.z);
			}
		}

		Vec3 center = { (maxExtents.x + minExtents.x) * 0.5f, minExtents.y, (maxExtents.z + minExtents.z) * 0.5f };

		std::string objFilename = "output/" + obj.name + ".obj";
		createDirectoryTreeForFile(objFilename);

		std::ofstream objFile(objFilename);
		assert(objFile.is_open());

		int posCount = 0;
		int uvCount = 0;
		int normCount = 0;

		for (unsigned int n = 0; n < obj.subobjects.size(); ++n)
		{
			const SubObject& subobj = obj.subobjects[n];

			// Build the obj file
			int posStart = INT_MAX;
			int posEnd = 0;
			int uvStart = INT_MAX;
			int uvEnd = 0;
			int normStart = INT_MAX;
			int normEnd = 0;

			for (unsigned int k = 0; k < subobj.verts.size(); ++k)
			{
				const Vertex& vert = subobj.verts[k];

				posStart = min(posStart, vert.pos);
				posEnd = max(posEnd, vert.pos);
				uvStart = min(uvStart, vert.uv);
				uvEnd = max(uvEnd, vert.uv);
				normStart = min(normStart, vert.norm);
				normEnd = max(normEnd, vert.norm);
			}

			// write positions
			for (int k = posStart; k <= posEnd; ++k)
			{
				const Vec3& pos = positions[k];
				sprintf_s(str, "v %f %f %f\n", (pos.x - center.x), (pos.y - center.y), (pos.z - center.z));
				objFile.write(str, strlen(str));
			}

			// write uvs
			for (int k = uvStart; k <= uvEnd; ++k)
			{
				const Vec2& uv = texcoords[k];
				sprintf_s(str, "vt %f %f\n", uv.x, uv.y);
				objFile.write(str, strlen(str));
			}

			// write normals
			for (int k = normStart; k <= normEnd; ++k)
			{
				const Vec3& normal = normals[k];
				sprintf_s(str, "vn %f %f %f\n", normal.x, normal.y, normal.z);
				objFile.write(str, strlen(str));
			}

			// write material to use for this subobject
			std::string materialName = sceneName + "/" + subobj.material;
			sprintf_s(str, "usemtl %s\n", materialName.c_str());
			objFile.write(str, strlen(str));

			// write faces
			for (unsigned int k = 0; k < subobj.verts.size(); k += 3)
			{
				const Vertex& vert1 = subobj.verts[k + 0];
				const Vertex& vert2 = subobj.verts[k + 1];
				const Vertex& vert3 = subobj.verts[k + 2];
				sprintf_s(str, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
					posCount + (vert1.pos - posStart + 1), uvCount + (vert1.uv - uvStart + 1), normCount + (vert1.norm - normStart + 1),
					posCount + (vert2.pos - posStart + 1), uvCount + (vert2.uv - uvStart + 1), normCount + (vert2.norm - normStart + 1),
					posCount + (vert3.pos - posStart + 1), uvCount + (vert3.uv - uvStart + 1), normCount + (vert3.norm - normStart + 1));
				objFile.write(str, strlen(str));
			}

			posCount += (posEnd - posStart) + 1;
			uvCount += (uvEnd - uvStart) + 1;
			normCount += (normEnd - normStart) + 1;
		}

		objFile.close();

		// Add instance to scene
		Json::Value& jObj = jSceneObjects.append(Json::objectValue);
		jObj["model"] = sceneName + "/" + obj.name;

		Json::Value& jPos = jObj["position"] = Json::arrayValue;
		jPos[0] = center.x; jPos[1] = center.y; jPos[2] = center.z;

		Json::Value& jRot = jObj["rotation"] = Json::arrayValue;
		jRot[0] = 0.f; jRot[1] = 0.f; jRot[2] = 0.f;

		Json::Value& jScale = jObj["scale"] = Json::arrayValue;
		jScale[0] = 1.f; jScale[1] = 1.f; jScale[2] = 1.f;
	}

	std::ofstream sceneFile("output/" + sceneName + ".scene");
	jsonWriter.write(sceneFile, jSceneRoot);
}