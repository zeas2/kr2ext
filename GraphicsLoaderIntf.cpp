//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000-2007 W.Dee <dee@kikyou.info> and contributors

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Graphics Loader ( loads graphic format from storage )
//---------------------------------------------------------------------------

#include <stdlib.h>
#include "hook_init.h"
#include "GraphicsLoaderIntf.h"
#include "LoadTLG.h"
#include <assert.h>
#include "ncbind.hpp"
#include <memory>

//---------------------------------------------------------------------------

struct VCLCallRouter {
	void *callbackdata;
	tTVPMetaInfoPushCallbackVCL metainfopushcallback;
	tTVPGraphicSizeCallbackVCL sizecallback;
	tTVPGraphicScanLineCallbackVCL scanlinecallback;

	VCLCallRouter(
		void * data,
		tTVPGraphicSizeCallbackVCL f1,
		tTVPGraphicScanLineCallbackVCL f2,
		tTVPMetaInfoPushCallbackVCL f3)
	: callbackdata(data)
	, sizecallback(f1)
	, scanlinecallback(f2)
	, metainfopushcallback(f3)
	{}
};

static __declspec(naked) void *VCLCallRouter_ScanLineCallback(void *callbackdata, tjs_int y) {
	__asm {
		mov ecx, [esp + 4]
		mov edx, [esp + 8]
		mov eax, [ecx]
		call[ecx + 0Ch]
		retn
	}
}

// static void *VCLCallRouter_ScanLineCallback(void *callbackdata, tjs_int y) {
// 	VCLCallRouter* router = (VCLCallRouter*)callbackdata;
// 	return router->scanlinecallback(y, router->callbackdata);
// }

static void __declspec(naked) VCLCallRouter_SizeCallback(void *callbackdata, tjs_uint w, tjs_uint h) {
	__asm {
		push ebp
		mov ebp, esp
		push ebx
		mov ebx, [ebp + 8]
		mov edx, [ebp + 0Ch]
		mov ecx, [ebp + 10h]
		mov eax, [ebx]
		call [ebx + 8]
		pop ebx
		pop ebp
		retn
	}
}
// static void VCLCallRouter_SizeCallback(void *callbackdata, tjs_uint w, tjs_uint h) {
// 	VCLCallRouter* router = (VCLCallRouter*)callbackdata;
// 	return router->sizecallback(h, w, router->callbackdata);
// }

static void __declspec(naked) VCLCallRouter_MetaInfoPushCallback(void *callbackdata, const ttstr & name, const ttstr & value) {
	__asm {
		push ebp
		mov ebp, esp
		push ebx
		mov ebx, [ebp + 8]
		mov edx, [ebp + 0Ch]
		mov ecx, [ebp + 10h]
		mov eax, [ebx]
		call [ebx + 4]
		pop ebx
		pop ebp
		retn
	}
}

// static void VCLCallRouter_MetaInfoPushCallback(void *callbackdata, const ttstr & name, const ttstr & value) {
// 	VCLCallRouter* router = (VCLCallRouter*)callbackdata;
// 	return router->metainfopushcallback(value, name, router->callbackdata);
// }

static void* PNGHandler;
static void* BMPHandler;
static void* TLGHandler;
static void* JPGHandler;

static void* TVPInternalLoadGraphicRoute(
	tTVPGraphicSizeCallback sizecallback,
	void *callbackdata,
	tTVPGraphicLoadMode mode,
	tjs_int32 keyidx,
	tTJSBinaryStream *src,
	tTVPMetaInfoPushCallback metainfopushcallback,
	tTVPGraphicScanLineCallback scanlinecallback
	//,void* formatdata
	)
{
    tjs_uint64 origSrcPos = src->GetPosition();

	tjs_uint8 magic[16] = {0};
    src->ReadBuffer(magic, sizeof(magic));
    src->SetPosition(origSrcPos);
#define CALL_LOAD_FUNC_AND_RET(f) f(nullptr/*formatdata*/, callbackdata, sizecallback, scanlinecallback, metainfopushcallback, src, keyidx, mode);
//#define CALL_HANDLER_AND_RET(f) f(sizecallback, callbackdata, mode, keyidx, src, metainfopushcallback, scanlinecallback); return;
	if(magic[0] == 'B' && magic[1] == 'M') {
		/*CALL_HANDLER_AND_RET*/return (BMPHandler);
		//CALL_LOAD_FUNC_AND_RET(TVPLoadBMP);
    } else if(
        magic[0] == 0x89 &&
        magic[1] == 'P' &&
        magic[2] == 'N' &&
        magic[3] == 'G'
        ) {
		/*CALL_HANDLER_AND_RET*/return (PNGHandler);
        //CALL_LOAD_FUNC_AND_RET(TVPLoadPNG);
    } else if(
        magic[0] == 'T' &&
        magic[1] == 'L' &&
        magic[2] == 'G'
        ) {
		//if (mode == glmGrayscale) {
			CALL_LOAD_FUNC_AND_RET(TVPLoadTLG);
// 		} else {
// 			CALL_HANDLER_AND_RET(TLGHandler);
// 		}
		//CALL_LOAD_FUNC_AND_RET(TVPLoadTLG);
	} else if (
		magic[0] == 'B' &&
		magic[1] == 'P' &&
		magic[2] == 'G'){
		CALL_LOAD_FUNC_AND_RET(TVPLoadBPG);
    } else if(
        magic[0] == 0xFF &&
        magic[1] == 0xD8 &&
        magic[2] == 0xFF &&
        magic[3] >= 0xE0 && magic[3] <= 0xEF
        ) {
		/*CALL_HANDLER_AND_RET*/ return (JPGHandler);
		//CALL_LOAD_FUNC_AND_RET(TVPLoadJPEG);
	} else if (!memcmp(magic, "RIFF", 4) && !memcmp(magic + 8, "WEBPVP8", 7)){
		CALL_LOAD_FUNC_AND_RET(TVPLoadWEBP);
    } else {
        TVPThrowExceptionMessage(
            TJS_W("Unsupported in-built graphic format."));
    }
#undef CALL_LOAD_FUNC_AND_RET
	return nullptr;
}



static void __fastcall TVPLoadGraphicRouteVCL(
	tTVPGraphicSizeCallbackVCL sizecallback,
	void *callbackdata,
	tTVPGraphicLoadMode mode,
	tjs_int32 keyidx,
	tTJSBinaryStream *src,
	tTVPMetaInfoPushCallbackVCL metainfopushcallback,
	tTVPGraphicScanLineCallbackVCL scanlinecallback
	//,void* formatdata
	) {
	VCLCallRouter router(callbackdata, sizecallback, scanlinecallback, metainfopushcallback);
	void *handler =
		TVPInternalLoadGraphicRoute(
		VCLCallRouter_SizeCallback, &router, mode, keyidx, src,
		VCLCallRouter_MetaInfoPushCallback, VCLCallRouter_ScanLineCallback
		);
	if (handler) {
		((tTVPGraphicLoadingHandlerVCL)handler)(sizecallback, callbackdata,
			mode, keyidx, src, metainfopushcallback, scanlinecallback);
	}
}

static void __cdecl TVPLoadGraphicRouteCdecl(void* formatdata,
	void *callbackdata,
	tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback,
	tTVPMetaInfoPushCallback metainfopushcallback,
	tTJSBinaryStream *src,
	tjs_int32 keyidx,
	tTVPGraphicLoadMode mode)
{
	void *handler =
		TVPInternalLoadGraphicRoute(
		sizecallback, callbackdata, mode, keyidx, src,
		metainfopushcallback, scanlinecallback
		);
	if (handler) {
		((tTVPGraphicLoadingHandler)handler)(formatdata, callbackdata,
			sizecallback, scanlinecallback, metainfopushcallback, src, keyidx, mode);
	}
}

static void *_TVPLoadGraphicRouteFunc = nullptr;

static __declspec(naked) void __fastcall TVPLoadGraphicRoute(
	tTVPGraphicSizeCallbackVCL sizecallback,
	void *callbackdata,
	tTVPGraphicLoadMode mode,
	tjs_int32 keyidx,
	tTJSBinaryStream *src,
	tTVPMetaInfoPushCallbackVCL metainfopushcallback,
	tTVPGraphicScanLineCallbackVCL scanlinecallback
	//,void* formatdata
	) {
	__asm {
		cmp _TVPLoadGraphicRouteFunc, 0;
		je InitLoadGraphicRoute;
		jmp _TVPLoadGraphicRouteFunc;
	InitLoadGraphicRoute:
		cmp dword ptr[esp + 08h], 0FFFFFFFFh;
		je UseVCLFunc;
	UseCdeclFunc:
		mov _TVPLoadGraphicRouteFunc, offset TVPLoadGraphicRouteCdecl;
		jmp _TVPLoadGraphicRouteFunc;
	UseVCLFunc:
		mov _TVPLoadGraphicRouteFunc, offset TVPLoadGraphicRouteVCL;
		jmp _TVPLoadGraphicRouteFunc;
	}
}
// static __declspec(naked) void TVPLoadGraphicRoute(
// 	void* formatdata,
// 	void *callbackdata,
// 	tTVPGraphicSizeCallback sizecallback,
// 	tTVPGraphicScanLineCallback scanlinecallback,
// 	tTVPMetaInfoPushCallback metainfopushcallback,
// 	tTJSBinaryStream *src,
// 	tjs_int32 keyidx,
// 	tTVPGraphicLoadMode mode)
// {
// 	__asm {
// 		push dword ptr[esp + 04h]
// 		push dword ptr[esp + 0Ch]
// 		push dword ptr[esp + 14h]
// 		push dword ptr[esp + 1Ch]
// 		push dword ptr[esp + 24h]
// 		push ecx
// 		push edx
// 		push eax
// 		call _TVPLoadGraphicRoute
// 		retn 14h
// 	}
// }


