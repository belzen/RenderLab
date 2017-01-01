#include "Precompiled.h"
#include "Image.h"
#include "Label.h"
#include "UtilsLib/Paths.h"

Image* Image::Create(const Widget& rParent, int x, int y, int width, int height, const char* text)
{
	return new Image(rParent, x, y, width, height, text);
}

Image::Image(const Widget& rParent, int x, int y, int width, int height, const char* text)
	: Widget(x, y, width, height, &rParent)
{
	m_pLabel = Label::Create(*this, 0, 0, width, height, text);

	/*
	std::string imagePath = Paths::GetDataDir();
	imagePath += "\\icons\\folder.bmp";
	m_hBitmap = LoadImageA(NULL, imagePath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE); //todo2 - image palette
	m_hImageControl = CreateChildWindow(GetWindowHandle(), "Static", 0, 0, width, height, SS_BITMAP, 0);
	SendMessage(m_hImageControl, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)m_hBitmap);
	*/
}

Image::~Image()
{

}

void Image::SetText(const char* text)
{
	m_pLabel->SetText(text);
}

const std::string& Image::GetText() const
{
	return m_pLabel->GetText();
}

