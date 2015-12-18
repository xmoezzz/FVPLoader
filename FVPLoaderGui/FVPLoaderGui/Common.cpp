#include "Common.h"

bool FileIsExist(const wchar_t* FileName)
{
	HANDLE fileHandle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	bool ret = true;
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		ret = false;
	}
	CloseHandle(fileHandle);
	return ret;
}

LPVOID GetDosHeader(LPVOID lpFile)
{
	assert(lpFile != NULL);
	PIMAGE_DOS_HEADER pDosHeader = NULL;
	if (lpFile != NULL)
		pDosHeader = (PIMAGE_DOS_HEADER)lpFile;

	return (LPVOID)pDosHeader;
}

LPVOID GetNtHeader(LPVOID lpFile, BOOL& bX64)
{
	assert(lpFile != NULL);
	bX64 = FALSE;
	PIMAGE_NT_HEADERS32 pNtHeader32 = NULL;
	PIMAGE_NT_HEADERS64 pHeaders64 = NULL;

	PIMAGE_DOS_HEADER pDosHeader = NULL;
	if (lpFile != NULL)
		pDosHeader = (PIMAGE_DOS_HEADER)GetDosHeader(lpFile);
	
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL;

	pNtHeader32 = (PIMAGE_NT_HEADERS32)((DWORD)pDosHeader + pDosHeader->e_lfanew);
	
	if (pNtHeader32->Signature != IMAGE_NT_SIGNATURE)
		return NULL;

	if (pNtHeader32->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) //64bit
	{
		bX64 = TRUE;
		pHeaders64 = (PIMAGE_NT_HEADERS64)((DWORD)pDosHeader + pDosHeader->e_lfanew);
		return pHeaders64;
	}

	return pNtHeader32;
}

//获得可选头
LPVOID GetOptionHeader(LPVOID lpFile, BOOL& bX64)
{
	assert(lpFile != NULL);
	bX64 = FALSE;
	LPVOID pOptionHeader = NULL;
	BOOL bX64Nt = FALSE;

	LPVOID pNtHeader = (LPVOID)GetNtHeader(lpFile, bX64Nt);
	if (pNtHeader == NULL)
		return NULL;

	if (bX64Nt) //64bit
	{
		bX64 = TRUE;
		pOptionHeader = (LPVOID)PIMAGE_OPTIONAL_HEADER64((DWORD)pNtHeader + sizeof(IMAGE_FILE_HEADER)+sizeof(DWORD));
	}
	else
	{
		pOptionHeader = (LPVOID)PIMAGE_OPTIONAL_HEADER32((DWORD)pNtHeader + sizeof(IMAGE_FILE_HEADER)+sizeof(DWORD));
	}
	return pOptionHeader;
}

BOOL IsDigiSigEX(HANDLE hFile)
{
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMapping == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}
	LPVOID lpFile = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (lpFile == NULL)
	{
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return FALSE;
	}

	IMAGE_DATA_DIRECTORY secData = { 0 };
	LPVOID pOptionHeader = NULL;
	BOOL bX64Opheader = FALSE;

	pOptionHeader = (LPVOID)GetOptionHeader(lpFile, bX64Opheader);
	if (pOptionHeader != NULL && bX64Opheader)
	{
		secData = ((PIMAGE_OPTIONAL_HEADER64)pOptionHeader)->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY];
	}
	else if (pOptionHeader != NULL)
	{
		secData = ((PIMAGE_OPTIONAL_HEADER32)pOptionHeader)->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY];
	}

	UnmapViewOfFile(lpFile);
	CloseHandle(hFileMapping);
	CloseHandle(hFile);
	if ((secData.VirtualAddress != 0) && (secData.Size != 0))
		return TRUE;
	return FALSE;
}

BOOL IsDigiSigA(LPCSTR pPath)
{
	HANDLE hFile = CreateFileA(pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	return IsDigiSigEX(hFile);
}

BOOL IsDigiSigW(LPCWSTR pPath)
{
	HANDLE hFile = CreateFileW(pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	return IsDigiSigEX(hFile);
}

BOOL IsPEFileEX(HANDLE hFile, BOOL &bIsSucceed)
{
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMapping == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}
	LPVOID lpFile = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (lpFile == NULL)
	{
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return FALSE;
	}

	PIMAGE_DOS_HEADER pDosHeader = NULL;
	PIMAGE_NT_HEADERS32 pNtHeader32 = NULL;

	pDosHeader = (PIMAGE_DOS_HEADER)lpFile;
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		UnmapViewOfFile(lpFile);
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return TRUE;
	}

	pNtHeader32 = (PIMAGE_NT_HEADERS32)((DWORD)pDosHeader + pDosHeader->e_lfanew);

	if (pNtHeader32->Signature != IMAGE_NT_SIGNATURE)
	{
		UnmapViewOfFile(lpFile);
		CloseHandle(hFileMapping);
		CloseHandle(hFile);
		return TRUE;
	}

	UnmapViewOfFile(lpFile);
	CloseHandle(hFileMapping);
	CloseHandle(hFile);

	bIsSucceed = TRUE;
	return TRUE;
}