//---------------------------------------------------------------------------
// Graphics Format Management
//---------------------------------------------------------------------------

void InstallGraphicType(tTJSHashTable<ttstr, tTVPGraphicHandlerType> *Hash) {
	PNGHandler = Hash->Find(TJS_W(".png"))->Handler;
	BMPHandler = Hash->Find(TJS_W(".bmp"))->Handler;
	JPGHandler = Hash->Find(TJS_W(".jpg"))->Handler;
	TLGHandler = Hash->Find(TJS_W(".tlg"))->Handler;
	// register some native-supported formats
	Hash->Clear();
#define ADD(ext, func, p) Hash->Add(ext, tTVPGraphicHandlerType(\
	ext, func, p));
	ADD(
		TJS_W(".dib"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".jpeg"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".jif"), TVPLoadGraphicRoute, NULL);
// 	ADD(
// 		TJS_W(".eri"), TVPLoadERI, NULL);
	ADD(
		TJS_W(".tlg5"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".tlg6"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".webp"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".bmp"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".jpg"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".tlg"), TVPLoadGraphicRoute, NULL);
	ADD(
		TJS_W(".png"), TVPLoadGraphicRoute, NULL);
}


/*
	loading handlers return whether the image contains an alpha channel.
*/
#define TVP_REVRGB(v) ((v & 0xFF00FF00) | ((v >> 16) & 0xFF) | ((v & 0xFF) << 16))
static void TVPReverseRGB(tjs_uint32 *src, unsigned int len)
{
	tjs_uint32 *dest = src + len;
	while (src < dest) {
        tjs_uint32 d = *src;
		*src = TVP_REVRGB(d);
        ++src;
    }
}

//---------------------------------------------------------------------------
// BMP loading handler
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#define TVP_BMP_READ_LINE_MAX 8
void TVPInternalLoadBMP(void *callbackdata,
						tTVPGraphicSizeCallback sizecallback,
						tTVPGraphicScanLineCallback scanlinecallback,
						TVP_WIN_BITMAPINFOHEADER &bi,
						const tjs_uint8 *palsrc,
						tTJSBinaryStream * src,
						tjs_int keyidx,
						tTVPBMPAlphaType alphatype,
						tTVPGraphicLoadMode mode)
{
	// mostly taken ( but totally re-written ) from SDL,
	// http://www.libsdl.org/

	// TODO: only checked on Win32 platform


	if (bi.biSize == 12)
	{
		// OS/2
		bi.biCompression = BI_RGB;
		bi.biClrUsed = 1 << bi.biBitCount;
	}

	tjs_uint16 orgbitcount = bi.biBitCount;
	if (bi.biBitCount == 1 || bi.biBitCount == 4)
	{
		bi.biBitCount = 8;
	}

	switch (bi.biCompression)
	{
	case BI_RGB:
		// if there are no masks, use the defaults
		break; // use default
		/*
		if( bf.bfOffBits == ( 14 + bi.biSize) )
		{
		}
		// fall through -- read the RGB masks
		*/
#define TVPImageLoadError TJS_W("Read Image Error/%1")
	case BI_BITFIELDS:
		TVPThrowExceptionMessage(TVPImageLoadError, TJS_W("bit fields not supported"));

	default:
		TVPThrowExceptionMessage(TVPImageLoadError, TJS_W("copmresssed bmp not supported"));
	}

	// load palette
	tjs_uint32 palette[256];   // (msb) argb (lsb)
	if (orgbitcount <= 8)
	{
		if (bi.biClrUsed == 0) bi.biClrUsed = 1 << orgbitcount;
		if (bi.biSize == 12)
		{
			// read OS/2 palette
			for (tjs_uint i = 0; i < bi.biClrUsed; i++)
			{
				palette[i] = palsrc[0] + (palsrc[1] << 8) + (palsrc[2] << 16) +
					0xff000000;
				palsrc += 3;
			}
		} else
		{
			// read Windows palette
			for (tjs_uint i = 0; i<bi.biClrUsed; i++)
			{
				palette[i] = palsrc[0] + (palsrc[1] << 8) + (palsrc[2] << 16) +
					0xff000000;
				// we assume here that the palette's unused segment is useless.
				// fill it with 0xff ( = completely opaque )
				palsrc += 4;
			}
		}

		if (mode == glmGrayscale)
		{
			TVPDoGrayScale(palette, 256);
		}

		if (keyidx != -1)
		{
			// if color key by palette index is specified
			palette[keyidx & 0xff] &= 0x00ffffff; // make keyidx transparent
		}
	} else
	{
		if (mode == glmPalettized)
			TVPThrowExceptionMessage(TVPImageLoadError, TJS_W("unsupported color mode for palett image"));
	}

	tjs_int height;
	height = bi.biHeight<0 ? -bi.biHeight : bi.biHeight;
	// positive value of bi.biHeight indicates top-down DIB

	sizecallback(callbackdata, bi.biWidth, height);

	tjs_int pitch;
	pitch = (((bi.biWidth * orgbitcount) + 31) & ~31) / 8;
	tjs_uint8 *readbuf = (tjs_uint8 *)TJSAlignedAlloc(pitch * TVP_BMP_READ_LINE_MAX, 4);
	tjs_uint8 *buf;
	tjs_int bufremain = 0;
	try
	{
		// process per a line
		tjs_int src_y = 0;
		tjs_int dest_y;
		if (bi.biHeight>0) dest_y = bi.biHeight - 1; else dest_y = 0;

		for (; src_y < height; src_y++)
		{
			if (bufremain == 0)
			{
				tjs_int remain = height - src_y;
				tjs_int read_lines = remain > TVP_BMP_READ_LINE_MAX ?
				TVP_BMP_READ_LINE_MAX : remain;
				src->ReadBuffer(readbuf, pitch * read_lines);
				bufremain = read_lines;
				buf = readbuf;
			}

			void *scanline = scanlinecallback(callbackdata, dest_y);
			if (!scanline) break;

			switch (orgbitcount)
			{
				// convert pixel format
			case 1:
				if (mode == glmPalettized)
				{
					TVPBLExpand1BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)buf, bi.biWidth);
				} else if (mode == glmGrayscale)
				{
					TVPBLExpand1BitTo8BitPal(
						(tjs_uint8*)scanline,
						(tjs_uint8*)buf, bi.biWidth, palette);
				} else
				{
					TVPBLExpand1BitTo32BitPal(
						(tjs_uint32*)scanline,
						(tjs_uint8*)buf, bi.biWidth, palette);
				}
				break;

			case 4:
				if (mode == glmPalettized)
				{
					TVPBLExpand4BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)buf, bi.biWidth);
				} else if (mode == glmGrayscale)
				{
					TVPBLExpand4BitTo8BitPal(
						(tjs_uint8*)scanline,
						(tjs_uint8*)buf, bi.biWidth, palette);
				} else
				{
					TVPBLExpand4BitTo32BitPal(
						(tjs_uint32*)scanline,
						(tjs_uint8*)buf, bi.biWidth, palette);
				}
				break;

			case 8:
				if (mode == glmPalettized)
				{
					// intact copy
					memcpy(scanline, buf, bi.biWidth);
				} else
					if (mode == glmGrayscale)
					{
						// convert to grayscale
						TVPBLExpand8BitTo8BitPal(
							(tjs_uint8*)scanline,
							(tjs_uint8*)buf, bi.biWidth, palette);
					} else
					{
						TVPBLExpand8BitTo32BitPal(
							(tjs_uint32*)scanline,
							(tjs_uint8*)buf, bi.biWidth, palette);
					}
					break;

			case 15:
			case 16:
				if (mode == glmGrayscale)
				{
					TVPBLConvert15BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint16*)buf, bi.biWidth);
				} else
				{
					TVPBLConvert15BitTo32Bit(
						(tjs_uint32*)scanline,
						(tjs_uint16*)buf, bi.biWidth);
				}
				break;

			case 24:
				if (mode == glmGrayscale)
				{
					TVPBLConvert24BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)buf, bi.biWidth);
				} else
				{
					TVPBLConvert24BitTo32Bit(
						(tjs_uint32*)scanline,
						(tjs_uint8*)buf, bi.biWidth);
				}
				break;

			case 32:
				if (mode == glmGrayscale)
				{
					TVPBLConvert32BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint32*)buf, bi.biWidth);
				} else
				{
					if (alphatype == batNone)
					{
						// alpha channel is not given by the bitmap.
						// destination alpha is filled with 255.
						TVPBLConvert32BitTo32Bit_NoneAlpha(
							(tjs_uint32*)scanline,
							(tjs_uint32*)buf, bi.biWidth);
					} else if (alphatype == batMulAlpha)
					{
						// this is the TVP native representation of the alpha channel.
						// simply copy from the buffer.
						TVPBLConvert32BitTo32Bit_MulAddAlpha(
							(tjs_uint32*)scanline,
							(tjs_uint32*)buf, bi.biWidth);
					} else if (alphatype == batAddAlpha)
					{
						// this is alternate representation of the alpha channel,
						// this must be converted to TVP native representation.
						TVPBLConvert32BitTo32Bit_AddAlpha(
							(tjs_uint32*)scanline,
							(tjs_uint32*)buf, bi.biWidth);

					}
				}
				break;
			}

			scanlinecallback(callbackdata, -1); // image was written

			if (bi.biHeight>0) dest_y--; else dest_y++;
			buf += pitch;
			bufremain--;
		}


	}
	catch (...)
	{
		TJSAlignedDealloc(readbuf);
		throw;
	}

	TJSAlignedDealloc(readbuf);
}
//---------------------------------------------------------------------------
void TVPLoadBMP(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
				tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
				tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode)
{
	// Windows BMP Loader
	// mostly taken ( but totally re-written ) from SDL,
	// http://www.libsdl.org/

	// TODO: only checked in Win32 platform



	tjs_uint64 firstpos = src->GetPosition();

	// check the magic
	tjs_uint8 magic[2];
	src->ReadBuffer(magic, 2);
	if (magic[0] != TJS_N('B') || magic[1] != TJS_N('M'))
		TVPThrowExceptionMessage(TVPImageLoadError, TJS_W("not windows bmp"));

	// read the BITMAPFILEHEADER
	TVP_WIN_BITMAPFILEHEADER bf;
	bf.bfSize = src->ReadI32LE();
	bf.bfReserved1 = src->ReadI16LE();
	bf.bfReserved2 = src->ReadI16LE();
	bf.bfOffBits = src->ReadI32LE();

	// read the BITMAPINFOHEADER
	TVP_WIN_BITMAPINFOHEADER bi;
	bi.biSize = src->ReadI32LE();
	if (bi.biSize == 12)
	{
		// OS/2 Bitmap
		memset(&bi, 0, sizeof(bi));
		bi.biWidth = (tjs_uint32)src->ReadI16LE();
		bi.biHeight = (tjs_uint32)src->ReadI16LE();
		bi.biPlanes = src->ReadI16LE();
		bi.biBitCount = src->ReadI16LE();
		bi.biClrUsed = 1 << bi.biBitCount;
	} else if (bi.biSize == 40)
	{
		// Windows Bitmap
		bi.biWidth = src->ReadI32LE();
		bi.biHeight = src->ReadI32LE();
		bi.biPlanes = src->ReadI16LE();
		bi.biBitCount = src->ReadI16LE();
		bi.biCompression = src->ReadI32LE();
		bi.biSizeImage = src->ReadI32LE();
		bi.biXPelsPerMeter = src->ReadI32LE();
		bi.biYPelsPerMeter = src->ReadI32LE();
		bi.biClrUsed = src->ReadI32LE();
		bi.biClrImportant = src->ReadI32LE();
	} else
	{
		TVPThrowExceptionMessage(TVPImageLoadError, TJS_W("unsupported header version"));
	}


	// load palette
	tjs_int palsize = (bi.biBitCount <= 8) ?
		((bi.biClrUsed == 0 ? (1 << bi.biBitCount) : bi.biClrUsed) *
		((bi.biSize == 12) ? 3 : 4)) : 0;  // bi.biSize == 12 ( OS/2 palette )
	tjs_uint8 *palette = NULL;

	if (palsize) palette = new tjs_uint8[palsize];

	try
	{
		src->ReadBuffer(palette, palsize);
		src->SetPosition(firstpos + bf.bfOffBits);

		TVPInternalLoadBMP(callbackdata, sizecallback, scanlinecallback,
						   bi, palette, src, keyidx, batMulAlpha, mode);
	}
	catch (...)
	{
		if (palette) delete[] palette;
		throw;
	}
	if (palette) delete[] palette;
}
//---------------------------------------------------------------------------


