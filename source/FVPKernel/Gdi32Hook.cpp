#include "Gdi32Hook.h"
#include "SectionProtector.h"

ULONG(NTAPI *GdiGetCodePage)(HDC NewDC);


BOOL IsSystemCall(PVOID Routine)
{
	PBYTE   Buffer;
	BOOL    HasMovEax;
	BOOL    HasCall;
	BOOL    HasRet;

	Buffer = (PBYTE)Routine;
	HasMovEax = FALSE;
	HasCall = FALSE;
	HasRet = FALSE;

	for (ULONG_PTR Count = 6; Count != 0; --Count)
	{
		switch (Buffer[0])
		{
		case 0xFF:
			if (Buffer[1] == 0x15 || Buffer[1] == 0x25)
				return FALSE;

			goto CHECK_CALL;


		case 0xB8:
			if ((HasMovEax | HasCall | HasRet) != FALSE)
				return FALSE;

			HasMovEax = TRUE;
			break;

		case 0x64:
			if (*(PUSHORT)&Buffer[1] != 0x15FF)
				break;

		case CALL:
		CHECK_CALL :
			if (HasMovEax == FALSE)
				return FALSE;

				   if ((HasCall | HasRet) != FALSE)
					   return FALSE;

				   HasCall = TRUE;
				   break;

		case 0xC2:
		case 0xC3:
			if (HasRet != FALSE)
				return FALSE;

			if ((HasMovEax & HasCall) == FALSE)
				return FALSE;

			HasRet = TRUE;
			Count = 1;
			continue;
		}

		Buffer += GetOpCodeSize(Buffer);
	}

	return HasMovEax & HasCall & HasRet;
}

PWSTR MByteToWChar(PSTR AnsiString, ULONG_PTR Length = -1)
{
	PWSTR Unicode;

	if (Length == -1)
		Length = StrLengthA(AnsiString);

	++Length;

	Unicode = (PWSTR)AllocateMemoryP(Length * sizeof(WCHAR));
	if (Unicode == nullptr)
		return nullptr;

	RtlMultiByteToUnicodeN(Unicode, Length * sizeof(WCHAR), nullptr, AnsiString, Length);

	return Unicode;
}


VOID FreeString(PVOID String)
{
	FreeMemoryP(String);
}

PSTR WCharToMByte(PWSTR Unicode, ULONG_PTR Length)
{
	PSTR AnsiString;

	if (Length == -1)
		Length = StrLengthW(Unicode);

	++Length;
	Length *= sizeof(WCHAR);

	AnsiString = (PSTR)AllocateMemoryP(Length);
	if (AnsiString == nullptr)
		return nullptr;

	RtlUnicodeToMultiByteN(AnsiString, Length, nullptr, Unicode, Length);

	return AnsiString;
}


HFONT GetFontFromFont(PHookKernel GlobalData, HFONT Font)
{
	LOGFONTW LogFont;

	if (GetObjectW(Font, sizeof(LogFont), &LogFont) == 0)
		return nullptr;

	LogFont.lfCharSet = SHIFTJIS_CHARSET;
	Font = CreateFontIndirectW(&LogFont);

	return Font;
}

HFONT GetFontFromDC(PHookKernel GlobalData, HDC hDC)
{
	HFONT       Font;
	LOGFONTW    LogFont;

	Font = (HFONT)GetCurrentObject(hDC, OBJ_FONT);
	if (Font == nullptr)
		return nullptr;

	return GetFontFromFont(GlobalData, Font);
}

BOOL IsGdiHookBypassed()
{
	return FindThreadFrame(GDI_HOOK_BYPASS) != nullptr;
}


struct TEB_ACTIVE_FRAME_CXX : TEB_ACTIVE_FRAME
{
	BOOL NeedPop;

	TEB_ACTIVE_FRAME_CXX(ULONG Context = 0)
	{
		NeedPop = FALSE;
		this->Flags = Context;
		this->Previous = nullptr;
		this->Context = 0;
	}

	~TEB_ACTIVE_FRAME_CXX()
	{
		if (NeedPop)
			Pop();
	}

	inline VOID Push()
	{
		NeedPop = TRUE;
		RtlPushFrame(this);
	}

	inline VOID Pop()
	{
		NeedPop = FALSE;
		RtlPopFrame(this);
	}
};


HFONT CreateFontIndirectBypassW(CONST LOGFONTW *lplf)
{
	TEB_ACTIVE_FRAME_CXX Bypass(GDI_HOOK_BYPASS);
	Bypass.Push();

	return CreateFontIndirectW(lplf);
}


