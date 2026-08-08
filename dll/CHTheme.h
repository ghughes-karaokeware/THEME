#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CHThemeMode
{
    CHTHEME_DARK = 0,
    CHTHEME_DARK_HIGH_CONTRAST = 1,
    CHTHEME_LIGHT = 2
};

enum CHSliderOrientation
{
    CHSLIDER_HORIZONTAL = 0,
    CHSLIDER_VERTICAL = 1
};

enum CHUIEntryType
{
    CHUI_PANEL = 1, CHUI_GROUP = 2, CHUI_HEADING = 3, CHUI_SEPARATOR = 4,
    CHUI_ENTRY = 10, CHUI_NUMBER = 11, CHUI_DROPDOWN = 12,
    CHUI_CHECKBOX = 13, CHUI_RADIO = 14, CHUI_SLIDER = 15
};

enum CHUIResult { CHUI_RESULT_CANCEL = 0, CHUI_RESULT_OK = 1 };

enum CHUIEntryFlags
{
    CHUI_FLAG_DISABLED = 0x00000001, CHUI_FLAG_REQUIRED = 0x00000002,
    CHUI_FLAG_RESTART_REQUIRED = 0x00000004,
    CHUI_FLAG_DEPEND_DISABLE = 0x00000008, CHUI_FLAG_DEPEND_HIDE = 0x00000010
};

enum CHUIDependencyOperator
{
    CHUI_DEPEND_NONE = 0, CHUI_DEPEND_EQUAL = 1, CHUI_DEPEND_NOT_EQUAL = 2
};

enum CHUIStatus
{
    CHUI_STATUS_OK = 1, CHUI_ERROR_ARGUMENT = -1, CHUI_ERROR_VERSION = -2,
    CHUI_ERROR_HEADER_SIZE = -3, CHUI_ERROR_ENTRY_SIZE = -4,
    CHUI_ERROR_ENTRY_COUNT = -5, CHUI_ERROR_STRING = -6,
    CHUI_ERROR_TYPE = -7, CHUI_ERROR_ID = -8, CHUI_ERROR_PARENT = -9,
    CHUI_ERROR_OPTIONS = -10, CHUI_ERROR_ALREADY_OPEN = -11,
    CHUI_ERROR_WINDOW = -12
};

#pragma pack(push, 1)
struct CHUI_DIALOG_HEADER
{
    DWORD version, headerSize, entrySize, entryCount, flags, instanceId;
    char title[128];
    char reserved[104];
};

struct CHUI_ENTRY_RECORD
{
    DWORD type, id, parentId, flags, dependencyId, dependencyOperator;
    LONG minimum, maximum, step;
    DWORD reservedNumber;
    char caption[128];
    char value[128];
    char defaultValue[128];
    char dependencyValue[128];
    char options[512];
    char helpText[256];
    char reserved[88];
};
#pragma pack(pop)

static_assert(sizeof(CHUI_DIALOG_HEADER) == 256, "CHUI header ABI changed");
static_assert(sizeof(CHUI_ENTRY_RECORD) == 1408, "CHUI entry ABI changed");

BOOL __stdcall CHTheme_AttachWindow(HWND window);
BOOL __stdcall CHTheme_DetachWindow(HWND window);
DWORD __stdcall CHTheme_GetWindowsBuild();
int __stdcall CHTheme_AttachAllControls(HWND window);
BOOL __stdcall CHTheme_AttachDropList(HWND listWindow);
BOOL __stdcall CHTheme_SetMode(HWND window, int mode);
BOOL __stdcall CHTheme_SetBackgroundColors(HWND window, COLORREF lightBackground,
    COLORREF darkBackground);
BOOL __stdcall CHTheme_SetFont(HWND window, const char* faceName, int pointSize,
    BOOL bold, BOOL italic);