extern "C"
{
#define XMD_H
#include <libjpeg/jinclude.h>
#include <libjpeg/jpeglib.h>
#include <libjpeg/jerror.h>
}
#include "GraphicsLoaderIntf.h"
#define TVPJPEGLoadError TJS_W("JPEG read error/%1")

//---------------------------------------------------------------------------
// JPEG loading handler
//---------------------------------------------------------------------------
tTVPJPEGLoadPrecision TVPJPEGLoadPrecision = jlpMedium;
//---------------------------------------------------------------------------
struct my_error_mgr
{
	struct jpeg_error_mgr pub;
};
//---------------------------------------------------------------------------
METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
	TVPThrowExceptionMessage(TVPJPEGLoadError,
							 (ttstr(TJS_W("error code : ")) + ttstr(cinfo->err->msg_code)).c_str());
}
//---------------------------------------------------------------------------
METHODDEF(void)
my_emit_message(j_common_ptr c, int n)
{
}
//---------------------------------------------------------------------------
METHODDEF(void)
my_output_message(j_common_ptr c)
{
}
//---------------------------------------------------------------------------
METHODDEF(void)
my_format_message(j_common_ptr c, char* m)
{
}
//---------------------------------------------------------------------------
METHODDEF(void)
my_reset_error_mgr(j_common_ptr c)
{
	c->err->num_warnings = 0;
	c->err->msg_code = 0;
}
//---------------------------------------------------------------------------
#define BUFFER_SIZE 8192
struct my_source_mgr
{
	jpeg_source_mgr pub;
	JOCTET * buffer;
	tTJSBinaryStream * stream;
	boolean start_of_file;
};
//---------------------------------------------------------------------------
METHODDEF(void)
init_source(j_decompress_ptr cinfo)
{
	// ??
	my_source_mgr * src = (my_source_mgr*)cinfo->src;

	src->start_of_file = TRUE;
}
//---------------------------------------------------------------------------
METHODDEF(boolean)
fill_input_buffer(j_decompress_ptr cinfo)
{
	my_source_mgr * src = (my_source_mgr*)cinfo->src;

	int nbytes = src->stream->Read(src->buffer, BUFFER_SIZE);

	if (nbytes <= 0)
	{
		if (src->start_of_file)
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		WARNMS(cinfo, JWRN_JPEG_EOF);

		src->buffer[0] = (JOCTET)0xFF;
		src->buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;

	src->start_of_file = FALSE;

	return TRUE;
}
//---------------------------------------------------------------------------
METHODDEF(void)
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	my_source_mgr * src = (my_source_mgr*)cinfo->src;

	if (num_bytes > 0) {
		while (num_bytes > (long)src->pub.bytes_in_buffer) {
			num_bytes -= (long)src->pub.bytes_in_buffer;
			fill_input_buffer(cinfo);
			/* note that we assume that fill_input_buffer will never return FALSE,
			* so suspension need not be handled.
			*/
		}
		src->pub.next_input_byte += (size_t)num_bytes;
		src->pub.bytes_in_buffer -= (size_t)num_bytes;
	}
}
//---------------------------------------------------------------------------
METHODDEF(void)
term_source(j_decompress_ptr cinfo)
{
	/* no work necessary here */
}
//---------------------------------------------------------------------------
GLOBAL(void)
jpeg_TStream_src(j_decompress_ptr cinfo, tTJSBinaryStream * infile)
{
	my_source_mgr * src;

	if (cinfo->src == NULL) {	/* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_PERMANENT,
			SIZEOF(my_source_mgr));
		src = (my_source_mgr *)cinfo->src;
		src->buffer = (JOCTET *)
			(*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_PERMANENT,
			BUFFER_SIZE * SIZEOF(JOCTET));
	}

	src = (my_source_mgr *)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
	src->pub.term_source = term_source;
	src->stream = infile;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */
}

//---------------------------------------------------------------------------
void TVPLoadJPEG(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
				 tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
				 tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode)
{
	// JPEG loading handler

	// JPEG does not support palettized image
	if (mode == glmPalettized)
		TVPThrowExceptionMessage(TVPJPEGLoadError,
		TJS_W("Unsupported color type for palattized image"));

	// prepare variables
	jpeg_decompress_struct cinfo;
	my_error_mgr jerr;
	JSAMPARRAY buffer;
	//tjs_int row_stride;

	// error handling
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	jerr.pub.emit_message = my_emit_message;
	jerr.pub.output_message = my_output_message;
	jerr.pub.format_message = my_format_message;
	jerr.pub.reset_error_mgr = my_reset_error_mgr;

	// create decompress object
	jpeg_create_decompress(&cinfo);

	// set data source
	jpeg_TStream_src(&cinfo, src);

	// read the header
	jpeg_read_header(&cinfo, TRUE);

	// decompress option
	switch (TVPJPEGLoadPrecision)
	{
	case jlpLow:
		cinfo.dct_method = JDCT_IFAST;
		cinfo.do_fancy_upsampling = FALSE;
		break;
	case jlpMedium:
		//cinfo.dct_method = JDCT_IFAST;
		cinfo.dct_method = JDCT_ISLOW;
		cinfo.do_fancy_upsampling = TRUE;
		break;
	case jlpHigh:
		cinfo.dct_method = JDCT_FLOAT;
		cinfo.do_fancy_upsampling = TRUE;
		break;
	}

	if (mode == glmGrayscale) cinfo.out_color_space = JCS_GRAYSCALE;

	// start decompression
	jpeg_start_decompress(&cinfo);

	try
	{
		sizecallback(callbackdata, cinfo.output_width, cinfo.output_height);
#if 1
		if (mode == glmNormal && cinfo.out_color_space == JCS_RGB) {
			buffer = new JSAMPROW[cinfo.output_height];
			for (unsigned int i = 0; i < cinfo.output_height; i++) {
				buffer[i] = (JSAMPLE*)scanlinecallback(callbackdata, i);
			}
			while (cinfo.output_scanline < cinfo.output_height) {
				jpeg_read_scanlines(&cinfo, buffer + cinfo.output_scanline, cinfo.output_height - cinfo.output_scanline);
			}
			delete[] buffer;
			for (unsigned int i = 0; i < cinfo.output_height; i++) {
				scanlinecallback(callbackdata, i);
				scanlinecallback(callbackdata, -1);
			}
		} else
#endif
		{
			buffer = (*cinfo.mem->alloc_sarray)
				((j_common_ptr)&cinfo, JPOOL_IMAGE,
				cinfo.output_width * cinfo.output_components + 3,
				cinfo.rec_outbuf_height);

			while (cinfo.output_scanline < cinfo.output_height)
			{
				tjs_int startline = cinfo.output_scanline;

				jpeg_read_scanlines(&cinfo, buffer, cinfo.rec_outbuf_height);

				tjs_int endline = cinfo.output_scanline;
				tjs_int bufline;
				tjs_int line;

				for (line = startline, bufline = 0; line < endline; line++, bufline++)
				{
					void *scanline =
						scanlinecallback(callbackdata, line);
					if (!scanline) break;

					// color conversion
					if (mode == glmGrayscale)
					{
						// write through
						memcpy(scanline,
							   buffer[bufline], cinfo.output_width);
					} else
					{
						if (cinfo.out_color_space == JCS_RGB)
						{
#if 1
							memcpy(scanline,
								   buffer[bufline], cinfo.output_width*sizeof(tjs_uint32));
#else
							// expand 24bits to 32bits
							TVPConvert24BitTo32Bit(
								(tjs_uint32*)scanline,
								(tjs_uint8*)buffer[bufline], cinfo.output_width);
#endif
						} else
						{
							// expand 8bits to 32bits
							TVPExpand8BitTo32BitGray(
								(tjs_uint32*)scanline,
								(tjs_uint8*)buffer[bufline], cinfo.output_width);
						}
					}

					scanlinecallback(callbackdata, -1);
				}
				if (line != endline) break; // interrupted by !scanline
			}
		}
	}
	catch (...)
	{
		jpeg_destroy_decompress(&cinfo);
		throw;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
}
//---------------------------------------------------------------------------
struct stream_destination_mgr {
	struct jpeg_destination_mgr	pub;		/* public fields */
	tTJSBinaryStream*			stream;
	JOCTET*						buffer;		/* buffer start address */
	int							bufsize;	/* size of buffer */
	int							bufsizeinit;/* size of buffer */
	size_t						datasize;	/* final size of compressed data */
	int*						outsize;	/* user pointer to datasize */
};
typedef stream_destination_mgr* stream_dest_ptr;

METHODDEF(void) JPEG_write_init_destination(j_compress_ptr cinfo) {
	stream_dest_ptr dest = (stream_dest_ptr)cinfo->dest;
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = dest->bufsize;
	dest->datasize = 0;	 /* reset output size */
}

METHODDEF(boolean) JPEG_write_empty_output_buffer(j_compress_ptr cinfo) {
	stream_dest_ptr dest = (stream_dest_ptr)cinfo->dest;

	// ‘«‚è‚È‚­‚È‚Á‚½‚ç“r’†‘‚«ž‚Ý
	size_t	wrotelen = dest->bufsizeinit - dest->pub.free_in_buffer;
	dest->stream->WriteBuffer(dest->buffer, wrotelen);

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = dest->bufsize;
	return TRUE;
}

METHODDEF(void) JPEG_write_term_destination(j_compress_ptr cinfo) {
	stream_dest_ptr dest = (stream_dest_ptr)cinfo->dest;
	dest->datasize = dest->bufsize - dest->pub.free_in_buffer;
	if (dest->outsize) *dest->outsize += (int)dest->datasize;
}
METHODDEF(void) JPEG_write_stream(j_compress_ptr cinfo, JOCTET* buffer, int bufsize, int* outsize, tTJSBinaryStream* stream) {
	stream_dest_ptr dest;

	/* first call for this instance - need to setup */
	if (cinfo->dest == 0) {
		cinfo->dest = (struct jpeg_destination_mgr*)(*cinfo->mem->alloc_small) ((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(stream_destination_mgr));
	}

	dest = (stream_dest_ptr)cinfo->dest;
	dest->stream = stream;
	dest->bufsize = bufsize;
	dest->bufsizeinit = bufsize;
	dest->buffer = buffer;
	dest->outsize = outsize;
	/* set method callbacks */
	dest->pub.init_destination = JPEG_write_init_destination;
	dest->pub.empty_output_buffer = JPEG_write_empty_output_buffer;
	dest->pub.term_destination = JPEG_write_term_destination;
}




#ifdef __BORLANDC__
#pragma option push -pr
#endif
#include <png.h>
#include <pngstruct.h>
#include <pnginfo.h>
#ifdef __BORLANDC__
#pragma option pop
#endif
#define TVPPNGLoadError TJS_W("PNG read error/%1")

//---------------------------------------------------------------------------
// PNG loading handler
//---------------------------------------------------------------------------
#define PNG_tag_offs_x TJS_W("offs_x")
#define PNG_tag_offs_y TJS_W("offs_y")
#define PNG_tag_offs_unit TJS_W("offs_unit")
#define PNG_tag_reso_x TJS_W("reso_x")
#define PNG_tag_reso_y TJS_W("reso_y")
#define PNG_tag_reso_unit TJS_W("reso_unit")
#define PNG_tag_vpag_w TJS_W("vpag_w")
#define PNG_tag_vpag_h TJS_W("vpag_h")
#define PNG_tag_vpag_unit TJS_W("vpag_unit")
#define PNG_tag_pixel TJS_W("pixel")
#define PNG_tag_micrometer TJS_W("micrometer")
#define PNG_tag_meter TJS_W("meter")
#define PNG_tag_unknown TJS_W("unknown")

//---------------------------------------------------------------------------
// meta callback information structure used by  PNG_read_chunk_callback
struct PNG_read_chunk_callback_user_struct
{
	void * callbackdata;
	tTVPMetaInfoPushCallback metainfopushcallback;
};
//---------------------------------------------------------------------------
// user_malloc_fn
static png_voidp PNG_malloc(png_structp ps, png_size_t size)
{
	return malloc(size);
}
//---------------------------------------------------------------------------
// user_free_fn
static void PNG_free(png_structp ps, void* /* png_structp*/ mem)
{
	free(mem);
}
//---------------------------------------------------------------------------
// user_error_fn
static void PNG_error(png_structp ps, png_const_charp msg)
{
	TVPThrowExceptionMessage(TVPPNGLoadError, msg);
}
//---------------------------------------------------------------------------
// user_warning_fn
static void PNG_warning(png_structp ps, png_const_charp msg)
{
	// do nothing
	//TVPAddLog( TJS_W("PNG warning:") );
	//TVPAddLog( msg );
}
//---------------------------------------------------------------------------
// user_read_data
static void PNG_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	((tTJSBinaryStream *)png_get_io_ptr(png_ptr))->ReadBuffer((void*)data, length);
}
//---------------------------------------------------------------------------
// read_row_callback
static void PNG_read_row_callback(png_structp png_ptr, png_uint_32 row, int pass)
{

}
//---------------------------------------------------------------------------
// read_chunk_callback
static int PNG_read_chunk_callback(png_structp png_ptr, png_unknown_chunkp chunk)
{
	// handle vpAg chunk (this will contain the virtual page size of the image)
	// vpAg chunk can be embeded by ImageMagick -trim option etc.
	// we don't care about how the chunk bit properties are being provided.
	if ((chunk->name[0] == 0x76/*'v'*/ || chunk->name[0] == 0x56/*'V'*/) &&
		(chunk->name[1] == 0x70/*'p'*/ || chunk->name[1] == 0x50/*'P'*/) &&
		(chunk->name[2] == 0x61/*'a'*/ || chunk->name[2] == 0x41/*'A'*/) &&
		(chunk->name[3] == 0x67/*'g'*/ || chunk->name[3] == 0x47/*'G'*/) && chunk->size >= 9)
	{
		PNG_read_chunk_callback_user_struct * user_struct =
			reinterpret_cast<PNG_read_chunk_callback_user_struct *>(png_get_user_chunk_ptr(png_ptr));
		// vpAg found
		/*
		uint32 width
		uint32 height
		uchar unit
		*/
		// be careful because the integers are stored in network byte order
#define PNG_read_be32(a) (((tjs_uint32)(a)[0]<<24)+\
	((tjs_uint32)(a)[1]<<16)+((tjs_uint32)(a)[2]<<8)+\
	((tjs_uint32)(a)[3]))
		tjs_uint32 width = PNG_read_be32(chunk->data + 0);
		tjs_uint32 height = PNG_read_be32(chunk->data + 4);
		tjs_uint8  unit = chunk->data[8];

		// push information into meta-info
		user_struct->metainfopushcallback(user_struct->callbackdata, PNG_tag_vpag_w, ttstr((tjs_int)width));
		user_struct->metainfopushcallback(user_struct->callbackdata, PNG_tag_vpag_h, ttstr((tjs_int)height));
		switch (unit)
		{
		case PNG_OFFSET_PIXEL:
			user_struct->metainfopushcallback(user_struct->callbackdata, PNG_tag_vpag_unit, PNG_tag_pixel);
			break;
		case PNG_OFFSET_MICROMETER:
			user_struct->metainfopushcallback(user_struct->callbackdata, PNG_tag_vpag_unit, PNG_tag_micrometer);
			break;
		default:
			user_struct->metainfopushcallback(user_struct->callbackdata, PNG_tag_vpag_unit, PNG_tag_unknown);
			break;
		}
		return 1; // chunk read success
	}
	return 0; // did not recognize
}
//---------------------------------------------------------------------------
void TVPLoadPNG(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
				tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
				tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode)
{
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_infop end_info = NULL;

	png_uint_32 i;

	png_bytep *row_pointers = NULL;
	tjs_uint8 *image = NULL;


	try
	{
		// create png_struct
		png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
										   (png_voidp)NULL, (png_error_ptr)PNG_error, (png_error_ptr)PNG_warning,
										   (png_voidp)NULL, (png_malloc_ptr)PNG_malloc, (png_free_ptr)PNG_free);
		//png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, (png_voidp)NULL, (png_error_ptr)PNG_error, (png_error_ptr)PNG_warning );
		if (!png_ptr) TVPThrowExceptionMessage(TVPPNGLoadError, L"libpng error");

		// set read_chunk_callback
		PNG_read_chunk_callback_user_struct read_chunk_callback_user_struct;
		read_chunk_callback_user_struct.callbackdata = callbackdata;
		read_chunk_callback_user_struct.metainfopushcallback = metainfopushcallback;
		png_set_read_user_chunk_fn(png_ptr,
								   reinterpret_cast<void*>(&read_chunk_callback_user_struct),
								   (png_user_chunk_ptr)PNG_read_chunk_callback);
		png_set_keep_unknown_chunks(png_ptr, 2, NULL, 0);
		// keep only if safe-to-copy chunks, for all unknown chunks

		// create png_info
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr) TVPThrowExceptionMessage(TVPPNGLoadError, L"libpng error");

		// create end_info
		end_info = png_create_info_struct(png_ptr);
		if (!end_info) TVPThrowExceptionMessage(TVPPNGLoadError, L"libpng error");

		// set stream interface
		png_set_read_fn(png_ptr, (png_voidp)src, (png_rw_ptr)PNG_read_data);

		// set read_row_callback
		png_set_read_status_fn(png_ptr, (png_read_status_ptr)PNG_read_row_callback);

		// set png_read_info
		png_read_info(png_ptr, info_ptr);

		// retrieve IHDR
		png_uint_32 width, height;
		int bit_depth, color_type, interlace_type, compression_type, filter_type;
		png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
					 &interlace_type, &compression_type, &filter_type);

		if (bit_depth == 16) png_set_strip_16(png_ptr);

		// retrieve offset information
		png_int_32 offset_x, offset_y;
		int offset_unit_type;
		if (metainfopushcallback &&
			png_get_oFFs(png_ptr, info_ptr, &offset_x, &offset_y, &offset_unit_type))
		{
			// push offset information into metainfo data
			metainfopushcallback(callbackdata, PNG_tag_offs_x, ttstr((tjs_int)offset_x));
			metainfopushcallback(callbackdata, PNG_tag_offs_y, ttstr((tjs_int)offset_y));
			switch (offset_unit_type)
			{
			case PNG_OFFSET_PIXEL:
				metainfopushcallback(callbackdata, PNG_tag_offs_unit, PNG_tag_pixel);
				break;
			case PNG_OFFSET_MICROMETER:
				metainfopushcallback(callbackdata, PNG_tag_offs_unit, PNG_tag_micrometer);
				break;
			default:
				metainfopushcallback(callbackdata, PNG_tag_offs_unit, PNG_tag_unknown);
				break;
			}
		}

		png_uint_32 reso_x, reso_y;
		int reso_unit_type;
		if (metainfopushcallback &&
			png_get_pHYs(png_ptr, info_ptr, &reso_x, &reso_y, &reso_unit_type))
		{
			// push offset information into metainfo data
			metainfopushcallback(callbackdata, PNG_tag_reso_x, ttstr((tjs_int)reso_x));
			metainfopushcallback(callbackdata, PNG_tag_reso_y, ttstr((tjs_int)reso_y));
			switch (reso_unit_type)
			{
			case PNG_RESOLUTION_METER:
				metainfopushcallback(callbackdata, PNG_tag_reso_unit, PNG_tag_meter);
				break;
			default:
				metainfopushcallback(callbackdata, PNG_tag_reso_unit, PNG_tag_unknown);
				break;
			}
		}


		bool do_convert_rgb_gray = false;

		if (mode == glmPalettized)
		{
			// convert the image to palettized one if needed
			if (bit_depth > 8)
				TVPThrowExceptionMessage(
				TVPPNGLoadError, L"unsupported color type palette");

			if (color_type == PNG_COLOR_TYPE_PALETTE)
			{
				png_set_packing(png_ptr);
			}

			if (color_type == PNG_COLOR_TYPE_GRAY) png_set_expand_gray_1_2_4_to_8(png_ptr);
		} else if (mode == glmGrayscale)
		{
			// convert the image to grayscale
			if (color_type == PNG_COLOR_TYPE_PALETTE)
			{
				png_set_palette_to_rgb(png_ptr);
				png_set_bgr(png_ptr);
				if (bit_depth < 8)
					png_set_packing(png_ptr);
				do_convert_rgb_gray = true; // manual conversion
			}
			if (color_type == PNG_COLOR_TYPE_GRAY &&
				bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
			if (color_type == PNG_COLOR_TYPE_RGB ||
				color_type == PNG_COLOR_TYPE_RGB_ALPHA)
				png_set_rgb_to_gray_fixed(png_ptr, 1, 0, 0);
			if (color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
				color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
				png_set_strip_alpha(png_ptr);
		} else
		{
			// glmNormal
			// convert the image to full color ( 32bits ) one if needed

			if (color_type == PNG_COLOR_TYPE_PALETTE)
			{
				if (keyidx == -1)
				{
					if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
					{
						// set expansion with palettized picture
						png_set_palette_to_rgb(png_ptr);
						png_set_tRNS_to_alpha(png_ptr);
						color_type = PNG_COLOR_TYPE_RGB_ALPHA;
					} else
					{
						png_set_palette_to_rgb(png_ptr);
						color_type = PNG_COLOR_TYPE_RGB;
					}
				} else
				{
					png_byte trans = (png_byte)keyidx;
					png_set_tRNS(png_ptr, info_ptr, &trans, 1, 0);
					// make keyidx transparent color.
					png_set_palette_to_rgb(png_ptr);
					png_set_tRNS_to_alpha(png_ptr);
					color_type = PNG_COLOR_TYPE_RGB_ALPHA;

				}
			}

			switch (color_type)
			{
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				png_set_gray_to_rgb(png_ptr);
				color_type = PNG_COLOR_TYPE_RGB_ALPHA;
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			case PNG_COLOR_TYPE_GRAY:
				png_set_expand(png_ptr);
				png_set_gray_to_rgb(png_ptr);
				color_type = PNG_COLOR_TYPE_RGB;
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				png_set_bgr(png_ptr);
				break;
			case PNG_COLOR_TYPE_RGB:
				png_set_bgr(png_ptr);
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
				break;
			default:
				TVPThrowExceptionMessage(
					TVPPNGLoadError, L"unsupported color type");
			}
		}


		// size checking
		if (width >= 65536 || height >= 65536)
		{
			// too large image to handle
			TVPThrowExceptionMessage(
				TVPPNGLoadError, L"too large image");
		}


		// call png_read_update_info
		png_read_update_info(png_ptr, info_ptr);

		// set size
		sizecallback(callbackdata, width, height);

		// load image
		if (info_ptr->interlace_type == PNG_INTERLACE_NONE)
		{
			// non-interlace
			if (do_convert_rgb_gray)
			{
				png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);
				image = new tjs_uint8[rowbytes];
			}
#if 1
			if (!do_convert_rgb_gray) {
				for (i = 0; i<height; i++) {
					void *scanline = scanlinecallback(callbackdata, i);
					if (!scanline) break;
					png_read_row(png_ptr, (png_bytep)scanline, NULL);
					scanlinecallback(callbackdata, -1);
				}
			} else {
				for (i = 0; i<height; i++) {
					void *scanline = scanlinecallback(callbackdata, i);
					if (!scanline) break;
					png_read_row(png_ptr, (png_bytep)image, NULL);
					TVPBLConvert24BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)image, width);
					scanlinecallback(callbackdata, -1);
				}
			}
