#pragma once

#define NOMINMAX
#include <windows.h>

#include <algorithm>
using std::min;
using std::max;

#include <malloc.h>
#include <assert.h>
#include <memory>
#include <map>
#include <vector>
#include <DirectXMath.h>

#include "../Types.h"
#include "MathLib/Maths.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/FileWatcher.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/Paths.h"
#include "UtilsLib/Error.h"
#include "UtilsLib/Color.h"
#include "AssetLib/AssetDef.h"
#include "shapes/Rect.h"
#include "shapes/Plane.h"
#include "FreeList.h"