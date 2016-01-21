//---------------------------------------------------------------------------
// Ogg Opus plugin for TSS ( stands for TVP Sound System )
// This is released under TVP(Kirikiri)'s license.
// See details for TVP source distribution.
//---------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "opusfile/include/opusfile.h"

#ifndef NOT_HAVE_TP_STUB
#include "tp_stub.h"
#endif
#pragma hdrstop

#include "tvpsnd.h" // TSS sound system interface definitions

static bool FloatExtraction = false; // true if output format is IEEE 32-bit float

#ifndef NOT_HAVE_TP_STUB
static bool TVPAlive = false; // true if the DLL is called from TVP
#endif

//---------------------------------------------------------------------------
int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	switch(reason){
	case DLL_PROCESS_ATTACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	case DLL_PROCESS_DETACH: break;
	}

	return TRUE;
}
//---------------------------------------------------------------------------
ITSSStorageProvider *StorageProvider = NULL;
//---------------------------------------------------------------------------
class OpusModule : public ITSSModule // module interface
{
	ULONG RefCount; // reference count

public:
	OpusModule();
	virtual ~OpusModule();

public:
	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
	
	// ITSSModule
	STDMETHODIMP GetModuleCopyright(LPWSTR buffer, unsigned long buflen );
	STDMETHODIMP GetModuleDescription(LPWSTR buffer, unsigned long buflen );
	STDMETHODIMP GetSupportExts(unsigned long index, LPWSTR mediashortname, LPWSTR buf, unsigned long buflen );
	STDMETHODIMP GetMediaInfo(LPWSTR url, ITSSMediaBaseInfo ** info );
	STDMETHODIMP GetMediaSupport(LPWSTR url );
	STDMETHODIMP GetMediaInstance(LPWSTR url, IUnknown ** instance );
};
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
class OpusWaveDecoder : public ITSSWaveDecoder // decoder interface
{
	ULONG RefCount; // refernce count
	OggOpusFile *InputFile; // OggVorbis_File instance
	IStream *InputStream; // input stream
	TSSWaveFormat Format; // output PCM format
	
public:
	OpusWaveDecoder();
	virtual ~OpusWaveDecoder();

public:
	public:
	// IUnkown
	STDMETHODIMP QueryInterface(REFIID iid, void ** ppvObject);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);

	// ITSSWaveDecoder
	STDMETHODIMP GetFormat(TSSWaveFormat *format);
	STDMETHODIMP Render(void *buf, unsigned long bufsamplelen, unsigned long *rendered, unsigned long *status);
	STDMETHODIMP SetPosition(unsigned __int64 samplepos);

	// others
	HRESULT SetStream(IStream *stream, LPWSTR url);

