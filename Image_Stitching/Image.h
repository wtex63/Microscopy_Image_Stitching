#pragma once
#include <Windows.h>

BYTE* LoadBMP(int% width, int% height, long% size, LPCTSTR bmpfile);
BYTE* LoadJPEG(int% width, int% height, long% size, LPCTSTR jpegfile);
BYTE* LoadImage(int% width, int% height, long% size, LPCTSTR imagefile);
BYTE* ConvertBMPToIntensity(BYTE* Buffer, int width, int height);
BYTE* ConvertIntensityToBMP(BYTE* Buffer, int width, int height, long% newsize);
bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile);