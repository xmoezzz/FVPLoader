#include <sdkddkver.h>
#include "FVPKernel.h"

#pragma comment(lib, "MyLibrary_x86.lib")
#pragma comment(linker, "/ENTRY:DllEntryMain")

OVERLOAD_CPP_NEW_WITH_HEAP(Nt_CurrentPeb()->ProcessHeap);

static HookKernel* FVPKernel = NULL;

ForceInline VOID CreateKernelObject()
{
	FVPKernel = new HookKernel();
}

PHookKernel LeGetGlobalData()
{
	if (FVPKernel == nullptr)
		CreateKernelObject();
	return FVPKernel;
}

//============================My Patch=======================================


LRESULT CALLBACK HookMainWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	FVPKernel->MainWindow = hWnd;
	return FVPKernel->StubMainWindowProc(hWnd, Msg, wParam, lParam);
}

ATOM WINAPI HookRegisterClassExA(CONST WNDCLASSEXA* lpWndClass)
{
	WNDCLASSEXW  ClassInfo;
	LPWSTR       MenuName,   ClassName;
	LONG_PTR     MenuLength, ClassLength;


	MenuLength  = StrLengthA(lpWndClass->lpszMenuName);
	ClassLength = StrLengthA(lpWndClass->lpszClassName);

	if (StrCompareA(lpWndClass->lpszClassName, "HS_MAIN_WINDOW_CLASS00"))
		return FVPKernel->StubRegisterClassExA(lpWndClass);

	MenuName  = (LPWSTR)AllocStack((MenuLength + 1) * 2);
	ClassName = (LPWSTR)AllocStack((ClassLength + 1)* 2);

	RtlZeroMemory(MenuName, (MenuLength + 1) * 2);
	RtlZeroMemory(ClassName, (ClassLength + 1) * 2);

	MultiByteToWideChar(932, 0, lpWndClass->lpszMenuName,  MenuLength,  MenuName, MAX_PATH);
	MultiByteToWideChar(932, 0, lpWndClass->lpszClassName, ClassLength, ClassName, MAX_PATH);

	ClassInfo.cbSize        = sizeof(WNDCLASSEXW);
	ClassInfo.style         = lpWndClass->style;
	ClassInfo.lpfnWndProc   = lpWndClass->lpfnWndProc;
	ClassInfo.cbClsExtra    = lpWndClass->cbClsExtra;
	ClassInfo.cbWndExtra    = lpWndClass->cbWndExtra;
	ClassInfo.hInstance     = lpWndClass->hInstance;
	ClassInfo.hIcon         = lpWndClass->hIcon;
	ClassInfo.hCursor       = lpWndClass->hCursor;
	ClassInfo.hbrBackground = lpWndClass->hbrBackground;
	ClassInfo.lpszMenuName  = MenuName;
	ClassInfo.lpszClassName = ClassName;
	ClassInfo.hIconSm       = lpWndClass->hIconSm;

	if (FVPKernel->MainWindow == nullptr && lpWndClass->lpfnWndProc)
	{
		Mp::PATCH_MEMORY_DATA p[] =
		{
			Mp::FunctionJumpVa(lpWndClass->lpfnWndProc, HookMainWindowProc, &FVPKernel->StubMainWindowProc)
		};

		Mp::PatchMemory(p, countof(p));
	}

	return RegisterClassExW(&ClassInfo);
}


HWND WINAPI HookCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	ULONG       Length;
	LPWSTR      ClassName, WindowName;

	if (StrCompareA(lpClassName, "HS_MAIN_WINDOW_CLASS00"))
		return FVPKernel->StubCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);


	Length = StrLengthA(lpClassName) + 1;
	ClassName = (LPWSTR)AllocStack(Length * sizeof(WCHAR));
	RtlZeroMemory(ClassName, Length * sizeof(WCHAR));
	MultiByteToWideChar(932, 0, lpClassName, Length, ClassName, Length * sizeof(WCHAR));

	Length = StrLengthA(lpWindowName) + 1;
	WindowName = (LPWSTR)AllocStack(Length * sizeof(WCHAR));
	RtlZeroMemory(WindowName, Length * sizeof(WCHAR));
	MultiByteToWideChar(932, 0, lpWindowName, Length, WindowName, Length * sizeof(WCHAR));

	FVPKernel->MainWindow = CreateWindowExW(dwExStyle, ClassName, WindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	return FVPKernel->MainWindow;
}


LRESULT CALLBACK HookDefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (hWnd == FVPKernel->MainWindow && FVPKernel->MainWindow != NULL)
		return DefWindowProcW(hWnd, Msg, wParam, lParam);
	else
		return FVPKernel->StubDefWindowProcA(hWnd, Msg, wParam, lParam);
}


LRESULT WINAPI HookDispatchMessageA(MSG *lpMsg)
{
	if (lpMsg->hwnd == FVPKernel->MainWindow && FVPKernel->MainWindow != NULL)
		return DispatchMessageW(lpMsg);
	else
		return FVPKernel->StubDispatchMessageA(lpMsg);
}