VOID HookKernel::GetTextMetricsWFromLogFont(PTEXTMETRICW TextMetricW, CONST LOGFONTW *LogFont)
{
	HDC     hDC;
	HFONT   Font, OldFont;
	ULONG   GraphicsMode;

	hDC = this->CreateCompatibleDC(nullptr);
	if (hDC == nullptr)
		return;

	GraphicsMode = SetGraphicsMode(hDC, GM_ADVANCED);

	LOOP_ONCE
	{
		Font = CreateFontIndirectBypassW(LogFont);
		if (Font == nullptr)
			break;

		OldFont = (HFONT)SelectObject(hDC, Font);
		if (OldFont != nullptr)
			GetTextMetricsW(hDC, TextMetricW);

		SelectObject(hDC, OldFont);
		DeleteObject(Font);
	}

	DeleteDC(hDC);
}

PTEXT_METRIC_INTERNAL HookKernel::GetTextMetricFromCache(LPENUMLOGFONTEXW LogFont)
{
	WCHAR buf[LF_FULLFACESIZE * 2];

	StringUpperW(buf, wsprintfW(buf, L"%s@%d", LogFont->elfFullName, LogFont->elfLogFont.lfCharSet));
	return this->TextMetricCache.Get((PCWSTR)buf);
}

VOID HookKernel::AddTextMetricToCache(LPENUMLOGFONTEXW LogFont, PTEXT_METRIC_INTERNAL TextMetric)
{
	WCHAR buf[LF_FULLFACESIZE * 2];

	StringUpperW(buf, wsprintfW(buf, L"%s@%d", LogFont->elfFullName, LogFont->elfLogFont.lfCharSet));
	this->TextMetricCache.Add((PCWSTR)buf, *TextMetric);
}

HGDIOBJ NTAPI LeGetStockObject(LONG Object)
{
	HGDIOBJ         StockObject;
	PULONG_PTR      Index;
	PHookKernel     GlobalData = LeGetGlobalData();

	StockObject = nullptr;

	static ULONG_PTR StockObjectIndex[] =
	{
		ANSI_FIXED_FONT,
		ANSI_VAR_FONT,
		DEVICE_DEFAULT_FONT,
		DEFAULT_GUI_FONT,
		OEM_FIXED_FONT,
		SYSTEM_FONT,
		SYSTEM_FIXED_FONT,
	};

	if (!GlobalData->HookRoutineData.Gdi32.StockObjectInitialized)
	{
		PROTECT_SECTION(&GlobalData->HookRoutineData.Gdi32.GdiLock)
		{
			if (GlobalData->HookRoutineData.Gdi32.StockObjectInitialized)
				break;

			FOR_EACH(Index, StockObjectIndex, countof(StockObjectIndex))
			{
				GlobalData->HookRoutineData.Gdi32.StockObject[*Index] = GetFontFromFont(GlobalData, (HFONT)GlobalData->GetStockObject(*Index));
			}

			GlobalData->HookRoutineData.Gdi32.StockObjectInitialized = TRUE;
		}
	}

	LOOP_ONCE
	{
		if (Object > countof(GlobalData->HookRoutineData.Gdi32.StockObject))
		break;

		StockObject = GlobalData->HookRoutineData.Gdi32.StockObject[Object];

		if (StockObject != nullptr)
			return StockObject;
	}

	return GlobalData->GetStockObject(Object);
}

BOOL NTAPI LeDeleteObject(HGDIOBJ GdiObject)
{
	HGDIOBJ*        StockObject;
	PHookKernel     GlobalData = LeGetGlobalData();

	if (GdiObject == nullptr || GlobalData->HookRoutineData.Gdi32.StockObjectInitialized == FALSE)
		return TRUE;

	FOR_EACH_ARRAY(StockObject, GlobalData->HookRoutineData.Gdi32.StockObject)
	{
		if (GdiObject == *StockObject)
			return TRUE;
	}

	return GlobalData->DeleteObject(GdiObject);
}

HDC NTAPI LeCreateCompatibleDC(HDC hDC)
{
	HDC             NewDC;
	HFONT           Font;
	LOGFONTW        LogFont;
	PHookKernel     GlobalData = LeGetGlobalData();

	NewDC = GlobalData->CreateCompatibleDC(hDC);

	if (NewDC == nullptr)
		return NewDC;

	Font = (HFONT)GetCurrentObject(NewDC, OBJ_FONT);
	if (Font == nullptr)
		return NewDC;

	SelectObject(NewDC, GetStockObject(SYSTEM_FONT));

	return NewDC;
}

