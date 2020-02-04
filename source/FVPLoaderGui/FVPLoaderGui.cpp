#include "targetver.h"
#include "Resource.h"
#include <my.h>
#include <shellapi.h>  
#include "Resource.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "undoc_k32.lib")


#define MAX_LOADSTRING 100

struct
{
	WCHAR   Title[MAX_LOADSTRING];
	WCHAR   WindowClass[MAX_LOADSTRING];
	HBITMAP Bitmap;
	BOOLEAN EnableWindowHook;
	BOOLEAN EnableMessageBoxHook;
	BOOLEAN EnableFontHook;
}static FVPLoader;

LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL FASTCALL InitInstance(HINSTANCE hInstance)
{
   HWND     hWnd;
   HMENU    hMenu, hMenuPop;

   hMenu    = CreateMenu();
   hMenuPop = CreateMenu();

   AppendMenuW(hMenu,    MF_STRING, IDM_INFO, TEXT("Infomation"));

   hWnd = CreateWindowExW(NULL, FVPLoader.WindowClass, FVPLoader.Title, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      0, 0, 480, 272, NULL, hMenu, hInstance, NULL);

   if (!hWnd)
	   return FALSE;

   SetWindowTextW(hWnd, L"FVPLoaderGui v0.7-re[X'moe]");
   ShowWindow(hWnd, SW_SHOW);
   UpdateWindow(hWnd);

   return TRUE;
}


ATOM FASTCALL MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = WndProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_FVPLOADERGUI));
	wcex.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName  = MAKEINTRESOURCEW(IDC_FVPLOADERGUI);
	wcex.lpszClassName = FVPLoader.WindowClass;
	wcex.hIconSm       = LoadIconW(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}


#define LOG_SIG "STmoeSTmoechu>_<"

BOOL FASTCALL ReadLogFile(LPWSTR ExePath, ULONG ccBuffer, BOOLEAN* bWindow, BOOLEAN* bMessageBox, BOOLEAN* bFont)
{
	NTSTATUS   Status;
	NtFileDisk File;
	BYTE       Mark[0x10];
	ULONG      Length;


	Status = STATUS_SUCCESS;
	LOOP_ONCE
	{
		Status = File.Open(L"FVPLOG.ini");
		if (NT_FAILED(Status))
			break;

		Status = File.Read(Mark, sizeof(Mark));
		if (NT_FAILED(Status))
			break;

		if (RtlCompareMemory(Mark, LOG_SIG, sizeof(Mark)) != sizeof(Mark))
			break;

		RtlZeroMemory(ExePath, ccBuffer * sizeof(ExePath[0]));
		Status = File.Read(&Length, sizeof(Length));
		if (NT_FAILED(Status))
			break;

		Status = STATUS_BUFFER_OVERFLOW;
		if (Length > MAX_PATH || Length > ccBuffer)
			break;

		Status = File.Read(ExePath, Length * 2);
		if (NT_FAILED(Status))
			break;

		Status = File.Seek(20 + Length * 2, FILE_BEGIN);
		if (NT_FAILED(Status))
			break;

		Status = File.Read(bWindow, 1);
		if (NT_FAILED(Status))
			break;

		Status = File.Read(bMessageBox, 1);
		if (NT_FAILED(Status))
			break;

		Status = File.Read(bFont, 1);
		if (NT_FAILED(Status))
			break;
	}

	File.Close();
	return NT_SUCCESS(Status);
}

BOOL WriteLogFile(LPCWSTR ExePath, BOOLEAN bWindow, BOOLEAN bMessage, BOOLEAN bFont)
{
	NTSTATUS   Status;
	NtFileDisk File;
	LONG_PTR   Length;
	BOOLEAN    Enable, Disable;

	Enable  = TRUE;
	Disable = FALSE;
	Status  = STATUS_SUCCESS;

	LOOP_ONCE
	{
		Status = File.Create(L"FVPLOG.ini");
		if (NT_FAILED(Status))
			break;

		Status = File.Write(LOG_SIG, strlen(LOG_SIG));
		if (NT_FAILED(Status))
			break;

		Length = wcslen(ExePath);
		Status = File.Write(&Length, sizeof(DWORD));
		if (NT_FAILED(Status))
			break;

		Status = File.Write((PVOID)ExePath, Length * 2);
		if (NT_FAILED(Status))
			break;

		if (bWindow)
			Status = File.Write(&Enable, 1);
		else
			Status = File.Write(&Disable, 1);

		if (NT_FAILED(Status))
			break;
		
		if (bMessage)
			Status = File.Write(&Enable, 1);
		else
			File.Write(&Disable, 1);

		if (NT_FAILED(Status))
			break;

		if (bFont)
			Status = File.Write(&Enable, 1);
		else
			Status = File.Write(&Disable, 1);

		if (NT_FAILED(Status))
			break;
	}

	File.Close();
	return NT_SUCCESS(Status);
}



