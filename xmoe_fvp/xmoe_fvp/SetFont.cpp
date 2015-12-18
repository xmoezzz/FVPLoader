#include "SetFont.h"
#include "resource.h"
#include "Common.h"
#include <vector>
#include <string>

using std::vector;
using std::wstring;

vector<wstring> FontNamePool;
static WCHAR FontName[64] = { 0 };
BOOL SelectOK = FALSE;
HINSTANCE hDllModule = nullptr;

HFONT NTAPI HookCreateFontA(
	_In_ int     nHeight,
	_In_ int     nWidth,
	_In_ int     nEscapement,
	_In_ int     nOrientation,
	_In_ int     fnWeight,
	_In_ DWORD   fdwItalic,
	_In_ DWORD   fdwUnderline,
	_In_ DWORD   fdwStrikeOut,
	_In_ DWORD   fdwCharSet,
	_In_ DWORD   fdwOutputPrecision,
	_In_ DWORD   fdwClipPrecision,
	_In_ DWORD   fdwQuality,
	_In_ DWORD   fdwPitchAndFamily,
	_In_ LPCSTR  lpszFace
	)
{
	LOGFONTW Font = { 0 };

	Font.lfHeight = nHeight;
	Font.lfWeight = nWidth;
	Font.lfEscapement = nEscapement;
	Font.lfOrientation = nOrientation;
	Font.lfWeight = fnWeight;
	Font.lfItalic = fdwItalic;
	Font.lfUnderline = fdwUnderline;
	Font.lfStrikeOut = fdwStrikeOut;
	Font.lfCharSet = fdwCharSet;
	Font.lfOutPrecision = fdwOutputPrecision;
	Font.lfClipPrecision = fdwClipPrecision;
	Font.lfQuality = fdwQuality;
	Font.lfPitchAndFamily = fdwPitchAndFamily;
	
	lstrcpyW(Font.lfFaceName, FontName[0] ? FontName : L"");

	return CreateFontIndirectW(&Font);
}


INT_PTR CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL SetFont(HMODULE hModule)
{
	//Check firstly

	hDllModule = hModule;
	HWND hWnd = NULL;
	hWnd = CreateDialogParamW(hModule, MAKEINTRESOURCEW(IDD_DIALOG1), /*GetDesktopWindow()*/ NULL, WndProc, WM_INITDIALOG);
	if (hWnd == nullptr)
	{
		MessageBoxW(NULL, L"Unable to Create window!", L"FATAL", MB_OK);
		return FALSE;
	}
	SetWindowLongW(hWnd, GWL_EXSTYLE, WS_EX_LAYERED);
	SetLayeredWindowAttributes(hWnd, NULL, 225, LWA_ALPHA);
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessageW(&msg, NULL, NULL, NULL))
	{
		if (msg.message == WM_KEYDOWN)
		{
			SendMessageW(hWnd, msg.message, msg.wParam, msg.lParam);
		}
		else if (!IsDialogMessageW(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	if (FontName[0] && SelectOK)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}



BOOL InstallFontByIAT()
{
	PROC pfCreateFontA = GetProcAddress(GetModuleHandleW(L"Gdi32.dll"), "CreateFontA");
	return StartHook("Gdi32.dll", pfCreateFontA, (PROC)HookCreateFontA);
}


int CALLBACK EnumFontFamProc(LPENUMLOGFONT lpelf, LPNEWTEXTMETRIC lpntm, DWORD nFontType, long lparam)
{
	FontNamePool.push_back(lpelf->elfLogFont.lfFaceName);
	return 1;
}


INT_PTR CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT Painter;
	switch (message)
	{

	case WM_INITDIALOG:
	{
		SendMessageW(hWnd, WM_SETICON, ICON_SMALL,
			(LPARAM)LoadIconW(hDllModule, MAKEINTRESOURCEW(IDI_ICON1)));

		LOGFONTW lf = { 0 };
		lf.lfCharSet = SHIFTJIS_CHARSET;

		EnumFontFamiliesExW(GetDC(hWnd), &lf,
			(FONTENUMPROC)EnumFontFamProc, (LPARAM)nullptr, 0);

		HWND hSubWnd = GetDlgItem(hWnd, IDC_COMBO1);
		for (ULONG i = 0; i <= (ULONG)FontNamePool.size(); i++)
		{
			WCHAR Info[MAX_PATH] = { 0 };
			lstrcpyW(Info, FontNamePool[i].c_str());
			SendMessageW(hSubWnd, CB_INSERTSTRING, i - 1, (LPARAM)Info);
		}
		SendMessageW(hSubWnd, CB_SETCURSEL, 0, 0);
	}
	break;

	case WM_SYSCOMMAND:
	{
		WORD wmIdsys = LOWORD(wParam);
		WORD wmEventsys = HIWORD(wParam);
		switch (wmIdsys)
		{
		case SC_CLOSE:
			DestroyWindow(hWnd);
			break;

		default:
			break;
		}
	}
	break;

	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		//Font select
		case IDC_COMBO1:
		{
			switch (wmEvent)
			{
			case CBN_SELCHANGE:
			{

				HWND hSubWnd = GetDlgItem(hWnd, IDC_COMBO1);
				ULONG SelectCur = SendMessageW(hSubWnd, CB_GETCURSEL, 0, 0);
				RtlZeroMemory(FontName, 64 * sizeof(WCHAR));
				lstrcpyW(FontName, FontNamePool[SelectCur].c_str());
			}
			break;
			default:
				break;
			}
		}
		break;

		case IDOK:
		{
			switch (wmEvent)
			{
			case BN_CLICKED:
			{
				SelectOK = TRUE;
				DestroyWindow(hWnd);
				return TRUE;
			}
			break;

			default:
				break;
			}
		}

		default:
			break;
		}
		break;

	case WM_DESTROY:
		DestroyWindow(hWnd);
		return 1;
		break;
	default:
		break;
	}
	return 0;
}

