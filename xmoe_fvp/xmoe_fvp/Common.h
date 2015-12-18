#ifndef __Common_H__
#define __Common_H__

#include <windows.h>
#include <string>

using std::wstring;

BOOL ReadLogFile(wstring& Path, bool& HookWin, bool& HookMess, bool& HookFont);
BOOL ReadFontName(wstring& Name);
BOOL WriteFontName(wstring& Name);

BOOL StartHook(LPCSTR szProcName, PROC pfnOrg, PROC pfnNew);

int NTAPI lstrcmpiEX(LPCSTR lpString1, LPCSTR lpString2);

HWND NTAPI MyCreateWindowExA(
	DWORD dwExStyle,
	LPCSTR lpClassName,
	LPCSTR lpWindowName,
	DWORD dwStyle,
	int x,
	int y,
	int nWidth,
	int nHeight,
	HWND hWndParent,
	HMENU hMenu,
	HINSTANCE hInstance,
	LPVOID lpParam
	);


int NTAPI MyMessageBoxA(
	_In_opt_  HWND hWnd,
	_In_opt_  LPCSTR lpText,
	_In_opt_  LPCSTR lpCaption,
	_In_      UINT uType
	);

BOOL WINAPI InstallFont();
BOOL WINAPI HookMessage();

BOOL NTAPI HookGetMenuItemInfo(HMENU hmenu, UINT item, BOOL fByPosition, int a4);
BOOL NTAPI HookSetMenuItemInfo(HMENU hmenu, UINT item, BOOL fByPositon, LPMENUITEMINFOW lpmii);

#endif
