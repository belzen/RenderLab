#include "Precompiled.h"
#include "Image.h"
#include "Label.h"
#include "UtilsLib/Paths.h"

Image* Image::Create(const Widget& rParent, int x, int y, int width, int height, Icon icon)
{
	return new Image(rParent, x, y, width, height, icon);
}

Image::Image(const Widget& rParent, int x, int y, int width, int height, Icon icon)
	: Widget(x, y, width, height, &rParent, SS_BITMAP | SS_CENTERIMAGE, WC_STATICA)
{
	m_hBitmap = IconLibrary::GetIconImage(icon);
	::SendMessage(GetWindowHandle(), STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)m_hBitmap);
}

Image::~Image()
{

}