BOOL WINAPI HookGetVersionExA(LPOSVERSIONINFOA lpVersionInfo)
{
	BOOL Result;

	Result = FVPKernel->StubGetVersionExA(lpVersionInfo);
	lpVersionInfo->dwMajorVersion = 5;
	lpVersionInfo->dwMinorVersion = 1;
	return Result;
}


INT WINAPI HooklStrcmpiA(LPCSTR lpString1, LPCSTR lpString2)
{
	Int Result;

	Result = CompareStringA(0x411, 1, lpString1, -1, lpString2, -1);
	return Result - 2;
}


INT WINAPI HookMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	LPWSTR       TextName, CaptionName;
	ULONG        TextLength, CaptionLength;

	TextLength    = lpText ? StrLengthA(lpText) : 0;
	CaptionLength = lpCaption ? StrLengthA(lpCaption) : 0;

	TextName    = nullptr;
	CaptionName = nullptr;

	if (TextLength)
	{
		TextName = (LPWSTR)AllocStack((TextLength + 1) * 2);
		RtlZeroMemory(TextName, (TextLength + 1) * 2);
		MultiByteToWideChar(932, 0, lpText, TextLength, TextName, (TextLength + 1) * 2);
	}

	if (CaptionLength)
	{
		CaptionName = (LPWSTR)AllocStack((CaptionLength + 1) * 2);
		RtlZeroMemory(CaptionName, (CaptionLength + 1) * 2);
		MultiByteToWideChar(932, 0, lpCaption, CaptionLength, CaptionName, (CaptionLength + 1) * 2);
	}

	return MessageBoxW(hWnd, TextName, CaptionName, uType);
}


BOOL WINAPI HookGetMenuItemInfoA(HMENU hMenu, UINT uItem, BOOL fByPosition, LPMENUITEMINFOA lpmii)
{
	BOOL          Success;
	DWORD         cchtmp, ccBuffer;
	MENUITEMINFOW miitmp;

	cchtmp = 0;
	RtlCopyMemory(&miitmp, lpmii, lpmii->cbSize);
	if (((miitmp.fMask & MIIM_TYPE) && miitmp.fType != 0) || (miitmp.fMask & MIIM_STRING) || miitmp.cch > 0)
	{
		cchtmp = miitmp.cch;
		miitmp.dwTypeData = (LPWSTR)AllocateMemoryP((cchtmp + 1) * sizeof(WCHAR), HEAP_ZERO_MEMORY);
	}

	Success = GetMenuItemInfoW(hMenu, uItem, fByPosition, &miitmp);
	if (Success)
		RtlCopyMemory(lpmii, &miitmp, miitmp.cbSize);

	if (cchtmp > 0)
	{
		ccBuffer = WideCharToMultiByte(932, 0, miitmp.dwTypeData, -1, lpmii->dwTypeData, lpmii->cch, NULL, NULL);
		if (ccBuffer > 0) 
			lpmii->cch = ccBuffer - 1;

		if (miitmp.dwTypeData) 
			FreeMemoryP(miitmp.dwTypeData);

		lpmii->dwTypeData[cchtmp - 1] = NULL;
	}

	return Success;
}

LPCWSTR FASTCALL MultiByteToWideCharInternal(LPCSTR lpString)
{
	LONG_PTR Size, ccBuffer;
	LPWSTR   UnicodeStr;

	Size = StrLengthA(lpString);
	UnicodeStr = (LPWSTR)AllocateMemoryP((Size + 1) << 1);

	if (UnicodeStr) 
	{
		ccBuffer = MultiByteToWideChar(932, 0, lpString, Size, UnicodeStr, Size);
		UnicodeStr[ccBuffer] = NULL;
	}
	return UnicodeStr;
}

BOOL WINAPI HookSetMenuItemInfoA(HMENU hMenu, UINT uItem, BOOL fByPosition, LPMENUITEMINFOA lpmii) 
{
	LPCSTR TypeDataA;
	BOOL   Success;

	TypeDataA = NULL;
	if (((lpmii->fMask & MIIM_TYPE) && lpmii->fType != 0) || (lpmii->fMask & MIIM_STRING) || lpmii->cch > 0) 
	{
		TypeDataA = lpmii->dwTypeData;
		lpmii->dwTypeData = (LPSTR)MultiByteToWideCharInternal(lpmii->dwTypeData);
	}

	Success = SetMenuItemInfoW(hMenu, uItem, fByPosition, (LPMENUITEMINFOW)lpmii);
	if (TypeDataA) 
	{ 
		FreeMemoryP((LPVOID)lpmii->dwTypeData);
		lpmii->dwTypeData = (LPSTR)TypeDataA;
	}
	return Success;
}

