#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CHButtonTheme
{
    CHBUTTON_THEME_DARK = 0,
    CHBUTTON_THEME_LIGHT = 1
};

enum CHButtonRole
{
    CHBUTTON_ROLE_STANDARD = 0,
    CHBUTTON_ROLE_PRIMARY = 1,
    CHBUTTON_ROLE_DESTRUCTIVE = 2,
    CHBUTTON_ROLE_TOOLBAR = 3,
    CHBUTTON_ROLE_SELECTED = 4,
    CHBUTTON_ROLE_SUCCESS = 5
};

BOOL __stdcall CHButton_Attach(HWND button);
int __stdcall CHButton_AttachAll(HWND parent);
BOOL __stdcall CHButton_Detach(HWND button);
void __stdcall CHButton_DetachAll(void);
BOOL __stdcall CHButton_SetColors(HWND button, COLORREF normalTop,
    COLORREF normalBottom, COLORREF hoverTop, COLORREF hoverBottom,
    COLORREF pressedTop, COLORREF pressedBottom, COLORREF textColor,
    COLORREF borderColor);
BOOL __stdcall CHButton_SetDisabledColors(HWND button, COLORREF topColor,
    COLORREF bottomColor, COLORREF textColor, COLORREF borderColor);
BOOL __stdcall CHButton_SetRole(HWND button, int role);
BOOL __stdcall CHButton_SetTheme(int theme);
BOOL __stdcall CHButton_SetMetrics(HWND button, int cornerRadius,
    int borderWidth, BOOL gradient);
BOOL __stdcall CHButton_SetIcon(HWND button, HICON icon, int iconWidth, int gap);
BOOL __stdcall CHButton_SetIconFile(HWND button, const char* iconName,
    int iconWidth, int gap);
BOOL __stdcall CHButton_SetContentAlignment(HWND button, int horizontal,
    int vertical);

#ifdef __cplusplus
}
#endif
