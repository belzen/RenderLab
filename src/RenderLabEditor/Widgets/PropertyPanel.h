#pragma once

#include "Widget.h"

class IViewModel;
class PropertyDef;

class PropertyPanel : public Widget
{
public:
	static PropertyPanel* Create(const Widget& rParent, int x, int y, int width, int height);

	void SetViewModel(IViewModel* pViewModel, bool freeOldViewModel);

private:
	PropertyPanel(const Widget& rParent, int x, int y, int width, int height);
	PropertyPanel(const PropertyPanel&);

	virtual ~PropertyPanel();

	void DestroyChildWidgets();

private:
	std::vector<Widget*> m_childWidgets;
	IViewModel* m_pViewModel;
	
};