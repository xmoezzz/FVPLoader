#include <Windows.h>
#include "detours.h"
#include <Psapi.h>

#pragma comment(lib, "detours.lib")
#pragma comment(lib, "Psapi.lib")

BOOL StartHook(LPCSTR szDllName, PROC pfnOrg, PROC pfnNew)
{
	HMODULE hmod;
	LPCSTR szLibName;
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
	PIMAGE_THUNK_DATA pThunk;
	DWORD dwOldProtect, dwRVA;
	PBYTE pAddr;

	hmod = GetModuleHandleW(NULL);
	pAddr = (PBYTE)hmod;
	pAddr += *((DWORD*)&pAddr[0x3C]);
	dwRVA = *((DWORD*)&pAddr[0x80]);

	pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)hmod + dwRVA);
	for (; pImportDesc->Name; pImportDesc++)
	{
		szLibName = (LPCSTR)((DWORD)hmod + pImportDesc->Name);
		if (!stricmp(szLibName, szDllName))
		{
			pThunk = (PIMAGE_THUNK_DATA)((DWORD)hmod + pImportDesc->FirstThunk);
			for (; pThunk->u1.Function; pThunk++)
			{
				if (pThunk->u1.Function == (DWORD)pfnOrg)
				{
					VirtualProtect((LPVOID)&pThunk->u1.Function, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect);
					pThunk->u1.Function = (DWORD)pfnNew;
					VirtualProtect((LPVOID)&pThunk->u1.Function, 4, dwOldProtect, &dwOldProtect);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

//lstrcmpiA
int NTAPI lstrcmpiEX(LPCSTR lpString1, LPCSTR lpString2)
{
	int ret = CompareStringA(0x411, 1, lpString1, -1, lpString2, -1);

	//mac os
	//int ret = MyCompareStringA(0x411, 1, lpString1, -1, lpString2, -1);
	return ret - 2;
}

int WINAPI MyMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	WCHAR WideTitle[1024] = { 0 };
	WCHAR WideInfo[1024]  = { 0 };
	MultiByteToWideChar(932, 0, lpCaption, lstrlenA(lpCaption), WideTitle, 1024);
	MultiByteToWideChar(932, 0, lpText,    lstrlenA(lpText),    WideInfo,  1024);
	return MessageBoxW(hWnd, WideInfo, WideTitle, uType);
}

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
	)
{
	WCHAR WideTitle[500] = {0};
	MultiByteToWideChar(932, 0, lpWindowName, lstrlenA(lpWindowName), WideTitle, 500);
	HWND hwnd = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	SetWindowTextW(hwnd, WideTitle);
	return hwnd;
}

FORCEINLINE VOID SetNopCode(PBYTE Addr, ULONG size)
{
	ULONG oldProtect;
	VirtualProtect((PVOID)Addr, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	for (ULONG i = 0; i<size; i++)
	{
		Addr[i] = 0x90;
	}
}

ULONG GetDataLen(LPVOID pBaseaddr, LPVOID pReadBuf)
{
	LPBYTE pBase = (LPBYTE)pBaseaddr;
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pReadBuf;
	ULONG uSize = 0;
	PIMAGE_SECTION_HEADER    pSec = (PIMAGE_SECTION_HEADER)(pBase + pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	for (int i = 0; i<PIMAGE_FILE_HEADER(pBase + pDosHeader->e_lfanew + 4)->NumberOfSections; ++i)
	{
		if (!strcmp((char*)pSec[i].Name, ".text"))
		{
			uSize += pSec[i].SizeOfRawData;
			break;
		}
	}
	return uSize;
}

//If the target compiler won't change....
//                            0    1     2     3     4     5     6
static BYTE ShellCode[] = { 0x83, 0xD8, 0xFF, 0x85, 0xC0, 0x75, 0x3F};

DWORD WINAPI FindShellCode(LPVOID lpParam)
{
	DWORD oldProtect;
	ULONG size = GetDataLen(GetModuleHandleW(NULL), GetModuleHandleW(NULL));
	if (size)
	{
		VirtualProtect((PVOID)GetModuleHandleW(NULL), size, PAGE_EXECUTE_READWRITE, &oldProtect);
		BYTE *start = (PBYTE)GetModuleHandleW(NULL);

		size_t Strlen = strlen((const char*)ShellCode);
		DWORD iPos = 0;
		DWORD zPos = 0;
		bool Found = false;
		while (iPos < size)
		{
			if (zPos == Strlen - 1)
			{
				Found = true;
				break;
			}
			if (start[iPos] == ShellCode[zPos])
			{
				iPos++;
				zPos++;
			}
			else
			{
				iPos++;
				zPos = 0;
			}
		}
		if (Found)
		{
			SetNopCode(start + iPos - 1, 2);
		}
	}
	return 1;
}

int SearchDataFromProcessByDllName(BYTE* pSearch, int size)
{
	int i, j;
	DWORD OldProtect;
	BYTE* pOrg;
	BYTE* pPare;
	MODULEINFO mMoudleInfo;
	HMODULE  hMoudle;

	hMoudle = GetModuleHandleW(NULL);
	pOrg = (BYTE*)hMoudle;

	VirtualProtectEx(GetCurrentProcess(), hMoudle, 1, PAGE_EXECUTE_READWRITE, &OldProtect);
	GetModuleInformation(GetCurrentProcess(), hMoudle, &mMoudleInfo, sizeof(mMoudleInfo));
	for (i = 0; i <(int)mMoudleInfo.SizeOfImage; i++)
	{
		pPare = pOrg + i;

		for (j = 0; j < size; j++)
		{
			if (pPare[j] != pSearch[j])
			{
				break;
			}
		}
		if (j == size)
		{
			VirtualProtectEx(GetCurrentProcess(), hMoudle, 1, OldProtect, NULL);

			return (int)(pPare);
		}
	}
	VirtualProtectEx(GetCurrentProcess(), hMoudle, 1, OldProtect, NULL);
	return 0;
}

void SetNopCodeFont(BYTE* pnop)
{
	DWORD oldProtect;
	VirtualProtect((PVOID)pnop, 2, PAGE_EXECUTE_READWRITE, &oldProtect);

	if (pnop[0] == ShellCode[5] && pnop[1] == ShellCode[6])
	{
		for (size_t i = 0; i < 2; i++)
		{
			pnop[i] = 0x90;
		}
	}
}

DWORD WINAPI GetFunAddr(LPVOID LpParam)
{
	int Addr;
	Addr = SearchDataFromProcessByDllName(ShellCode, sizeof(ShellCode));

	if (0 == Addr)
	{
		return false;
	}
	DWORD g_Addr_Function = Addr - 2 + 7;
	SetNopCodeFont((PBYTE)g_Addr_Function);
	return true;
}

BOOL WINAPI InstallFont()
{
	GetFunAddr(NULL);
	return TRUE;
}

typedef int (WINAPI *PfuncMessageBoxA)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

BOOL WINAPI HookMessage()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	PVOID g_pOldMessgeBoxA = DetourFindFunction("User32.dll", "MessageBoxA");
	if (g_pOldMessgeBoxA)
	{
		DetourAttach(&g_pOldMessgeBoxA, MyMessageBoxA);
		DetourTransactionCommit();
		return TRUE;
	}
	else
		return FALSE;
}


BOOL NTAPI MySetMenuItemInfo(HMENU hMenu, UINT uItem, BOOL fByPosition, LPMENUITEMINFOA lpmii)
{
	return SetMenuItemInfoA(hMenu, uItem, fByPosition, lpmii);
}

LPVOID AllocateHeapInternal(SIZE_T dwBytes)
{
	return HeapAlloc(GetProcessHeap(), 0, dwBytes);
}

BOOL FreeStringInternal(LPVOID lpMem)
{
	return HeapFree(GetProcessHeap(), 0, lpMem);
}

LPVOID AllocateZeroedMemory(SIZE_T dwBytes)
{
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytes);
}


LPVOID NTAPI MultiByteToWideCharInternal(LPCSTR lpString)
{
	LPCSTR    v1;
	char      v2;
	int       v3;
	LPVOID    v4;
	int       v5;
	int       v6;

	v1 = lpString;
	do
	{
		v2 = *v1++;
	}
	while (v2);

	v3 = v1 - lpString;
	v4 = AllocateHeapInternal(2 * (v1 - lpString));
	v5 = 0;
	v6 = v3 - 1;
	if (v6)
	{
		v5 = ((int(__stdcall *)(DWORD, DWORD, LPCSTR, int, LPVOID, int))MultiByteToWideChar)(
			932,
			0,
			lpString,
			v6,
			v4,
			v6);
	}
	*((WORD *)v4 + v5) = 0;
	return v4;
}

BOOL NTAPI HookGetMenuItemInfo(HMENU hmenu, UINT item, BOOL fByPosition, int a4)
{
	int v4;
	int v5;
	int v6;
	BOOL result;
	int v8;
	BOOL v9;
	int v10;
	unsigned int v11;
	char v12;
	int v13;
	int v14;
	int v15;
	int cchtmp;

	v4 = 0;
	v5 = *(DWORD *)a4 >> 2;
	RtlCopyMemory(&v11, (const void *)a4, 4 * v5);
	v6 = (int)(&v11 + v5);
	if (v12 & MIIM_STRING || (v12 & MIIM_TYPE || *(WORD *)&v12 & MIIM_FTYPE) && !v13)
	{
		cchtmp = v15;
		v4 = (int)AllocateZeroedMemory(0);
		v14 = v4;
	}
	result = GetMenuItemInfoW(hmenu, item, fByPosition, (LPMENUITEMINFOW)&v11);
	if (result)
	{
		v8 = *(DWORD *)(a4 + 36);
		memcpy((void *)a4, &v11, 4 * (v11 >> 2));
		v6 = a4;
		if (!v4)
		{
			return result;
		}
		*(DWORD *)(a4 + 36) = v8;
		result = ((int(NTAPI *)(DWORD, DWORD, int, signed int, int, DWORD, DWORD, DWORD))WideCharToMultiByte)(
			932,
			0,
			v4,
			-1,
			v8,
			*(DWORD *)(a4 + 40),
			0,
			0);
		if (result)
		{
			--result;
			*(DWORD *)(a4 + 40) = result;
		}
	}
	if (v4)
	{
		v9 = result;
		FreeStringInternal((LPVOID)v4);
		result = v9;
		v10 = cchtmp - 1;
		*(BYTE *)(*(DWORD *)(v6 + 36) + cchtmp - 1) = v9;
		*(DWORD *)(v6 + 40) = v10;
	}
	return result;
}


BOOL NTAPI HookSetMenuItemInfo(HMENU hmenu, UINT item, BOOL fByPositon, LPMENUITEMINFOW lpmii)
{
	WCHAR*   v4;
	UINT     v5;
	WCHAR*   v6;
	BOOL     result;
	BOOL     v8;

	v4 = 0;
	v5 = lpmii->fMask;
	v6 = lpmii->dwTypeData;
	if (v5 & MIIM_STRING || (v5 & MIIM_TYPE || v5 & MIIM_FTYPE) && !lpmii->fType)
	{
		if (v6)
		{
			v4 = (WCHAR *)MultiByteToWideCharInternal((LPCSTR)lpmii->dwTypeData);
			lpmii->dwTypeData = v4;
		}
	}
	result = SetMenuItemInfoW(hmenu, item, fByPositon, lpmii);
	lpmii->dwTypeData = v6;
	if (v4)
	{
		v8 = result;
		FreeStringInternal(v4);
		result = v8;
	}
	return result;
}