BOOL IsPEFileA(LPCSTR pPath, BOOL &bIsSucceed)
{
	bIsSucceed = FALSE;
	HANDLE hFile = CreateFileA(pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	return IsPEFileEX(hFile, bIsSucceed);
}

BOOL IsPEFileW(LPCWSTR pPath, BOOL &bIsSucceed)
{
	bIsSucceed = FALSE;
	HANDLE hFile = CreateFileW(pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	return IsPEFileEX(hFile, bIsSucceed);
}

bool ReadLogFile(wstring& Path, bool& HookWin, bool& HookMess, bool& HookFont)
{
	HANDLE fileHandle = CreateFileW(L"FVPLOG.ini", GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		Path = L"";
		return false;
	}
	else
	{
		SetFilePointer(fileHandle, 0, NULL, FILE_BEGIN);

		char Signature[16] = { 0 };
		DWORD Readed = 0;
		ReadFile(fileHandle, Signature, 16, &Readed, NULL);
		SetFilePointer(fileHandle, 16, NULL, FILE_BEGIN);
		if (memcmp(Signature, "STmoeSTmoechu>_<", 16))
		{
			CloseHandle(fileHandle);
			Path = L"";
			return false;
		}
		DWORD Length = 0;
		ReadFile(fileHandle, &Length, 4, &Readed, NULL);
		if (Length > MAX_PATH)
		{
			CloseHandle(fileHandle);
			Path = L"";
			return false;
		}
		SetFilePointer(fileHandle, 20, NULL, FILE_BEGIN);
		wchar_t FilePath[MAX_PATH] = { 0 };
		try
		{
			ReadFile(fileHandle, FilePath, Length * 2, &Readed, NULL);
		}
		catch (...)
		{
			CloseHandle(fileHandle);
			Path = L"";
			return false;
		}
		SetFilePointer(fileHandle, 20 + Length * 2, NULL, FILE_BEGIN);
		BYTE HookWinB = 0;
		BYTE HookMessB = 0;
		BYTE HookFontB = 0;

		ReadFile(fileHandle, &HookWinB, 1, &Readed, NULL);
		SetFilePointer(fileHandle, 20 + Length * 2 + 1, NULL, FILE_BEGIN);
		ReadFile(fileHandle, &HookMessB, 1, &Readed, NULL);
		SetFilePointer(fileHandle, 20 + Length * 2 + 2, NULL, FILE_BEGIN);
		ReadFile(fileHandle, &HookFontB, 1, &Readed, NULL);
		CloseHandle(fileHandle);
		Path = FilePath;

		HookWin = (bool)HookWinB;
		HookMess = (bool)HookMessB;
		HookFont = (bool)HookFontB;
		return true;
	}
}

void WriteLogFile(const wchar_t* Path, bool HookWin, bool HookMess, bool HookFont)
{
	HANDLE hFile = CreateFileW(L"FVPLOG.ini", GENERIC_WRITE, FILE_SHARE_WRITE,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}
	else
	{
		BYTE xOK = 1;
		BYTE xNo = 0;
		DWORD nRet = 0;
		WriteFile(hFile, "STmoeSTmoechu>_<", 16, &nRet, NULL);
		DWORD Length = wcslen(Path);
		WriteFile(hFile, &Length, 4, &nRet, NULL);
		WriteFile(hFile, (const char*)Path, Length * 2, &nRet, NULL);
		if (HookWin)
		{
			WriteFile(hFile, &xOK, 1, &nRet, NULL);
		}
		else
		{
			WriteFile(hFile, &xNo, 1, &nRet, NULL);
		}
		if (HookMess)
		{
			WriteFile(hFile, &xOK, 1, &nRet, NULL);
		}
		else
		{
			WriteFile(hFile, &xNo, 1, &nRet, NULL);
		}
		if (HookFont)
		{
			WriteFile(hFile, &xOK, 1, &nRet, NULL);
		}
		else
		{
			WriteFile(hFile, &xNo, 1, &nRet, NULL);
		}
		CloseHandle(hFile);
	}
}

