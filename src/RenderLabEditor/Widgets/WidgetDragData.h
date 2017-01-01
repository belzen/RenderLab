#pragma once
#include "AssetLib/AssetLibForwardDecl.h"

class Widget;

enum class WidgetDragDataType
{
	kNone,
	kModelAsset,
	kObjectAsset,
};

struct WidgetDragData
{
	WidgetDragDataType type;
	Widget* pWidget;
	union
	{
		const char* assetName;
		void* pTypeless;
	} data;
};