#pragma once

#include "Widget.h"

enum class TextAlignment
{
	kLeft   = 0,
	// Mapping to window style values
	kCenter = SS_CENTER,
	kRight  = SS_RIGHT,
};

class Label : public Widget
{
public:
	static Label* Create(const Widget& rParent, int x, int y, int width, int height, const char* text, TextAlignment alignment = TextAlignment::kLeft);

	void SetText(const char* text);
	const std::string& GetText() const;

private:
	Label(const Widget& rParent, int x, int y, int width, int height, const char* text, TextAlignment alignment);
	Label(const Label&);

	virtual ~Label();

	std::string m_text;
};