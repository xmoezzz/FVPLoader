#include "targetver.h"
#include <Windows.h>
#include "FVPLoaderGui.h"
#include <shellapi.h>  
#include "Launcher.h"
#include "Common.h"
#include <string>
#include "Resource.h"

using std::wstring;

#pragma comment(lib, "shell32.lib")  

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

static HBITMAP hBitmap;

ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

static bool HookWindow = false;
static bool HookMess = false;
static bool Hookfont = false;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg;
	HACCEL hAccelTable;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FVPLOADERGUI, szWindowClass, MAX_LOADSTRING);

	hBitmap = ::LoadBitmapW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP1));

	wstring FilePath;
	bool ret = ReadLogFile(FilePath, HookWindow, HookMess, Hookfont);
	if (ret)
	{
		BOOL See = FALSE;
		BOOL m_Ret = IsPEFile(FilePath.c_str(), See);
		WCHAR szFile[MAX_PATH] = { 0 };
		if (See && m_Ret)
		{
			lstrcpyW(szFile, FilePath.c_str());
			bool eRet = Launch(szFile);
			if (eRet)
			{
				PostQuitMessage(0);
			}
			else
			{
				MessageBoxW(NULL, L"Failed to CreateProcess for this executable file", L"FVPLoaderGui", MB_OK);
			}
		}
	}

	LPWSTR *szArglist = nullptr;
	int nArgs = 1;
	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if (NULL != szArglist)
	{
		if (nArgs == 2)
		{
			BOOL See = FALSE;
			BOOL m_Ret = IsPEFile(szArglist[1], See);
			if (See && m_Ret)
			{
				bool eRet = Launch(szArglist[1]);
				if (eRet)
				{
					WriteLogFile(szArglist[1], false, false, false);
					PostQuitMessage(0);
				}
				else
				{
					MessageBoxW(NULL, L"Failed to CreateProcess for this executable file", L"FVPLogGui", MB_OK);
				}
			}
			else
			{
				MessageBoxW(NULL, L"Not an executable file", L"FVPLoaderGui", MB_OK);
			}
		}
		LocalFree(szArglist);
	}

	MyRegisterClass(hInstance);
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FVPLOADERGUI));

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FVPLOADERGUI));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_FVPLOADERGUI);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

HMENU hMenuPop;
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance;

   HMENU    hMenu;
   hMenu = CreateMenu();
   hMenuPop = CreateMenu();

   AppendMenuW(hMenu, MF_STRING, IDM_INFO, TEXT("Infomation"));
   AppendMenu(hMenuPop, MF_STRING, IDM_Window, L"Change Window");
   AppendMenu(hMenuPop, MF_STRING, IDM_FONT, L"Change Font");
   AppendMenu(hMenuPop, MF_STRING, IDM_Mess, L"Change Message");
   AppendMenu(hMenu, MF_POPUP, (unsigned int)hMenuPop, L"Operation");

   hWnd = CreateWindowExW(NULL, szWindowClass, szTitle, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      0, 0, 480, 272, NULL, hMenu, hInstance, NULL);

   SetWindowTextW(hWnd, L"FVPLoaderGui v0.6[X'moe]");
   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}