typedef
BOOL
(WINAPI
*FuncCreateProcessInternalW)(
HANDLE                  hToken,
LPCWSTR                 lpApplicationName,
LPWSTR                  lpCommandLine,
LPSECURITY_ATTRIBUTES   lpProcessAttributes,
LPSECURITY_ATTRIBUTES   lpThreadAttributes,
BOOL                    bInheritHandles,
ULONG                   dwCreationFlags,
LPVOID                  lpEnvironment,
LPCWSTR                 lpCurrentDirectory,
LPSTARTUPINFOW          lpStartupInfo,
LPPROCESS_INFORMATION   lpProcessInformation,
PHANDLE                 phNewToken
);

BOOL
(WINAPI
*StubCreateProcessInternalW)(
HANDLE                  hToken,
LPCWSTR                 lpApplicationName,
LPWSTR                  lpCommandLine,
LPSECURITY_ATTRIBUTES   lpProcessAttributes,
LPSECURITY_ATTRIBUTES   lpThreadAttributes,
BOOL                    bInheritHandles,
ULONG                   dwCreationFlags,
LPVOID                  lpEnvironment,
LPCWSTR                 lpCurrentDirectory,
LPSTARTUPINFOW          lpStartupInfo,
LPPROCESS_INFORMATION   lpProcessInformation,
PHANDLE                 phNewToken
);

BOOL
WINAPI
VMeCreateProcess(
HANDLE                  hToken,
LPCWSTR                 lpApplicationName,
LPWSTR                  lpCommandLine,
LPCWSTR                 lpDllPath,
LPSECURITY_ATTRIBUTES   lpProcessAttributes,
LPSECURITY_ATTRIBUTES   lpThreadAttributes,
BOOL                    bInheritHandles,
ULONG                   dwCreationFlags,
LPVOID                  lpEnvironment,
LPCWSTR                 lpCurrentDirectory,
LPSTARTUPINFOW          lpStartupInfo,
LPPROCESS_INFORMATION   lpProcessInformation,
PHANDLE                 phNewToken
)
{
	BOOL             Result, IsSuspended;
	UNICODE_STRING   FullDllPath;

	RtlInitUnicodeString(&FullDllPath, lpDllPath);

	IsSuspended = !!(dwCreationFlags & CREATE_SUSPENDED);
	dwCreationFlags |= CREATE_SUSPENDED;
	Result = StubCreateProcessInternalW(
		hToken,
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation,
		phNewToken);

	if (!Result)
		return Result;

	InjectDllToRemoteProcess(
		lpProcessInformation->hProcess,
		lpProcessInformation->hThread,
		&FullDllPath,
		IsSuspended
		);

	NtResumeThread(lpProcessInformation->hThread, NULL);

	return TRUE;
}

#define KERNEL_DLL L"FVPKernel.dll"

FORCEINLINE BOOL CheckDll()
{
	DWORD  Attribute;

	Attribute = GetFileAttributesW(KERNEL_DLL);
	return Attribute != 0xFFFFFFFF && (!(Attribute & FILE_ATTRIBUTE_DIRECTORY));
}


BOOL FASTCALL CreateProcessInternalWithDll(LPCWSTR ProcessName)
{
	STARTUPINFOW        si;
	PROCESS_INFORMATION pi;

	RtlZeroMemory(&si, sizeof(si));
	RtlZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	return VMeCreateProcess(NULL, ProcessName, NULL, KERNEL_DLL, NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi, NULL);
}



int NTAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR     lpCmdLine,
	int       nShowCmd
)
{
	MSG          Message;
	HACCEL       AccelTable;
	HINSTANCE    Instance;
	LPWSTR*      Argv;
	LONG_PTR     Argc;
	BOOL         Success;
	BOOL         HasParameter;
	WCHAR        ExeFullPath[MAX_PATH];
	BOOLEAN      PatchWindow, PatchMessageBox, PatchFont;

	StubCreateProcessInternalW = (FuncCreateProcessInternalW)EATLookupRoutineByHashPNoFix(GetKernel32Handle(), KERNEL32_CreateProcessInternalW);

	FVPLoader.EnableWindowHook     = FALSE;
	FVPLoader.EnableMessageBoxHook = FALSE;
	FVPLoader.EnableFontHook       = FALSE;
	
	Instance = (HINSTANCE)GetModuleHandleW(nullptr);

	LoadStringW(Instance, IDS_APP_TITLE, FVPLoader.Title, MAX_LOADSTRING);
	LoadStringW(Instance, IDC_FVPLOADERGUI, FVPLoader.WindowClass, MAX_LOADSTRING);

	FVPLoader.Bitmap = LoadBitmapW(Instance, MAKEINTRESOURCE(IDB_BITMAP1));
	Argv             = CmdLineToArgv(::GetCommandLineW(), &Argc);

	switch (Argc)
	{
	case 1:
	default:
		HasParameter = FALSE;
		break;

	case 2:
		HasParameter = TRUE;
		break;
	}

	Success = ReadLogFile(ExeFullPath, countof(ExeFullPath), &PatchWindow, &PatchMessageBox, &PatchFont);
	if (Success)
	{
		Success = CreateProcessInternalWithDll(ExeFullPath);
		if (Success)
		{
			ReleaseArgv(Argv);
			Ps::ExitProcess(0);
		}
		else
		{
			MessageBoxW(NULL, L"Failed to CreateProcess", L"FVPLoader", MB_OK);
		}
	}

	if (HasParameter)
	{
		Success = CreateProcessInternalWithDll(Argv[1]);
		if (Success)
		{
			WriteLogFile(Argv[1], FALSE, FALSE, FALSE);
			ReleaseArgv(Argv);
			Ps::ExitProcess(0);
		}
		else
		{
			MessageBoxW(NULL, L"Failed to CreateProcess", L"FVPLoader", MB_OK);
		}
	}

	MyRegisterClass(Instance);
	if (!InitInstance (Instance))
	{
		ReleaseArgv(Argv);
		Ps::ExitProcess(0);
	}

	AccelTable = LoadAcceleratorsW(Instance, MAKEINTRESOURCE(IDC_FVPLOADERGUI));

	while (GetMessageW(&Message, NULL, 0, 0))
	{
		if (!TranslateAcceleratorW(Message.hwnd, AccelTable, &Message))
		{
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
		}
	}

	ReleaseArgv(Argv);
	ExitProcess(Message.wParam);
}




