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
#include "hook_init.h"


class tMediaRecord
{
public:
	class tHashFunc
	{
	public:
		static tjs_uint32 Make(const ttstr &key)
		{
			if (key.IsEmpty()) return 0;
			const tjs_char *str = key.c_str();
			tjs_uint32 ret = 0;
			while (*str && *str != ':')
			{
				ret += *str;
				ret += (ret << 10);
				ret ^= (ret >> 6);
				str++;
			}
			ret += (ret << 3);
			ret ^= (ret >> 11);
			ret += (ret << 15);
			if (!ret) ret = (tjs_uint32)-1;
			return ret;
		}
	};
	ttstr CurrentDomain;
	ttstr CurrentPath;
	tTJSRefHolder<iTVPStorageMedia> MediaIntf;
	tjs_int MediaNameLen;
	//		bool IsCaseSensitive;

	tMediaRecord(iTVPStorageMedia *media) : MediaIntf(media), CurrentDomain("."), CurrentPath("/")
	{
		ttstr name;
		media->GetName(name); MediaNameLen = name.GetLen();
		/*IsCaseSensitive = media->IsCaseSensitive();*/
	}

	const tjs_char *GetDomainAndPath(const ttstr &name)
	{
		return name.c_str() + MediaNameLen + 3;
		// 3 = strlen("://")
	}
};

typedef tTJSHashTable<ttstr, tMediaRecord, tMediaRecord::tHashFunc, 16> MediaStorageHashTable;

//---------------------------------------------------------------------------
// tTJSNC_Storages : TJS Storages class
//---------------------------------------------------------------------------
// class tTJSNC_Storages : public tTJSNativeClass
// {
// 	typedef tTJSNativeClass inherited;
// 
// public:
// 	tTJSNC_Storages();
// 	static tjs_uint32 ClassID;
// 
// protected:
// 	tTJSNativeInstance *CreateNativeInstance();
// };
// //---------------------------------------------------------------------------
// extern tTJSNativeClass * TVPCreateNativeClass_Storages();
//---------------------------------------------------------------------------



#endif
