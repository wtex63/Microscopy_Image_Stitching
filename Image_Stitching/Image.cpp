#include "Image.h"
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <comdef.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

BYTE* LoadBMP(int% width, int% height, long% size, LPCTSTR bmpfile)
{
	// declare bitmap structures
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	// value to be used in ReadFile funcs
	DWORD bytesread;
	// open file to read from
	HANDLE file = CreateFile(bmpfile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return NULL; // coudn't open file

	// read file header
	if (ReadFile(file, &bmpheader, sizeof(BITMAPFILEHEADER), &bytesread, NULL) == false || bytesread != sizeof(BITMAPFILEHEADER)) {
		CloseHandle(file);
		return NULL;
	}

	//read bitmap info
	if (ReadFile(file, &bmpinfo, sizeof(BITMAPINFOHEADER), &bytesread, NULL) == false || bytesread != sizeof(BITMAPINFOHEADER)) {
		CloseHandle(file);
		return NULL;
	}

	// check if file is actually a bmp
	if (bmpheader.bfType != 0x4d42) {
		CloseHandle(file);
		return NULL;
	}

	// get image measurements
	width = bmpinfo.biWidth;
	height = abs(bmpinfo.biHeight);
	if (width <= 0 || height <= 0) {
		CloseHandle(file);
		return NULL;
	}

	// check if bmp is uncompressed
	if (bmpinfo.biCompression != BI_RGB) {
		CloseHandle(file);
		return NULL;
	}

	// check if we have 24 bit bmp
	if (bmpinfo.biBitCount != 24) {
		CloseHandle(file);
		return NULL;
	}

	if (bmpheader.bfSize <= bmpheader.bfOffBits) {
		CloseHandle(file);
		return NULL;
	}

	long long minImageSize = static_cast<long long>(width) * static_cast<long long>(height) * 3;
	if (minImageSize <= 0 || static_cast<long long>(bmpheader.bfSize - bmpheader.bfOffBits) < minImageSize) {
		CloseHandle(file);
		return NULL;
	}

	// create buffer to hold the data
	size = bmpheader.bfSize - bmpheader.bfOffBits;
	BYTE* Buffer = new BYTE[size];
	// move file pointer to start of bitmap data
	if (SetFilePointer(file, bmpheader.bfOffBits, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		delete[] Buffer;
		CloseHandle(file);
		return NULL;
	}
	// read bmp data
	if (ReadFile(file, Buffer, static_cast<DWORD>(size), &bytesread, NULL) == false || bytesread != static_cast<DWORD>(size)) {
		delete[] Buffer;
		CloseHandle(file);
		return NULL;
	}

	// everything successful here: close file and return buffer
	CloseHandle(file);

	return Buffer;
}//LoadBMP

BYTE* ConvertBMPToIntensity(BYTE* Buffer, int width, int height)
{
	// first make sure the parameters are valid
	if ((NULL == Buffer) || (width == 0) || (height == 0))
		return NULL;

	// find the number of padding bytes

	int padding = 0;
	int scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)     // DWORD = 4 bytes
		padding++;
	// get the padded scanline width
	int psw = scanlinebytes + padding;

	// create new buffer
	BYTE* newbuf = new BYTE[width * height];

	// now we loop trough all bytes of the original buffer, 
	// swap the R and B bytes and the scanlines
	long bufpos = 0;
	long newpos = 0;
	for (int row = 0; row < height; row++)
		for (int column = 0; column < width; column++) {
			newpos = row * width + column;
			bufpos = (height - row - 1) * psw + column * 3;
			newbuf[newpos] = (BYTE)(0.11 * Buffer[bufpos + 2] + 0.59 * Buffer[bufpos + 1] + 0.3 * Buffer[bufpos]);
		}

	return newbuf;
}//ConvertToBMTToIntensity(..)
BYTE* ConvertIntensityToBMP(BYTE* Buffer, int width, int height, long% newsize)
{
	// first make sure the parameters are valid
	if ((NULL == Buffer) || (width == 0) || (height == 0))
		return NULL;

	// now we have to find with how many bytes
	// we have to pad for the next DWORD boundary	

	int padding = 0;
	int scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)     // DWORD = 4 bytes
		padding++;
	// get the padded scanline width
	int psw = scanlinebytes + padding;
	// we can already store the size of the new padded buffer
	newsize = height * psw;

	// and create new buffer
	BYTE* newbuf = new BYTE[newsize];

	// fill the buffer with zero bytes then we dont have to add
	// extra padding zero bytes later on
	memset(newbuf, 0, newsize);

	// now we loop trough all bytes of the original buffer, 
	// swap the R and B bytes and the scanlines
	long bufpos = 0;
	long newpos = 0;
	for (int row = 0; row < height; row++)
		for (int column = 0; column < width; column++) {
			bufpos = row * width + column;     // position in original buffer
			newpos = (height - row - 1) * psw + column * 3;           // position in padded buffer

			newbuf[newpos] = Buffer[bufpos];       //  blue
			newbuf[newpos + 1] = Buffer[bufpos];   //  green
			newbuf[newpos + 2] = Buffer[bufpos];   //  red
		}

	return newbuf;
}//ConvertIntensityToBMP
bool SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	// declare bmp structures 
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;

	// andinitialize them to zero
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// fill the fileheader with data
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;		// number of bytes to start of bitmap bits

	// fill the infoheader
	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;			// we only have one bitplane
	info.biBitCount = 24;		// RGB mode is 24 bits
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;		// can be 0 for 24 bit images
	info.biXPelsPerMeter = 0x0ec4;	    // paint and PSP use this values
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;			// we are in RGB mode and have no palette
	info.biClrImportant = 0;	    // all colors are important

	// now we open the file to write to
	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// write file header
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER), &bwritten, NULL) == false || bwritten != sizeof(BITMAPFILEHEADER))
	{
		CloseHandle(file);
		return false;
	}
	// write infoheader
	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER), &bwritten, NULL) == false || bwritten != sizeof(BITMAPINFOHEADER))
	{
		CloseHandle(file);
		return false;
	}
	// write image data
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false || bwritten != static_cast<unsigned long>(paddedsize))
	{
		CloseHandle(file);
		return false;
	}

	// and close file
	CloseHandle(file);

	return true;
} //saveBMP