DWORD WINAPI HookGetGlyphOutlineA(
	_In_        HDC            hdc,
	_In_        UINT           uChar,
	_In_        UINT           uFormat,
	_Out_       LPGLYPHMETRICS lpgm,
	_In_        DWORD          cbBuffer,
	_Out_       LPVOID         lpvBuffer,
	_In_  const MAT2           *lpmat2
	)
{
	ULONG      len;
	CHAR       mbchs[2];
	UINT       cp = 932;

	if (IsDBCSLeadByteEx(cp, uChar >> 8))
	{
		len = 2;
		mbchs[0] = (uChar & 0xff00) >> 8;
		mbchs[1] = (uChar & 0xff);
	}
	else
	{
		len = 1;
		mbchs[0] = (uChar & 0xff);
	}

	uChar = 0;
	MultiByteToWideChar(cp, 0, mbchs, len, (LPWSTR)&uChar, 1);

	return GetGlyphOutlineW(hdc, uChar, uFormat,
			lpgm, cbBuffer, lpvBuffer, lpmat2);
}

HFONT WINAPI HookCreateFontA(int nHeight, int  nWidth, int  nEscapement, int nOrientation,
	int fnWeight, DWORD fdwItalic, DWORD fdwUnderline, DWORD fdwStrikeOut,
	DWORD fdwCharSet, DWORD fdwOutputPrecision, DWORD fdwClipPrecision,
	DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCSTR lpszFace)
{
	WCHAR FontFace[128];

	RtlZeroMemory(FontFace, sizeof(FontFace));
	MultiByteToWideChar(932, 0, lpszFace, StrLengthA(lpszFace), FontFace, 128);

	return  CreateFontW(nHeight, nWidth, nEscapement, nOrientation,
		fnWeight, fdwItalic, fdwUnderline, fdwStrikeOut,
		0x86, fdwOutputPrecision, fdwClipPrecision,
		fdwQuality, fdwPitchAndFamily, FontFace);
}


//=========================Kernel==============================

BOOL FASTCALL Initialize(HMODULE hModule)
{
	NTSTATUS        Status;
	BOOL            Success;
	WCHAR           ExePath[MAX_PATH];
	PVOID           NlsBaseAddress;
	LCID            DefaultLocaleID;
	LARGE_INTEGER   DefaultCasingTableSize;


	Status = ml::MlInitialize();
	if (NT_FAILED(Status))
		return FALSE;

	CreateKernelObject();

	Status = NtInitializeNlsFiles(&NlsBaseAddress, &DefaultLocaleID, &DefaultCasingTableSize);
	if (NT_FAILED(Status))
		return FALSE;

	Status = FVPKernel->TextMetricCache.Initialize();
	if (NT_FAILED(Status))
		return FALSE;

	FVPKernel->OriginalLocaleID = DefaultLocaleID;
	FVPKernel->LocaleID = 0x411;

	
	FVPKernel->HookGdi32Routines(Nt_LoadLibrary(L"GDI32.DLL"));
	
	LOOP_ONCE
	{

		Mp::PATCH_MEMORY_DATA p[] =
		{
			Mp::FunctionJumpVa(GetVersionExA,    HookGetVersionExA,    &FVPKernel->StubGetVersionExA),
			Mp::FunctionJumpVa(lstrcmpiA,        HooklStrcmpiA),
			Mp::FunctionJumpVa(GetGlyphOutlineA, HookGetGlyphOutlineA),
			Mp::FunctionJumpVa(CreateFontA,      HookCreateFontA),
			Mp::FunctionJumpVa(RegisterClassExA, HookRegisterClassExA, &FVPKernel->StubRegisterClassExA),
			Mp::FunctionJumpVa(CreateWindowExA,  HookCreateWindowExA,  &FVPKernel->StubCreateWindowExA),
			Mp::FunctionJumpVa(DefWindowProcA,   HookDefWindowProcA,   &FVPKernel->StubDefWindowProcA),
			Mp::FunctionJumpVa(DispatchMessageA, HookDispatchMessageA, &FVPKernel->StubDispatchMessageA),
			Mp::FunctionJumpVa(GetMenuItemInfoA, HookGetMenuItemInfoA),
			Mp::FunctionJumpVa(SetMenuItemInfoA, HookSetMenuItemInfoA),
			Mp::FunctionJumpVa(MessageBoxA,      HookMessageBoxA),
		};

		Status = Mp::PatchMemory(p, countof(p));
		if (NT_FAILED(Status))
			break;
	}

	return NT_SUCCESS(Status);
}

BOOL FASTCALL UnInitialize(HMODULE hModule, BOOL ForceUnload = FALSE)
{
	return (!ForceUnload) & 1;
}

BOOL NTAPI DllEntryMain(HMODULE hModule, DWORD Reason, LPVOID lpReserved)
{
	switch (Reason)
	{
	case DLL_PROCESS_ATTACH:
		return Initialize(hModule) || UnInitialize(hModule, TRUE);

	case DLL_PROCESS_DETACH:
		return UnInitialize(hModule);
	}
	return TRUE;
}




