#include <windows.h>
#include "kr2ext.h"
#include "tp_stub.h"
#include "hook_init.h"

//#pragma comment(linker, "/align:512")
#ifndef _DEBUG
//#pragma comment(linker, "/merge:.data=.text")
#pragma comment(linker, "/merge:.rdata=.text")
#endif

//---------------------------------------------------------------------------
void TVP_tTVPXP3ArchiveExtractionFilter_CONVENTION
TVPXP3ArchiveExtractionFilter(tTVPXP3ExtractionFilterInfo *info)
{
	if (info->SizeOfSelf != sizeof(tTVPXP3ExtractionFilterInfo))
		TVPThrowExceptionMessage(TJS_W("Incompatible tTVPXP3ExtractionFilterInfo size"));

	DWORD hash = info->FileHash;
	unsigned char key[] = {
		(hash >> 8) & 0xFF,
		(hash >> 8) & 0xFF,
		(hash >> 1) & 0xFF,
		(hash >> 7) & 0xFF,
		(hash >> 5) & 0xFF};
	DWORD buffsize = info->BufferSize;
	unsigned char *buf = (unsigned char*)info->Buffer;
	for (int i = 0; i < buffsize; ++i) {
		unsigned int entryoff = info->Offset + i;
		if (entryoff <= 100) {
			buf[i] ^= hash >> 5;
		} else {
			buf[i] ^= key[entryoff & 4];
		}
	}
	return;
// 	union {
// 		DWORD dword;
// 		BYTE  byte[4];
// 	} key;
// 	DWORD hash = info->FileHash;
// 	key.byte[0]= hash>>5;
// 	key.byte[1]= hash>>4;
// 	key.byte[2]= hash>>3;
// 	key.byte[3]= hash>>8;
// 	DWORD offset = info->Offset, BufferSize = info->BufferSize;
// 	LPBYTE buffer= (LPBYTE)info->Buffer;
// 	for(int n = offset;n < offset + BufferSize; n++, buffer++) {
// // 		if(n<=100) *buffer++^=key.byte[2];
// // 		else       *buffer++^=key.byte[n&3];
// 		if(n>4) *buffer^=hash>>12;
//     }
}
extern void RegistXP3Filter();
extern void RegistGraphicLoader();
//---------------------------------------------------------------------------
#pragma comment(linker, "/EXPORT:V2Link=_V2Link@4,PRIVATE")
extern "C" HRESULT _stdcall V2Link(iTVPFunctionExporter *exporter)
{
	TVPInitImportStub(exporter);

 	Hooker hooker;
 	hooker.init_hook();
	RegistGraphicLoader();

	ttstr patch = TVPGetAppPath() + "patch.tjs";
	if (TVPIsExistentStorageNoSearch(patch))
		TVPExecuteStorage(patch);

	//RegistXP3Filter();

	return S_OK;
}
//---------------------------------------------------------------------------
#pragma comment(linker, "/EXPORT:V2Unlink=_V2Unlink@0,PRIVATE")
extern "C" HRESULT _stdcall V2Unlink()
{
	//TVPSetXP3ArchiveExtractionFilter(NULL);

	TVPUninitImportStub();

	return S_OK;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}
