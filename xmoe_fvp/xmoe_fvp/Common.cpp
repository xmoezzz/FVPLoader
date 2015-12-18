#include "Common.h"
#include "WinFile.h"

#define StaticSign "STmoeSTmoechu>_<"

BOOL ReadLogFile(wstring& Path, bool& HookWin, bool& HookMess, bool& HookFont)
{
	HANDLE fileHandle = CreateFileW(L"FVPLOG.ini", GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		Path = L"";
		return FALSE;
	}
	else
	{
		SetFilePointer(fileHandle, 0, NULL, FILE_BEGIN);

		char Signature[16] = { 0 };
		DWORD Readed = 0;
		ReadFile(fileHandle, Signature, 16, &Readed, NULL);
		SetFilePointer(fileHandle, 16, NULL, FILE_BEGIN);
		if (memcmp(Signature, StaticSign, 16))
		{
			CloseHandle(fileHandle);
			Path = L"";
			return FALSE;
		}
		DWORD Length = 0;
		ReadFile(fileHandle, &Length, 4, &Readed, NULL);
		if (Length > MAX_PATH)
		{
			CloseHandle(fileHandle);
			Path = L"";
			return FALSE;
		}
		SetFilePointer(fileHandle, 20, NULL, FILE_BEGIN);
		WCHAR FilePath[MAX_PATH] = { 0 };
		ReadFile(fileHandle, FilePath, Length * 2, &Readed, NULL);
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

		HookWin = HookWinB;
		HookMess = HookMessB;
		HookFont = HookFontB;
		return TRUE;
	}
}


BOOL ReadFontName(wstring& Name)
{
	UCHAR   Sign[16];
	WinFile File;
	ULONG   PathLength;
	ULONG   FontLength;
	ULONG   FileLength;

	if (FAILED(File.Open(L"FVPLOG.ini", WinFile::FileRead)))
		return FALSE;
	if (File.GetSize32() <= 0x10)
		return FALSE;

	FileLength = File.GetSize32();
	File.Read(Sign, 16);
	if (memcmp(Sign, StaticSign, 16))
	{
		File.Release();
		return FALSE;
	}
	if (FileLength <= 20)
	{
		File.Release();
		return FALSE;
	}
	File.Read((PBYTE)&PathLength, sizeof(ULONG));
	if (FileLength <= 20 + PathLength * 2)
	{
		File.Release();
		return FALSE;
	}
	File.Read((PBYTE)&FontLength, sizeof(ULONG));
	Name.resize(FontLength + 1);
	Name.clear();
	File.Read((PBYTE)&Name[0], FontLength * 2);
	File.Release();
	return TRUE;
}

BOOL WriteFontName(wstring& Name)
{
	UCHAR   Sign[16];
	WinFile File;
	WinFile WriteFile;
	ULONG   PathLength;
	ULONG   FontLength;
	ULONG   FileLength;

	if (FAILED(File.Open(L"FVPLOG.ini", WinFile::FileRead)))
		return FALSE;
	if (File.GetSize32() <= 0x10)
		return FALSE;
	
	FileLength = File.GetSize32();
	File.Read(Sign, 16);
	if (memcmp(Sign, StaticSign, 16))
	{
		File.Release();
		return FALSE;
	}
	if (FileLength <= 20)
	{
		File.Release();
		return FALSE;
	}
	File.Read((PBYTE)&PathLength, sizeof(ULONG));
	if (FileLength <= 20 + PathLength * 2)
	{
		File.Release();
		return FALSE;
	}
	File.Release();
	
	if (!FAILED(WriteFile.Open(L"FVPLOG.ini", WinFile::FileWrite)))
	{
		return FALSE;
	}
	else
	{
		FontLength = (ULONG)Name.length();
		WriteFile.Seek(20 + PathLength * 2, FILE_BEGIN);
		WriteFile.Write((PBYTE)&FontLength, sizeof(ULONG));
		WriteFile.Write((PBYTE)&Name[0], FontLength * 2);
		WriteFile.Release();
		return TRUE;
	}
}
