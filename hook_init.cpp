#include "hook_init.h"
#include "GraphicsLoaderIntf.h"

//---------------------------------------------------------------------------
// tTJSBinaryStream
//---------------------------------------------------------------------------

void TJS_INTF_METHOD tTJSBinaryStream::SetEndOfStorage()
{
	TVPThrowExceptionMessage(TJS_W("write error"));
}
//---------------------------------------------------------------------------
tjs_uint64 TJS_INTF_METHOD tTJSBinaryStream::GetSize()
{
	tjs_uint64 orgpos = GetPosition();
	tjs_uint64 size = Seek(0, TJS_BS_SEEK_END);
	Seek(orgpos, SEEK_SET);
	return size;
}
//---------------------------------------------------------------------------
tjs_uint64 tTJSBinaryStream::GetPosition()
{
	return Seek(0, SEEK_CUR);
}
//---------------------------------------------------------------------------
void tTJSBinaryStream::SetPosition(tjs_uint64 pos)
{
	if (pos != Seek(pos, TJS_BS_SEEK_SET))
		TVPThrowExceptionMessage(TJS_W("seek error"));
}
//---------------------------------------------------------------------------
void tTJSBinaryStream::ReadBuffer(void *buffer, tjs_uint read_size)
{
	if (Read(buffer, read_size) != read_size)
		TVPThrowExceptionMessage(TJS_W("read error"));
}
//---------------------------------------------------------------------------
void tTJSBinaryStream::WriteBuffer(const void *buffer, tjs_uint write_size)
{
	if (Write(buffer, write_size) != write_size)
		TVPThrowExceptionMessage(TJS_W("write error"));
}
//---------------------------------------------------------------------------
tjs_uint64 tTJSBinaryStream::ReadI64LE()
{
#if TJS_HOST_IS_BIG_ENDIAN
	tjs_uint8 buffer[8];
	ReadBuffer(buffer, 8);
	tjs_uint64 ret = 0;
	for (tjs_int i = 0; i < 8; i++)
		ret += (tjs_uint64)buffer[i] << (i * 8);
	return ret;
#else
	tjs_uint64 temp;
	ReadBuffer(&temp, 8);
	return temp;
#endif
}
//---------------------------------------------------------------------------
tjs_uint32 tTJSBinaryStream::ReadI32LE()
{
#if TJS_HOST_IS_BIG_ENDIAN
	tjs_uint8 buffer[4];
	ReadBuffer(buffer, 4);
	tjs_uint32 ret = 0;
	for (tjs_int i = 0; i < 4; i++)
		ret += (tjs_uint32)buffer[i] << (i * 8);
	return ret;
#else
	tjs_uint32 temp;
	ReadBuffer(&temp, 4);
	return temp;
#endif
}
//---------------------------------------------------------------------------
tjs_uint16 tTJSBinaryStream::ReadI16LE()
{
#if TJS_HOST_IS_BIG_ENDIAN
	tjs_uint8 buffer[2];
	ReadBuffer(buffer, 2);
	tjs_uint16 ret = 0;
	for (tjs_int i = 0; i < 2; i++)
		ret += (tjs_uint16)buffer[i] << (i * 8);
	return ret;
#else
	tjs_uint16 temp;
	ReadBuffer(&temp, 2);
	return temp;
#endif
}
//---------------------------------------------------------------------------


BYTE* KMPSearch(BYTE* BaseStr, DWORD BaseStrLen, BYTE* SubStr, DWORD SubStrLen)
{
	BYTE P[256];
	P[0] = 0;
	DWORD i, j;
	for (i = 1, j = 0; i < SubStrLen; i++)
	{
		while (j && SubStr[j] != SubStr[i]) j = P[j - 1];
		if (SubStr[j] == SubStr[i]) j++;
		P[i] = j;
	}
	for (i = 0, j = 0; i < BaseStrLen; i++)
	{
		while (j && SubStr[j] != BaseStr[i]) j = P[j - 1];
		if (BaseStr[i] == SubStr[j]) j++;
		if (j == SubStrLen)
			return BaseStr + i - j + 1;
	}
	return NULL;
}
tTJSHashTable<ttstr, tTVPGraphicHandlerType> *TVPGraphicType_Hash = nullptr;
extern void InstallGraphicType(tTJSHashTable<ttstr, tTVPGraphicHandlerType> *TVPGraphicType_Hash);