void DrawBmp(HDC hDC, HBITMAP bitmap, int nWidth, int nHeight)
{
	BITMAP            bm;
	HDC hdcImage;
	HDC hdcMEM;
	hdcMEM = CreateCompatibleDC(hDC);
	hdcImage = CreateCompatibleDC(hDC);
	HBITMAP bmp = ::CreateCompatibleBitmap(hDC, nWidth, nHeight);
	GetObject(bitmap, sizeof(bm), (LPSTR)&bm);
	SelectObject(hdcMEM, bmp);
	SelectObject(hdcImage, bitmap);
	StretchBlt(hdcMEM, 0, 0, nWidth, nHeight, hdcImage, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	StretchBlt(hDC, 0, 0, nWidth, nHeight, hdcMEM, 0, 0, nWidth, nHeight, SRCCOPY);

	DeleteObject(hdcImage);
	DeleteDC(hdcImage);
	DeleteDC(hdcMEM);
}

static wchar_t strFileName[MAX_PATH] = {0};
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HMENU hmm = GetMenu(hWnd);
	HMENU hfmn = GetSubMenu(hmm, 1);

	int wmId, wmEvent;
	static HDC compDC = 0;
	static RECT rect;

	switch (message)
	{
	case WM_CREATE:
	{
		DragAcceptFiles(hWnd, TRUE);
		ULONG scrWidth = 0, scrHeight = 0;
		scrWidth = GetSystemMetrics(SM_CXSCREEN);
		scrHeight = GetSystemMetrics(SM_CYSCREEN);
		GetWindowRect(hWnd, &rect);
		rect.left = (scrWidth - rect.right) / 2;
		rect.top = (scrHeight - rect.bottom) / 2;
		SetWindowPos(hWnd, HWND_TOP, rect.left, rect.top, rect.right, rect.bottom, SWP_SHOWWINDOW);
		DrawBmp(GetDC(hWnd), hBitmap, 480, 272);
	}
	break;

	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
	{
		 wmId = LOWORD(wParam);
		 wmEvent = HIWORD(wParam);
		if(LOWORD(wParam) == IDM_INFO)
		{
			  MessageBoxW(hWnd, L"Author : X'moe\nxmoe.project@gmail.com", L"FVPLoaderGui", MB_OK);
		}
		if (LOWORD(wParam) == IDM_Window)
		{
			if (!HookWindow)
			{
				//Ignored ansi DlgProc
				//or load LocaleEmulator to solve this problem
				CheckMenuItem(hfmn, IDM_Window, MF_CHECKED);
				HookWindow = true;
			}
			else
			{
				CheckMenuItem(hfmn, IDM_Window, MF_UNCHECKED);
				HookWindow = false;
			}
		}
		else if (LOWORD(wParam) == IDM_FONT)
		{
			if (!Hookfont)
			{
				CheckMenuItem(hfmn, IDM_FONT, MF_CHECKED);
				Hookfont = true;
			}
			else
			{
				CheckMenuItem(hfmn, IDM_FONT, MF_UNCHECKED);
				Hookfont = false;
			}
		}
		else if (LOWORD(wParam) == IDM_Mess)
		{
			if (!HookMess)
			{
				CheckMenuItem(hfmn, IDM_Mess, MF_CHECKED);
				HookMess = true;
			}
			else
			{
				CheckMenuItem(hfmn, IDM_Mess, MF_UNCHECKED);
				HookMess = false;
			}
		}
	}
	break;
	case WM_ERASEBKGND:
	break;
	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wParam;

		UINT nFileNum = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		if (nFileNum == 1)
		{
			InvalidateRect(hWnd, NULL, TRUE);
			DragQueryFileW(hDrop, 0, strFileName, MAX_PATH);

			BOOL See = FALSE;
			BOOL m_Ret = IsPEFile(strFileName, See);
			if (See && m_Ret)
			{
				WriteLogFile(strFileName, HookWindow, HookMess, Hookfont);
				bool ret = Launch(strFileName);
				if (ret)
				{
					DragFinish(hDrop);
					PostQuitMessage(0);
				}
				else
				{
					DeleteFileW(L"FVPLOG.ini");
				}
			}
			else
			{
				MessageBoxW(NULL, L"Not an executable file", L"FVPLoaderGui", MB_OK);
				wmemset(strFileName, 0, MAX_PATH);
			}
		}
		else
		{
			MessageBoxW(hWnd, L"Please Drop one file on this window", L"FVPLoader", MB_OK);
			DragFinish(hDrop);
			wmemset(strFileName, 0, MAX_PATH);
		}
		return 0;
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