private:
	static int read_func(void *_stream,unsigned char *_ptr,int _nbytes);
	static int seek_func(void *_stream,opus_int64 _offset,int _whence);
	static opus_int64 tell_func(void *_stream);
	static int close_func(void *_stream);
};
//---------------------------------------------------------------------------
// OpusModule implementation ################################################
//---------------------------------------------------------------------------
OpusModule::OpusModule() : RefCount(1)
{
	// VorbisModule constructor
}
//---------------------------------------------------------------------------
OpusModule::~OpusModule()
{
	// VorbisModule destructor
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::QueryInterface(REFIID iid, void ** ppvObject)
{
	// IUnknown::QueryInterface

	if(ppvObject == NULL) return E_INVALIDARG;

	*ppvObject = NULL;
	if(IsEqualIID(iid,IID_IUnknown)){ *ppvObject = static_cast<IUnknown *>(this); }
	else if(IsEqualIID(iid,IID_ITSSModule)){ *ppvObject = static_cast<ITSSModule * >(this); }

	if(*ppvObject != NULL){ AddRef(); return S_OK; }
	return E_NOINTERFACE;
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) OpusModule::AddRef()
{
	return InterlockedIncrement(&RefCount);
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) OpusModule::Release()
{
	long Ref = InterlockedDecrement(&RefCount);
	if(Ref <= 0){ if(Ref != 0){ InterlockedExchange(&RefCount,0); } delete this; return 0; }
	return Ref;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetModuleCopyright(LPWSTR buffer, unsigned long buflen)
{
	// return module copyright information
	wcscpy_s(buffer,buflen,L"Opus Plug-in for TVP Sound System (C) 2014 Shiku Otomiya <shiku.otomiya@gmail.com>");
	return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetModuleDescription(LPWSTR buffer, unsigned long buflen )
{
	// return module description
	wcscpy_s(buffer,buflen,L"Opus (*.opus) decoder");
	return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetSupportExts(unsigned long index, LPWSTR mediashortname, LPWSTR buf, unsigned long buflen )
{
	// return supported file extensios
	switch(index){
	case 0: wcscpy(mediashortname,L"Opus Stream Format"); wcscpy_s(buf,buflen,L".opus"); break;
	case 1: wcscpy(mediashortname,L"Ogg Audio Stream Format"); wcscpy_s(buf,buflen,L".oga"); break;
	default: return S_FALSE;
	}
	return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetMediaInfo(LPWSTR url, ITSSMediaBaseInfo ** info )
{
	// return media information interface
	return E_NOTIMPL; // not implemented
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetMediaSupport(LPWSTR url )
{
	// return media support interface
	return E_NOTIMPL; // not implemented
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusModule::GetMediaInstance(LPWSTR url, IUnknown ** instance )
{
	// create a new media interface
	HRESULT hr;

	// retrieve input stream interface
	IStream *stream;
	hr = StorageProvider->GetStreamForRead(url, (IUnknown **)&stream);
	if(FAILED(hr)) return hr;

	// create vorbis decoder
	OpusWaveDecoder *decoder = new OpusWaveDecoder();
	hr = decoder->SetStream(stream, url);
	if(FAILED(hr))
	{
		// error; stream may not be a vorbis stream
		delete decoder;
		stream->Release();
		return hr;
	}

	*instance = static_cast<IUnknown *>(decoder); // return as IUnknown
	stream->Release(); // release stream because the decoder already holds it

	return S_OK;
}
//---------------------------------------------------------------------------
// OpusWaveDecoder implementation ###########################################
//---------------------------------------------------------------------------
OpusWaveDecoder::OpusWaveDecoder() : RefCount(1), InputFile(NULL), InputStream(NULL)
{
	// OpusWaveDecoder constructor
}
//---------------------------------------------------------------------------
OpusWaveDecoder::~OpusWaveDecoder()
{
	// OpusWaveDecoder destructor
	if(InputFile != NULL)
	{
		op_free(InputFile);
		InputFile = NULL;
	}
	if(InputStream != NULL)
	{
		InputStream->Release();
		InputStream = NULL;
	}
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusWaveDecoder::QueryInterface(REFIID iid, void ** ppvObject)
{
	// IUnknown::QueryInterface

	if(ppvObject == NULL) return E_INVALIDARG;

	*ppvObject = NULL;
	if(IsEqualIID(iid,IID_IUnknown)){ *ppvObject = static_cast<IUnknown *>(this); }
	else if(IsEqualIID(iid,IID_ITSSWaveDecoder)){ *ppvObject = static_cast<ITSSWaveDecoder * >(this); }

	if(*ppvObject != NULL){ AddRef(); return S_OK; }
	return E_NOINTERFACE;
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) OpusWaveDecoder::AddRef()
{
	return InterlockedIncrement(&RefCount);
}
//---------------------------------------------------------------------------
STDMETHODIMP_(ULONG) OpusWaveDecoder::Release()
{
	long Ref = InterlockedDecrement(&RefCount);
	if(Ref <= 0){ if(Ref != 0){ InterlockedExchange(&RefCount,0); } delete this; return 0; }
	return Ref;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusWaveDecoder::GetFormat(TSSWaveFormat *format)
{
	// return PCM format
	if(InputFile == NULL) return E_FAIL;
	
	*format = Format;
	return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusWaveDecoder::Render(void *buf, unsigned long bufsamplelen, unsigned long *rendered, unsigned long *status)
{
	// render output PCM
	if(InputFile == NULL) return E_FAIL; // InputFile is yet not inited

	int res,pos = 0,remain = bufsamplelen * Format.dwChannels;
	while(remain > 0){
		if(FloatExtraction){
			res = op_read_float(InputFile,(float *)buf + pos,remain,NULL);
				// decode via ov_read (IEEE 32-bit float)
		}
		else{
			res = op_read(InputFile,(opus_int16 *)buf + pos,remain,NULL);
				// decode via ov_read (16bit integer)
		}
		if(res == 0) break;
		res *= Format.dwChannels; pos += res; remain -= res;
	}

	pos /= Format.dwChannels;
	if(status != NULL){
		*status = ((unsigned int)pos < bufsamplelen) ? 0 : 1;
			// *status will be 0 if the decoding is ended
	}
	if(rendered != NULL){
		*rendered = pos;
			// return renderd PCM samples
	}
	return S_OK;
}
//---------------------------------------------------------------------------
STDMETHODIMP OpusWaveDecoder::SetPosition(unsigned __int64 samplepos)
{
	// set PCM position (seek)
	if(InputFile == NULL) return E_FAIL;

	if(op_pcm_seek(InputFile,samplepos) != 0){ return E_FAIL; }
	return S_OK;
}
//---------------------------------------------------------------------------
HRESULT OpusWaveDecoder::SetStream(IStream *stream, LPWSTR url)
{
	// set input stream
	InputStream = stream; InputStream->AddRef(); // add-ref

	OpusFileCallbacks callbacks = {read_func, seek_func, tell_func, close_func};
		// callback functions

	// open input stream via op_open_callbacks
	int err;
	InputFile = op_open_callbacks(this,&callbacks,NULL,0,&err);
	if(InputFile == NULL || err < 0)
	{
		// error!
		InputStream->Release();
		InputStream = NULL;
		return E_FAIL;
	}
	
	// retrieve PCM information
	const OpusHead *head = op_head(InputFile, -1);
	if(head == NULL){ return E_FAIL; }

	// set Format up
	ZeroMemory(&Format, sizeof(Format));
	Format.dwSamplesPerSec = 48000;
	Format.dwChannels = head->channel_count;
	Format.dwBitsPerSample = FloatExtraction ? (0x10000 + 32) : 16;
	Format.dwSeekable = 2;

	__int64 pcmtotal = op_pcm_total(InputFile, -1); // PCM total samples
	if(pcmtotal < 0) pcmtotal = 0;
	Format.ui64TotalSamples = pcmtotal;
	Format.dwTotalTime = (unsigned long)(pcmtotal / (48000 / 1000));

	return S_OK;
}
//---------------------------------------------------------------------------
int _cdecl OpusWaveDecoder::read_func(void *_stream,unsigned char *_ptr,int _nbytes)
{
	// read function (wrapper for IStream)
	OpusWaveDecoder *decoder = (OpusWaveDecoder *)_stream;
	if(decoder->InputStream == NULL) return 0;

	ULONG bytesread;
	if(FAILED(decoder->InputStream->Read(_ptr, _nbytes, &bytesread))) return -1;
	return bytesread;
}
//---------------------------------------------------------------------------
int OpusWaveDecoder::seek_func(void *_stream,opus_int64 _offset,int _whence)
{
	// seek function (wrapper for IStream)
	OpusWaveDecoder *decoder = (OpusWaveDecoder *)_stream;
	if(decoder->InputStream == NULL) return -1;

	LARGE_INTEGER newpos; ULARGE_INTEGER result; int seek_type; newpos.QuadPart = _offset;
	
	switch(_whence){
	case SEEK_SET: seek_type = STREAM_SEEK_SET; break;
	case SEEK_CUR: seek_type = STREAM_SEEK_CUR; break;
	case SEEK_END: seek_type = STREAM_SEEK_END; break;
	default: return -1;
	}
	if(FAILED(decoder->InputStream->Seek(newpos, seek_type, &result))) return -1;
	return 0;
}
//---------------------------------------------------------------------------
opus_int64 OpusWaveDecoder::tell_func(void *_stream)
{
	// tell function (wrapper for IStream)
	OpusWaveDecoder *decoder = (OpusWaveDecoder *)_stream;
	if(decoder->InputStream == NULL) return EOF;

	LARGE_INTEGER newpos; ULARGE_INTEGER result; newpos.QuadPart = 0;
	if(FAILED(decoder->InputStream->Seek(newpos, STREAM_SEEK_CUR, &result))) return EOF;
	return result.QuadPart;
}
//---------------------------------------------------------------------------
int OpusWaveDecoder::close_func(void *_stream)
{
	// close function (wrapper for IStream)
	OpusWaveDecoder *decoder = (OpusWaveDecoder *)_stream;
	if(decoder->InputStream == NULL) return EOF;
	
	decoder->InputStream->Release();
	decoder->InputStream = NULL;

	return 0;
}
//---------------------------------------------------------------------------
// ##########################################################################
//---------------------------------------------------------------------------
extern "C" __declspec(dllexport) STDMETHODIMP GetModuleInstance(ITSSModule **out, ITSSStorageProvider *provider, IStream * config, HWND mainwin)
{
	// GetModuleInstance function (exported)
	StorageProvider = provider;
	*out = new OpusModule();
	return S_OK;
}
//---------------------------------------------------------------------------
extern "C" __declspec(dllexport) STDMETHODIMP V2Link(iTVPFunctionExporter *exporter)
{

#ifndef NOT_HAVE_TP_STUB
	// exported function, only called by kirikiri ver 2+
	TVPAlive = true;
	TVPInitImportStub(exporter);

	tTJSVariant val;
	if(TVPGetCommandLine(TJS_W("-opus_pcm_format"), &val))
	{
		ttstr sval(val);
		if(sval == TJS_W("f32"))
		{
			FloatExtraction = true;
			TVPAddLog(TJS_W("wuopus: IEEE 32bit float output enabled."));
		}
	}
#endif

	return S_OK;
}
//---------------------------------------------------------------------------
extern "C" __declspec(dllexport) STDMETHODIMP V2Unlink(void)
{
#ifndef NOT_HAVE_TP_STUB
	TVPUninitImportStub();
#endif
	return S_OK;
}
//---------------------------------------------------------------------------
extern "C" __declspec(dllexport) STDMETHODIMP_(const wchar_t *) GetOptionDesc(void)
{
	if(GetACP() == 932) // 932 == Japan
	{
		return 
L"Opusデコーダ:出力形式;Opus デコーダが出力する PCM 形式の設定です。|"
L"opus_pcm_format|select,*i16;16bit 整数 PCM,f32;32bit 浮動小数点数 PCM\n";
	}
	else
	{
		return
L"Opus decoder:Output format;Specify PCM format that the Opus decoder outputs.|"
L"opus_pcm_format|select,*i16;16bit integer PCM,f32;32bit floating-point PCM\n";
	}
}