BYTE* LoadJPEG(int% width, int% height, long% size, LPCTSTR jpegfile)
{
	HRESULT hr = S_OK;
	IWICImagingFactory* factory = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICFormatConverter* converter = NULL;
	IWICBitmapScaler* scaler = NULL;
	UINT w = 0, h = 0;
	BYTE* buffer = NULL;

	try {
		// Note: COM is already initialized by Windows Forms
		// Do NOT initialize/uninitialize COM here to avoid conflicts

		// Create WIC imaging factory
		hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&factory);
		if (FAILED(hr) || !factory) {
			return NULL;
		}

		// Create decoder for JPEG with lenient metadata caching
		hr = factory->CreateDecoderFromFilename(jpegfile, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
		if (FAILED(hr) || !decoder) {
			factory->Release();
			return NULL;
		}

		// Get first frame
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr) || !frame) {
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Get image dimensions
		hr = frame->GetSize(&w, &h);
		if (FAILED(hr) || w <= 0 || h <= 0) {
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Create format converter to convert to 24-bit BGR (BMP format)
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr) || !converter) {
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Convert to 24-bit BGR format with dithering to handle color profiles better
		hr = converter->Initialize(frame, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeOrdered4x4, NULL, 0.0, WICBitmapPaletteTypeMedianCut);
		if (FAILED(hr)) {
			// If dithering fails, try without dithering
			hr = converter->Initialize(frame, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
			if (FAILED(hr)) {
				converter->Release();
				frame->Release();
				decoder->Release();
				factory->Release();
				return NULL;
			}
		}

		// Calculate stride (padded to 4-byte boundary like BMP)
		UINT stride = ((w * 3 + 3) / 4) * 4;
		UINT totalSize = stride * h;

		// Sanity check on size
		if (totalSize == 0 || totalSize > 500000000) {  // Reject images larger than 500MB
			converter->Release();
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Allocate buffer
		buffer = new BYTE[totalSize];
		if (!buffer) {
			converter->Release();
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Copy pixels (note: WIC loads top-down, BMP expects bottom-up)
		// Create temporary buffer for top-down copy
		BYTE* tempBuffer = new BYTE[totalSize];
		if (!tempBuffer) {
			delete[] buffer;
			converter->Release();
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		WICRect rect = { 0, 0, (INT)w, (INT)h };
		hr = converter->CopyPixels(&rect, stride, totalSize, tempBuffer);
		if (FAILED(hr)) {
			delete[] tempBuffer;
			delete[] buffer;
			converter->Release();
			frame->Release();
			decoder->Release();
			factory->Release();
			return NULL;
		}

		// Flip vertically to convert from top-down to bottom-up (BMP format)
		for (UINT row = 0; row < h; row++) {
			memcpy(&buffer[row * stride], &tempBuffer[(h - 1 - row) * stride], stride);
		}
		delete[] tempBuffer;

		// Set output parameters
		width = w;
		height = h;
		size = totalSize;

		// Cleanup
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();

		return buffer;
	}
	catch (...) {
		if (buffer) delete[] buffer;
		if (converter) converter->Release();
		if (frame) frame->Release();
		if (decoder) decoder->Release();
		if (factory) factory->Release();
		return NULL;
	}
}//LoadJPEG

BYTE* LoadImage(int% width, int% height, long% size, LPCTSTR imagefile)
{
	if (!imagefile) return NULL;

	// Get file extension
	LPCTSTR ext = NULL;
	for (LPCTSTR p = imagefile; *p; p++) {
		if (*p == L'.' || *p == L'.') {
			ext = p;
		}
	}

	if (!ext) return NULL;

	// Check file extension and call appropriate loader
	if (_wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".dib") == 0) {
		return LoadBMP(width, height, size, imagefile);
	}
	else if (_wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0) {
		return LoadJPEG(width, height, size, imagefile);
	}

	return NULL;
}//LoadImage