// tar_zip.cpp : 定义 DLL 应用程序的导出函数。
//

#include <windows.h>
#include "tar.h"
#include <string>
#include <vector>
#include <map>
#include "minizip/unzip.h"
#include "minizip/crypt.h"
#include <deque>
#include <unordered_map>

unsigned __int64 parseOctNum(const char *oct, int length)
{
	unsigned __int64 num = 0;
	for (int i = 0; i < length; i++){
		char c = oct[i];
		if ('0' <= c && c <= '9'){
			num = num * 8 + (c - '0');
		}
	}
	return num;
}

typedef wchar_t tjs_char;
typedef std::string ttstr;
typedef FILE tTJSBinaryStream;
typedef unsigned __int64 tjs_uint64;
typedef int tjs_int;    /* at least 32bits */
typedef unsigned int tjs_uint;    /* at least 32bits */

static __int64 FILE_GetSize(FILE* f) {
	fseek(f, 0, SEEK_END);
	__int64 ret = _ftelli64(f);
	fseek(f, 0, SEEK_SET);
	return ret;
}

#pragma pack(push,1)
typedef struct
{
	unsigned char method[8];	/*圧縮法の種類*/
	unsigned long position;		/*ファイル上での位置*/
	unsigned long compsize;		/*圧縮されたサイズ*/
	unsigned long filesize;		/*元のファイルサイズ*/
	long timestamp;		/*ファイルの更新日時*/
	char path[200];			/*相対パス*/
	char filename[200];		/*ファイルネーム*/
	unsigned long crc;		/*CRC*/
} fileInfo;
#pragma pack(pop)