#else
			for (i = 0; i<height; i++)
			{
				void *scanline = scanlinecallback(callbackdata, i);
				if (!scanline) break;
				if (!do_convert_rgb_gray)
				{
					png_read_row(png_ptr, (png_bytep)scanline, NULL);
				} else
				{
					png_read_row(png_ptr, (png_bytep)image, NULL);
					TVPBLConvert24BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)image, width);
				}
				scanlinecallback(callbackdata, -1);
			}
#endif
			// finish loading
			png_read_end(png_ptr, info_ptr);
		} else
		{
			// interlace handling
			// load the image at once

			row_pointers = new png_bytep[height];
			png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);
			image = new tjs_uint8[rowbytes * height];
			for (i = 0; i<height; i++)
			{
				row_pointers[i] = image + i*rowbytes;
			}

			// loads image
			png_read_image(png_ptr, row_pointers);

			// finish loading
			png_read_end(png_ptr, info_ptr);

			// set the pixel data
			for (i = 0; i<height; i++)
			{
				void *scanline = scanlinecallback(callbackdata, i);
				if (!scanline) break;
				if (!do_convert_rgb_gray)
				{
					memcpy(scanline, row_pointers[i], rowbytes);
				} else
				{
					TVPBLConvert24BitTo8Bit(
						(tjs_uint8*)scanline,
						(tjs_uint8*)row_pointers[i], width);
				}
				scanlinecallback(callbackdata, -1);
			}
		}
	}
	catch (...)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		if (row_pointers) delete[] row_pointers;
		if (image) delete[] image;
		throw;
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	if (row_pointers) delete[] row_pointers;
	if (image) delete[] image;
}
//---------------------------------------------------------------------------
#pragma pack(push, 1)
struct tTVPLayerBitmapMemoryRecord
{
	void * alloc_ptr; // allocated pointer
	tjs_uint size; // original bmp bits size, in bytes
	tjs_uint32 sentinel_backup1; // sentinel value 1
	tjs_uint32 sentinel_backup2; // sentinel value 2
};
#pragma pack(pop)
typedef uint32_t tTJSPointerSizedInteger;
#define TVPCannotAllocateBitmapBits \
	TJS_W("¥Ó¥Ã¥È¥Þ¥Ã¥×ÓÃ¥á¥â¥ê¤ò´_±£¤Ç¤­¤Þ¤»¤ó/%1(size=%2)")