static PIMAGE_NT_HEADERS32 PEHdr;
static LPBYTE pCodeBase;
static LPBYTE pDataBase;
static LPBYTE pDataEnd;
bool Hooker::init_hook()
{
	HMODULE EXEHDR = GetModuleHandle(NULL);
	PEHdr = (PIMAGE_NT_HEADERS32)((int)EXEHDR + ((IMAGE_DOS_HEADER*)EXEHDR)->e_lfanew);
	pCodeBase = (LPBYTE)EXEHDR + PEHdr->OptionalHeader.BaseOfCode;
	pDataBase = (LPBYTE)EXEHDR + PEHdr->OptionalHeader.BaseOfData;
	pDataEnd = (LPBYTE)EXEHDR + PEHdr->OptionalHeader.SizeOfImage;
	const wchar_t *_p = L"\0_p\0";
	BYTE *p = KMPSearch((LPBYTE)EXEHDR, PEHdr->OptionalHeader.SizeOfImage, (BYTE*)_p, sizeof(wchar_t)* 4);
	if (!p) {
		return hook_fail();
	}
	p += 2;
	BYTE *p_pRef = KMPSearch(pCodeBase, PEHdr->OptionalHeader.SizeOfCode, (BYTE*)&p, sizeof(BYTE *));
	if (!p_pRef) {
		return hook_fail();
	}
	p = p_pRef;
	*(int*)&p &= ~0xF;
	while (p[0] != 0x55 || p[1] != 0x8B || p[2] != 0xEC) { // push ebp; mov ebp, esp
		p -= 16;
		if (p <= pCodeBase) {
			return hook_fail();
		}
	}

	x86_insn_t insn;
	unsigned int buf_len, size, count = 0, bytes = 0;

	/* buf_len is implied by the arguments */
	for (unsigned int bytes = 0; bytes < p_pRef - p; ) {
		size = x86_disasm(p, p_pRef - p, 0, bytes, &insn);
		if (size) {
			count++; bytes += size;
			if (TVPGraphicType_Hash) {
				x86_oplist_free(&insn);
				break;
			}
			if (insn.size < 5) continue;
			if (insn.group != insn_move) continue;
			for (x86_oplist_t *op = insn.operands; op; op = op->next) {
				if (op->op.access != op_read) continue;
				if (op->op.datatype != op_dword) continue;
				if (op->op.type != op_offset) continue;
				BYTE *p = (BYTE *)op->op.data.dword;
				if (p >= pDataEnd || p <= pDataBase) continue;
				unsigned long *pNFirst = (unsigned long *)op->op.data.dword;
				if (pNFirst[-1] != 10) continue;
				// hit TVPGraphicType !
				TVPGraphicType_Hash = (tTJSHashTable<ttstr, tTVPGraphicHandlerType> *)(op->op.data.dword - 0xA04);
				break;
			}
		} else {
			/* error */
			bytes++;        /* try next byte */
		}

		x86_oplist_free(&insn);
	}

	if (!TVPGraphicType_Hash) {
		return hook_fail();
	}

	InstallGraphicType(TVPGraphicType_Hash);

	return true;
}

Hooker::~Hooker()
{
	x86_cleanup();
}

Hooker::Hooker()
{
	req_interrupt = false;
	x86_init(opt_none, err_reporter, this);
}

bool Hooker::hook_fail()
{
	MessageBoxA(NULL, "unsupported kirikiri2 core!", "hook fail", MB_OK);
	return false;
}

void Hooker::err_reporter(enum x86_report_codes code, void *arg, void *junk)
{
	((Hooker*)arg)->req_interrupt = true;
}