class tArchive {
public:
	std::deque<fileInfo> FileList;
	std::unordered_map<std::string, fileInfo*> FileIdxByName;
	std::string ArchiveName;
	tArchive(const std::string &name) : ArchiveName(name) { }
	virtual fileInfo* GetFileInfo(unsigned long off_or_idx) = 0;
	virtual bool GetFileData(fileInfo* info, void *buf) = 0; // get data into buf
};
extern zlib_filefunc64_def TVPZlibFileFunc;
static bool inline TVPUtf8ToWideChar(const char * & in, tjs_char *out)
{
	// convert a utf-8 charater from 'in' to wide charater 'out'
	const unsigned char * & p = (const unsigned char * &)in;
	if (p[0] < 0x80)
	{
		if (out) *out = (tjs_char)in[0];
		in++;
		return true;
	} else if (p[0] < 0xc2)
	{
		// invalid character
		return false;
	} else if (p[0] < 0xe0)
	{
		// two bytes (11bits)
		if ((p[1] & 0xc0) != 0x80) return false;
		if (out) *out = ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
		in += 2;
		return true;
	} else if (p[0] < 0xf0)
	{
		// three bytes (16bits)
		if ((p[1] & 0xc0) != 0x80) return false;
		if ((p[2] & 0xc0) != 0x80) return false;
		if (out) *out = ((p[0] & 0x1f) << 12) + ((p[1] & 0x3f) << 6) + (p[2] & 0x3f);
		in += 3;
		return true;
	} else if (p[0] < 0xf8)
	{
		// four bytes (21bits)
		if ((p[1] & 0xc0) != 0x80) return false;
		if ((p[2] & 0xc0) != 0x80) return false;
		if ((p[3] & 0xc0) != 0x80) return false;
		if (out) *out = ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12) +
			((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
		in += 4;
		return true;
	} else if (p[0] < 0xfc)
	{
		// five bytes (26bits)
		if ((p[1] & 0xc0) != 0x80) return false;
		if ((p[2] & 0xc0) != 0x80) return false;
		if ((p[3] & 0xc0) != 0x80) return false;
		if ((p[4] & 0xc0) != 0x80) return false;
		if (out) *out = ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18) +
			((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6) + (p[4] & 0x3f);
		in += 5;
		return true;
	} else if (p[0] < 0xfe)
	{
		// six bytes (31bits)
		if ((p[1] & 0xc0) != 0x80) return false;
		if ((p[2] & 0xc0) != 0x80) return false;
		if ((p[3] & 0xc0) != 0x80) return false;
		if ((p[4] & 0xc0) != 0x80) return false;
		if ((p[5] & 0xc0) != 0x80) return false;
		if (out) *out = ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24) +
			((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12) +
			((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
		in += 6;
		return true;
	}
	return false;
}
//---------------------------------------------------------------------------
tjs_int TVPUtf8ToWideCharString(const char * in, tjs_char *out)
{
	// convert input utf-8 string to output wide string
	int count = 0;
	while (*in)
	{
		tjs_char c;
		if (out)
		{
			if (!TVPUtf8ToWideChar(in, &c))
				return -1; // invalid character found
			*out++ = c;
		} else
		{
			if (!TVPUtf8ToWideChar(in, NULL))
				return -1; // invalid character found
		}
		count++;
	}
	return count;
}
//---------------------------------------------------------------------------


static bool storeUTF8Filename(fileInfo &info, const char *narrowName, const ttstr &filename)
{
	// convert input utf-8 string to output wide string
	std::vector<wchar_t> wbuf;
	while (*narrowName) {
		tjs_char c;
		if (!TVPUtf8ToWideChar(narrowName, &c)) {
			// invalid character found
			ttstr msg("Filename is not encoded in UTF8 in archive:\n");
			MessageBoxA(0, (msg + filename).c_str(), "Error", MB_OK | MB_ICONERROR);
			return false;
		}
		if (c >= ('A') && c <= ('Z'))
			c += ('a') - ('A');
		wbuf.push_back(c);
	}
	
	std::vector<char> cbuf; cbuf.resize(wbuf.size() * 2);
	int len = WideCharToMultiByte(CP_ACP, 0, &wbuf[0], wbuf.size(), &cbuf[0], cbuf.size(), nullptr, nullptr);
	if (len == 0) return false;
	narrowName = &cbuf[0];
	info.path[0] = 0;

	strncpy(info.filename, narrowName, sizeof(info.filename));
	info.filename[sizeof(info.filename) - 1] = 0;
	return true;
}
class ZipArchive : public tArchive {
	unzFile uf;
	std::vector<unz64_file_pos> entrylist;

public:
	~ZipArchive() {
		if (uf) {
			unzClose(uf);
			uf = NULL;
		}
	}
	ZipArchive(const ttstr & name) : tArchive(name) {
		if ((uf = unzOpen2_64(name.c_str(), &TVPZlibFileFunc)) != NULL) {
			unzGoToFirstFile(uf);
			unz_file_info file_info;
			do {
				unz64_file_pos entry;
				if (unzGetFilePos64(uf, &entry) == UNZ_OK) {
					char filename_inzip[1024];
					if (unzGetCurrentFileInfo(uf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) == UNZ_OK) {
						fileInfo item;
						if (!storeUTF8Filename(item, filename_inzip, name)) {
							unzClose(uf);
							uf = NULL;
							return;
						}
						if (!item.filename[0]) continue; // skip folder
						item.compsize = file_info.compressed_size;
						item.filesize = file_info.uncompressed_size;
						memcpy(item.method, "zip", 4);
						item.position = FileList.size(); // as index
						item.crc = file_info.crc;
						FileList.push_back(item);
						entrylist.push_back(entry);
					}
				}
			} while (unzGoToNextFile(uf) == UNZ_OK);
			for (auto it = FileList.begin(); it != FileList.end(); ++it) {
				std::string name(it->path); name += "/"; name += it->filename;
				FileIdxByName[name] = &*it;
			}
		}
	}

	bool isValid() { return uf != nullptr; }
	virtual fileInfo* GetFileInfo(unsigned long off_or_idx) {
		if (off_or_idx < FileList.size()) return &FileList[off_or_idx];
		return nullptr;
	}
	virtual bool GetFileData(fileInfo* info, void *buf) {
		if (unzGoToFilePos64(uf, &entrylist[info->position]) != UNZ_OK) return false;
		unz_file_info file_info;
		if (unzGetCurrentFileInfo(uf, &file_info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) return false;
		unzData data;
		if (unzOpenData(uf, &data, nullptr, nullptr, 0, nullptr) != UNZ_OK) return false;
		unzReadData(data, buf, file_info.uncompressed_size);
		unzCloseData(data);
		return true;
	}
};

class TarArchive : public tArchive {
	std::map<unsigned long, fileInfo*> FileIdxByOffset;
	tTJSBinaryStream * _instr;

public:
	TarArchive(const std::string &name) : tArchive(name), _instr(nullptr) {}
	~TarArchive() {
		if (_instr) fclose(_instr);
	}
	bool init() {
		_instr = fopen(ArchiveName.c_str(), "rb");
		if (!_instr) return nullptr;
		tjs_uint64 archiveSize = FILE_GetSize(_instr);
		TAR_HEADER tar_header;
		// check first header
		if (fread(&tar_header, 1, sizeof(tar_header), _instr) != sizeof(tar_header)) {
			return false;
		}
		unsigned int checksum = parseOctNum(tar_header.dbuf.chksum, sizeof(tar_header.dbuf.chksum));
		if (checksum != tar_header.compsum() && (int)checksum != tar_header.compsum_oldtar()) {
			return false;
		}
		_fseeki64(_instr, 0, SEEK_SET);
		while (_ftelli64(_instr) <= archiveSize - sizeof(tar_header)) {
			if (fread(&tar_header, 1, sizeof(tar_header), _instr) != sizeof(tar_header)) break;
			tjs_uint64 original_size = parseOctNum(tar_header.dbuf.size, sizeof(tar_header.dbuf.size));
			std::vector<char> filename;
			if (tar_header.dbuf.typeflag == LONGLINK) {	// tar_header.dbuf.name == "././@LongLink"
				unsigned int readsize = (original_size + (TBLOCK - 1)) & ~(TBLOCK - 1);
				filename.resize(readsize + 1);	//TODO:size lost
				if (fread(&filename[0], 1, readsize, _instr) != readsize) break;
				filename[readsize] = 0;
				if (fread(&tar_header, 1, sizeof(tar_header), _instr) != sizeof(tar_header)) break;
				original_size = parseOctNum(tar_header.dbuf.size, sizeof(tar_header.dbuf.size));
			}
			if (tar_header.dbuf.typeflag != REGTYPE) continue; // only accept regular file
			if (filename.empty()) {
				filename.resize(101);
				memcpy(&filename[0], tar_header.dbuf.name, sizeof(tar_header.dbuf.name));
				filename[100] = 0;
			}

			FileList.emplace_back(fileInfo());
			fileInfo &item = FileList.back();
			if (!storeUTF8Filename(item, &filename[0], ArchiveName)) {
				return false;
			}
			item.compsize = original_size;
			item.filesize = original_size;
			memcpy(item.method, "tar", 4);
			item.position = _ftelli64(_instr) / 16; // support up to 32G archive
			item.crc = 0;
			tjs_uint64 readsize = (original_size + (TBLOCK - 1)) & ~(TBLOCK - 1);
			_fseeki64(_instr, readsize, SEEK_CUR);
		}
		// index file
		for (auto it = FileList.begin(); it != FileList.end(); ++it) {
			std::string name(it->path); name += "/"; name += it->filename;
			FileIdxByName[name] = &*it;
			FileIdxByOffset[it->position] = &*it;
		}
		return true;
	}
	virtual fileInfo* GetFileInfo(unsigned long off_or_idx) {
		auto it = FileIdxByOffset.find(off_or_idx);
		if (it == FileIdxByOffset.end()) return nullptr;
		return it->second;
	}
	virtual bool GetFileData(fileInfo* info, void *buf) {
		_fseeki64(_instr, info->position * 16, SEEK_SET);
		fread(buf, 1, info->filesize, _instr);
		return true;
	}
};

extern "C" {
#include "7zip/7z.h"
#include "7zip/7zFile.h"
#include "7zip/7zCrc.h"
}

class SevenZipArchive : public tArchive {
	CSzArEx db;
	static ISzAlloc allocImp;
	FILE *_stream;
	struct CSeekInStream : public ISeekInStream {
		SevenZipArchive *Host;
	} archiveStream;
	CLookToRead lookStream;
	//std::vector<std::pair<std::string, tjs_uint> > filelist;
	std::vector<unsigned int> _idxTab;

	SRes StreamRead(void *buf, size_t *size) {
		*size = fread(buf, 1, *size, _stream);
		return SZ_OK;
	}
	SRes StreamSeek(Int64 *pos, ESzSeek origin) {
		tjs_int whence = SEEK_SET;
		switch (origin) {
		case SZ_SEEK_CUR: whence = SEEK_CUR; break;
		case SZ_SEEK_END: whence = SEEK_END; break;
		case SZ_SEEK_SET: whence = SEEK_SET; break;
		default: break;
		}

		*pos = _fseeki64(_stream, *pos, whence);
		return SZ_OK;
	}

public:
	SevenZipArchive(const std::string & name) : tArchive(name), _stream(fopen(name.c_str(), "rb")) {
		archiveStream.Host = this;
		archiveStream.Read = [](void *p, void *buf, size_t *size)->SRes{return ((CSeekInStream*)p)->Host->StreamRead(buf, size); };
		archiveStream.Seek = [](void *p, Int64 *pos, ESzSeek origin)->SRes{return ((CSeekInStream*)p)->Host->StreamSeek(pos, origin); };
		LookToRead_CreateVTable(&lookStream, false);
		lookStream.realStream = &archiveStream;
		SzArEx_Init(&db);
		if (!g_CrcTable[1]) CrcGenerateTable();
	}

	virtual ~SevenZipArchive() {
		SzArEx_Free(&db, &allocImp);
		fclose(_stream);
	}

	bool Open() {
		SRes res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocImp);
		if (res != SZ_OK) return false;
		_idxTab.reserve(db.NumFiles);
		for (unsigned int i = 0; i < db.NumFiles; i++) {
			size_t offset = 0;
			size_t outSizeProcessed = 0;
			bool isDir = SzArEx_IsDir(&db, i);
			if (isDir) continue;
			size_t len = SzArEx_GetFileNameUtf16(&db, i, NULL);
			_idxTab.push_back(i);
			FileList.emplace_back(fileInfo());
			fileInfo &info = FileList.back();
			std::vector<wchar_t> wbuf;
			wbuf.resize(len + 1);
			SzArEx_GetFileNameUtf16(&db, i, (UInt16*)&wbuf.front());
			for (wchar_t &c : wbuf) {
				if (c >= ('A') && c <= ('Z'))
					c += ('a') - ('A');
			}
			info.filename[sizeof(info.filename) - 1] = 0;
			if (WideCharToMultiByte(CP_ACP, 0, &wbuf[0], wbuf.size(), info.filename, 200, nullptr, nullptr) == 0)
				return false;
			info.path[0] = 0;
			info.position = i;
			UInt64 fileSize = SzArEx_GetFileSize(&db, i);
			info.filesize = fileSize;
			info.compsize = fileSize;
			info.crc = 0;
			info.method[0] = '7';
			info.method[1] = 'z';
			info.method[2] = '\0';
			info.timestamp = 0;
		}

		return true;
	}

	virtual fileInfo* GetFileInfo(unsigned long off_or_idx) {
		return &FileList[off_or_idx];
	}
	virtual bool GetFileData(fileInfo* info, void *buf) {
		unsigned int fileIndex = _idxTab[info->position];
		UInt64 fileSize = SzArEx_GetFileSize(&db, fileIndex);

		UInt32 folderIndex = db.FileToFolder[fileIndex];
		if (folderIndex == (UInt32)-1) return nullptr;

		const CSzAr *p = &db.db;
		CSzFolder folder;
		CSzData sd;
		const Byte *data = p->CodersData + p->FoCodersOffsets[folderIndex];
		sd.Data = data;
		sd.Size = p->FoCodersOffsets[folderIndex + 1] - p->FoCodersOffsets[folderIndex];

		if (SzGetNextFolderItem(&folder, &sd) != SZ_OK) return nullptr;
		if (folder.NumCoders == 1) {
			UInt64 startPos = db.dataPos;
			const UInt64 *packPositions = p->PackPositions + p->FoStartPackStreamIndex[folderIndex];
			UInt64 offset = packPositions[0];
			UInt64 inSize = packPositions[1] - offset;
#define k_Copy 0
			if (folder.Coders[0].MethodID == k_Copy && inSize == fileSize) {
				_fseeki64(_stream, startPos + offset, SEEK_SET);
				fread(buf, 1, inSize, _stream);
				return true;
			}
		}

		UInt32 blockIndex;
		Byte *outBuffer = nullptr;
		size_t outBufferSize;
		size_t offset, outSizeProcessed;
		SRes res = SzArEx_Extract(&db, &lookStream.s, fileIndex, &blockIndex, &outBuffer, &outBufferSize,
			&offset, &outSizeProcessed, &allocImp, &allocImp);
		delete outBuffer;
		memcpy(buf, outBuffer, outBufferSize);
		return true;
	}
};

ISzAlloc SevenZipArchive::allocImp = {
	[](void *p, size_t size) -> void *{ return malloc(size); },
	[](void *p, void *addr) { free(addr); }
};

static std::map<std::string, tArchive*> s_allArchive;
static std::map<long, std::map<std::string, fileInfo>* > s_allArchiveByOffset;

static tArchive* GetArchive(const std::string &buf) {
	auto it = s_allArchive.find(buf);
	if (it != s_allArchive.end()) return it->second;

	char header[8];
	FILE *fp = fopen(buf.c_str(), "rb");
	if (!fp) return nullptr;
	if (fread(header, 1, 8, fp) != 8) {
		fclose(fp);
		return nullptr;
	}
	fclose(fp);
	tArchive *pArc = nullptr;
	if (header[0] == 'P' && header[1] == 'K') {
		ZipArchive *p = new ZipArchive(buf);
		if (p->isValid()) {
			pArc = p;
		} else {
			delete p;
		}
	}
	if (!pArc && header[0] == '7' && header[1] == 'z') {
		SevenZipArchive *p = new SevenZipArchive(buf);
		if (p->Open()) {
			pArc = p;
		} else {
			delete pArc;
		}
	}
	if (!pArc) {
		TarArchive *pTar = new TarArchive(buf);
		if (pTar->init()) {
			pArc = pTar;
		} else {
			delete pTar;
		}
	}

	if (pArc) {
		s_allArchive[pArc->ArchiveName] = pArc;
	}

	return pArc;
}

extern "C" int WINAPI GetArchiveInfo(LPSTR buf, long len, unsigned int flag, HLOCAL *lphInf)
{
	int nCnt;
	fileInfo *pInf;
	HLOCAL hInf;

	if (0 != (flag & 0x0007)) {
		// FileImage Pointer not supported.
		return -1;
	}
	tArchive *arc = GetArchive(buf);
	if (!arc) return -1;

	nCnt = arc->FileList.size();
	if (NULL == (hInf = LocalAlloc(LHND, (nCnt + 1)*sizeof(fileInfo)))) {
		return 4;
	}
	if (NULL == (pInf = (fileInfo*)LocalLock(hInf))) {
		return 5;
	}
	std::copy(arc->FileList.begin(), arc->FileList.end(), pInf);
	pInf[nCnt].method[0] = 0;
	pInf[nCnt].filename[0] = 0;
	pInf[nCnt].path[0] = 0;
	pInf[nCnt].filesize = 0;
	pInf[nCnt].compsize = 0;
	pInf[nCnt].position = 0;

	LocalUnlock(hInf);
	*lphInf = hInf;

	return 0;
}

extern "C" int WINAPI GetFileInfo(LPSTR buf, long len, LPSTR filename, unsigned int flag, fileInfo *lpInfo)
{
	char szName[512];

	strcpy(szName, filename);
	char *p = &szName[0];
	while (*p)
	{
		if (*p >= ('A') && *p <= ('Z'))
			*p += ('a') - ('A');
		p++;
	}

	tArchive *arc = GetArchive(buf);
	if (!arc) return 8;
	auto it = arc->FileIdxByName.find(szName);
	if (it == arc->FileIdxByName.end()) return 8;
	*lpInfo = *it->second;
	return 0;
}

extern "C" int WINAPI GetFile(LPSTR src, long len, LPSTR dest, unsigned int flag, FARPROC prgressCallback, long lData)
{
	if (0 != (flag & 0x0007)) {
		return -1;						// input must be file
	}

	tArchive *arc = GetArchive(src);
	if (!arc) return 8;
	fileInfo *info = arc->GetFileInfo(len);
	if (!info) return 8;

	if (0x0000 == (flag & 0x0700)) {			// output is file
		return 0;
	} else if (0x0100 == (flag & 0x0700)) {	// output is memory
		HANDLE hBuf;
		void *pBuf;
		if (NULL == (hBuf = LocalAlloc(LHND, info->filesize + 1))) {
			return 4;
		} else {
			if (NULL == (pBuf = LocalLock(hBuf))) {
				LocalFree(hBuf);
				return 5;
			} else if (arc->GetFileData(info, pBuf)){
				LocalUnlock(hBuf);
				*((HANDLE*)dest) = hBuf;
				return 0;
			} else {
				LocalUnlock(hBuf);
				LocalFree(hBuf);
				return 8;
			}
		}
	} else {
		return -1;
	}

	return 0;
}