//---------------------------------------------------------------------------
static void * TVPAllocBitmapBits(tjs_uint size, tjs_uint width, tjs_uint height)
{
	if (size == 0) return NULL;

	tjs_uint8 * ptrorg, *ptr;
	tjs_uint allocbytes = 16 + size + sizeof(tTVPLayerBitmapMemoryRecord)+sizeof(tjs_uint32)* 2;
	ptr = ptrorg = (tjs_uint8*)malloc(allocbytes);
	if (!ptr) TVPThrowExceptionMessage(TVPCannotAllocateBitmapBits,
		TJS_W("at TVPAllocBitmapBits"), ttstr((tjs_int)allocbytes) + TJS_W("(") +
		ttstr((int)width) + TJS_W("x") + ttstr((int)height) + TJS_W(")"));
	// align to a paragraph ( 16-bytes )
	ptr += 16 + sizeof(tTVPLayerBitmapMemoryRecord);
	*reinterpret_cast<tTJSPointerSizedInteger*>(&ptr) >>= 4;
	*reinterpret_cast<tTJSPointerSizedInteger*>(&ptr) <<= 4;

	tTVPLayerBitmapMemoryRecord * record =
		(tTVPLayerBitmapMemoryRecord*)
		(ptr - sizeof(tTVPLayerBitmapMemoryRecord)-sizeof(tjs_uint32));

	// fill memory allocation record
	record->alloc_ptr = (void *)ptrorg;
	record->size = size;
	record->sentinel_backup1 = rand() + (rand() << 16);
	record->sentinel_backup2 = rand() + (rand() << 16);

	// set sentinel
	*(tjs_uint32*)(ptr - sizeof(tjs_uint32)) = ~record->sentinel_backup1;
	*(tjs_uint32*)(ptr + size) = ~record->sentinel_backup2;
	// Stored sentinels are nagated, to avoid that the sentinel backups in
	// tTVPLayerBitmapMemoryRecord becomes the same value as the sentinels.
	// This trick will make the detection of the memory corruption easier.
	// Because on some occasions, running memory writing will write the same
	// values at first sentinel and the tTVPLayerBitmapMemoryRecord.

	// return buffer pointer
	return ptr;
}
//---------------------------------------------------------------------------
static void TVPFreeBitmapBits(void *ptr)
{
	if (ptr)
	{
		// get memory allocation record pointer
		tjs_uint8 *bptr = (tjs_uint8*)ptr;
		tTVPLayerBitmapMemoryRecord * record =
			(tTVPLayerBitmapMemoryRecord*)
			(bptr - sizeof(tTVPLayerBitmapMemoryRecord)-sizeof(tjs_uint32));

		// check sentinel
		if (~(*(tjs_uint32*)(bptr - sizeof(tjs_uint32))) != record->sentinel_backup1)
			TVPThrowExceptionMessage(
			TJS_W("Layer bitmap: Buffer underrun detected. Check your drawing code!"));
		if (~(*(tjs_uint32*)(bptr + record->size)) != record->sentinel_backup2)
			TVPThrowExceptionMessage(
			TJS_W("Layer bitmap: Buffer overrun detected. Check your drawing code!"));

		free(record->alloc_ptr);
	}
}
//---------------------------------------------------------------------------

