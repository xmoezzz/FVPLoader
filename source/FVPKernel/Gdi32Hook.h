#include "FVPKernel.h"

#define FMS_CALL_MAGIC TAG4('FMSC')
#define GDI_HOOK_BYPASS TAG4('GHBP')


struct FMS_CALL_CONTEXT : public TEB_ACTIVE_FRAME
{
	HDC hDC;

	FMS_CALL_CONTEXT()
	{
		this->Flags = FMS_CALL_MAGIC;
	}

	static FMS_CALL_CONTEXT* Find()
	{
		return (FMS_CALL_CONTEXT *)Ps::FindThreadFrame(FMS_CALL_MAGIC);
	}
};

typedef FMS_CALL_CONTEXT *PFMS_CALL_CONTEXT;

typedef struct GDI_ENUM_FONT_PARAM
{
	LPARAM                  lParam;
	PHookKernel             GlobalData;
	PVOID                   Callback;
	ULONG                   Charset;
	HDC                     DC;

	GDI_ENUM_FONT_PARAM()
	{
		this->DC = nullptr;
	}

	NTSTATUS Prepare(PHookKernel GlobalData)
	{
		this->DC = GlobalData->CreateCompatibleDC(nullptr);
		return this->DC == nullptr ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
	}

	~GDI_ENUM_FONT_PARAM()
	{
		if (this->DC != nullptr)
			DeleteDC(this->DC);
	}

} GDI_ENUM_FONT_PARAM, *PGDI_ENUM_FONT_PARAM;

extern ULONG(NTAPI *GdiGetCodePage)(HDC NewDC);

HFONT GetFontFromDC(PHookKernel GlobalData, HDC hDC);
HFONT GetFontFromFont(PHookKernel GlobalData, HFONT Font);


