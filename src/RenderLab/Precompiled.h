#pragma once

#define NOMINMAX
#include <windows.h>


#include <malloc.h>
#include <assert.h>
#include <memory>
#include <map>
#include <vector>
#include <DirectXMath.h>

#include "json/json.h"
#include "Util.h"
#include "Types.h"
#include "math/maths.h"
#include "shapes/Rect.h"
#include "shapes/Plane.h"
#include "render/Color.h"
#include "FreeList.h"
#include "FileWatcher.h"
#include "FileLoader.h"
#include "AssetDef.h"