struct tTVPBitmap
{
	tjs_int RefCount;

	void * Bits; // pointer to bitmap bits
	BITMAPINFO *BitmapInfo; // DIB information
	tjs_int BitmapInfoSize;

	tjs_int PitchBytes; // bytes required in a line
	tjs_int PitchStep; // step bytes to next(below) line
	tjs_int Width; // actual width
	tjs_int Height; // actual height

public:
	tTVPBitmap(tjs_uint width, tjs_uint height, tjs_uint bpp);

	tTVPBitmap(const tTVPBitmap & r);

	~tTVPBitmap();

	void Allocate(tjs_uint width, tjs_uint height, tjs_uint bpp);

	void AddRef(void)
	{
		RefCount++;
	}

	void Release(void)
	{
		if (RefCount == 1)
			delete this;
		else
			RefCount--;
	}

	tjs_uint GetWidth() const { return Width; }
	tjs_uint GetHeight() const { return Height; }

	tjs_uint GetBPP() const;
	bool Is32bit() const;
	bool Is8bit() const;


	void * GetScanLine(tjs_uint l) const;

	tjs_int GetPitch() const { return PitchStep; }

	bool IsIndependent() const { return RefCount == 1; }

	const void * GetBits() const { return Bits; }
};


//---------------------------------------------------------------------------
// tTVPBitmap : internal bitmap object
//---------------------------------------------------------------------------
/*
important:
Note that each lines must be started at tjs_uint32 ( 4bytes ) aligned address.
This is the default Windows bitmap allocate behavior.
*/
tTVPBitmap::tTVPBitmap(tjs_uint width, tjs_uint height, tjs_uint bpp)
{
	// tTVPBitmap constructor

	//TVPInitWindowOptions(); // ensure window/bitmap usage options are initialized

	RefCount = 1;

	Allocate(width, height, bpp); // allocate initial bitmap
}
//---------------------------------------------------------------------------
tTVPBitmap::~tTVPBitmap()
{
	TVPFreeBitmapBits(Bits);
	free(BitmapInfo);
}
//---------------------------------------------------------------------------
tTVPBitmap::tTVPBitmap(const tTVPBitmap & r)
{
	// constructor for cloning bitmap
	//TVPInitWindowOptions(); // ensure window/bitmap usage options are initialized

	RefCount = 1;

	// allocate bitmap which has the same metrics to r
	Allocate(r.GetWidth(), r.GetHeight(), r.GetBPP());

	// copy BitmapInfo
	memcpy(BitmapInfo, r.BitmapInfo, BitmapInfoSize);

	// copy Bits
	if (r.Bits) memcpy(Bits, r.Bits, r.BitmapInfo->bmiHeader.biSizeImage);

	// copy pitch
	PitchBytes = r.PitchBytes;
	PitchStep = r.PitchStep;
}
//---------------------------------------------------------------------------
struct _BITMAPINFO : public BITMAPINFO {

};
void tTVPBitmap::Allocate(tjs_uint width, tjs_uint height, tjs_uint bpp)
{
	// allocate bitmap bits
	// bpp must be 8 or 32

	// create BITMAPINFO
	BitmapInfoSize = sizeof(BITMAPINFOHEADER)+
		((bpp == 8) ? sizeof(RGBQUAD)* 256 : 0);
	BitmapInfo = (_BITMAPINFO*)calloc(1, BitmapInfoSize);
	//GlobalAlloc(GPTR, BitmapInfoSize);
	if (!BitmapInfo) TVPThrowExceptionMessage(TVPCannotAllocateBitmapBits,
		TJS_W("allocating BITMAPINFOHEADER"), ttstr((tjs_int)BitmapInfoSize));

	Width = width;
	Height = height;

	tjs_uint bitmap_width = width;
	// note that the allocated bitmap size can be bigger than the
	// original size because the horizontal pitch of the bitmap
	// is aligned to a paragraph (16bytes)

	if (bpp == 8)
	{
		bitmap_width = (((bitmap_width - 1) / 16) + 1) * 16; // align to a paragraph
		PitchBytes = (((bitmap_width - 1) >> 2) + 1) << 2;
	} else
	{
		bitmap_width = (((bitmap_width - 1) / 4) + 1) * 4; // align to a paragraph
		PitchBytes = bitmap_width * 4;
	}

	PitchStep = /*-*/PitchBytes;


	BitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	BitmapInfo->bmiHeader.biWidth = bitmap_width;
	BitmapInfo->bmiHeader.biHeight = height;
	BitmapInfo->bmiHeader.biPlanes = 1;
	BitmapInfo->bmiHeader.biBitCount = bpp;
	BitmapInfo->bmiHeader.biCompression = 0/*BI_RGB*/;
	BitmapInfo->bmiHeader.biSizeImage = PitchBytes * height;
	BitmapInfo->bmiHeader.biXPelsPerMeter = 0;
	BitmapInfo->bmiHeader.biYPelsPerMeter = 0;
	BitmapInfo->bmiHeader.biClrUsed = 0;
	BitmapInfo->bmiHeader.biClrImportant = 0;

	// create grayscale palette
	if (bpp == 8)
	{
		RGBQUAD *pal = (RGBQUAD*)((tjs_uint8*)BitmapInfo + sizeof(BITMAPINFOHEADER));

		for (tjs_int i = 0; i < 256; i++)
		{
			pal[i].rgbBlue = pal[i].rgbGreen = pal[i].rgbRed = (BYTE)i;
			pal[i].rgbReserved = 0;
		}
	}

	// allocate bitmap bits
	try
	{
		Bits = TVPAllocBitmapBits(BitmapInfo->bmiHeader.biSizeImage,
			width, height);
	} catch (...)
	{
		free(BitmapInfo), BitmapInfo = NULL;
		throw;
	}
}

#define TVPScanLineRangeOver \
	TJS_W("¥¹¥­¥ã¥ó¥é¥¤¥ó %1 ¤Ï¹ ‡ì(0¡«%2)¤ò³¬¤¨¤Æ¤¤¤Þ¤¹")