NTSTATUS
HookKernel::
GetNameRecordFromNameTable(
PVOID           TableBuffer,
ULONG_PTR       TableSize,
ULONG_PTR       NameID,
ULONG_PTR       LanguageID,
PUNICODE_STRING Name
)
{
	using namespace Gdi;

	ULONG_PTR               StorageOffset, NameRecordCount;
	PTT_NAME_TABLE_HEADER   NameHeader;
	PTT_NAME_RECORD         NameRecord, NameRecordUser, NameRecordEn;

	NameHeader = (PTT_NAME_TABLE_HEADER)TableBuffer;

	NameRecordCount = Bswap(NameHeader->NameRecordCount);
	StorageOffset = Bswap(NameHeader->StorageOffset);

	if (StorageOffset >= TableSize)
		return STATUS_NOT_SUPPORTED;

	if (StorageOffset < NameRecordCount * sizeof(*NameRecord))
		return STATUS_NOT_SUPPORTED;

	LanguageID = Bswap((USHORT)LanguageID);
	NameRecordUser = nullptr;
	NameRecordEn = nullptr;
	NameRecord = (PTT_NAME_RECORD)(NameHeader + 1);

	FOR_EACH(NameRecord, NameRecord, NameRecordCount)
	{
		if (NameRecord->PlatformID != TT_PLATFORM_ID_WINDOWS)
			continue;

		if (NameRecord->EncodingID != TT_ENCODEING_ID_UTF16_BE)
			continue;

		if (NameRecord->NameID != NameID)
			continue;

		if (NameRecord->LanguageID == Bswap((USHORT)0x0409))
			NameRecordEn = NameRecord;

		if (NameRecord->LanguageID != LanguageID)
			continue;

		NameRecordUser = NameRecord;
		break;
	}

	NameRecordUser = NameRecordUser == nullptr ? NameRecordEn : NameRecordUser;
	if (NameRecordUser == nullptr)
		return STATUS_NOT_FOUND;

	PWSTR       FaceName, Buffer;
	ULONG_PTR   Offset, Length;

	Offset = StorageOffset + Bswap(NameRecordUser->StringOffset);
	Length = Bswap(NameRecordUser->StringLength);
	FaceName = (PWSTR)PtrAdd(TableBuffer, Offset);

	Buffer = Name->Buffer;
	Length = (USHORT)min(Length, Name->MaximumLength);
	Name->Length = Length;

	for (ULONG_PTR Index = 0; Index != Length / sizeof(WCHAR); ++Index)
	{
		Buffer[Index] = Bswap(FaceName[Index]);
	}

	if (Length < Name->MaximumLength)
		*PtrAdd(Buffer, Length) = 0;

	return STATUS_SUCCESS;
}

NTSTATUS HookKernel::AdjustFontDataInternal(PADJUST_FONT_DATA AdjustData)
{
	NTSTATUS        Status;
	PVOID           Table;
	ULONG_PTR       TableSize, TableName;
	WCHAR           FaceNameBuffer[LF_FACESIZE];
	WCHAR           FullNameBuffer[LF_FULLFACESIZE];
	UNICODE_STRING  FaceName, FullName;

	if (FLAG_ON(AdjustData->FontType, RASTER_FONTTYPE))
		return STATUS_NOT_SUPPORTED;

	TableName = Gdi::TT_TABLE_TAG_NAME;
	TableSize = GetFontData(AdjustData->DC, TableName, 0, 0, 0);
	if (TableSize == GDI_ERROR)
		return STATUS_OBJECT_NAME_NOT_FOUND;

	Table = AllocStack(TableSize);
	TableSize = GetFontData(AdjustData->DC, TableName, 0, Table, TableSize);
	if (TableSize == GDI_ERROR)
		return STATUS_OBJECT_NAME_NOT_FOUND;

	RtlInitEmptyString(&FaceName, FaceNameBuffer, sizeof(FaceNameBuffer));
	RtlInitEmptyString(&FullName, FullNameBuffer, sizeof(FullNameBuffer));

	Status = this->GetNameRecordFromNameTable(
		Table,
		TableSize,
		Gdi::TT_NAME_ID_FACENAME,
		this->OriginalLocaleID,
		&FaceName
		);

	PCWSTR lfFaceName = AdjustData->EnumLogFontEx->elfLogFont.lfFaceName;
	BOOL Vertical = lfFaceName[0] == '@';

	if (NT_FAILED(Status) || StrICompareW(FaceName.Buffer, lfFaceName + Vertical, StrCmp_ToUpper) != 0)
		return STATUS_CONTEXT_MISMATCH;

	Status = this->GetNameRecordFromNameTable(
		Table,
		TableSize,
		Gdi::TT_NAME_ID_FACENAME,
		this->LocaleID,
		&FaceName
		);

	Status = NT_SUCCESS(Status) ?
		this->GetNameRecordFromNameTable(
		Table,
		TableSize,
		Gdi::TT_NAME_ID_FULLNAME,
		this->LocaleID,
		&FullName)
		: Status;

	if (NT_SUCCESS(Status))
	{
		BOOL        Vertical;
		PWSTR       Buffer;
		ULONG_PTR   Length;

		Vertical = AdjustData->EnumLogFontEx->elfLogFont.lfFaceName[0] == '@';

		Buffer = AdjustData->EnumLogFontEx->elfLogFont.lfFaceName + Vertical;
		Length = min(sizeof(AdjustData->EnumLogFontEx->elfLogFont.lfFaceName) - Vertical, FaceName.Length);
		CopyMemory(Buffer, FaceName.Buffer, Length);
		*PtrAdd(Buffer, Length) = 0;

		Buffer = AdjustData->EnumLogFontEx->elfFullName + Vertical;
		Length = min(sizeof(AdjustData->EnumLogFontEx->elfFullName) - Vertical, FullName.Length);
		CopyMemory(Buffer, FullName.Buffer, Length);
		*PtrAdd(Buffer, Length) = 0;
	}

	return Status;
}

