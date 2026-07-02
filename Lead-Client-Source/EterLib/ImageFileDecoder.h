#pragma once

#include <windows.h>
#include <vector>

// Decoded 32-bit B8G8R8A8 image (rows top-down, tightly packed).
struct SDecodedImage
{
	UINT uWidth;
	UINT uHeight;
	std::vector<BYTE> kPixels;
};

// Decodes a TGA (type 2/10, 24/32bpp) or WIC-supported (JPG/BMP/PNG) image file
// held in memory. Returns false on unknown or malformed input.
bool DecodeImageFileFromMemory(const void* c_pvBuf, UINT uBufSize, SDecodedImage* pkRetImage);
