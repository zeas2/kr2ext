//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Universal Storage System
//---------------------------------------------------------------------------
#include "tp_stub.h"
#include "StorageIntf.h"
#include "hook_init.h"

class tTVPFileMedia : public iTVPStorageMedia
{
	tjs_uint RefCount;
	iTVPStorageMedia *FileMedia;

public:
	tTVPFileMedia(iTVPStorageMedia* media) {
		media->AddRef();
		FileMedia = media;
		RefCount = 1;
	}
	~tTVPFileMedia() {
		FileMedia->Release();
	}

	void TJS_INTF_METHOD AddRef() { RefCount++; }
	void TJS_INTF_METHOD Release()
	{
		if (RefCount == 1)
			delete this;
		else
			RefCount--;
	}

	void TJS_INTF_METHOD GetName(ttstr &name) { name = TJS_W("file"); }

	void TJS_INTF_METHOD NormalizeDomainName(ttstr &name) {
		FileMedia->NormalizeDomainName(name);
	}
	void TJS_INTF_METHOD NormalizePathName(ttstr &name) {
		FileMedia->NormalizePathName(name);
	}
	bool TJS_INTF_METHOD CheckExistentStorage(const ttstr &name) {
		return FileMedia->CheckExistentStorage(name);
	}
	tTJSBinaryStream * TJS_INTF_METHOD Open(const ttstr & name, tjs_uint32 flags);
	void TJS_INTF_METHOD GetListAt(const ttstr &name, iTVPStorageLister *lister) {
		return FileMedia->GetListAt(name, lister);
	}
	void TJS_INTF_METHOD GetLocallyAccessibleName(ttstr &name) {
		return FileMedia->GetLocallyAccessibleName(name);
	}
	bool TJS_INTF_METHOD GetFileSystemChanged() {
		return FileMedia->GetFileSystemChanged();
	}
};

//---------------------------------------------------------------------------
tTJSBinaryStream * TJS_INTF_METHOD tTVPFileMedia::Open(const ttstr & name, tjs_uint32 flags)
{
	// open storage named "name".
	// currently only local/network(by OS) storage systems are supported.
// 	if (name.IsEmpty())
// 		TVPThrowExceptionMessage(TVPCannotOpenStorage, TJS_W("\"\""));
// 
// 	ttstr origname = name;
// 	ttstr _name(name);
// 	GetLocalName(_name);
// 
// 	return new tTVPLocalFileStream(origname, _name, flags);
	return nullptr;
}

void InstallStorageMedia(MediaStorageHashTable* pTable) {
	iTVPStorageMedia* pMedia = pTable->GetFirst().GetValue().MediaIntf.GetObjectNoAddRef();
	ttstr name;
	pMedia->GetName(name);
	tTVPFileMedia *filemedia = new tTVPFileMedia(pMedia);
	TVPUnregisterStorageMedia(pMedia);
	TVPRegisterStorageMedia(filemedia);
}

