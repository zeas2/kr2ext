//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Universal Storage System
//---------------------------------------------------------------------------
#include <cderr.h>
#include <objbase.h>
#include <time.h>
#include <string>
#include "StorageIntf.h"

//---------------------------------------------------------------------------
// TVPSearchCD
//---------------------------------------------------------------------------
ttstr TVPSearchCD(const ttstr & name)
{
	// search CD which has specified volume label name.
	// return drive letter ( such as 'A' or 'B' )
	// return empty string if not found.

	wchar_t dr[4];
	for(dr[0]=L'A',dr[1]=L':',dr[2]=L'\\',dr[3]=0;dr[0]<=L'Z';dr[0]++)
	{
		if(::GetDriveType(dr) == DRIVE_CDROM)
		{
			wchar_t vlabel[256];
			wchar_t fs[256];
			DWORD mcl = 0,sfs = 0;
			GetVolumeInformation(dr, vlabel, 255, NULL, &mcl, &sfs, fs, 255);
			if (wcsicmp(vlabel, name.c_str()) == 0)
			//if(std::string(vlabel).AnsiCompareIC(narrow_name)==0)
				return ttstr((tjs_char)dr[0]);
		}
	}
	return ttstr();
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
// TVPCreateNativeClass_Storages
//---------------------------------------------------------------------------
tTJSNativeClass * TVPCreateNativeClass_Storages()
{
	tTJSNC_Storages *cls = new tTJSNC_Storages();


	// setup some platform-specific members
//----------------------------------------------------------------------

//-- methods

//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/searchCD)
{
	if(numparams < 1) return TJS_E_BADPARAMCOUNT;

	if(result)
		*result = TVPSearchCD(*param[0]);

	return TJS_S_OK;
}
TJS_END_NATIVE_STATIC_METHOD_DECL_OUTER(/*object to register*/cls,
	/*func. name*/searchCD)
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/getLocalName)
{
	if(numparams < 1) return TJS_E_BADPARAMCOUNT;

	if(result)
	{
		ttstr str(TVPNormalizeStorageName(*param[0]));
		TVPGetLocalName(str);
		*result = str;
	}

	return TJS_S_OK;
}
TJS_END_NATIVE_STATIC_METHOD_DECL_OUTER(/*object to register*/cls,
	/*func. name*/getLocalName)
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/selectFile)
{
	if(numparams < 1) return TJS_E_BADPARAMCOUNT;

	iTJSDispatch2 * dsp =  param[0]->AsObjectNoAddRef();

	bool res = TVPSelectFile(dsp);

	if(result) *result = (tjs_int)res;

	return TJS_S_OK;
}
TJS_END_NATIVE_STATIC_METHOD_DECL_OUTER(/*object to register*/cls,
	/*func. name*/selectFile)
//----------------------------------------------------------------------


	return cls;

}
//---------------------------------------------------------------------------