BOOL __stdcall CHTheme_SetNotifyButton(HWND window, HWND notifyButton);
BOOL __stdcall CHTheme_ConsumeFlatOptionNotify(HWND window);
BOOL __stdcall CHTheme_SetApplicationMode(HWND window, int mode);
int __stdcall CHTheme_GetMode(HWND window);
BOOL __stdcall CHTheme_SetAccent(HWND window, COLORREF accent);
BOOL __stdcall CHTheme_ForceRedraw(HWND window);
BOOL __stdcall CHTheme_BeginUpdate(HWND window);
BOOL __stdcall CHTheme_EndUpdate(HWND window);
BOOL __stdcall CHTheme_AttachMenu(HWND window);
BOOL __stdcall CHTheme_DetachMenu(HWND window);
BOOL __stdcall CHTheme_AttachClarionMenu(HWND window, const char* definition);
HWND __stdcall CHTheme_GetListHeader(HWND listWindow);
BOOL __stdcall CHTheme_AttachListHeader(HWND listWindow);
BOOL __stdcall CHTheme_DetachListHeader(HWND listWindow);
BOOL __stdcall CHTheme_AttachClarionListHeader(HWND listWindow, int headerHeight);
BOOL __stdcall CHTheme_SetClarionListColumns(HWND listWindow, const char* definition);
BOOL __stdcall CHTheme_SetClarionListSort(HWND listWindow, int column, int direction);
int __stdcall CHTheme_GetClarionListHeaderClick(HWND listWindow);
BOOL __stdcall CHTheme_DetachClarionListHeader(HWND listWindow);
int __stdcall CHTheme_GetClarionListResizedColumn(HWND listWindow);
int __stdcall CHTheme_GetClarionListColumnWidth(HWND listWindow, int column);
BOOL __stdcall CHTheme_SetClarionListColumnBasis(HWND listWindow, int column, int clarionWidth);
BOOL __stdcall CHTheme_SetClarionListControlBasis(HWND listWindow, int clarionControlWidth);
BOOL __stdcall CHTheme_SetClarionListColumnResizable(HWND listWindow, int column,
    BOOL resizable);
HWND __stdcall CHSlider_Create(HWND parentWindow, int x, int y, int width, int height,
    int orientation, int minimum, int maximum, int value, HWND notifyButton);
BOOL __stdcall CHSlider_Destroy(HWND sliderWindow);
BOOL __stdcall CHSlider_SetRange(HWND sliderWindow, int minimum, int maximum);
BOOL __stdcall CHSlider_SetValue(HWND sliderWindow, int value);
int __stdcall CHSlider_GetValue(HWND sliderWindow);
BOOL __stdcall CHSlider_SetStep(HWND sliderWindow, int step);
BOOL __stdcall CHSlider_SetEnabled(HWND sliderWindow, BOOL enabled);
BOOL __stdcall CHSlider_SetColors(HWND sliderWindow, COLORREF lightTrack,
    COLORREF lightFill, COLORREF darkTrack, COLORREF darkFill);
HWND __stdcall CHTabStrip_Create(HWND parentWindow, int x, int y, int width, int height,
    const char* labels, int selected, HWND notifyButton);
BOOL __stdcall CHTabStrip_Destroy(HWND tabWindow);
BOOL __stdcall CHTabStrip_SetSelected(HWND tabWindow, int selected);
int __stdcall CHTabStrip_GetSelected(HWND tabWindow);
BOOL __stdcall CHTabStrip_SetLabels(HWND tabWindow, const char* labels);
BOOL __stdcall CHTabStrip_SetEnabled(HWND tabWindow, int tab, BOOL enabled);
BOOL __stdcall CHTabStrip_SetTabVisible(HWND tabWindow, int tab, BOOL visible);
BOOL __stdcall CHTabStrip_SetVisible(HWND tabWindow, BOOL visible);
BOOL __stdcall CHTabStrip_SetSheetBounds(HWND tabWindow, int x, int y,
    int width, int height);
HWND __stdcall CHTheme_CreateFlatOptionMask(HWND parentWindow, int x, int y,
    int width, int height);
BOOL __stdcall CHTheme_SetFlatOptionBounds(HWND maskWindow, int x, int y,
    int width, int height);
BOOL __stdcall CHTheme_SetFlatOptionVisible(HWND maskWindow, BOOL visible);
BOOL __stdcall CHTheme_SetFlatOptionCaption(HWND maskWindow, const char* caption);
BOOL __stdcall CHTheme_SetFlatOptionChoices(HWND maskWindow, const char* labels,
    int selected);
BOOL __stdcall CHTheme_SetFlatOptionChoiceBounds(HWND maskWindow, int choice,
    int x, int y, int width, int height);
BOOL __stdcall CHTheme_SetFlatOptionChoice(HWND maskWindow, int selected);
int __stdcall CHTheme_GetFlatOptionChoice(HWND maskWindow);
BOOL __stdcall CHTheme_DestroyFlatOptionMask(HWND maskWindow);
DWORD __stdcall CHUI_GetAbiVersion();
DWORD __stdcall CHUI_GetHeaderSize();
DWORD __stdcall CHUI_GetEntrySize();
LONG __stdcall CHUI_ValidateDialog(const CHUI_DIALOG_HEADER* header,
    const CHUI_ENTRY_RECORD* entries);
LONG __stdcall CHUI_OpenDialog(HWND ownerWindow, CHUI_DIALOG_HEADER* header,
    CHUI_ENTRY_RECORD* entries, HWND completionButton);
LONG __stdcall CHUI_ConsumeCompletion(HWND completionButton,
    DWORD* instanceId, LONG* result);

#ifdef __cplusplus
}
#endif