//---------------------------------------------------------------------------
void * tTVPBitmap::GetScanLine(tjs_uint l) const
{
	if ((tjs_int)l >= BitmapInfo->bmiHeader.biHeight)
	{
		TVPThrowExceptionMessage(TVPScanLineRangeOver, ttstr((tjs_int)l),
			ttstr((tjs_int)BitmapInfo->bmiHeader.biHeight - 1));
	}

	return /*(BitmapInfo->bmiHeader.biHeight - l -1 )*/l * PitchBytes + (tjs_uint8*)Bits;
}

tjs_uint tTVPBitmap::GetBPP() const {
	return BitmapInfo->bmiHeader.biBitCount;
}

bool tTVPBitmap::Is32bit() const {
	return BitmapInfo->bmiHeader.biBitCount == 32;
}

bool tTVPBitmap::Is8bit() const {
	return BitmapInfo->bmiHeader.biBitCount == 8;
}

//---------------------------------------------------------------------------
// TVPLoadGraphic related
//---------------------------------------------------------------------------
enum tTVPLoadGraphicType
{
	lgtFullColor, // full 32bit color
	lgtPalGray, // palettized or grayscale
	lgtMask // mask
};
struct tTVPGraphicMetaInfoPair
{
	ttstr Name;
	ttstr Value;
	tTVPGraphicMetaInfoPair(const ttstr &name, const ttstr &value) :
	Name(name), Value(value) {;}
};
struct tTVPFont
{
	tjs_int Height; // height of text
	tjs_uint32 Flags;
	tjs_int Angle; // rotation angle ( in tenths of degrees ) 0 .. 1800 .. 3600

	ttstr Face; // font name
};
class tTVPPrerenderedFont;
#pragma pack(push, 4)
class tTVPNativeBaseBitmap
{
public:
	virtual ~tTVPNativeBaseBitmap() {}

private:
	tTVPFont Font;
	bool FontChanged;
	tjs_int GlobalFontState;

	// v--- these can be recreated in ApplyFont if FontChanged flag is set
	tTVPPrerenderedFont *PrerenderedFont;
	LOGFONTA LogFont;
	tjs_int AscentOfsX;
	tjs_int AscentOfsY;
	double RadianAngle;
	tjs_uint32 FontHash;
	// ^---

private:
	tjs_int TextWidth;
	tjs_int TextHeight;
	ttstr CachedText;

public:
	tTVPBitmap *Bitmap;
};
class tTVPBaseBitmap : public tTVPNativeBaseBitmap {};
#pragma pack(pop)
struct tTVPLoadGraphicData
{
	ttstr Name;
	tTVPBaseBitmap *Dest;
	tTVPLoadGraphicType Type;
	tjs_int ColorKey;
	tjs_uint8 *Buffer;
	tjs_uint ScanLineNum;
	tjs_uint DesW;
	tjs_uint DesH;
	tjs_uint OrgW;
	tjs_uint OrgH;
	tjs_uint BufW;
	tjs_uint BufH;
	bool NeedMetaInfo;
	std::vector<tTVPGraphicMetaInfoPair> * MetaInfo;
};

#include "webp/decode.h"
#include "webp/demux.h"
void TVPLoadWEBP( void* formatdata, void *callbackdata,
	tTVPGraphicSizeCallback sizecallback, tTVPGraphicScanLineCallback scanlinecallback,
	tTVPMetaInfoPushCallback metainfopushcallback, tTJSBinaryStream *src, tjs_int keyidx,
	tTVPGraphicLoadMode mode )
{
	int datasize = src->GetSize();
	uint8_t *data = new uint8_t[datasize];
	src->ReadBuffer(data, datasize);

	int width, height;
	WebPGetInfo((uint8_t*)data, datasize, &width, &height);
	sizecallback(callbackdata, width, height);
	tTVPLoadGraphicData * graphicInfo = *(tTVPLoadGraphicData **)callbackdata;
	unsigned int stride = graphicInfo->Dest->Bitmap->PitchBytes;
	unsigned int step = graphicInfo->Dest->Bitmap->PitchStep;
#if 0
	WebPData webp_data = { data, datasize };
	WebPDemuxer* demux = WebPDemux(&webp_data);
	WebPChunkIterator chunk_iter;
	if (WebPDemuxGetChunk(demux, "USER", 1, &chunk_iter)) {
		chunk_iter.chunk.bytes;
		WebPDemuxReleaseChunkIterator(&chunk_iter);
	}

	WebPDemuxDelete(demux);
#endif
	uint8_t* scanline = (uint8_t*)scanlinecallback(callbackdata, 0);
	if(glmNormal == mode) {
		if (!WebPDecodeBGRAInto(data, datasize, scanline, height * stride, step)) {
			delete []data;
			TVPThrowExceptionMessage(TJS_W("Invalid WebP image(RGBA mode)"));
		}
	} else if(glmGrayscale == mode) {
		unsigned int uvSize = width * height / 4;
		uint8_t *dummy = new uint8_t[uvSize];
		if(!WebPDecodeYUVInto(data, datasize,
			scanline, height * stride, step,
			dummy, uvSize, width / 2,
			dummy, uvSize, width / 2))
		{
			delete []data;
			delete []dummy;
			TVPThrowExceptionMessage(TJS_W("Invalid WebP image(Grayscale Mode)"));
		}
		delete []dummy;
	} else {
		delete []data;
		TVPThrowExceptionMessage(TJS_W("WebP does not support palettized image"));
	}
	delete []data;
}

extern "C" {
#include "libbpg/libbpg.h"
}
void TVPLoadBPG(void* formatdata, void *callbackdata,
	tTVPGraphicSizeCallback sizecallback, tTVPGraphicScanLineCallback scanlinecallback,
	tTVPMetaInfoPushCallback metainfopushcallback, tTJSBinaryStream *src, tjs_int keyidx,
	tTVPGraphicLoadMode mode)
{
	struct CBPGDecoderContext {
		BPGDecoderContext *ctx;
		CBPGDecoderContext() {
			//TVPInitLibAVCodec();
			ctx = bpg_decoder_open();
		}
		~CBPGDecoderContext() {
			bpg_decoder_close(ctx);
		}
		BPGDecoderContext *get() { return ctx; }
	} img;
	int datasize = src->GetSize();
	std::unique_ptr<uint8_t[]> data(new uint8_t[datasize]);
	src->ReadBuffer(data.get(), datasize);

	if (bpg_decoder_decode(img.get(), data.get(), datasize) < 0) {
		TVPThrowExceptionMessage(TJS_W("Invalid BPG image"));
	}

	BPGImageInfo img_info;

	bpg_decoder_get_info(img.get(), &img_info);

	sizecallback(callbackdata, img_info.width, img_info.height);
	bpg_decoder_start(img.get(), BPG_OUTPUT_FORMAT_RGBA32);
	if (glmNormal == mode) {
		for (uint32_t y = 0; y < img_info.height; y++) {
			bpg_decoder_get_line(img.get(), (uint8_t*)scanlinecallback(callbackdata, y));
		}
	} else if (glmGrayscale == mode) {
		for (uint32_t y = 0; y < img_info.height; y++) {
			bpg_decoder_get_gray_line(img.get(), (uint8_t*)scanlinecallback(callbackdata, y));
		}
	}
	scanlinecallback(callbackdata, -1); // image was written
}

struct tTVPGraphicsSearchData
{
	ttstr Name;
	tjs_int32 KeyIdx; // color key index
	tTVPGraphicLoadMode Mode; // image mode
	tjs_uint DesW; // desired width ( 0 for original size )
	tjs_uint DesH; // desired height ( 0 for original size )

	bool operator == (const tTVPGraphicsSearchData &rhs) const
	{
		return KeyIdx == rhs.KeyIdx && Mode == rhs.Mode &&
			Name == rhs.Name && DesW == rhs.DesW && DesH == rhs.DesH;
	}
};

class tTVPGraphicsSearchHashFunc
{
public:
	static tjs_uint32 Make(const tTVPGraphicsSearchData &val)
	{
		tjs_uint32 v = tTJSHashFunc<ttstr>::Make(val.Name);

		v ^= val.KeyIdx + (val.KeyIdx >> 23);
		v ^= (val.Mode << 30);
		v ^= val.DesW + (val.DesW >> 8);
		v ^= val.DesH + (val.DesH >> 8);
		return v;
	}
};

bool TVPAllocGraphicCacheOnHeap = false;
class tTVPGraphicImageData
{
private:
	tTVPBitmap *Bitmap;
	tjs_uint8 * RawData;
	tjs_int Width;
	tjs_int Height;
	tjs_int PixelSize;

public:
	ttstr ProvinceName;

	std::vector<tTVPGraphicMetaInfoPair> * MetaInfo;

private:
	tjs_int RefCount;
	tjs_uint Size;

public:
	tTVPGraphicImageData()
	{
		RefCount = 1; Size = 0; Bitmap = NULL; RawData = NULL;
		MetaInfo = NULL; 
	}
	~tTVPGraphicImageData()
	{
		if (Bitmap) Bitmap->Release(), Bitmap = NULL;
		if (RawData) delete[] RawData;
		if (MetaInfo) delete MetaInfo;
	}