NTSTATUS HookKernel::AdjustFontData(HDC DC, LPENUMLOGFONTEXW EnumLogFontEx, PTEXT_METRIC_INTERNAL TextMetric, ULONG_PTR FontType)
{
	NTSTATUS Status;
	ADJUST_FONT_DATA AdjustData;

	ZeroMemory(&AdjustData, sizeof(AdjustData));
	AdjustData.EnumLogFontEx = EnumLogFontEx;
	AdjustData.DC = DC;

	Status = STATUS_UNSUCCESSFUL;

	LOOP_ONCE
	{
		AdjustData.Font = CreateFontIndirectBypassW(&EnumLogFontEx->elfLogFont);
		if (AdjustData.Font == nullptr)
			break;

		AdjustData.OldFont = (HFONT)SelectObject(DC, AdjustData.Font);
		if (AdjustData.OldFont == nullptr)
			break;

		Status = this->AdjustFontDataInternal(&AdjustData);
		if (Status == STATUS_CONTEXT_MISMATCH)
			break;

		if (TextMetric != nullptr)
		{
			if (GetTextMetricsA(DC, &TextMetric->TextMetricA) &&
				GetTextMetricsW(DC, &TextMetric->TextMetricW))
			{
				TextMetric->Filled = TRUE;
			}
		}
	}

		if (AdjustData.OldFont != nullptr)
			SelectObject(DC, AdjustData.OldFont);

	if (AdjustData.Font != nullptr)
		this->DeleteObject(AdjustData.Font);

	return Status;
}

VOID ConvertAnsiLogfontToUnicode(PLOGFONTW LogFontW, PLOGFONTA LogFontA)
{
	CopyMemory(LogFontW, LogFontA, PtrOffset(&LogFontA->lfFaceName, LogFontA));
	RtlMultiByteToUnicodeN(LogFontW->lfFaceName, sizeof(LogFontW->lfFaceName), nullptr, LogFontA->lfFaceName, StrLengthA(LogFontA->lfFaceName) + 1);
}

VOID ConvertUnicodeLogfontToAnsi(PLOGFONTA LogFontA, PLOGFONTW LogFontW)
{
	CopyMemory(LogFontA, LogFontW, PtrOffset(&LogFontW->lfFaceName, LogFontW));
	RtlUnicodeToMultiByteN(LogFontA->lfFaceName, sizeof(LogFontA->lfFaceName), nullptr, LogFontW->lfFaceName, (StrLengthW(LogFontW->lfFaceName) + 1) * sizeof(WCHAR));
}

VOID ConvertUnicodeLogfontToAnsi(PLOGFONTA LogFontA, PLOGFONTW LogFontW, UINT CodePage)
{
	CopyMemory(LogFontA, LogFontW, PtrOffset(&LogFontW->lfFaceName, LogFontW));
	WideCharToMultiByte(CodePage, 0, LogFontW->lfFaceName, StrLengthW(LogFontW->lfFaceName) + 1, LogFontA->lfFaceName, sizeof(LogFontA->lfFaceName), nullptr, nullptr);
}


