#ifndef _Common_
#define _Common_

#include <Windows.h>
#include <assert.h>
#include <string>

using std::wstring;

BOOL IsPEFileA(LPCSTR pPath, BOOL &bIsSucceed);
BOOL IsPEFileW(LPCWSTR pPath, BOOL &bIsSucceed);


#ifdef UNICODE
#define IsPEFile  IsPEFileW
#define IsDigiSig IsDigiSigW
#else
#define IsPEFile  IsPEFileA
#define IsDigiSig IsDigiSigA
#endif // !UNICODE

bool FileIsExist(const wchar_t* FileName);

bool ReadLogFile(std::wstring& Path, bool& HookWin, bool& HookMess, bool& HookFont);
void WriteLogFile(const wchar_t* Path, bool HookWin, bool HookMess, bool HookFont);

#endif