	void AssignBitmap(tTVPBitmap *bmp)
	{
		if (Bitmap) Bitmap->Release(), Bitmap = NULL;
		if (RawData) delete[] RawData, RawData = NULL;

		Width = bmp->GetWidth();
		Height = bmp->GetHeight();
		PixelSize = bmp->GetBPP() / 8;
		Size = Width*Height*PixelSize;

		if (!TVPAllocGraphicCacheOnHeap)
		{
			// simply assin to Bitmap
			Bitmap = bmp;
			Bitmap->AddRef();
		} else
		{
			// allocate heap and copy to it
			tjs_int h = Height;
			RawData = new tjs_uint8[Size];
			tjs_uint8 *p = RawData;
			tjs_int rawpitch = Width * PixelSize;
			for (h--; h >= 0; h--)
			{
				memcpy(p, bmp->GetScanLine(h), rawpitch);
				p += rawpitch;
			}
		}
	}
#if 0
	void AssignToBitmap(iTJSDispatch2 *bmp) const
	{
		if (!TVPAllocGraphicCacheOnHeap) {
			// simply assign to Bitmap
			if (Bitmap) bmp->AssignBitmap(Bitmap);
		} else {
			// copy from the rawdata heap
			if (RawData)
			{
				bmp->Recreate(Width, Height, PixelSize == 4 ? 32 : 8);
				tjs_int h = Height;
				tjs_uint8 *p = RawData;
				tjs_int rawpitch = Width * PixelSize;
				for (h--; h >= 0; h--)
				{
					memcpy(bmp->GetScanLineForWrite(h), p, rawpitch);
					p += rawpitch;
				}
			}
		}
	}
#endif
	tjs_uint GetSize() const { return Size; }

	void AddRef() { RefCount++; }
	void Release()
	{
		if (RefCount == 1)
		{
			delete this;
		} else
		{
			RefCount--;
		}
	}
};

typedef tTJSRefHolder<tTVPGraphicImageData> tTVPGraphicImageHolder;
typedef tTJSHashTable<tTVPGraphicsSearchData, tTVPGraphicImageHolder, tTVPGraphicsSearchHashFunc>
tTVPGraphicCache;
tTVPGraphicCache TVPGraphicCache;
static tjs_uint TVPGraphicCacheLimit = 0;
static tjs_uint TVPGraphicCacheTotalBytes = 0;

static iTJSDispatch2 * TVPMetaInfoPairsToDictionary(
	std::vector<tTVPGraphicMetaInfoPair> *vec)
{
	if (!vec) return NULL;
	std::vector<tTVPGraphicMetaInfoPair>::iterator i;
	iTJSDispatch2 *dic = TJSCreateDictionaryObject();
	try
	{
		for (i = vec->begin(); i != vec->end(); i++)
		{
			tTJSVariant val(i->Value);
			dic->PropSet(TJS_MEMBERENSURE, i->Name.c_str(), 0,
				&val, dic);
		}
	}
	catch (...)
	{
		dic->Release();
		throw;
	}
	return dic;
}

static void TVPCheckGraphicCacheLimit()
{
	while (TVPGraphicCacheTotalBytes > TVPGraphicCacheLimit)
	{
		// chop last graphics
		tTVPGraphicCache::tIterator i;
		i = TVPGraphicCache.GetLast();
		if (!i.IsNull())
		{
			tjs_uint size = i.GetValue().GetObjectNoAddRef()->GetSize();
			TVPGraphicCacheTotalBytes -= size;
			TVPGraphicCache.ChopLast(1);
		} else {
			break;
		}
	}
}
#if 0
class LayerExGraphicLoader {
	enum { clBlack = 0x000000, clWhite = 0xFFFFFF, clWindow = 0x8000005, clWindowText = 0x8000008, clNone = 0x1fffffff };

	int TVPLoadGraphic(iTJSDispatch2 *dest, const ttstr &name, tjs_int32 keyidx,
		tjs_uint desw, tjs_uint desh,
		tTVPGraphicLoadMode mode, ttstr *provincename, iTJSDispatch2 ** metainfo)
	{
		// loading with cache management
		tjs_uint32 hash;
		ttstr nname = TVPNormalizeStorageName(name);
		tTVPGraphicsSearchData searchdata;
		if (/*TVPGraphicCacheEnabled*/1) {
			searchdata.Name = nname;
			searchdata.KeyIdx = keyidx;
			searchdata.Mode = mode;
			searchdata.DesW = desw;
			searchdata.DesH = desh;

			hash = tTVPGraphicCache::MakeHash(searchdata);

			tTVPGraphicImageHolder * ptr =
				TVPGraphicCache.FindAndTouchWithHash(searchdata, hash);
			if (ptr) {
				// found in cache
				if (dest) {
					ptr->GetObjectNoAddRef()->AssignToBitmap(dest);
				}
				if (provincename) *provincename = ptr->GetObjectNoAddRef()->ProvinceName;
				if (metainfo)
					*metainfo = TVPMetaInfoPairsToDictionary(ptr->GetObjectNoAddRef()->MetaInfo);
				return ptr->GetObjectNoAddRef()->GetSize();
			}
		}
		
		// load into dest
		tTVPGraphicImageData * data = NULL;

		ttstr pn;
		std::vector<tTVPGraphicMetaInfoPair> * mi = NULL;
		int ret = 0;
#if 0
		try
		{
			tTVPBitmap *bmp = TVPInternalLoadGraphic(nname, keyidx, desw, desh, &mi, mode, &pn);

			if (provincename) *provincename = pn;
			if (metainfo)
				*metainfo = TVPMetaInfoPairsToDictionary(mi);

			if (/*TVPGraphicCacheEnabled*/1)
			{
				data = new tTVPGraphicImageData();
				data->AssignBitmap(bmp);
				if (dest) {
					data->AssignToBitmap(dest);
				}
				data->ProvinceName = pn;
				data->MetaInfo = mi; // now mi is managed under tTVPGraphicImageData
				mi = NULL;

				// check size limit
				TVPCheckGraphicCacheLimit();

				// push into hash table
				tjs_uint datasize = data->GetSize();
				//			if(datasize < TVPGraphicCacheLimit)
				//			{
				TVPGraphicCacheTotalBytes += datasize;
				tTVPGraphicImageHolder holder(data);
				TVPGraphicCache.AddWithHash(searchdata, hash, holder);
				//			}
			} else if (dest) {
				// if(TVPGetBitmapType() == eTVPTexture) {
				tTVPGraphicImageData data;
				data.AssignBitmap(bmp);
				data.AssignToBitmap(dest);
				//             } else {
				//                 dest->AssignTexture(bmp);
				//             }
			}
			ret = bmp->GetWidth() * bmp->GetHeight() * bmp->GetBPP() / 8;
			bmp->Release();
		}
		catch (...)
		{
			if (mi) delete mi;
			if (data) data->Release();
			throw;
		}

		if (mi) delete mi;
		if (data) data->Release();
#endif
		return ret;
	}

	static iTJSDispatch2 *LoadImages(iTJSDispatch2 *objthis, const ttstr &name, tjs_uint32 colorkey)
	{
		// loads image(s) from specified storage.
		// colorkey must be a color that should be transparent, or:
		// 0x 01 ff ff ff (clAdapt) : the color key will be automatically chosen from
		//                            target image, by selecting most used color from
		//                            the top line of the image.
		// 0x 1f ff ff ff (clNone)  : does not apply the colorkey, or uses image alpha
		//                            channel.
		// 0x30000000 (clPalIdx) + nn ( nn = palette index )
		//                          : select the color key by specified palette index.
		// 0x40000000 (TVP_clAlphaMat) + 0xRRGGBB ( 0xRRGGBB = matting color )
		//                          : do matting with the color using alpha blending.
		// returns graphic image metainfo.

		//if (!MainImage) TVPThrowExceptionMessage(TVPNotDrawableLayerType);

		ttstr provincename;
		iTJSDispatch2 * metainfo = NULL;

		TVPLoadGraphic(MainImage, name, colorkey, 0, 0, glmNormal, &provincename, &metainfo);
		try
		{

			InternalSetImageSize(MainImage->GetWidth(), MainImage->GetHeight());

			if (!provincename.IsEmpty())
			{
				// province image exists
				AllocateProvinceImage();

				try
				{
					TVPLoadGraphicProvince(ProvinceImage, provincename, 0,
						MainImage->GetWidth(), MainImage->GetHeight());

					if (ProvinceImage->GetWidth() != MainImage->GetWidth() ||
						ProvinceImage->GetHeight() != MainImage->GetHeight())
						TVPThrowExceptionMessage(TVPProvinceSizeMismatch, provincename);
				}
				catch (...)
				{
					DeallocateProvinceImage();
					throw;
				}
			} else
			{
				// province image does not exist
				DeallocateProvinceImage();
			}

			ImageModified = true;

			ResetClip();  // cliprect is reset

			Update(false);
		}
		catch (...)
		{
			if (metainfo) metainfo->Release();
			throw;
		}

		return metainfo;
	}
public:
	static tjs_error loadImages(tTJSVariant *result, tjs_int numparams, tTJSVariant **param,iTJSDispatch2 *objthis) {
		if (numparams < 1) return TJS_E_BADPARAMCOUNT;
		ttstr name(*param[0]);
		tjs_uint32 key = clNone;
		if (numparams >= 2 && param[1]->Type() != tvtVoid)
			key = (tjs_uint32)param[1]->AsInteger();
		iTJSDispatch2 * metainfo = LoadImages(objthis, name, key);
		try
		{
			if (result) *result = metainfo;
		}
		catch (...)
		{
			if (metainfo) metainfo->Release();
			throw;
		}
		if (metainfo) metainfo->Release();
		return TJS_S_OK;
	}
};

NCB_ATTACH_CLASS(LayerExGraphicLoader, Layer) {
	NCB_METHOD_RAW_CALLBACK(loadImages, LayerExGraphicLoader::loadImages, 0);
}
#endif