VOID ConvertUnicodeTextMetricToAnsi(PTEXTMETRICA TextMetricA, CONST TEXTMETRICW *TextMetricW)
{
	TextMetricA->tmHeight = TextMetricW->tmHeight;
	TextMetricA->tmAscent = TextMetricW->tmAscent;
	TextMetricA->tmDescent = TextMetricW->tmDescent;
	TextMetricA->tmInternalLeading = TextMetricW->tmInternalLeading;
	TextMetricA->tmExternalLeading = TextMetricW->tmExternalLeading;
	TextMetricA->tmAveCharWidth = TextMetricW->tmAveCharWidth;
	TextMetricA->tmMaxCharWidth = TextMetricW->tmMaxCharWidth;
	TextMetricA->tmWeight = TextMetricW->tmWeight;
	TextMetricA->tmOverhang = TextMetricW->tmOverhang;
	TextMetricA->tmDigitizedAspectX = TextMetricW->tmDigitizedAspectX;
	TextMetricA->tmDigitizedAspectY = TextMetricW->tmDigitizedAspectY;

	TextMetricA->tmFirstChar = TextMetricW->tmStruckOut;
	TextMetricA->tmLastChar = min(0xFF, TextMetricW->tmLastChar);
	TextMetricA->tmDefaultChar = TextMetricW->tmDefaultChar;
	TextMetricA->tmBreakChar = TextMetricW->tmBreakChar;

	TextMetricA->tmItalic = TextMetricW->tmItalic;
	TextMetricA->tmUnderlined = TextMetricW->tmUnderlined;
	TextMetricA->tmStruckOut = TextMetricW->tmStruckOut;
	TextMetricA->tmPitchAndFamily = TextMetricW->tmPitchAndFamily;
	TextMetricA->tmCharSet = TextMetricW->tmCharSet;
}

INT NTAPI LeEnumFontCallbackAFromW(CONST LOGFONTW *lf, CONST TEXTMETRICW *TextMetricW, DWORD FontType, LPARAM param)
{
	ENUMLOGFONTEXA          EnumLogFontA;
	LPENUMLOGFONTEXW        EnumLogFontW;
	PGDI_ENUM_FONT_PARAM    EnumParam;
	PTEXT_METRIC_INTERNAL   TextMetric;
	TEXTMETRICA             TextMetricA;
	PTEXTMETRICA            tma;

	TextMetric = FIELD_BASE(TextMetricW, TEXT_METRIC_INTERNAL, TextMetricW);
	EnumParam = (PGDI_ENUM_FONT_PARAM)param;

	EnumLogFontW = (LPENUMLOGFONTEXW)lf;
	ConvertUnicodeLogfontToAnsi(&EnumLogFontA.elfLogFont, &EnumLogFontW->elfLogFont, 932);

	WideCharToMultiByte(932, 0, EnumLogFontW->elfFullName, StrLengthW(EnumLogFontW->elfFullName) + 1, (PSTR)EnumLogFontA.elfFullName, sizeof(EnumLogFontA.elfFullName), nullptr, nullptr);
	WideCharToMultiByte(932, 0, EnumLogFontW->elfScript, StrLengthW(EnumLogFontW->elfScript) + 1, (PSTR)EnumLogFontA.elfScript, sizeof(EnumLogFontA.elfScript), nullptr, nullptr);
	WideCharToMultiByte(932, 0, EnumLogFontW->elfStyle, StrLengthW(EnumLogFontW->elfStyle) + 1, (PSTR)EnumLogFontA.elfStyle, sizeof(EnumLogFontA.elfStyle), nullptr, nullptr);

	if (TextMetric->VerifyMagic() == FALSE)
	{
		ConvertUnicodeTextMetricToAnsi(&TextMetricA, TextMetricW);
		tma = &TextMetricA;
	}
	else
	{
		tma = &TextMetric->TextMetricA;
	}

	static BYTE SJISIndentor[] = { 0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA, 0x00 };
	RtlCopyMemory(&EnumLogFontA.elfScript, SJISIndentor, sizeof(SJISIndentor));

	return ((FONTENUMPROCA)(EnumParam->Callback))(&EnumLogFontA.elfLogFont, tma, FontType, EnumParam->lParam);
}

