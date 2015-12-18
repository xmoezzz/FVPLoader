#pragma comment(linker, "/SECTION:.text,ERW /MERGE:.rdata=.text /MERGE:.data=.text")
#pragma comment(linker, "/SECTION:.Xmoe,ERW /MERGE:.text=.Xmoe")

#include "Common.h"
#include "SetFont.h"

#pragma comment(lib, "User32.lib")

static bool HookWindow = false;
static bool HookMess = false;
static bool HookFont = false;

static bool bOk = false;
wstring Path;

BOOL APIENTRY DllMain( HMODULE hModule, ULONG reason, LPVOID Reserved )
{
	UNREFERENCED_PARAMETER(Reserved);

	if (reason == DLL_PROCESS_ATTACH)
	{
		FARPROC pflstrcmpiA        = GetProcAddress(GetModuleHandleW(L"KERNEL32.dll"), "lstrcmpiA");
		FARPROC pfCreateWindowExA  = GetProcAddress(GetModuleHandleW(L"User32.dll"), "CreateWindowExA");
		FARPROC pfMessageBoxA      = GetProcAddress(GetModuleHandleW(L"User32.dll"), "MessageBoxA");
		FARPROC pfSetMenuItemInfoA = GetProcAddress(GetModuleHandleW(L"User32.dll"), "SetMenuItemInfoA");
		FARPROC pfGetMenuItemInfoA = GetProcAddress(GetModuleHandleW(L"User32.dll"), "GetMenuItemInfoA");

		if (FALSE == StartHook("KERNEL32.dll", pflstrcmpiA, (PROC)lstrcmpiEX))
		{
			MessageBoxW(NULL, L"Error Code[1]\n"
				              L"If you're sure this bug is caused by my tool\n"
							  L"Please contact me:\n"
							  L"xmoe.project@gmail.com", L"FVPLoaderGui", MB_OK);

			DeleteFileW(L"FVPLOG.ini");
		}
		bOk = ReadLogFile(Path, HookWindow, HookMess, HookFont);
		if (bOk)
		{
			if (HookMess)
			{
				if (FALSE == HookMessage())
				{
					MessageBoxW(NULL, L"Error Code[2]\n"
					L"If you're sure this bug is caused by my tool\n"
					L"Please contact me:\n"
					L"xmoe.project@gmail.com", L"FVPLoaderGui", MB_OK);

					DeleteFileW(L"FVPLOG.ini");
				}
			}
			if (HookWindow)
			{
				StartHook("User32.dll", pfSetMenuItemInfoA, (PROC)HookSetMenuItemInfo);
				StartHook("User32.dll", pfGetMenuItemInfoA, (PROC)HookGetMenuItemInfo);
				if (FALSE == StartHook("User32.dll", pfCreateWindowExA, (PROC)MyCreateWindowExA))
				{
					MessageBoxW(NULL,   L"Error Code[2]\n"
										L"If you're sure this bug is caused by my tool\n"
										L"Please contact me:\n"
										L"xmoe.project@gmail.com", L"FVPLoaderGui", MB_OK);

					DeleteFileW(L"FVPLOG.ini");
				}
			}
			if (HookFont)
			{
				if (FALSE == InstallFont())
				{
					if (SetFont(hModule))
					{
						if (!InstallFontByIAT())
						{
							DeleteFileW(L"FVPLOG.ini");
						}
					}
				}
			}
		}
	}
	return TRUE;
}