VOID FASTCALL DrawBmp(HDC hDC, HBITMAP Bitmap, ULONG nWidth, ULONG nHeight)
{
	BITMAP        bm;
	HDC           hdcImage;
	HDC           hdcMEM;
	HBITMAP       bmp;

	hdcMEM      = CreateCompatibleDC(hDC);
	hdcImage    = CreateCompatibleDC(hDC);
	bmp         = CreateCompatibleBitmap(hDC, nWidth, nHeight);

	GetObjectW(Bitmap, sizeof(bm), (LPSTR)&bm);
	SelectObject(hdcMEM, bmp);
	SelectObject(hdcImage, Bitmap);
	StretchBlt(hdcMEM, 0, 0, nWidth, nHeight, hdcImage, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	StretchBlt(hDC, 0, 0, nWidth, nHeight, hdcMEM, 0, 0, nWidth, nHeight, SRCCOPY);

	DeleteObject(hdcImage);
	DeleteDC(hdcImage);
	DeleteDC(hdcMEM);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WORD   WindowId, WindowEvent;
	BOOL   Success;
	HMENU  WindowMenu, SubMemu;

	WindowMenu = GetMenu(hWnd);
	SubMemu    = GetSubMenu(WindowMenu, 1);

	switch (message)
	{
	case WM_CREATE:
	{
		RECT  Rect;
		ULONG Width, Height;

		DragAcceptFiles(hWnd, TRUE);
		
		Width  = GetSystemMetrics(SM_CXSCREEN);
		Height = GetSystemMetrics(SM_CYSCREEN);
		GetWindowRect(hWnd, &Rect);
		Rect.left = (Width - Rect.right) / 2;
		Rect.top  = (Height - Rect.bottom) / 2;
		SetWindowPos(hWnd, HWND_TOP, Rect.left, Rect.top, Rect.right, Rect.bottom, SWP_SHOWWINDOW);
		DrawBmp(GetDC(hWnd), FVPLoader.Bitmap, 480, 272);
	}
	break;

	case WM_INITDIALOG:
		break;
	case WM_COMMAND:
	{
		WindowId    = LOWORD(wParam);
		WindowEvent = HIWORD(wParam);

		switch (WindowId)
		{
		case IDM_INFO:
			MessageBoxW(hWnd, L"Author : X'moe\nxmoe.project@gmail.com", L"FVPLoaderGui", MB_OK);
			break;
		}
	}
	break;

	case WM_ERASEBKGND:
		break;

	case WM_DROPFILES:
	{
		WCHAR FilePath[MAX_PATH];
		HDROP hDrop = (HDROP)wParam;
		UINT  nFileNum = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);

		RtlZeroMemory(FilePath, sizeof(FilePath));
		if (nFileNum == 1)
		{
			InvalidateRect(hWnd, NULL, TRUE);
			DragQueryFileW(hDrop, 0, FilePath, MAX_PATH);
			Success = WriteLogFile(FilePath, FVPLoader.EnableWindowHook, FVPLoader.EnableMessageBoxHook, FVPLoader.EnableFontHook);
			Success = CreateProcessInternalWithDll(FilePath);
			if (Success)
			{
				DragFinish(hDrop);
				PostQuitMessage(0);
			}
			else
			{
				Io::DeleteFileW(L"FVPLOG.ini");
			}
		}
		else
		{
			MessageBoxW(hWnd, L"Please Drop an executable file on this window", L"FVPLoader", MB_OK);
			DragFinish(hDrop);
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