INT NTAPI LeEnumFontCallbackW(CONST LOGFONTW *lf, CONST TEXTMETRICW *TextMetricW, DWORD FontType, LPARAM param)
{
	NTSTATUS                Status;
	PGDI_ENUM_FONT_PARAM    EnumParam;
	TEXT_METRIC_INTERNAL    TextMetric;
	LPENUMLOGFONTEXW        EnumLogFontEx;
	ULONG_PTR               LogFontCharset;

	EnumParam = (PGDI_ENUM_FONT_PARAM)param;
	EnumLogFontEx = (LPENUMLOGFONTEXW)lf;
	LogFontCharset = EnumLogFontEx->elfLogFont.lfCharSet;

	LOOP_ONCE
	{
		PTEXT_METRIC_INTERNAL CachedTextMetric;

		CachedTextMetric = EnumParam->GlobalData->GetTextMetricFromCache(EnumLogFontEx);
		if (CachedTextMetric != nullptr)
		{
			if (CachedTextMetric->VerifyMagic() == FALSE)
				return TRUE;

			if (CachedTextMetric->Filled == FALSE)
				break;

			TextMetric.TextMetricA = CachedTextMetric->TextMetricA;
			TextMetric.TextMetricW = CachedTextMetric->TextMetricW;
			TextMetric.Filled = TRUE;
			break;
		}

		LOOP_ONCE
		{
			if (EnumParam->Charset != DEFAULT_CHARSET)
			break;

			if (lf->lfCharSet != ANSI_CHARSET &&
				lf->lfCharSet != DEFAULT_CHARSET &&
				lf->lfCharSet != EnumParam->GlobalData->OriginalCharset)
			{
				break;
			}

			EnumLogFontEx->elfLogFont.lfCharSet = SHIFTJIS_CHARSET;
		}

		Status = EnumParam->GlobalData->AdjustFontData(EnumParam->DC, EnumLogFontEx, &TextMetric, FontType);

		ENUMLOGFONTEXW Captured = *EnumLogFontEx;

		Captured.elfLogFont.lfCharSet = LogFontCharset;

		if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Status == STATUS_CONTEXT_MISMATCH)
		{
			TextMetric.Magic = 0;
			EnumParam->GlobalData->AddTextMetricToCache(&Captured, &TextMetric);
			return TRUE;
		}

		EnumParam->GlobalData->AddTextMetricToCache(&Captured, &TextMetric);
	}

	if (TextMetric.Filled == FALSE)
	{
		TextMetric.TextMetricW = *TextMetricW;
		TextMetric.Magic = 0;
		EnumLogFontEx->elfLogFont.lfCharSet = LogFontCharset;
	}

	TextMetricW = &TextMetric.TextMetricW;

	return ((FONTENUMPROCW)(EnumParam->Callback))(lf, TextMetricW, FontType, EnumParam->lParam);
}

int NTAPI LeEnumFontFamiliesExW(HDC hdc, LPLOGFONTW lpLogfont, FONTENUMPROCW lpProc, LPARAM lParam, DWORD dwFlags)
{
	INT                 Result;
	GDI_ENUM_FONT_PARAM Param;
	LOGFONTW            LocalLogFont;
	PHookKernel         GlobalData = LeGetGlobalData();

	if (NT_FAILED(Param.Prepare(GlobalData)))
		return FALSE;

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = lpLogfont->lfCharSet;

	LocalLogFont = *lpLogfont;

	return GlobalData->EnumFontFamiliesExW(hdc, &LocalLogFont, LeEnumFontCallbackW, (LPARAM)&Param, dwFlags);
}

int NTAPI LeEnumFontFamiliesExA(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags)
{
	LOGFONTW            lf;
	PHookKernel         GlobalData = LeGetGlobalData();
	GDI_ENUM_FONT_PARAM Param;

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = lpLogfont->lfCharSet;

	ConvertAnsiLogfontToUnicode(&lf, lpLogfont);

	return LeEnumFontFamiliesExW(hdc, &lf, LeEnumFontCallbackAFromW, (LPARAM)&Param, dwFlags);
}

int NTAPI LeEnumFontFamiliesW(HDC hdc, PCWSTR lpszFamily, FONTENUMPROCW lpProc, LPARAM lParam)
{
	GDI_ENUM_FONT_PARAM Param;
	LOGFONTW            LocalLogFont;
	PHookKernel         GlobalData = LeGetGlobalData();

	if (NT_FAILED(Param.Prepare(GlobalData)))
		return FALSE;

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = DEFAULT_CHARSET;

	return GlobalData->EnumFontFamiliesW(hdc, lpszFamily, LeEnumFontCallbackW, (LPARAM)&Param);
}

int NTAPI LeEnumFontFamiliesA(HDC hdc, PSTR lpszFamily, FONTENUMPROCA lpProc, LPARAM lParam)
{
	INT                 Result;
	PWSTR               Family;
	GDI_ENUM_FONT_PARAM Param;
	LOGFONTW            LocalLogFont;
	PHookKernel         GlobalData = LeGetGlobalData();

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = DEFAULT_CHARSET;

	if (lpszFamily != nullptr)
	{
		Family = MByteToWChar(lpszFamily);
		if (Family == nullptr)
			return FALSE;
	}
	else
	{
		Family = nullptr;
	}

	Result = LeEnumFontFamiliesW(hdc, Family, LeEnumFontCallbackAFromW, (LPARAM)&Param);
	FreeString(Family);

	return Result;
}

