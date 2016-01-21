//---------------------------------------------------------------------------
/*
	TVP2 ( T Visual Presenter 2 )  A script authoring tool
	Copyright (C) 2000 W.Dee <dee@kikyou.info> and contributors

	See details of license at "license.txt"
*/
//---------------------------------------------------------------------------
// Universal Storage System
//---------------------------------------------------------------------------
#ifndef StorageIntfH
#define StorageIntfH

#include "tp_stub.h"
#include <vector>





//---------------------------------------------------------------------------
// tTJSNC_Storages : TJS Storages class
//---------------------------------------------------------------------------
class tTJSNC_Storages : public tTJSNativeClass
{
	typedef tTJSNativeClass inherited;

public:
	tTJSNC_Storages();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance();
};
//---------------------------------------------------------------------------
extern tTJSNativeClass * TVPCreateNativeClass_Storages();
//---------------------------------------------------------------------------



#endif