int NTAPI LeEnumFontsW(HDC hdc, PCWSTR lpFaceName, FONTENUMPROCW lpProc, LPARAM lParam)
{
	GDI_ENUM_FONT_PARAM Param;
	LOGFONTW            LocalLogFont;
	PHookKernel         GlobalData = LeGetGlobalData();

	if (NT_FAILED(Param.Prepare(GlobalData)))
		return FALSE;

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = DEFAULT_CHARSET;

	return GlobalData->EnumFontsW(hdc, lpFaceName, LeEnumFontCallbackW, (LPARAM)&Param);
}

int NTAPI LeEnumFontsA(HDC hdc, PSTR lpFaceName, FONTENUMPROCA lpProc, LPARAM lParam)
{
	INT                 Result;
	PWSTR               FaceName;
	GDI_ENUM_FONT_PARAM Param;
	LOGFONTW            LocalLogFont;
	PHookKernel         GlobalData = LeGetGlobalData();

	Param.Callback = lpProc;
	Param.GlobalData = GlobalData;
	Param.lParam = lParam;
	Param.Charset = DEFAULT_CHARSET;

	if (lpFaceName != nullptr)
	{
		FaceName = MByteToWChar(lpFaceName);
		if (FaceName == nullptr)
			return FALSE;
	}
	else
	{
		FaceName = nullptr;
	}

	Result = LeEnumFontsW(hdc, FaceName, LeEnumFontCallbackAFromW, (LPARAM)&Param);
	FreeString(FaceName);

	return Result;
}

HFONT
NTAPI
LeNtGdiHfontCreate(
PENUMLOGFONTEXDVW   EnumLogFont,
ULONG               SizeOfEnumLogFont,
LONG                LogFontType,
LONG                Unknown,
PVOID               FreeListLocalFont
)
{
	PENUMLOGFONTEXDVW   enumlfex;
	PHookKernel         GlobalData = LeGetGlobalData();

	if (EnumLogFont != nullptr && IsGdiHookBypassed() == FALSE) LOOP_ONCE
	{
		ULONG_PTR Charset;

		Charset = EnumLogFont->elfEnumLogfontEx.elfLogFont.lfCharSet;

		if (Charset != ANSI_CHARSET && Charset != DEFAULT_CHARSET)
			break;

		enumlfex = (PENUMLOGFONTEXDVW)AllocStack(SizeOfEnumLogFont);

		CopyMemory(enumlfex, EnumLogFont, SizeOfEnumLogFont);
		enumlfex->elfEnumLogfontEx.elfLogFont.lfCharSet = SHIFTJIS_CHARSET;

		EnumLogFont = enumlfex;
	}

	return GlobalData->HookStub.StubNtGdiHfontCreate(EnumLogFont, SizeOfEnumLogFont, LogFontType, Unknown, FreeListLocalFont);
}

API_POINTER(SelectObject)  StubSelectObject;

HGDIOBJ NTAPI LeSelectObject(HDC hdc, HGDIOBJ h)
{
	HGDIOBJ obj;

	obj = StubSelectObject(hdc, h);

	switch (GdiGetCodePage(hdc))
	{
		//case 0x3A4:
	case 0x3A8:
	{
		ULONG objtype = GetObjectType(h);

		union
		{
			LOGFONTW lf;
		};

		switch (objtype)
		{
		case OBJ_FONT:
			GetObjectW(h, sizeof(lf), &lf);
			ExceptionBox(lf.lfFaceName, L"FUCK FACE");
			break;

		default:
			return obj;
		}

		break;
	}
	}

	return obj;
}

/************************************************************************
init
************************************************************************/

PVOID FindNtGdiHfontCreate(PVOID Gdi32)
{
	PVOID CreateFontIndirectExW, NtGdiHfontCreate;

	CreateFontIndirectExW = LookupExportTable(Gdi32, GDI32_CreateFontIndirectExW);

	NtGdiHfontCreate = WalkOpCodeT(CreateFontIndirectExW, 0xA0,
		WalkOpCodeM(Buffer, OpLength, Ret)
	{
		switch (Buffer[0])
		{
		case CALL:
			Buffer = GetCallDestination(Buffer);
			if (IsSystemCall(Buffer) == FALSE)
				break;

			Ret = Buffer;
			//return STATUS_SUCCESS;
			break;
		}

		return STATUS_NOT_FOUND;
	}
	);

	return NtGdiHfontCreate;
}


#define LeHookFromEAT(_Base, _Prefix, _Name)    Mp::FunctionJumpVa(LookupExportTable(_Base, _Prefix##_##_Name), Le##_Name, &HookStub.Stub##_Name)
#define LeHookFromEAT2(_Base, _Prefix, _Name)   Mp::FunctionJumpVa(LookupExportTable(_Base, _Prefix##_##_Name), Le##_Name)
#define LeFunctionJump(_Name)                   Mp::FunctionJumpVa(_Name, Le##_Name, &HookStub.Stub##_Name)
#define LeFunctionCall(_Name)                   Mp::FunctionCallVa(_Name, Le##_Name, &HookStub.Stub##_Name)

/************************************************************************
init end
************************************************************************/

NTSTATUS HookKernel::HookGdi32Routines(PVOID Gdi32)
{
	PVOID NtGdiHfontCreate, Fms;

	*(PVOID *)&GdiGetCodePage = GetProcAddress((HMODULE)Gdi32, "GdiGetCodePage");

	NtGdiHfontCreate = FindNtGdiHfontCreate(Gdi32);
	if (NtGdiHfontCreate == nullptr)
		NtGdiHfontCreate = Nt_GetProcAddress(Nt_LoadLibrary(L"win32u.dll"), "NtGdiHfontCreate");

	if (NtGdiHfontCreate == nullptr)
		return STATUS_NOT_FOUND;

	RtlInitializeCriticalSectionAndSpinCount(&HookRoutineData.Gdi32.GdiLock, 4000);
	InitFontCharsetInfo();

	Mp::PATCH_MEMORY_DATA p[] =
	{
		LeHookFromEAT(Gdi32, GDI32, GetStockObject),
		LeHookFromEAT(Gdi32, GDI32, DeleteObject),
		LeHookFromEAT(Gdi32, GDI32, CreateCompatibleDC),
		LeHookFromEAT(Gdi32, GDI32, EnumFontsW),
		LeHookFromEAT(Gdi32, GDI32, EnumFontsA),
		LeHookFromEAT(Gdi32, GDI32, EnumFontFamiliesA),
		LeHookFromEAT(Gdi32, GDI32, EnumFontFamiliesW),
		LeHookFromEAT(Gdi32, GDI32, EnumFontFamiliesExA),
		LeHookFromEAT(Gdi32, GDI32, EnumFontFamiliesExW),

		//LeFunctionJump(NtGdiHfontCreate),
	};

	return Mp::PatchMemory(p, countof(p));
}

NTSTATUS HookKernel::UnHookGdi32Routines()
{
	Mp::RestoreMemory(HookStub.StubGetStockObject);
	Mp::RestoreMemory(HookStub.StubDeleteObject);
	Mp::RestoreMemory(HookStub.StubCreateCompatibleDC);
	Mp::RestoreMemory(HookStub.StubEnumFontFamiliesExA);
	Mp::RestoreMemory(HookStub.StubEnumFontFamiliesExW);
	Mp::RestoreMemory(HookStub.StubEnumFontsA);
	Mp::RestoreMemory(HookStub.StubEnumFontsW);
	Mp::RestoreMemory(HookStub.StubNtGdiHfontCreate);

	return 0;
}


VOID HookKernel::InitFontCharsetInfo()
{
	HDC DC;
	LOGFONTW lf;

	DC = GetDC(nullptr);
	this->OriginalCharset = GetTextCharset(DC);

	lf.lfCharSet = SHIFTJIS_CHARSET;
	lf.lfFaceName[0] = 0;

	auto EnumFontCallback = [](CONST LOGFONTW *lf, CONST TEXTMETRICW *, DWORD, LPARAM Param)
	{
		HookKernel *GlobalData = (HookKernel *)Param;
		LPENUMLOGFONTEXW elf = (LPENUMLOGFONTEXW)lf;

		CopyStruct(GlobalData->ScriptNameW, elf->elfScript, sizeof(elf->elfScript));
		UnicodeToAnsi(GlobalData->ScriptNameA, countof(GlobalData->ScriptNameA), GlobalData->ScriptNameW);

		return FALSE;
	};

	if (HookStub.StubEnumFontFamiliesExW == nullptr)
	{
		::EnumFontFamiliesExW(DC, &lf, EnumFontCallback, (LPARAM)this, 0);
	}
	else
	{
		EnumFontFamiliesExW(DC, &lf, EnumFontCallback, (LPARAM)this, 0);
	}

	ReleaseDC(nullptr, DC);
}
