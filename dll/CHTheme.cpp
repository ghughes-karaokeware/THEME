#include "CHTheme.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <richedit.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <cctype>
#include <cwchar>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")

namespace
{
constexpr UINT_PTR kWindowSubclassId = 0x43485448; // CHTH
constexpr UINT_PTR kMdiClientSubclassId = 0x43484D44; // CHMD
constexpr UINT kRefreshMergedMenuMessage = WM_APP + 0x434;
constexpr UINT kApplyApplicationThemeMessage = WM_APP + 0x435;
constexpr UINT kForceApplicationRedrawMessage = WM_APP + 0x436;
constexpr UINT kEnsureWindowVisibleMessage = WM_APP + 0x437;
constexpr UINT_PTR kDeferredThemeRedrawTimerId = 0x43485452; // CHTR
constexpr UINT_PTR kStartupRevealTimerId = 0x43485352; // CHSR
constexpr UINT kStartupRevealDelayMs = 1250;
constexpr UINT_PTR kEntrySubclassId = 0x4348454E; // CHEN
constexpr UINT_PTR kEntryParentSubclassId = 0x43484550; // CHEP
constexpr UINT_PTR kComboParentSubclassId = 0x43484350; // CHCP
constexpr UINT_PTR kClarionDropSubclassId = 0x43484450; // CHDP
constexpr UINT_PTR kClarionDropRepaintTimerId = 0x4348;
constexpr UINT_PTR kHeaderSubclassId = 0x43484844; // CHHD
constexpr UINT_PTR kClarionListSubclassId = 0x43484C53; // CHLS
constexpr wchar_t kClarionHeaderClass[] = L"CHTheme.ClarionListHeader";
constexpr wchar_t kSliderClass[] = L"CHTheme.Slider";
constexpr COLORREF kSliderTransparencyKey = RGB(1, 0, 1);
constexpr wchar_t kTabStripClass[] = L"CHTheme.TabStrip";
constexpr wchar_t kSheetEdgeClass[] = L"CHTheme.SheetEdge";
constexpr wchar_t kFlatMaskClass[] = L"CHTheme.FlatMask";
constexpr wchar_t kOptionEdgeClass[] = L"CHTheme.OptionEdge";
constexpr wchar_t kOptionChoiceClass[] = L"CHTheme.OptionChoice";
constexpr DWORD kMenuItemMagic = 0x43484D49; // CHMI
constexpr DWORD kSliderMagic = 0x4348534C; // CHSL
constexpr DWORD kTabStripMagic = 0x43485442; // CHTB
constexpr DWORD kFlatMaskMagic = 0x4348464D; // CHFM

struct MenuItemData
{
    DWORD magic = kMenuItemMagic;
    HWND window = nullptr;
    HMENU menu = nullptr;
    UINT position = 0;
    UINT originalType = 0;
    ULONG_PTR originalData = 0;
    HBITMAP bitmap = nullptr;
    HICON icon = nullptr;
    std::wstring text;
    bool separator = false;
    bool hidden = false;
    bool topLevel = false;
    bool hasSubmenu = false;

    ~MenuItemData()
    {
        if (icon) DestroyIcon(icon);
    }
};

struct MenuSourceData
{
    std::unordered_map<std::wstring, std::wstring> captions;
    std::unordered_map<std::wstring, std::wstring> icons;
    std::unordered_set<std::wstring> hiddenPaths;
    std::unordered_map<UINT, std::wstring> captionsById;
    std::unordered_map<UINT, std::wstring> iconsById;
    std::unordered_set<UINT> hiddenIds;
};

struct ThemeData
{
    COLORREF background = RGB(7, 13, 20);
    COLORREF lightBackground = RGB(242, 246, 250);
    COLORREF darkBackground = RGB(7, 13, 20);
    COLORREF surface = RGB(13, 23, 33);
    COLORREF menuSurface = RGB(18, 30, 42);
    COLORREF input = RGB(8, 16, 24);
    COLORREF text = RGB(235, 241, 248);
    COLORREF secondaryText = RGB(158, 173, 189);
    COLORREF disabledText = RGB(104, 118, 132);
    COLORREF accent = RGB(35, 132, 255);
    HBRUSH backgroundBrush = nullptr;
    HBRUSH surfaceBrush = nullptr;
    HBRUSH menuBrush = nullptr;
    HBRUSH inputBrush = nullptr;
    HFONT font = nullptr;
    std::wstring fontFace = L"Segoe UI";
    int fontPointSize = 9;
    int fontWeight = FW_NORMAL;
    BOOL fontItalic = FALSE;
    int mode = CHTHEME_DARK;
    bool menuAttached = false;
    HMENU attachedMenu = nullptr;
    HWND mdiClient = nullptr;
    HWND notifyButton = nullptr;
    unsigned int flatOptionNotifyCount = 0;
    HWND activeMenuSource = nullptr;
    std::vector<std::unique_ptr<MenuItemData>> menuItems;
    std::unordered_map<std::wstring, std::wstring> menuCaptions;
    std::unordered_map<std::wstring, std::wstring> menuIcons;
    std::unordered_set<std::wstring> hiddenMenuPaths;
    std::unordered_map<UINT, std::wstring> menuCaptionsById;
    std::unordered_map<UINT, std::wstring> menuIconsById;
    std::unordered_set<UINT> hiddenMenuIds;
    std::unordered_map<HWND, MenuSourceData> menuSources;
};

struct MonitorVisibilityContext
{
    RECT windowRectangle{};
    bool visible = false;
};

BOOL CALLBACK CheckMonitorWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM parameter)
{
    auto* context = reinterpret_cast<MonitorVisibilityContext*>(parameter);
    MONITORINFO information{ sizeof(information) };
    RECT intersection{};
    if (GetMonitorInfoW(monitor, &information) &&
        IntersectRect(&intersection, &context->windowRectangle, &information.rcWork)) {
        context->visible = true;
        return FALSE;
    }
    return TRUE;
}

bool EnsureWindowVisible(HWND window)
{
    RECT windowRectangle{};
    if (!GetWindowRect(window, &windowRectangle)) return false;

    RECT targetRectangle{};
    HWND parent = nullptr;
    if (GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD) {
        parent = GetParent(window);
        if (!IsWindow(parent) || !GetClientRect(parent, &targetRectangle))
            return false;
        MapWindowPoints(parent, HWND_DESKTOP,
            reinterpret_cast<POINT*>(&targetRectangle), 2);

        RECT intersection{};
        if (IntersectRect(&intersection, &windowRectangle, &targetRectangle))
            return true;
    } else {
        MonitorVisibilityContext context{};
        context.windowRectangle = windowRectangle;
        EnumDisplayMonitors(nullptr, nullptr, CheckMonitorWorkArea,
            reinterpret_cast<LPARAM>(&context));
        if (context.visible) return true;

        const POINT primaryPoint{ 0, 0 };
        HMONITOR monitor = MonitorFromPoint(primaryPoint, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO information{ sizeof(information) };
        if (!monitor || !GetMonitorInfoW(monitor, &information)) return false;
        targetRectangle = information.rcWork;
    }

    const LONG windowWidth = windowRectangle.right - windowRectangle.left;
    const LONG windowHeight = windowRectangle.bottom - windowRectangle.top;
    const LONG targetWidth = targetRectangle.right - targetRectangle.left;
    const LONG targetHeight = targetRectangle.bottom - targetRectangle.top;
    POINT position{
        targetRectangle.left + std::max<LONG>(0, (targetWidth - windowWidth) / 2),
        targetRectangle.top + std::max<LONG>(0, (targetHeight - windowHeight) / 2)
    };
    if (parent)
        MapWindowPoints(HWND_DESKTOP, parent, &position, 1);

    return SetWindowPos(window, nullptr, position.x, position.y, 0, 0,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER) != FALSE;
}

struct SliderData
{
    DWORD magic = kSliderMagic;
    HWND window = nullptr;
    HWND parent = nullptr;
    HWND notifyButton = nullptr;
    int orientation = CHSLIDER_HORIZONTAL;
    int minimum = 0;
    int maximum = 100;
    int value = 0;
    int step = 1;
    bool tracking = false;
    bool hover = false;
    COLORREF lightTrack = RGB(190, 202, 214);
    COLORREF lightFill = RGB(35, 132, 255);
    COLORREF darkTrack = RGB(25, 39, 53);
    COLORREF darkFill = RGB(35, 132, 255);
};

struct TabStripData
{
    DWORD magic = kTabStripMagic;
    HWND window = nullptr;
    HWND parent = nullptr;
    HWND notifyButton = nullptr;
    std::vector<std::wstring> labels;
    std::vector<bool> enabled;
    std::vector<bool> visible;
    int selected = 1;
    int hover = 0;
    bool stripVisible = true;
    HWND leftEdge = nullptr;
    HWND rightEdge = nullptr;
    HWND bottomEdge = nullptr;
};

struct FlatMaskData
{
    DWORD magic = kFlatMaskMagic;
    HWND window = nullptr;
    HWND parent = nullptr;
    HWND leftEdge = nullptr;
    HWND rightEdge = nullptr;
    HWND bottomEdge = nullptr;
    HWND choiceWindow = nullptr;
    std::wstring caption;
    std::vector<std::wstring> choiceLabels;
    std::vector<RECT> choiceBounds;
    POINT choiceOrigin{};
    bool choiceBoundsDirty = false;
    int selectedChoice = 1;
    int hotChoice = 0;
    int optionX = 0;
    int optionY = 0;
    int optionWidth = 0;
    int optionHeight = 0;
    bool visible = true;
};

std::mutex g_mutex;
std::unordered_set<HWND> g_windows;
struct UpdateState
{
    unsigned int depth = 0;
    bool restoreVisible = false;
    bool pendingReveal = false;
    HWND frozenMdiClient = nullptr;
    HWND frozenMdiFrame = nullptr;
};
std::unordered_map<HWND, UpdateState> g_updateStates;
int g_applicationMode = CHTHEME_DARK;
std::unordered_set<MenuItemData*> g_menuItems;
HWINEVENTHOOK g_menuPopupHook = nullptr;

void DeleteResources(ThemeData& data)
{
    if (data.backgroundBrush) DeleteObject(data.backgroundBrush);
    if (data.surfaceBrush) DeleteObject(data.surfaceBrush);
    if (data.menuBrush) DeleteObject(data.menuBrush);
    if (data.inputBrush) DeleteObject(data.inputBrush);
    if (data.font) DeleteObject(data.font);
    data.backgroundBrush = data.surfaceBrush = data.menuBrush = data.inputBrush = nullptr;
    data.font = nullptr;
}

void CreateResources(HWND window, ThemeData& data)
{
    DeleteResources(data);
    data.backgroundBrush = CreateSolidBrush(data.background);
    data.surfaceBrush = CreateSolidBrush(data.surface);
    data.menuBrush = CreateSolidBrush(data.menuSurface);
    data.inputBrush = CreateSolidBrush(data.input);

    HDC dc = GetDC(window);
    const int dpiY = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) ReleaseDC(window, dc);
    data.font = CreateFontW(-MulDiv(std::max(1, data.fontPointSize), dpiY, 72), 0, 0, 0,
        data.fontWeight, data.fontItalic, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, data.fontFace.c_str());
}

void ApplyTitleBarMode(HWND window, bool dark)
{
    BOOL enabled = dark ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    constexpr DWORD kImmersiveDarkModeBefore20H1 = 19;
    if (FAILED(DwmSetWindowAttribute(window, kImmersiveDarkMode, &enabled, sizeof(enabled))))
        DwmSetWindowAttribute(window, kImmersiveDarkModeBefore20H1, &enabled, sizeof(enabled));
}

ThemeData* FindData(HWND window);
std::vector<std::wstring> ParseTabLabels(const char* definition);
bool AttachMenu(HWND window, ThemeData& data);

ThemeData* FindPopupMenuTheme(HWND popup)
{
    HWND owner = GetWindow(popup, GW_OWNER);
    while (owner) {
        if (ThemeData* theme = FindData(owner)) return theme;
        owner = GetParent(owner);
    }
    return nullptr;
}

void StylePopupMenuWindow(HWND popup)
{
    if (!IsWindow(popup)) return;
    wchar_t className[32]{};
    GetClassNameW(popup, className, static_cast<int>(std::size(className)));
    if (lstrcmpW(className, L"#32768") != 0) return;

    ThemeData* theme = FindPopupMenuTheme(popup);
    if (!theme) return;

    // Windows 11: retain the system-managed DWM shadow while requesting the
    // small rounded-corner treatment used by native popup surfaces.
    constexpr DWORD kDwmWindowCornerPreference = 33;
    constexpr DWORD kDwmBorderColor = 34;
    constexpr int kDwmRound = 2;
    const int cornerPreference = kDwmRound;
    const HRESULT rounded = DwmSetWindowAttribute(popup, kDwmWindowCornerPreference,
        &cornerPreference, sizeof(cornerPreference));
    const COLORREF border = theme->mode == CHTHEME_LIGHT
        ? RGB(183, 198, 214) : RGB(55, 78, 99);
    DwmSetWindowAttribute(popup, kDwmBorderColor, &border, sizeof(border));

    // Pre-Windows 11 fallback. A region supplies the same modest radius;
    // Windows continues to provide the normal popup-menu shadow where the
    // user's visual-effects settings permit it.
    if (FAILED(rounded)) {
        RECT bounds{};
        if (GetWindowRect(popup, &bounds)) {
            const int width = bounds.right - bounds.left;
            const int height = bounds.bottom - bounds.top;
            const UINT dpi = GetDpiForWindow(popup);
            const int radius = std::max(6, MulDiv(8, dpi ? dpi : 96, 96));
            HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1,
                radius * 2, radius * 2);
            if (region && !SetWindowRgn(popup, region, TRUE)) DeleteObject(region);
        }
    }
    SetWindowPos(popup, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void CALLBACK MenuPopupWinEventProc(HWINEVENTHOOK, DWORD, HWND window,
    LONG, LONG, DWORD, DWORD)
{
    StylePopupMenuWindow(window);
}

void EnsureMenuPopupHook()
{
    if (g_menuPopupHook) return;
    g_menuPopupHook = SetWinEventHook(EVENT_SYSTEM_MENUPOPUPSTART,
        EVENT_SYSTEM_MENUPOPUPSTART, nullptr, MenuPopupWinEventProc,
        GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
}

ThemeData* FindAncestorTheme(HWND window)
{
    HWND candidate = window;
    while (candidate) {
        if (ThemeData* data = FindData(candidate)) return data;
        candidate = GetParent(candidate);
    }
    return nullptr;
}

LRESULT CALLBACK SheetEdgeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        ThemeData* theme = FindAncestorTheme(window);
        const COLORREF color = theme ? theme->background : RGB(7, 13, 20);
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        EndPaint(window, &paint);
        return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureSheetEdgeClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = SheetEdgeWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kSheetEdgeClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

LRESULT CALLBACK FlatMaskWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<FlatMaskData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        data = reinterpret_cast<FlatMaskData*>(create->lpCreateParams);
        if (!data) return FALSE;
        data->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        ThemeData* theme = FindAncestorTheme(window);
        const COLORREF background = theme ? theme->surface : RGB(13, 23, 33);
        const COLORREF border = theme && theme->mode == CHTHEME_LIGHT
            ? RGB(183, 198, 214) : RGB(43, 65, 85);
        const COLORREF text = theme ? theme->secondaryText : RGB(158, 173, 189);
        HBRUSH brush = CreateSolidBrush(background);
        FillRect(dc, &client, brush);
        DeleteObject(brush);

        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HBRUSH hollowBrush = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, hollowBrush);
        Rectangle(dc, client.left, client.top, client.right, client.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        if (!data->caption.empty()) {
            HFONT font = theme && theme->font
                ? theme->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HGDIOBJ oldFont = SelectObject(dc, font);
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, background);
            SetTextColor(dc, text);
            RECT captionRect{ 8, 0, client.right - 6,
                std::max(1L, client.bottom / 2 + 6) };
            DrawTextW(dc, data->caption.c_str(), -1, &captionRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, oldFont);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_NCDESTROY:
        if (IsWindow(data->choiceWindow)) DestroyWindow(data->choiceWindow);
        if (IsWindow(data->leftEdge)) DestroyWindow(data->leftEdge);
        if (IsWindow(data->rightEdge)) DestroyWindow(data->rightEdge);
        if (IsWindow(data->bottomEdge)) DestroyWindow(data->bottomEdge);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        data->magic = 0;
        delete data;
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK OptionEdgeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        ThemeData* theme = FindAncestorTheme(window);
        const COLORREF background = theme ? theme->background : RGB(7, 13, 20);
        const COLORREF border = theme && theme->mode == CHTHEME_LIGHT
            ? RGB(183, 198, 214) : RGB(43, 65, 85);
        HBRUSH brush = CreateSolidBrush(background);
        FillRect(dc, &client, brush);
        DeleteObject(brush);

        HBRUSH borderBrush = CreateSolidBrush(border);
        switch (GetDlgCtrlID(window)) {
        case 1: // Left edge.
        {
            RECT vertical{ 0, 0, 1, client.bottom };
            RECT topJoin{ 0, 0, client.right, 1 };
            FillRect(dc, &vertical, borderBrush);
            FillRect(dc, &topJoin, borderBrush);
            break;
        }
        case 2: // Right edge.
        {
            RECT vertical{ std::max(0L, client.right - 1), 0,
                client.right, client.bottom };
            RECT topJoin{ 0, 0, client.right, 1 };
            FillRect(dc, &vertical, borderBrush);
            FillRect(dc, &topJoin, borderBrush);
            break;
        }
        default: // Bottom edge.
        {
            RECT horizontal{ 0, std::max(0L, client.bottom - 1),
                client.right, client.bottom };
            RECT leftJoin{ 0, 0, 1, client.bottom };
            RECT rightJoin{ std::max(0L, client.right - 1), 0,
                client.right, client.bottom };
            FillRect(dc, &horizontal, borderBrush);
            FillRect(dc, &leftJoin, borderBrush);
            FillRect(dc, &rightJoin, borderBrush);
            break;
        }
        }
        DeleteObject(borderBrush);
        EndPaint(window, &paint);
        return 0;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool HasChoiceBounds(const FlatMaskData& data)
{
    if (data.choiceBounds.size() != data.choiceLabels.size() ||
        data.choiceBounds.empty()) return false;
    for (const RECT& bounds : data.choiceBounds) {
        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top)
            return false;
    }
    return true;
}

void PositionChoiceWindow(FlatMaskData& data)
{
    if (!IsWindow(data.choiceWindow) || !HasChoiceBounds(data)) return;
    RECT combined = data.choiceBounds.front();
    for (size_t index = 1; index < data.choiceBounds.size(); ++index) {
        combined.left = std::min(combined.left, data.choiceBounds[index].left);
        combined.top = std::min(combined.top, data.choiceBounds[index].top);
        combined.right = std::max(combined.right, data.choiceBounds[index].right);
        combined.bottom = std::max(combined.bottom, data.choiceBounds[index].bottom);
    }
    data.choiceOrigin = POINT{ combined.left, combined.top };
    HRGN combinedRegion = CreateRectRgn(0, 0, 0, 0);
    if (combinedRegion) {
        for (const RECT& bounds : data.choiceBounds) {
            HRGN itemRegion = CreateRoundRectRgn(
                bounds.left - combined.left,
                bounds.top - combined.top,
                bounds.right - combined.left,
                bounds.bottom - combined.top, 10, 10);
            if (itemRegion) {
                CombineRgn(combinedRegion, combinedRegion, itemRegion, RGN_OR);
                DeleteObject(itemRegion);
            }
        }
        SetWindowRgn(data.choiceWindow, combinedRegion, TRUE);
    }
    SetWindowPos(data.choiceWindow, HWND_TOP, combined.left, combined.top,
        combined.right - combined.left, combined.bottom - combined.top,
        SWP_NOACTIVATE | SWP_HIDEWINDOW);
    if (data.visible) ShowWindow(data.choiceWindow, SW_SHOWNA);
    RedrawWindow(data.choiceWindow, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
}

int OptionChoiceAt(const FlatMaskData& data, HWND window, int x, int y)
{
    if (HasChoiceBounds(data)) {
        POINT point{ x + data.choiceOrigin.x, y + data.choiceOrigin.y };
        for (size_t index = 0; index < data.choiceBounds.size(); ++index) {
            if (PtInRect(&data.choiceBounds[index], point))
                return static_cast<int>(index + 1);
        }
        return 0;
    }
    RECT client{};
    GetClientRect(window, &client);
    const int count = static_cast<int>(data.choiceLabels.size());
    const int width = client.right - client.left;
    if (count <= 0 || width <= 0 || x < 0 || x >= width) return 0;
    return std::clamp(MulDiv(x, count, width) + 1, 1, count);
}

void RepaintOptionChoice(HWND window)
{
    if (IsWindow(window)) {
        RedrawWindow(window, nullptr, nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    }
}

void NotifyOptionChoice(HWND window, FlatMaskData& data, int selected)
{
    if (selected < 1 ||
        selected > static_cast<int>(data.choiceLabels.size())) {
        return;
    }

    data.selectedChoice = selected;
    RepaintOptionChoice(window);

    ThemeData* theme = FindAncestorTheme(window);
    if (!theme || !IsWindow(theme->notifyButton) ||
        !IsWindowEnabled(theme->notifyButton)) {
        return;
    }

    HWND owner = GetParent(theme->notifyButton);
    if (!IsWindow(owner)) return;

    ++theme->flatOptionNotifyCount;
    if (!PostMessageW(owner, WM_COMMAND,
        MAKEWPARAM(static_cast<WORD>(
            GetDlgCtrlID(theme->notifyButton)), BN_CLICKED),
        reinterpret_cast<LPARAM>(theme->notifyButton))) {
        --theme->flatOptionNotifyCount;
    }
}

LRESULT CALLBACK OptionChoiceWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<FlatMaskData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        data = reinterpret_cast<FlatMaskData*>(create->lpCreateParams);
        if (!data) return FALSE;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_MOUSEMOVE: {
        const int hot = OptionChoiceAt(*data, window,
            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hot != data->hotChoice) {
            data->hotChoice = hot;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, window, 0 };
            TrackMouseEvent(&track);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        data->hotChoice = 0;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        const int selected = OptionChoiceAt(*data, window,
            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (selected > 0)
            NotifyOptionChoice(window, *data, selected);
        SetFocus(window);
        return 0;
    }
    case WM_KEYDOWN: {
        int selected = data->selectedChoice;
        if (wParam == VK_LEFT && selected > 1) --selected;
        else if (wParam == VK_RIGHT &&
            selected < static_cast<int>(data->choiceLabels.size()))
            ++selected;
        else if (wParam >= '1' && wParam <= '9' &&
            static_cast<int>(wParam - '0') <= static_cast<int>(data->choiceLabels.size()))
            selected = static_cast<int>(wParam - '0');
        else break;
        NotifyOptionChoice(window, *data, selected);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        ThemeData* theme = FindAncestorTheme(window);
        const bool light = theme && theme->mode == CHTHEME_LIGHT;
        const COLORREF normal = light ? RGB(255, 255, 255) : RGB(38, 51, 66);
        const COLORREF hover = light ? RGB(232, 241, 252) : RGB(31, 54, 76);
        const COLORREF selected = light ? RGB(31, 126, 238) : RGB(27, 112, 219);
        const COLORREF text = light ? RGB(28, 39, 52) : RGB(245, 248, 252);
        const COLORREF background = theme ? theme->surface :
            (light ? RGB(250, 252, 254) : RGB(13, 23, 33));
        const int count = static_cast<int>(data->choiceLabels.size());
        HBRUSH backgroundBrush = CreateSolidBrush(background);
        FillRect(dc, &client, backgroundBrush);
        DeleteObject(backgroundBrush);
        HFONT font = theme && theme->font ? theme->font :
            static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        for (int index = 0; index < count; ++index) {
            RECT item{};
            if (HasChoiceBounds(*data)) {
                item = data->choiceBounds[static_cast<size_t>(index)];
                OffsetRect(&item, -data->choiceOrigin.x, -data->choiceOrigin.y);
            } else {
                item = RECT{ MulDiv(client.right, index, count), 0,
                    MulDiv(client.right, index + 1, count), client.bottom };
            }
            COLORREF fillColor = index + 1 == data->selectedChoice ? selected :
                (index + 1 == data->hotChoice ? hover : normal);
            HBRUSH fill = CreateSolidBrush(fillColor);
            HPEN pen = CreatePen(PS_SOLID, 1, fillColor);
            HGDIOBJ oldBrush = SelectObject(dc, fill);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            RoundRect(dc, item.left, item.top, item.right, item.bottom, 8, 8);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(pen);
            DeleteObject(fill);
            SetTextColor(dc, index + 1 == data->selectedChoice ? RGB(255,255,255) : text);
            InflateRect(&item, -5, -2);
            DrawTextW(dc, data->choiceLabels[static_cast<size_t>(index)].c_str(), -1,
                &item, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        SelectObject(dc, oldFont);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureOptionChoiceClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = OptionChoiceWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
        windowClass.lpszClassName = kOptionChoiceClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

bool EnsureFlatMaskClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = FlatMaskWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kFlatMaskClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

bool EnsureOptionEdgeClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = OptionEdgeWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kOptionEdgeClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

FlatMaskData* GetFlatMaskData(HWND window)
{
    if (!IsWindow(window)) return nullptr;
    auto* data = reinterpret_cast<FlatMaskData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    return data && data->magic == kFlatMaskMagic ? data : nullptr;
}

int ClampSliderValue(const SliderData& data, int value)
{
    return std::clamp(value, data.minimum, data.maximum);
}

int SliderValueFromPoint(const SliderData& data, int x, int y)
{
    RECT client{};
    GetClientRect(data.window, &client);
    constexpr int inset = 10;
    const int span = data.orientation == CHSLIDER_VERTICAL
        ? std::max(1, static_cast<int>(client.bottom - client.top) - inset * 2)
        : std::max(1, static_cast<int>(client.right - client.left) - inset * 2);
    int position = data.orientation == CHSLIDER_VERTICAL
        ? (client.bottom - inset - y)
        : (x - client.left - inset);
    position = std::clamp(position, 0, span);
    const int range = data.maximum - data.minimum;
    if (range <= 0) return data.minimum;
    return data.minimum + MulDiv(position, range, span);
}

void NotifySliderChanged(const SliderData& data)
{
    if (!IsWindow(data.notifyButton)) {
        // WM_NULL does not produce a Clarion event, so the ACCEPT loop can
        // remain blocked until some unrelated control is used. WM_TIMER is
        // translated by Clarion and lets CHModernSlider.TakeEvent() poll the
        // value and post EVENT:Accepted to its REGION immediately.
        if (IsWindow(data.parent))
            PostMessageW(data.parent, WM_TIMER, 0x4348534C, 0);
        return;
    }
    if (!IsWindowEnabled(data.notifyButton))
        return;
    // BM_CLICK makes a BUTTON briefly take/release mouse capture, which
    // interrupts an active slider drag. Deliver the same BN_CLICKED command
    // directly to the Clarion control's parent without touching capture.
    HWND owner = GetParent(data.notifyButton);
    const int controlId = GetDlgCtrlID(data.notifyButton);
    if (IsWindow(owner))
        PostMessageW(owner, WM_COMMAND,
            MAKEWPARAM(static_cast<WORD>(controlId), BN_CLICKED),
            reinterpret_cast<LPARAM>(data.notifyButton));
}

bool SetSliderValueInternal(SliderData& data, int value, bool notify)
{
    value = ClampSliderValue(data, value);
    if (value == data.value) return false;
    data.value = value;
    InvalidateRect(data.window, nullptr, FALSE);
    if (notify) NotifySliderChanged(data);
    return true;
}

COLORREF MixColor(COLORREF first, COLORREF second, int secondWeight)
{
    secondWeight = std::clamp(secondWeight, 0, 255);
    const int firstWeight = 255 - secondWeight;
    return RGB(
        (GetRValue(first) * firstWeight + GetRValue(second) * secondWeight) / 255,
        (GetGValue(first) * firstWeight + GetGValue(second) * secondWeight) / 255,
        (GetBValue(first) * firstWeight + GetBValue(second) * secondWeight) / 255);
}

void FillRoundedVerticalGradient(HDC dc, const RECT& rectangle,
    int radius, COLORREF top, COLORREF bottom)
{
    const int height = std::max(1, static_cast<int>(rectangle.bottom - rectangle.top));
    const int saved = SaveDC(dc);
    HRGN clip = CreateRoundRectRgn(rectangle.left, rectangle.top,
        rectangle.right + 1, rectangle.bottom + 1, radius, radius);
    SelectClipRgn(dc, clip);
    for (int row = 0; row < height; ++row) {
        const int weight = height > 1 ? MulDiv(row, 255, height - 1) : 0;
        RECT band{ rectangle.left, rectangle.top + row, rectangle.right,
            rectangle.top + row + 1 };
        HBRUSH brush = CreateSolidBrush(MixColor(top, bottom, weight));
        FillRect(dc, &band, brush);
        DeleteObject(brush);
    }
    DeleteObject(clip);
    RestoreDC(dc, saved);
}

void PaintSlider(HWND window, SliderData& data, HDC target)
{
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(1, static_cast<int>(client.right - client.left));
    const int height = std::max(1, static_cast<int>(client.bottom - client.top));
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    ThemeData* theme = FindAncestorTheme(window);
    const bool light = theme && theme->mode == CHTHEME_LIGHT;
    const COLORREF track = light ? data.lightTrack : data.darkTrack;
    const COLORREF accent = light ? data.lightFill : data.darkFill;
    const bool enabled = IsWindowEnabled(window) != FALSE;
    const COLORREF activeAccent = enabled
        ? (data.tracking || data.hover ? MixColor(accent, RGB(255, 255, 255), 16) : accent)
        : (light ? RGB(158, 170, 182) : RGB(69, 83, 97));

    // The slider is a color-keyed layered child. Only its channel, fill and
    // thumb are opaque; the untouched client area exposes the Clarion PANEL,
    // SHEET page, image, or card underneath.
    HBRUSH backgroundBrush = CreateSolidBrush(kSliderTransparencyKey);
    FillRect(dc, &client, backgroundBrush);
    DeleteObject(backgroundBrush);

    constexpr int inset = 10;
    constexpr int trackThickness = 6;
    const int range = std::max(1, data.maximum - data.minimum);
    const int valueOffset = data.value - data.minimum;
    RECT trackRect{};
    RECT fillRect{};
    POINT thumbCenter{};
    if (data.orientation == CHSLIDER_VERTICAL) {
        const int centerX = width / 2;
        const int span = std::max(1, height - inset * 2);
        const int thumbY = height - inset - MulDiv(valueOffset, span, range);
        trackRect = { centerX - trackThickness / 2, inset,
            centerX + (trackThickness + 1) / 2, height - inset };
        fillRect = { trackRect.left, thumbY, trackRect.right, trackRect.bottom };
        thumbCenter = { centerX, thumbY };
    } else {
        const int centerY = height / 2;
        const int span = std::max(1, width - inset * 2);
        const int thumbX = inset + MulDiv(valueOffset, span, range);
        trackRect = { inset, centerY - trackThickness / 2,
            width - inset, centerY + (trackThickness + 1) / 2 };
        fillRect = { trackRect.left, trackRect.top, thumbX, trackRect.bottom };
        thumbCenter = { thumbX, centerY };
    }

    // Recessed channel with a restrained inner highlight.
    RECT channel = trackRect;
    InflateRect(&channel, data.orientation == CHSLIDER_VERTICAL ? 2 : 0,
        data.orientation == CHSLIDER_VERTICAL ? 0 : 2);
    HBRUSH channelBrush = CreateSolidBrush(light ? RGB(222, 229, 236) : RGB(13, 23, 33));
    HPEN channelPen = CreatePen(PS_SOLID, 1,
        light ? RGB(157, 173, 188) : RGB(45, 64, 81));
    HGDIOBJ oldBrush = SelectObject(dc, channelBrush);
    HGDIOBJ oldPen = SelectObject(dc, channelPen);
    RoundRect(dc, channel.left, channel.top, channel.right, channel.bottom, 6, 6);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(channelPen);
    DeleteObject(channelBrush);

    HBRUSH trackBrush = CreateSolidBrush(track);
    FillRect(dc, &trackRect, trackBrush);
    DeleteObject(trackBrush);
    HBRUSH fillBrush = CreateSolidBrush(activeAccent);
    FillRect(dc, &fillRect, fillBrush);
    DeleteObject(fillBrush);

    // A compact dimensional fader handle, matching the V4 control-center look.
    RECT thumbRect{};
    if (data.orientation == CHSLIDER_VERTICAL)
        thumbRect = { thumbCenter.x - 9, thumbCenter.y - 6,
            thumbCenter.x + 10, thumbCenter.y + 7 };
    else
        thumbRect = { thumbCenter.x - 6, thumbCenter.y - 9,
            thumbCenter.x + 7, thumbCenter.y + 10 };

    RECT shadowRect = thumbRect;
    OffsetRect(&shadowRect, 1, 2);
    HBRUSH shadowBrush = CreateSolidBrush(light ? RGB(145, 157, 169) : RGB(3, 8, 13));
    HPEN shadowPen = CreatePen(PS_SOLID, 1, light ? RGB(145, 157, 169) : RGB(3, 8, 13));
    oldBrush = SelectObject(dc, shadowBrush);
    oldPen = SelectObject(dc, shadowPen);
    RoundRect(dc, shadowRect.left, shadowRect.top, shadowRect.right, shadowRect.bottom, 5, 5);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(shadowPen);
    DeleteObject(shadowBrush);

    // The handle is a neutral piece of chrome. Custom track/fill colors must
    // not tint it; otherwise multiple sliders appear to have different knob
    // materials simply because their value channels use different colors.
    const COLORREF thumbTop = enabled
        ? (light ? RGB(250, 252, 254) : RGB(164, 178, 192))
        : (light ? RGB(218, 224, 230) : RGB(105, 117, 129));
    const COLORREF thumbBottom = enabled
        ? (light ? RGB(174, 188, 202) : RGB(69, 83, 97))
        : (light ? RGB(174, 184, 194) : RGB(51, 63, 75));
    FillRoundedVerticalGradient(dc, thumbRect, 5, thumbTop, thumbBottom);
    HPEN outlinePen = CreatePen(PS_SOLID, 1,
        light ? RGB(89, 105, 121) : RGB(28, 42, 56));
    oldPen = SelectObject(dc, outlinePen);
    oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, thumbRect.left, thumbRect.top, thumbRect.right, thumbRect.bottom, 5, 5);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(outlinePen);

    // Fine center grip gives the handle a tactile fader appearance.
    HPEN gripPen = CreatePen(PS_SOLID, 1,
        enabled
            ? (light ? RGB(255, 255, 255) : RGB(210, 220, 230))
            : (light ? RGB(205, 211, 217) : RGB(134, 144, 154)));
    oldPen = SelectObject(dc, gripPen);
    if (data.orientation == CHSLIDER_VERTICAL) {
        MoveToEx(dc, thumbRect.left + 4, thumbCenter.y, nullptr);
        LineTo(dc, thumbRect.right - 4, thumbCenter.y);
    } else {
        MoveToEx(dc, thumbCenter.x, thumbRect.top + 4, nullptr);
        LineTo(dc, thumbCenter.x, thumbRect.bottom - 4);
    }
    SelectObject(dc, oldPen);
    DeleteObject(gripPen);

    if (GetFocus() == window && enabled) {
        // Keep keyboard focus visible without drawing the traditional dotted
        // rectangle around the entire slider.
        RECT focusRect = thumbRect;
        InflateRect(&focusRect, 2, 2);
        const COLORREF focusColor = light ? RGB(77, 145, 221) : RGB(76, 157, 255);
        HPEN focusPen = CreatePen(PS_SOLID, 1, focusColor);
        HGDIOBJ previousPen = SelectObject(dc, focusPen);
        HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, focusRect.left, focusRect.top, focusRect.right, focusRect.bottom, 7, 7);
        SelectObject(dc, previousBrush);
        SelectObject(dc, previousPen);
        DeleteObject(focusPen);
    }

    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK SliderWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<SliderData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        data = reinterpret_cast<SliderData*>(create->lpCreateParams);
        if (!data) return FALSE;
        data->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintSlider(window, *data, dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (IsWindowEnabled(window)) {
            SetFocus(window);
            SetCapture(window);
            data->tracking = true;
            SetSliderValueInternal(*data,
                SliderValueFromPoint(*data, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), true);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (!data->hover) {
            data->hover = true;
            TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, window, 0 };
            TrackMouseEvent(&tracking);
            InvalidateRect(window, nullptr, FALSE);
        }
        if (data->tracking && GetCapture() == window)
            SetSliderValueInternal(*data,
                SliderValueFromPoint(*data, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), true);
        return 0;
    case WM_MOUSELEAVE:
        data->hover = false;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        if (data->tracking) {
            data->tracking = false;
            if (GetCapture() == window) ReleaseCapture();
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        data->tracking = false;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        if (IsWindowEnabled(window)) {
            const int direction = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
            SetSliderValueInternal(*data, data->value + direction * data->step, true);
        }
        return 0;
    case WM_KEYDOWN: {
        if (!IsWindowEnabled(window)) return 0;
        int value = data->value;
        switch (wParam) {
        case VK_LEFT:
        case VK_DOWN: value -= data->step; break;
        case VK_RIGHT:
        case VK_UP: value += data->step; break;
        case VK_HOME: value = data->minimum; break;
        case VK_END: value = data->maximum; break;
        case VK_PRIOR: value += data->step * 10; break;
        case VK_NEXT: value -= data->step * 10; break;
        default: return DefWindowProcW(window, message, wParam, lParam);
        }
        SetSliderValueInternal(*data, value, true);
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_NCDESTROY:
        KillTimer(window, 1);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        data->magic = 0;
        delete data;
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureSliderClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        windowClass.lpfnWndProc = SliderWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
        windowClass.lpszClassName = kSliderClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

SliderData* GetSliderData(HWND window)
{
    if (!IsWindow(window)) return nullptr;
    auto* data = reinterpret_cast<SliderData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    return data && data->magic == kSliderMagic ? data : nullptr;
}

void RestoreSliderChildren(HWND parent)
{
    EnumChildWindows(parent,
        [](HWND child, LPARAM) -> BOOL {
            if (!GetSliderData(child) || !IsWindowVisible(child))
                return TRUE;

            SetWindowPos(child, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
            RedrawWindow(child, nullptr, nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            return TRUE;
        }, 0);
}

std::vector<std::wstring> ParseTabLabels(const char* definition)
{
    std::vector<std::wstring> labels;
    if (!definition || !*definition) return labels;
    std::string source(definition);
    size_t start = 0;
    while (start <= source.size()) {
        const size_t end = source.find('|', start);
        std::string item = source.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        const int needed = MultiByteToWideChar(CP_ACP, 0, item.c_str(),
            static_cast<int>(item.size()), nullptr, 0);
        std::wstring wide(static_cast<size_t>(std::max(0, needed)), L'\0');
        if (needed > 0)
            MultiByteToWideChar(CP_ACP, 0, item.c_str(),
                static_cast<int>(item.size()), wide.data(), needed);
        labels.push_back(std::move(wide));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return labels;
}

RECT GetTabRectangle(const TabStripData& data, int index, const RECT& client)
{
    int count = 0;
    int visibleIndex = 0;
    for (int candidate = 0;
        candidate < static_cast<int>(data.labels.size()); ++candidate) {
        const bool isVisible = candidate >= static_cast<int>(data.visible.size()) ||
            data.visible[static_cast<size_t>(candidate)];
        if (!isVisible) continue;
        if (candidate < index) ++visibleIndex;
        ++count;
    }
    if (index < static_cast<int>(data.visible.size()) &&
        !data.visible[static_cast<size_t>(index)]) return RECT{};
    count = std::max(1, count);
    const int width = client.right - client.left;
    return {
        client.left + MulDiv(visibleIndex, width, count),
        client.top,
        client.left + MulDiv(visibleIndex + 1, width, count),
        client.bottom
    };
}

int TabFromPoint(const TabStripData& data, int x, int y)
{
    RECT client{};
    GetClientRect(data.window, &client);
    if (!PtInRect(&client, POINT{ x, y }) || data.labels.empty()) return 0;
    const POINT point{ x, y };
    for (int index = 0; index < static_cast<int>(data.labels.size()); ++index) {
        if (index < static_cast<int>(data.visible.size()) &&
            !data.visible[static_cast<size_t>(index)]) continue;
        const RECT rectangle = GetTabRectangle(data, index, client);
        if (PtInRect(&rectangle, point)) return index + 1;
    }
    return 0;
}

void NotifyTabChanged(const TabStripData& data)
{
    if (!IsWindow(data.notifyButton) || !IsWindowEnabled(data.notifyButton)) return;
    HWND owner = GetParent(data.notifyButton);
    if (!IsWindow(owner)) return;
    PostMessageW(owner, WM_COMMAND,
        MAKEWPARAM(static_cast<WORD>(GetDlgCtrlID(data.notifyButton)), BN_CLICKED),
        reinterpret_cast<LPARAM>(data.notifyButton));
}

void PaintTabStrip(HWND window, TabStripData& data, HDC target)
{
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(1, static_cast<int>(client.right));
    const int height = std::max(1, static_cast<int>(client.bottom));
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    ThemeData* theme = FindAncestorTheme(window);
    const bool light = theme && theme->mode == CHTHEME_LIGHT;
    const COLORREF background = theme ? theme->surface :
        (light ? RGB(250, 252, 254) : RGB(13, 23, 33));
    const COLORREF text = theme ? theme->text :
        (light ? RGB(25, 38, 52) : RGB(235, 241, 248));
    const COLORREF secondary = theme ? theme->secondaryText :
        (light ? RGB(86, 101, 116) : RGB(158, 173, 189));
    const COLORREF disabled = theme ? theme->disabledText :
        (light ? RGB(151, 161, 171) : RGB(104, 118, 132));
    const COLORREF accent = theme ? theme->accent : RGB(35, 132, 255);

    HBRUSH backgroundBrush = CreateSolidBrush(background);
    FillRect(dc, &client, backgroundBrush);
    DeleteObject(backgroundBrush);

    HFONT font = theme && theme->font ? theme->font :
        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);

    for (int index = 0; index < static_cast<int>(data.labels.size()); ++index) {
        if (index < static_cast<int>(data.visible.size()) &&
            !data.visible[static_cast<size_t>(index)]) continue;
        const int tab = index + 1;
        RECT rectangle = GetTabRectangle(data, index, client);
        const bool isSelected = tab == data.selected;
        const bool isHover = tab == data.hover;
        const bool isEnabled = index < static_cast<int>(data.enabled.size())
            ? data.enabled[static_cast<size_t>(index)] : true;

        if (isSelected || isHover) {
            HBRUSH brush = CreateSolidBrush(isSelected
                ? (light ? RGB(232, 241, 252) : RGB(22, 42, 61))
                : (light ? RGB(241, 246, 251) : RGB(18, 32, 46)));
            FillRect(dc, &rectangle, brush);
            DeleteObject(brush);
        }

        if (rectangle.left > client.left) {
            HPEN separator = CreatePen(PS_SOLID, 1,
                light ? RGB(214, 223, 232) : RGB(38, 57, 74));
            HGDIOBJ oldPen = SelectObject(dc, separator);
            MoveToEx(dc, rectangle.left, rectangle.top + 7, nullptr);
            LineTo(dc, rectangle.left, rectangle.bottom - 7);
            SelectObject(dc, oldPen);
            DeleteObject(separator);
        }

        RECT textRectangle = rectangle;
        InflateRect(&textRectangle, -8, 0);
        SetTextColor(dc, !isEnabled ? disabled :
            (isSelected ? text : secondary));
        DrawTextW(dc, data.labels[static_cast<size_t>(index)].c_str(), -1,
            &textRectangle, DT_CENTER | DT_VCENTER | DT_SINGLELINE |
            DT_END_ELLIPSIS | DT_NOPREFIX);

        if (isSelected) {
            RECT indicator{ rectangle.left + 5, rectangle.bottom - 3,
                rectangle.right - 5, rectangle.bottom };
            HBRUSH accentBrush = CreateSolidBrush(accent);
            FillRect(dc, &indicator, accentBrush);
            DeleteObject(accentBrush);
        }
    }

    HPEN bottomPen = CreatePen(PS_SOLID, 1,
        light ? RGB(190, 204, 217) : RGB(45, 66, 84));
    HGDIOBJ oldPen = SelectObject(dc, bottomPen);
    MoveToEx(dc, client.left, client.bottom - 1, nullptr);
    LineTo(dc, client.right, client.bottom - 1);
    SelectObject(dc, oldPen);
    DeleteObject(bottomPen);
    SelectObject(dc, oldFont);

    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK TabStripWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<TabStripData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        data = reinterpret_cast<TabStripData*>(create->lpCreateParams);
        if (!data) return FALSE;
        data->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        PaintTabStrip(window, *data, dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int hover = TabFromPoint(*data, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hover != data->hover) {
            data->hover = hover;
            InvalidateRect(window, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, window, 0 };
        TrackMouseEvent(&tracking);
        return 0;
    }
    case WM_MOUSELEAVE:
        data->hover = 0;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(window);
        const int selected = TabFromPoint(*data,
            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (selected > 0 &&
            data->enabled[static_cast<size_t>(selected - 1)] &&
            selected != data->selected) {
            data->selected = selected;
            InvalidateRect(window, nullptr, FALSE);
            NotifyTabChanged(*data);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (data->labels.empty()) return 0;
        int candidate = data->selected;
        const int direction = wParam == VK_LEFT ? -1 : (wParam == VK_RIGHT ? 1 : 0);
        if (!direction) return DefWindowProcW(window, message, wParam, lParam);
        for (size_t attempt = 0; attempt < data->labels.size(); ++attempt) {
            candidate += direction;
            if (candidate < 1) candidate = static_cast<int>(data->labels.size());
            if (candidate > static_cast<int>(data->labels.size())) candidate = 1;
            const size_t candidateIndex = static_cast<size_t>(candidate - 1);
            const bool isVisible = candidateIndex >= data->visible.size() ||
                data->visible[candidateIndex];
            if (isVisible && data->enabled[candidateIndex]) {
                data->selected = candidate;
                InvalidateRect(window, nullptr, FALSE);
                NotifyTabChanged(*data);
                break;
            }
        }
        return 0;
    }
    case WM_NCDESTROY:
        if (IsWindow(data->leftEdge)) DestroyWindow(data->leftEdge);
        if (IsWindow(data->rightEdge)) DestroyWindow(data->rightEdge);
        if (IsWindow(data->bottomEdge)) DestroyWindow(data->bottomEdge);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        data->magic = 0;
        delete data;
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureTabStripClass()
{
    static std::once_flag once;
    static ATOM atom = 0;
    std::call_once(once, [] {
        WNDCLASSEXW windowClass{ sizeof(windowClass) };
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = TabStripWindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
        windowClass.lpszClassName = kTabStripClass;
        atom = RegisterClassExW(&windowClass);
    });
    return atom != 0;
}

TabStripData* GetTabStripData(HWND window)
{
    if (!IsWindow(window)) return nullptr;
    auto* data = reinterpret_cast<TabStripData*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    return data && data->magic == kTabStripMagic ? data : nullptr;
}


void ApplyMenuBackground(HMENU menu, HBRUSH brush)
{
    if (!menu) return;
    MENUINFO info{ sizeof(info) };
    info.fMask = MIM_BACKGROUND | MIM_STYLE;
    info.hbrBack = brush;
    info.dwStyle = MNS_CHECKORBMP;
    SetMenuInfo(menu, &info);
    const int count = GetMenuItemCount(menu);
    for (int position = 0; position < count; ++position) {
        HMENU submenu = GetSubMenu(menu, position);
        if (submenu) ApplyMenuBackground(submenu, brush);
    }
}

void RestoreMenu(HWND window, ThemeData& data)
{
    for (auto iterator = data.menuItems.rbegin(); iterator != data.menuItems.rend(); ++iterator) {
        MenuItemData& item = **iterator;
        if (!IsMenu(item.menu)) continue;
        MENUITEMINFOW info{ sizeof(info) };
        info.fMask = MIIM_FTYPE | MIIM_DATA;
        info.fType = item.originalType;
        info.dwItemData = item.originalData;
        SetMenuItemInfoW(item.menu, item.position, TRUE, &info);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_menuItems.erase(&item);
        }
    }
    HMENU root = GetMenu(window);
    if (root) ApplyMenuBackground(root, GetSysColorBrush(COLOR_MENU));
    data.menuItems.clear();
    data.menuAttached = false;
    data.attachedMenu = nullptr;
    DrawMenuBar(window);
}

int MenuSourceTopLevelCount(const MenuSourceData& source)
{
    int count = 0;
    for (const auto& entry : source.captions) {
        if (entry.first.find(L'/') != std::wstring::npos) continue;
        wchar_t* end = nullptr;
        const long position = std::wcstol(entry.first.c_str(), &end, 10);
        if (end && *end == L'\0' && position >= 0)
            count = std::max(count, static_cast<int>(position) + 1);
    }
    return count;
}

std::wstring RebaseMenuPath(const std::wstring& path, int topLevelOffset)
{
    const size_t slash = path.find(L'/');
    const std::wstring first = slash == std::wstring::npos
        ? path : path.substr(0, slash);
    wchar_t* end = nullptr;
    const long position = std::wcstol(first.c_str(), &end, 10);
    if (!end || *end != L'\0' || position < 0) return path;
    return std::to_wstring(position + topLevelOffset) +
        (slash == std::wstring::npos ? L"" : path.substr(slash));
}

void AppendMenuSource(ThemeData& data, const MenuSourceData& source, int offset)
{
    for (const auto& entry : source.captions)
        data.menuCaptions[RebaseMenuPath(entry.first, offset)] = entry.second;
    for (const auto& entry : source.icons)
        data.menuIcons[RebaseMenuPath(entry.first, offset)] = entry.second;
    for (const auto& path : source.hiddenPaths)
        data.hiddenMenuPaths.insert(RebaseMenuPath(path, offset));
    data.menuCaptionsById.insert(source.captionsById.begin(),
        source.captionsById.end());
    data.menuIconsById.insert(source.iconsById.begin(),
        source.iconsById.end());
    data.hiddenMenuIds.insert(source.hiddenIds.begin(), source.hiddenIds.end());
}

HWND ActiveMdiChild(const ThemeData& data, bool* maximized = nullptr)
{
    if (!IsWindow(data.mdiClient)) return nullptr;
    BOOL isMaximized = FALSE;
    HWND child = reinterpret_cast<HWND>(SendMessageW(data.mdiClient,
        WM_MDIGETACTIVE, 0, reinterpret_cast<LPARAM>(&isMaximized)));
    if (maximized) *maximized = isMaximized != FALSE;
    return child;
}

void BuildMergedMenuDefinition(HWND owner, ThemeData& data)
{
    data.menuCaptions.clear();
    data.menuIcons.clear();
    data.hiddenMenuPaths.clear();
    data.menuCaptionsById.clear();
    data.menuIconsById.clear();
    data.hiddenMenuIds.clear();

    HWND active = ActiveMdiChild(data);
    if (!active || data.menuSources.find(active) == data.menuSources.end())
        active = IsWindow(data.activeMenuSource) ? data.activeMenuSource : nullptr;

    const auto frame = data.menuSources.find(owner);
    const auto child = active && active != owner
        ? data.menuSources.find(active) : data.menuSources.end();
    const int frameCount = frame != data.menuSources.end()
        ? MenuSourceTopLevelCount(frame->second) : 0;
    const int childCount = child != data.menuSources.end()
        ? MenuSourceTopLevelCount(child->second) : 0;

    // A maximized MDI child contributes a system-menu icon to the actual
    // merged HMENU, but Clarion does not include it in either declaration.
    // Clarion's NOFRAME/MAXIMIZE child does not reliably set the BOOL returned
    // by WM_MDIGETACTIVE, so derive all synthetic leading entries from the
    // real merged menu size.
    const int actualCount = GetMenuItemCount(GetMenu(owner));
    int offset = std::max(0, actualCount - frameCount - childCount);

    if (frame != data.menuSources.end()) {
        AppendMenuSource(data, frame->second, offset);
        offset += frameCount;
    }

    if (child != data.menuSources.end())
        AppendMenuSource(data, child->second, offset);
}

bool RefreshMergedMenu(HWND owner, ThemeData& data, bool force)
{
    if (!IsWindow(owner)) return false;
    const HMENU current = GetMenu(owner);
    if (!current) return false;

    BuildMergedMenuDefinition(owner, data);
    if (!force && data.menuAttached && data.attachedMenu == current) {
        ApplyMenuBackground(current, data.menuBrush);
        DrawMenuBar(owner);
        return true;
    }
    if (data.menuAttached) RestoreMenu(owner, data);
    return AttachMenu(owner, data);
}

HICON LoadClarionMenuIcon(const std::wstring& value)
{
    if (value.empty()) return nullptr;

    // Clarion built-in ICON:* values are four-byte tokens rather than
    // filenames: FF, family, ordinal, 7F. PROP:Icon returns that token
    // verbatim. Family 2 icons are stored in ClaRUN.dll using resource IDs
    // 7F00h + ordinal (for example ICON:Open => 7F05h).
    if (value.size() >= 4 &&
        (static_cast<unsigned int>(value[0]) & 0xFFu) == 0xFFu &&
        (static_cast<unsigned int>(value[3]) & 0xFFu) == 0x7Fu) {
        const unsigned int family =
            static_cast<unsigned int>(value[1]) & 0xFFu;
        const unsigned int ordinal =
            static_cast<unsigned int>(value[2]) & 0xFFu;
        if (family == 2 && ordinal) {
            HMODULE clarionRuntime = GetModuleHandleW(L"ClaRUN.dll");
            if (clarionRuntime) {
                HICON icon = reinterpret_cast<HICON>(LoadImageW(
                    clarionRuntime,
                    MAKEINTRESOURCEW(0x7F00u + ordinal),
                    IMAGE_ICON, 16, 16, 0));
                if (icon) return icon;
            }
        } else if (family == 1 && ordinal) {
            HICON icon = reinterpret_cast<HICON>(LoadImageW(
                nullptr,
                MAKEINTRESOURCEW(0x7EFFu + ordinal),
                IMAGE_ICON, 16, 16, LR_SHARED));
            if (icon) return CopyIcon(icon);
        }
    }

    std::wstring name = value;
    if (!name.empty() && name.front() == L'~') name.erase(name.begin());
    if (name.empty()) return nullptr;

    HINSTANCE module = GetModuleHandleW(nullptr);
    HICON icon = reinterpret_cast<HICON>(LoadImageW(module, name.c_str(),
        IMAGE_ICON, 16, 16, 0));
    if (!icon) {
        icon = reinterpret_cast<HICON>(LoadImageW(nullptr, name.c_str(),
            IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
    }
    return icon;
}

bool AttachMenuItems(HWND window, ThemeData& data, HMENU menu, bool topLevel,
    const std::wstring& parentPath)
{
    const int count = GetMenuItemCount(menu);
    if (count < 0) return false;
    for (int position = 0; position < count; ++position) {
        MENUITEMINFOW query{ sizeof(query) };
        wchar_t text[512]{};
        query.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_DATA |
            MIIM_SUBMENU | MIIM_STRING | MIIM_BITMAP | MIIM_CHECKMARKS;
        query.dwTypeData = text;
        query.cch = static_cast<UINT>(std::size(text) - 1);
        if (!GetMenuItemInfoW(menu, position, TRUE, &query)) continue;

        const std::wstring path = parentPath.empty()
            ? std::to_wstring(position)
            : parentPath + L"/" + std::to_wstring(position);
        const bool hasCommandId = !query.hSubMenu && query.wID != 0 &&
            query.wID != static_cast<UINT>(-1);
        const auto suppliedCaptionById = hasCommandId
            ? data.menuCaptionsById.find(query.wID) : data.menuCaptionsById.end();
        const auto suppliedIconById = hasCommandId
            ? data.menuIconsById.find(query.wID) : data.menuIconsById.end();
        const auto suppliedCaptionByPath = data.menuCaptions.find(path);
        const auto suppliedIconByPath = data.menuIcons.find(path);
        const std::wstring* suppliedCaption =
            suppliedCaptionById != data.menuCaptionsById.end()
            ? &suppliedCaptionById->second
            : suppliedCaptionByPath != data.menuCaptions.end()
            ? &suppliedCaptionByPath->second : nullptr;
        const std::wstring* suppliedIcon =
            suppliedIconById != data.menuIconsById.end()
            ? &suppliedIconById->second
            : suppliedIconByPath != data.menuIcons.end()
            ? &suppliedIconByPath->second : nullptr;
        const bool hasNativeText = text[0] != L'\0';
        const bool suppliedSeparator = !hasNativeText && suppliedCaption &&
            *suppliedCaption == L"-";
        const bool suppliedHidden =
            (hasCommandId && data.hiddenMenuIds.find(query.wID) !=
                data.hiddenMenuIds.end()) ||
            data.hiddenMenuPaths.find(path) != data.hiddenMenuPaths.end();

        if (suppliedHidden) {
            // Keep Clarion's private HMENU structure intact. Windows reserves
            // a minimal row for a zero-sized owner-draw item, but mutating
            // these entries can suppress unrelated live Clarion commands.
        }

        // Clarion stores owner-drawn menu captions and layout metadata in a
        // private item-data structure rather than the Win32 text field.
        // Only wrap those items when the Clarion-side bridge supplied a safe
        // caption for this exact position.
        if ((query.fType & MFT_OWNERDRAW) &&
            !suppliedCaption && !suppliedHidden)
            continue;

        auto item = std::make_unique<MenuItemData>();
        item->window = window;
        item->menu = menu;
        item->position = static_cast<UINT>(position);
        item->originalType = query.fType;
        item->originalData = query.dwItemData;
        item->bitmap = query.hbmpItem;
        if (suppliedIcon)
            item->icon = LoadClarionMenuIcon(*suppliedIcon);
        item->text = suppliedSeparator ? std::wstring() :
            hasNativeText ? std::wstring(text) :
            suppliedCaption ? *suppliedCaption : std::wstring();
        item->separator = suppliedSeparator ||
            (query.fType & MFT_SEPARATOR) != 0;
        item->hidden = suppliedHidden;
        item->topLevel = topLevel;
        item->hasSubmenu = query.hSubMenu != nullptr;
        MenuItemData* itemPointer = item.get();
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_menuItems.insert(itemPointer);
        }
        data.menuItems.push_back(std::move(item));

        MENUITEMINFOW update{ sizeof(update) };
        update.fMask = MIIM_FTYPE | MIIM_DATA;
        update.fType = query.fType | MFT_OWNERDRAW;
        update.dwItemData = reinterpret_cast<ULONG_PTR>(itemPointer);
        SetMenuItemInfoW(menu, position, TRUE, &update);
        if (query.hSubMenu) AttachMenuItems(window, data, query.hSubMenu, false, path);
    }
    return true;
}

bool AttachMenu(HWND window, ThemeData& data)
{
    HMENU menu = GetMenu(window);
    if (!menu) return false;
    EnsureMenuPopupHook();
    if (data.menuAttached) RestoreMenu(window, data);
    data.menuAttached = AttachMenuItems(window, data, menu, true, L"");
    data.attachedMenu = data.menuAttached ? menu : nullptr;
    ApplyMenuBackground(menu, data.menuBrush);
    // Menu-bar item widths are cached when Clarion first attaches its
    // owner-drawn HMENU. Reattach the same handle on ordinary windows so
    // Windows requests fresh WM_MEASUREITEM values from CHTheme. Do not
    // detach an MDI frame menu: doing so makes the MDI client rebuild its
    // Window-menu state and can append the automatic numbered child list.
    if (!IsWindow(data.mdiClient) && GetMenu(window) == menu) {
        SetMenu(window, nullptr);
        SetMenu(window, menu);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    DrawMenuBar(window);
    return data.menuAttached;
}

HWND FindMenuOwner(HWND window)
{
    // An MDI child may retain its source HMENU even while Windows displays a
    // merged HMENU on the frame. Always theme the visible frame menu.
    if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_MDICHILD) != 0) {
        HWND mdiClient = GetParent(window);
        HWND frame = mdiClient ? GetParent(mdiClient) : nullptr;
        if (frame && GetMenu(frame)) return frame;
    }
    HWND candidate = window;
    while (candidate) {
        if (GetMenu(candidate)) return candidate;
        candidate = GetParent(candidate);
    }
    candidate = GetAncestor(window, GA_ROOT);
    return candidate && GetMenu(candidate) ? candidate : nullptr;
}

std::wstring AnsiToWide(const char* text)
{
    if (!text || !*text) return {};
    const int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (length <= 1) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), length);
    result.pop_back();
    return result;
}

std::wstring VisibleMenuPath(const std::wstring& path,
    const std::unordered_set<std::wstring>& hiddenPaths)
{
    std::wstring result;
    std::wstring rawParent;
    size_t segmentStart = 0;
    while (segmentStart < path.size()) {
        const size_t slash = path.find(L'/', segmentStart);
        const std::wstring segment = path.substr(segmentStart,
            slash == std::wstring::npos ? std::wstring::npos :
            slash - segmentStart);
        wchar_t* end = nullptr;
        const long ordinal = std::wcstol(segment.c_str(), &end, 10);
        if (!end || *end != L'\0' || ordinal < 0) return path;

        long hiddenBefore = 0;
        for (const auto& hiddenPath : hiddenPaths) {
            const size_t hiddenSlash = hiddenPath.find_last_of(L'/');
            const std::wstring hiddenParent = hiddenSlash == std::wstring::npos
                ? L"" : hiddenPath.substr(0, hiddenSlash + 1);
            if (hiddenParent != rawParent) continue;
            const wchar_t* hiddenSegment = hiddenPath.c_str() +
                (hiddenSlash == std::wstring::npos ? 0 : hiddenSlash + 1);
            wchar_t* hiddenEnd = nullptr;
            const long hiddenOrdinal = std::wcstol(hiddenSegment, &hiddenEnd, 10);
            if (hiddenEnd && *hiddenEnd == L'\0' &&
                hiddenOrdinal >= 0 && hiddenOrdinal < ordinal)
                ++hiddenBefore;
        }
        if (!result.empty()) result += L"/";
        result += std::to_wstring(ordinal - hiddenBefore);
        rawParent += segment + L"/";
        if (slash == std::wstring::npos) break;
        segmentStart = slash + 1;
    }
    return result;
}

void ParseMenuDefinition(const char* definition,
    std::unordered_map<std::wstring, std::wstring>& captions,
    std::unordered_map<std::wstring, std::wstring>& icons,
    std::unordered_set<std::wstring>& hiddenPaths,
    std::unordered_map<UINT, std::wstring>& captionsById,
    std::unordered_map<UINT, std::wstring>& iconsById,
    std::unordered_set<UINT>& hiddenIds)
{
    captions.clear();
    icons.clear();
    hiddenPaths.clear();
    captionsById.clear();
    iconsById.clear();
    hiddenIds.clear();
    const std::wstring source = AnsiToWide(definition);
    size_t start = 0;
    while (start < source.size()) {
        size_t end = source.find(L';', start);
        if (end == std::wstring::npos) end = source.size();
        const std::wstring entry = source.substr(start, end - start);
        const size_t equals = entry.find(L'=');
        if (equals != std::wstring::npos && equals > 0) {
            std::wstring path = entry.substr(0, equals);
            std::wstring caption = entry.substr(equals + 1);
            std::wstring icon;
            const size_t iconSeparator = caption.rfind(L'|');
            if (iconSeparator != std::wstring::npos) {
                icon = caption.substr(iconSeparator + 1);
                caption.erase(iconSeparator);
            }
            const size_t pathFirst = path.find_first_not_of(L" \t\r\n");
            const size_t pathLast = path.find_last_not_of(L" \t\r\n");
            const size_t captionFirst = caption.find_first_not_of(L" \t\r\n");
            const size_t captionLast = caption.find_last_not_of(L" \t\r\n");
            if (pathFirst != std::wstring::npos && captionFirst != std::wstring::npos) {
                path = path.substr(pathFirst, pathLast - pathFirst + 1);
                caption = caption.substr(captionFirst, captionLast - captionFirst + 1);
                UINT commandId = 0;
                bool hasCommandId = false;
                const size_t idSeparator = path.rfind(L'@');
                if (idSeparator != std::wstring::npos && idSeparator + 1 < path.size()) {
                    wchar_t* idEnd = nullptr;
                    const unsigned long parsedId =
                        std::wcstoul(path.c_str() + idSeparator + 1, &idEnd, 10);
                    if (idEnd && *idEnd == L'\0' && parsedId <= UINT_MAX) {
                        commandId = static_cast<UINT>(parsedId);
                        hasCommandId = commandId != 0 &&
                            commandId != static_cast<UINT>(-1);
                        path.erase(idSeparator);
                    }
                }
                if (caption == L"~H~") {
                    hiddenPaths.insert(path);
                    if (hasCommandId) hiddenIds.insert(commandId);
                } else {
                    captions[path] = caption;
                    if (hasCommandId) captionsById[commandId] = caption;
                }
                const size_t iconFirst = icon.find_first_not_of(L" \t\r\n");
                const size_t iconLast = icon.find_last_not_of(L" \t\r\n");
                if (iconFirst != std::wstring::npos) {
                    const std::wstring iconName =
                        icon.substr(iconFirst, iconLast - iconFirst + 1);
                    icons[path] = iconName;
                    if (hasCommandId) iconsById[commandId] = iconName;
                }
            }
        }
        start = end + 1;
    }

    // Clarion removes HIDE child items from the live HMENU instead of leaving
    // disabled placeholders. Rebase every later sibling around those omitted
    // entries so its generated declaration path matches the actual popup.
    std::unordered_map<std::wstring, std::wstring> visibleCaptions;
    std::unordered_map<std::wstring, std::wstring> visibleIcons;
    for (const auto& entry : captions)
        visibleCaptions[VisibleMenuPath(entry.first, hiddenPaths)] = entry.second;
    for (const auto& entry : icons)
        visibleIcons[VisibleMenuPath(entry.first, hiddenPaths)] = entry.second;
    captions.swap(visibleCaptions);
    icons.swap(visibleIcons);

    // These declaration paths describe controls Clarion has already removed
    // from the live HMENU. Retaining them would hide whichever visible sibling
    // was compressed into the same ordinal.
    hiddenPaths.clear();
    hiddenIds.clear();
}

bool IsThemedMenuItem(ULONG_PTR itemData)
{
    if (!itemData) return false;
    auto* item = reinterpret_cast<MenuItemData*>(itemData);
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_menuItems.find(item) != g_menuItems.end();
}

void MeasureMenuItem(MEASUREITEMSTRUCT& measure)
{
    auto* item = reinterpret_cast<MenuItemData*>(measure.itemData);
    if (item->hidden) {
        measure.itemWidth = 0;
        measure.itemHeight = 0;
        return;
    }
    ThemeData* theme = FindData(item->window);
    HDC dc = GetDC(item->window);
    HGDIOBJ oldFont = dc && theme && theme->font ? SelectObject(dc, theme->font) : nullptr;
    SIZE extent{};
    if (dc && !item->separator)
        GetTextExtentPoint32W(dc, item->text.c_str(), static_cast<int>(item->text.size()), &extent);
    if (oldFont) SelectObject(dc, oldFont);
    if (dc) ReleaseDC(item->window, dc);
    measure.itemHeight = item->separator ? 9 : (item->topLevel ? 30 : 28);
    BITMAP bitmapInfo{};
    const bool hasBitmap = item->icon || (item->bitmap &&
        GetObjectW(item->bitmap, sizeof(bitmapInfo), &bitmapInfo) == sizeof(bitmapInfo));
    const int bitmapSpace = hasBitmap
        ? (item->topLevel ? 22 : 0)
        : 0;
    measure.itemWidth = item->separator ? 12 :
        static_cast<UINT>(extent.cx + (item->topLevel ? 42 : 62) + bitmapSpace);
}

bool DrawMenuBitmap(HDC destination, HBITMAP bitmap, const RECT& itemRect,
    bool topLevel)
{
    if (!destination || !bitmap) return false;
    BITMAP info{};
    if (GetObjectW(bitmap, sizeof(info), &info) != sizeof(info) ||
        info.bmWidth <= 0 || info.bmHeight <= 0)
        return false;

    const int maximum = topLevel ? 16 : 16;
    const double scale = std::min(1.0,
        std::min(static_cast<double>(maximum) / info.bmWidth,
            static_cast<double>(maximum) / info.bmHeight));
    const int width = std::max(1, static_cast<int>(info.bmWidth * scale));
    const int height = std::max(1, static_cast<int>(info.bmHeight * scale));
    const int x = itemRect.left + (topLevel ? 5 : 7);
    const int y = itemRect.top + ((itemRect.bottom - itemRect.top) - height) / 2;

    HDC source = CreateCompatibleDC(destination);
    if (!source) return false;
    HGDIOBJ oldBitmap = SelectObject(source, bitmap);
    SetStretchBltMode(destination, HALFTONE);
    const BOOL drawn = StretchBlt(destination, x, y, width, height,
        source, 0, 0, info.bmWidth, info.bmHeight, SRCCOPY);
    SelectObject(source, oldBitmap);
    DeleteDC(source);
    return drawn != FALSE;
}

void DrawMenuItem(const DRAWITEMSTRUCT& draw)
{
    auto* item = reinterpret_cast<MenuItemData*>(draw.itemData);
    if (item->hidden) return;
    ThemeData* theme = FindData(item->window);
    if (!theme) return;
    const bool light = theme->mode == CHTHEME_LIGHT;
    const bool selected = (draw.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    const bool disabled = (draw.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    const COLORREF background = selected
        ? (light ? RGB(216, 232, 249) : RGB(24, 55, 82))
        : (item->topLevel ? theme->background : theme->menuSurface);
    const COLORREF foreground = disabled ? theme->disabledText : theme->text;
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(draw.hDC, &draw.rcItem, brush);
    DeleteObject(brush);

    if (item->separator) {
        const COLORREF lineColor = light ? RGB(210, 219, 228) : RGB(43, 60, 76);
        HPEN pen = CreatePen(PS_SOLID, 1, lineColor);
        HGDIOBJ oldPen = SelectObject(draw.hDC, pen);
        const int y = (draw.rcItem.top + draw.rcItem.bottom) / 2;
        MoveToEx(draw.hDC, draw.rcItem.left + 12, y, nullptr);
        LineTo(draw.hDC, draw.rcItem.right - 8, y);
        SelectObject(draw.hDC, oldPen);
        DeleteObject(pen);
        return;
    }

    HGDIOBJ oldFont = theme->font ? SelectObject(draw.hDC, theme->font) : nullptr;
    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, foreground);
    bool bitmapDrawn = false;
    if (item->icon) {
        const int x = draw.rcItem.left + (item->topLevel ? 5 : 7);
        const int y = draw.rcItem.top +
            ((draw.rcItem.bottom - draw.rcItem.top) - 16) / 2;
        bitmapDrawn = DrawIconEx(draw.hDC, x, y, item->icon, 16, 16, 0,
            nullptr, DI_NORMAL) != FALSE;
    } else {
        bitmapDrawn = DrawMenuBitmap(draw.hDC, item->bitmap,
            draw.rcItem, item->topLevel);
    }
    RECT textRect = draw.rcItem;
    // Clarion caches compact rectangles for its owner-drawn menu-bar items.
    // Even after Windows accepts our new measurements it can continue to
    // supply those original rectangles. Keep menu-bar insets deliberately
    // small so the complete caption fits; popup items retain their icon and
    // submenu gutters.
    textRect.left += item->topLevel
        ? (bitmapDrawn ? 27 : 5)
        : 30;
    textRect.right -= item->topLevel
        ? 3
        : (item->hasSubmenu ? 22 : 10);
    DrawTextW(draw.hDC, item->text.c_str(), -1, &textRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if ((draw.itemState & ODS_CHECKED) && !item->topLevel) {
        HPEN pen = CreatePen(PS_SOLID, 2, theme->accent);
        HGDIOBJ oldPen = SelectObject(draw.hDC, pen);
        const int centerY = (draw.rcItem.top + draw.rcItem.bottom) / 2;
        MoveToEx(draw.hDC, draw.rcItem.left + 8, centerY, nullptr);
        LineTo(draw.hDC, draw.rcItem.left + 12, centerY + 4);
        LineTo(draw.hDC, draw.rcItem.left + 20, centerY - 5);
        SelectObject(draw.hDC, oldPen);
        DeleteObject(pen);
    }
    if (item->hasSubmenu && !item->topLevel) {
        POINT arrow[3] = {
            { draw.rcItem.right - 13, (draw.rcItem.top + draw.rcItem.bottom) / 2 - 4 },
            { draw.rcItem.right - 8, (draw.rcItem.top + draw.rcItem.bottom) / 2 },
            { draw.rcItem.right - 13, (draw.rcItem.top + draw.rcItem.bottom) / 2 + 4 }
        };
        HBRUSH arrowBrush = CreateSolidBrush(foreground);
        HGDIOBJ oldBrush = SelectObject(draw.hDC, arrowBrush);
        HGDIOBJ oldPen = SelectObject(draw.hDC, GetStockObject(NULL_PEN));
        Polygon(draw.hDC, arrow, 3);
        SelectObject(draw.hDC, oldPen);
        SelectObject(draw.hDC, oldBrush);
        DeleteObject(arrowBrush);
    }
    if (oldFont) SelectObject(draw.hDC, oldFont);
}

struct ClarionColumn
{
    std::wstring caption;
    int width = 100;
    int pixelBasis = 100;
    int clarionBasis = 0;
    bool resizable = true;
};

struct ClarionListData
{
    HWND listWindow = nullptr;
    HWND themeWindow = nullptr;
    HWND overlay = nullptr;
    int height = 26;
    int hotColumn = -1;
    int pressedColumn = -1;
    int sortColumn = -1;
    int sortDirection = 0;
    int lastClick = 0;
    int startupTicks = 0;
    int resizingColumn = -1;
    int lastResizedColumn = 0;
    int controlPixelBasis = 0;
    int controlClarionBasis = 0;
    int dragStartX = 0;
    int dragStartWidth = 0;
    LONG_PTR originalStyle = 0;
    LONG_PTR originalExStyle = 0;
    bool frameFlattened = false;
    bool tracking = false;
    std::vector<ClarionColumn> columns;
};

std::unordered_set<ClarionListData*> g_clarionLists;

void ApplyClarionListTheme(ClarionListData& data, const ThemeData& theme)
{
    if (!IsWindow(data.listWindow)) return;
    SetWindowTheme(data.listWindow,
        theme.mode == CHTHEME_LIGHT ? L"Explorer" : L"DarkMode_Explorer",
        nullptr);
    RedrawWindow(data.listWindow, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
}

LRESULT CALLBACK ClarionHeaderWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void DrawSortArrow(HDC dc, const RECT& itemRect, bool ascending, COLORREF color);

ATOM EnsureClarionHeaderClass()
{
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ClarionHeaderWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClarionHeaderClass;
    atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) atom = 1;
    return atom;
}

void PositionClarionHeader(ClarionListData& data)
{
    if (!IsWindow(data.listWindow) || !IsWindow(data.overlay)) return;
    // The Clarion runtime hides LIST controls that belong to inactive
    // SHEET/WIZARD tabs.  The themed header is a sibling overlay, so it does
    // not inherit that visibility automatically.  Never let a repaint,
    // resize, or startup timer resurrect the overlay while its LIST (or one
    // of the LIST's ancestors) is hidden.
    const LONG_PTR listStyle = GetWindowLongPtrW(data.listWindow, GWL_STYLE);
    if ((listStyle & WS_VISIBLE) == 0 || !IsWindowVisible(data.listWindow)) {
        ShowWindow(data.overlay, SW_HIDE);
        return;
    }
    HWND parent = GetParent(data.listWindow);
    RECT rect{};
    GetWindowRect(data.listWindow, &rect);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<POINT*>(&rect), 2);
    const int overlayWidth = std::max(1L, rect.right - rect.left);
    // Clarion LIST controls paint the final column through the unused client
    // area. Match that behavior instead of leaving an unlabelled header band.
    // Earlier columns retain their FORMAT/user-resized widths; the last column
    // absorbs the remaining width and contracts again when the LIST narrows.
    if (!data.columns.empty()) {
        int precedingWidth = 0;
        for (size_t index = 0; index + 1 < data.columns.size(); ++index)
            precedingWidth += data.columns[index].width;
        data.columns.back().width = std::max(36, overlayWidth - precedingWidth);
    }
    SetWindowPos(data.overlay, HWND_TOP, rect.left, rect.top,
        overlayWidth, data.height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

int ClarionColumnAt(const ClarionListData& data, int x)
{
    int edge = 0;
    for (size_t index = 0; index < data.columns.size(); ++index) {
        edge += data.columns[index].width;
        if (x < edge) return static_cast<int>(index);
    }
    return -1;
}

int ClarionDividerAt(const ClarionListData& data, int x)
{
    int edge = 0;
    for (size_t index = 0; index + 1 < data.columns.size(); ++index) {
        edge += data.columns[index].width;
        if (data.columns[index].resizable && std::abs(x - edge) <= 5)
            return static_cast<int>(index);
    }
    return -1;
}

LPARAM OverlayPointForList(const ClarionListData& data, LPARAM overlayPoint)
{
    POINT point{ GET_X_LPARAM(overlayPoint), GET_Y_LPARAM(overlayPoint) };
    ClientToScreen(data.overlay, &point);
    ScreenToClient(data.listWindow, &point);
    return MAKELPARAM(point.x, point.y);
}

void PaintClarionHeader(HWND window, ClarionListData& data)
{
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    if (!dc) return;
    ThemeData* theme = FindData(data.themeWindow);
    RECT client{};
    GetClientRect(window, &client);
    if (!theme) { EndPaint(window, &paint); return; }

    const bool light = theme->mode == CHTHEME_LIGHT;
    const COLORREF base = light ? RGB(232, 239, 246) : RGB(17, 30, 43);
    const COLORREF hot = light ? RGB(214, 230, 246) : RGB(23, 52, 78);
    const COLORREF pressed = light ? RGB(196, 219, 242) : RGB(19, 68, 108);
    const COLORREF border = light ? RGB(190, 204, 218) : RGB(42, 61, 79);
    HBRUSH brush = CreateSolidBrush(base);
    FillRect(dc, &client, brush);
    DeleteObject(brush);

    HGDIOBJ oldFont = theme->font ? SelectObject(dc, theme->font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, IsWindowEnabled(data.listWindow) ? theme->text : theme->disabledText);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    int left = 0;
    for (size_t index = 0; index < data.columns.size(); ++index) {
        RECT item{ left, 0, left + data.columns[index].width, client.bottom };
        COLORREF fill = static_cast<int>(index) == data.pressedColumn ? pressed :
            (static_cast<int>(index) == data.hotColumn ? hot : base);
        HBRUSH itemBrush = CreateSolidBrush(fill);
        FillRect(dc, &item, itemBrush);
        DeleteObject(itemBrush);
        RECT textRect = item;
        textRect.left += 10;
        textRect.right -= static_cast<int>(index) == data.sortColumn ? 23 : 8;
        DrawTextW(dc, data.columns[index].caption.c_str(), -1, &textRect,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (static_cast<int>(index) == data.sortColumn && data.sortDirection)
            DrawSortArrow(dc, item, data.sortDirection > 0, theme->text);
        MoveToEx(dc, item.right - 1, item.top + 5, nullptr);
        LineTo(dc, item.right - 1, item.bottom - 5);
        left = item.right;
    }
    MoveToEx(dc, client.left, client.bottom - 1, nullptr);
    LineTo(dc, client.right, client.bottom - 1);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    if (oldFont) SelectObject(dc, oldFont);
    EndPaint(window, &paint);
}

struct EntryData
{
    HWND themeWindow = nullptr;
    bool hover = false;
};

void ApplyEntryRegion(HWND control)
{
    if (!IsWindow(control)) return;

    RECT window{};
    if (!GetWindowRect(control, &window)) return;
    const int width = window.right - window.left;
    const int height = window.bottom - window.top;
    if (width <= 0 || height <= 0) return;

    // Match the visible frame's 7-pixel corner diameter. SetWindowRgn takes
    // ownership of the HRGN after a successful call.
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, 7, 7);
    if (!region) return;
    if (!SetWindowRgn(control, region, TRUE)) {
        DeleteObject(region);
    }
}

void DrawEntryFrame(HWND control, const EntryData& entry)
{
    ThemeData* theme = FindData(entry.themeWindow);
    if (!theme || !IsWindow(control)) return;

    HDC dc = GetWindowDC(control);
    if (!dc) return;
    RECT windowRect{};
    GetWindowRect(control, &windowRect);
    OffsetRect(&windowRect, -windowRect.left, -windowRect.top);
    InflateRect(&windowRect, -1, -1);

    const bool light = theme->mode == CHTHEME_LIGHT;
    COLORREF border = light ? RGB(184, 198, 212) : RGB(47, 66, 85);
    if (!IsWindowEnabled(control)) border = light ? RGB(210, 218, 226) : RGB(31, 43, 55);
    else if (GetFocus() == control) border = theme->accent;
    else if (entry.hover) border = light ? RGB(109, 142, 175) : RGB(68, 92, 116);

    HPEN pen = CreatePen(PS_SOLID, GetFocus() == control ? 2 : 1, border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, windowRect.left, windowRect.top, windowRect.right, windowRect.bottom, 7, 7);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    ReleaseDC(control, dc);
}

LRESULT CALLBACK EntrySubclassProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR reference)
{
    auto* entry = reinterpret_cast<EntryData*>(reference);
    switch (message) {
    case WM_MOUSEMOVE:
        if (!entry->hover) {
            entry->hover = true;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, control, 0 };
            TrackMouseEvent(&track);
            RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        }
        break;
    case WM_MOUSELEAVE:
        entry->hover = false;
        RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
        DrawEntryFrame(control, *entry);
        return result;
    }
    case WM_SIZE: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        ApplyEntryRegion(control);
        RedrawWindow(control, nullptr, nullptr,
            RDW_INVALIDATE | RDW_FRAME | RDW_ERASE);
        return result;
    }
    case WM_NCPAINT: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        DrawEntryFrame(control, *entry);
        return result;
    }
    case WM_PAINT: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        DrawEntryFrame(control, *entry);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(control, EntrySubclassProc, kEntrySubclassId);
        delete entry;
        return DefSubclassProc(control, message, wParam, lParam);
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

LRESULT CALLBACK EntryParentSubclassProc(HWND parent, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    if (message == WM_CTLCOLOREDIT) {
        HWND control = reinterpret_cast<HWND>(lParam);
        DWORD_PTR entryReference = 0;
        if (IsWindow(control) &&
            GetWindowSubclass(control, EntrySubclassProc, kEntrySubclassId, &entryReference)) {
            if (ThemeData* theme = FindAncestorTheme(parent)) {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, theme->input);
                SetTextColor(dc, IsWindowEnabled(control)
                    ? theme->text : theme->disabledText);
                return reinterpret_cast<LRESULT>(theme->inputBrush);
            }
        }
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(parent, EntryParentSubclassProc, kEntryParentSubclassId);
    }
    return DefSubclassProc(parent, message, wParam, lParam);
}

void AttachEntryFrame(HWND control, HWND themeWindow)
{
    DWORD_PTR existing = 0;
    if (GetWindowSubclass(control, EntrySubclassProc, kEntrySubclassId, &existing)) return;
    auto* entry = new (std::nothrow) EntryData();
    if (!entry) return;
    entry->themeWindow = themeWindow;
    if (!SetWindowSubclass(control, EntrySubclassProc, kEntrySubclassId,
        reinterpret_cast<DWORD_PTR>(entry))) {
        delete entry;
        return;
    }
    // WM_CTLCOLOREDIT is sent to the immediate parent. Clarion can place ENTRY
    // controls inside an intermediate host HWND rather than directly on the
    // themed window, so make that parent relay the themed input brush.
    HWND parent = GetParent(control);
    if (IsWindow(parent)) {
        SetWindowSubclass(parent, EntryParentSubclassProc, kEntryParentSubclassId, 0);
    }
    ApplyEntryRegion(control);
    DrawEntryFrame(control, *entry);
}

LRESULT CALLBACK ComboParentSubclassProc(HWND parent, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX) {
        if (ThemeData* theme = FindAncestorTheme(parent)) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, theme->input);
            SetTextColor(dc, IsWindowEnabled(control)
                ? theme->text : theme->disabledText);
            return reinterpret_cast<LRESULT>(theme->inputBrush);
        }
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(parent, ComboParentSubclassProc, kComboParentSubclassId);
    }
    return DefSubclassProc(parent, message, wParam, lParam);
}

void AttachComboTheme(HWND control, ThemeData& theme)
{
    // Explorer does not consistently theme the closed field and arrow of a
    // Win32/Clarion dropdown. CFD is the native dark/light combo theme class.
    SetWindowTheme(control,
        theme.mode == CHTHEME_LIGHT ? L"Explorer" : L"DarkMode_CFD", nullptr);

    // Combo edit/list popup colour messages are delivered to the immediate
    // parent. Clarion may insert a host HWND between the combo and window.
    HWND parent = GetParent(control);
    if (IsWindow(parent)) {
        SetWindowSubclass(parent, ComboParentSubclassProc,
            kComboParentSubclassId, 0);
    }
    RedrawWindow(control, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

struct ClarionDropData
{
    HWND themeWindow = nullptr;
    HWND arrowOverlay = nullptr;
};

void PositionClarionDropOverlay(HWND control, ClarionDropData& drop)
{
    if (!IsWindow(drop.arrowOverlay)) return;
    RECT client{};
    GetClientRect(control, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const UINT dpi = GetDpiForWindow(control);
    const int arrowWidth = std::max(MulDiv(18, dpi ? dpi : 96, 96), height);
    SetWindowPos(drop.arrowOverlay, HWND_TOP,
        std::max(1, width - arrowWidth), 1,
        std::min(arrowWidth - 1, width - 2), std::max(1, height - 2),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void PaintClarionDropArrow(HWND overlay, const ClarionDropData& drop)
{
    ThemeData* theme = FindData(drop.themeWindow);
    HWND control = GetParent(overlay);
    if (!theme || !IsWindow(control)) return;

    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(overlay, &paint);
    if (!dc) return;
    RECT client{};
    GetClientRect(overlay, &client);

    // Sample the closed-field surface immediately beside the overlay so the
    // arrow follows Clarion's normal/selected field state.
    COLORREF fill = theme->input;
    HDC ownerDc = GetDC(control);
    if (ownerDc) {
        RECT overlayRect{};
        GetWindowRect(overlay, &overlayRect);
        MapWindowPoints(nullptr, control,
            reinterpret_cast<POINT*>(&overlayRect), 2);
        const int sampleX = std::max(2, static_cast<int>(overlayRect.left) - 2);
        const int sampleY = std::max(1,
            static_cast<int>((overlayRect.top + overlayRect.bottom) / 2));
        const COLORREF sampled = GetPixel(ownerDc, sampleX, sampleY);
        if (sampled != CLR_INVALID) fill = sampled;
        ReleaseDC(control, ownerDc);
    }
    HBRUSH fillBrush = CreateSolidBrush(fill);
    FillRect(dc, &client, fillBrush);
    DeleteObject(fillBrush);

    const bool enabled = IsWindowEnabled(control) != FALSE;
    const COLORREF arrowColor = enabled ? theme->text : theme->disabledText;
    const UINT dpi = GetDpiForWindow(control);
    const int centerX = (client.left + client.right) / 2;
    const int centerY = (client.top + client.bottom) / 2;
    const int half = std::max(3, MulDiv(4, dpi ? dpi : 96, 96));
    HPEN pen = CreatePen(PS_SOLID,
        std::max(1, MulDiv(1, dpi ? dpi : 96, 96)), arrowColor);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, centerX - half, centerY - 2, nullptr);
    LineTo(dc, centerX, centerY + 2);
    LineTo(dc, centerX + half, centerY - 2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    EndPaint(overlay, &paint);
}

LRESULT CALLBACK ClarionDropOverlaySubclassProc(HWND overlay, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    auto* drop = reinterpret_cast<ClarionDropData*>(reference);
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        PaintClarionDropArrow(overlay, *drop);
        return 0;
    case WM_NCDESTROY:
        RemoveWindowSubclass(overlay, ClarionDropOverlaySubclassProc,
            kClarionDropSubclassId);
        if (drop && drop->arrowOverlay == overlay)
            drop->arrowOverlay = nullptr;
        break;
    }
    return DefSubclassProc(overlay, message, wParam, lParam);
}

void PaintClarionDropChrome(HWND control, const ClarionDropData& drop)
{
    ThemeData* theme = FindData(drop.themeWindow);
    if (!theme) return;

    HDC dc = GetDC(control);
    if (!dc) return;
    RECT client{};
    GetClientRect(control, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width > 4 && height > 4) {
        const UINT dpi = GetDpiForWindow(control);
        const int arrowWidth = std::max(MulDiv(18, dpi ? dpi : 96, 96), height);
        RECT arrowArea{
            std::max(client.left + 1, client.right - arrowWidth),
            client.top + 1, client.right - 1, client.bottom - 1
        };

        // Match Clarion's selected/normal closed-field fill so only the
        // obsolete native arrow button is replaced.
        const int sampleX = std::max(client.left + 2, arrowArea.left - 2);
        const int sampleY = client.top + height / 2;
        COLORREF fill = GetPixel(dc, sampleX, sampleY);
        if (fill == CLR_INVALID) fill = theme->input;
        HBRUSH fillBrush = CreateSolidBrush(fill);
        FillRect(dc, &arrowArea, fillBrush);
        DeleteObject(fillBrush);

        const bool enabled = IsWindowEnabled(control) != FALSE;
        const bool focused = GetFocus() == control;
        const COLORREF frame = focused ? theme->accent :
            (enabled ? RGB(70, 91, 111) : RGB(54, 67, 80));
        HBRUSH frameBrush = CreateSolidBrush(frame);
        FrameRect(dc, &client, frameBrush);
        DeleteObject(frameBrush);

        const COLORREF arrowColor = enabled ? theme->text : theme->disabledText;
        const int centerX = (arrowArea.left + arrowArea.right) / 2;
        const int centerY = (arrowArea.top + arrowArea.bottom) / 2;
        const int half = std::max(3, MulDiv(4, dpi ? dpi : 96, 96));
        HPEN pen = CreatePen(PS_SOLID,
            std::max(1, MulDiv(1, dpi ? dpi : 96, 96)), arrowColor);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, centerX - half, centerY - 2, nullptr);
        LineTo(dc, centerX, centerY + 2);
        LineTo(dc, centerX + half, centerY - 2);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
    ReleaseDC(control, dc);
}

LRESULT CALLBACK ClarionDropSubclassProc(HWND control, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    auto* drop = reinterpret_cast<ClarionDropData*>(reference);
    switch (message) {
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        PaintClarionDropChrome(control, *drop);
        return result;
    }
    case WM_TIMER:
        if (wParam == kClarionDropRepaintTimerId) {
            KillTimer(control, kClarionDropRepaintTimerId);
            PaintClarionDropChrome(control, *drop);
            PositionClarionDropOverlay(control, *drop);
            RedrawWindow(drop->arrowOverlay, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        PositionClarionDropOverlay(control, *drop);
        RedrawWindow(drop->arrowOverlay, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        return result;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
    case WM_THEMECHANGED:
        InvalidateRect(control, nullptr, FALSE);
        SetTimer(control, kClarionDropRepaintTimerId, 10, nullptr);
        break;
    case WM_SHOWWINDOW:
    case WM_SIZE:
    case WM_WINDOWPOSCHANGED:
        // ClaDrop paints its native arrow after the ordinary child paint
        // during startup, restore and layout changes.  Reapply our chrome
        // once that deferred Clarion paint has completed.
        SetTimer(control, kClarionDropRepaintTimerId, 10, nullptr);
        PositionClarionDropOverlay(control, *drop);
        break;
    case WM_NCDESTROY:
        KillTimer(control, kClarionDropRepaintTimerId);
        if (IsWindow(drop->arrowOverlay))
            DestroyWindow(drop->arrowOverlay);
        RemoveWindowSubclass(control, ClarionDropSubclassProc,
            kClarionDropSubclassId);
        delete drop;
        return DefSubclassProc(control, message, wParam, lParam);
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

void AttachClarionDropChrome(HWND control, HWND themeWindow)
{
    DWORD_PTR existing = 0;
    if (GetWindowSubclass(control, ClarionDropSubclassProc,
        kClarionDropSubclassId, &existing)) {
        auto* drop = reinterpret_cast<ClarionDropData*>(existing);
        if (drop) drop->themeWindow = themeWindow;
        InvalidateRect(control, nullptr, FALSE);
        SetTimer(control, kClarionDropRepaintTimerId, 10, nullptr);
        return;
    }
    auto* drop = new (std::nothrow) ClarionDropData{};
    if (!drop) return;
    drop->themeWindow = themeWindow;
    if (!SetWindowSubclass(control, ClarionDropSubclassProc,
        kClarionDropSubclassId, reinterpret_cast<DWORD_PTR>(drop))) {
        delete drop;
        return;
    }
    drop->arrowOverlay = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, control, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (IsWindow(drop->arrowOverlay)) {
        SetWindowSubclass(drop->arrowOverlay, ClarionDropOverlaySubclassProc,
            kClarionDropSubclassId, reinterpret_cast<DWORD_PTR>(drop));
        PositionClarionDropOverlay(control, *drop);
        RedrawWindow(drop->arrowOverlay, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
    InvalidateRect(control, nullptr, FALSE);
    PaintClarionDropChrome(control, *drop);
    SetTimer(control, kClarionDropRepaintTimerId, 10, nullptr);
}

struct HeaderData
{
    HWND themeWindow = nullptr;
    int hotItem = -1;
    int pressedItem = -1;
    bool tracking = false;
};

HWND FindHeaderWindow(HWND listWindow)
{
    if (!IsWindow(listWindow)) return nullptr;

    wchar_t className[64]{};
    GetClassNameW(listWindow, className, static_cast<int>(std::size(className)));
    if (lstrcmpiW(className, WC_HEADERW) == 0) return listWindow;

    HWND header = reinterpret_cast<HWND>(SendMessage(listWindow, LVM_GETHEADER, 0, 0));
    if (IsWindow(header)) return header;

    header = FindWindowExW(listWindow, nullptr, WC_HEADERW, nullptr);
    if (IsWindow(header)) return header;

    HWND parent = GetParent(listWindow);
    for (HWND sibling = parent ? GetWindow(parent, GW_CHILD) : nullptr;
         sibling; sibling = GetWindow(sibling, GW_HWNDNEXT)) {
        GetClassNameW(sibling, className, static_cast<int>(std::size(className)));
        if (lstrcmpiW(className, WC_HEADERW) != 0) continue;
        RECT listRect{}, headerRect{};
        GetWindowRect(listWindow, &listRect);
        GetWindowRect(sibling, &headerRect);
        const bool overlaps = headerRect.right > listRect.left && headerRect.left < listRect.right;
        const bool nearTop = headerRect.top >= listRect.top - 8 &&
            headerRect.top <= listRect.top + 48;
        if (overlaps && nearTop) return sibling;
    }
    return nullptr;
}

int HeaderItemAt(HWND header, LPARAM position)
{
    HDHITTESTINFO hit{};
    hit.pt.x = GET_X_LPARAM(position);
    hit.pt.y = GET_Y_LPARAM(position);
    return static_cast<int>(SendMessage(header, HDM_HITTEST, 0,
        reinterpret_cast<LPARAM>(&hit)));
}

void DrawSortArrow(HDC dc, const RECT& itemRect, bool ascending, COLORREF color)
{
    const int x = itemRect.right - 13;
    const int y = (itemRect.top + itemRect.bottom) / 2;
    POINT points[3]{};
    if (ascending) {
        points[0] = { x - 4, y + 2 }; points[1] = { x + 4, y + 2 }; points[2] = { x, y - 3 };
    } else {
        points[0] = { x - 4, y - 2 }; points[1] = { x + 4, y - 2 }; points[2] = { x, y + 3 };
    }
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, points, 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

void PaintHeader(HWND header, HeaderData& state)
{
    ThemeData* theme = FindData(state.themeWindow);
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(header, &paint);
    if (!dc) return;

    RECT client{};
    GetClientRect(header, &client);
    if (!theme) {
        EndPaint(header, &paint);
        return;
    }

    const bool light = theme->mode == CHTHEME_LIGHT;
    const COLORREF base = light ? RGB(232, 239, 246) : RGB(17, 30, 43);
    const COLORREF hot = light ? RGB(214, 230, 246) : RGB(23, 52, 78);
    const COLORREF pressed = light ? RGB(196, 219, 242) : RGB(19, 68, 108);
    const COLORREF border = light ? RGB(190, 204, 218) : RGB(42, 61, 79);
    const COLORREF text = IsWindowEnabled(header) ? theme->text : theme->disabledText;

    HBRUSH baseBrush = CreateSolidBrush(base);
    FillRect(dc, &client, baseBrush);
    DeleteObject(baseBrush);

    HFONT font = reinterpret_cast<HFONT>(SendMessage(header, WM_GETFONT, 0, 0));
    if (!font) font = theme->font;
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);

    const int count = Header_GetItemCount(header);
    HPEN separator = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dc, separator);
    for (int index = 0; index < count; ++index) {
        RECT item{};
        if (!Header_GetItemRect(header, index, &item)) continue;
        COLORREF fill = index == state.pressedItem ? pressed :
            (index == state.hotItem ? hot : base);
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(dc, &item, brush);
        DeleteObject(brush);

        wchar_t caption[256]{};
        HDITEMW hdItem{};
        hdItem.mask = HDI_TEXT | HDI_FORMAT;
        hdItem.pszText = caption;
        hdItem.cchTextMax = static_cast<int>(std::size(caption));
        Header_GetItem(header, index, &hdItem);

        RECT textRect = item;
        textRect.left += 10;
        textRect.right -= (hdItem.fmt & (HDF_SORTUP | HDF_SORTDOWN)) ? 22 : 8;
        UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
        if (hdItem.fmt & HDF_CENTER) flags |= DT_CENTER;
        else if (hdItem.fmt & HDF_RIGHT) flags |= DT_RIGHT;
        else flags |= DT_LEFT;
        DrawTextW(dc, caption, -1, &textRect, flags);
        if (hdItem.fmt & HDF_SORTUP) DrawSortArrow(dc, item, true, text);
        else if (hdItem.fmt & HDF_SORTDOWN) DrawSortArrow(dc, item, false, text);

        MoveToEx(dc, item.right - 1, item.top + 5, nullptr);
        LineTo(dc, item.right - 1, item.bottom - 5);
    }
    MoveToEx(dc, client.left, client.bottom - 1, nullptr);
    LineTo(dc, client.right, client.bottom - 1);
    SelectObject(dc, oldPen);
    DeleteObject(separator);
    if (oldFont) SelectObject(dc, oldFont);
    EndPaint(header, &paint);
}

LRESULT CALLBACK HeaderSubclassProc(HWND header, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR reference)
{
    auto* state = reinterpret_cast<HeaderData*>(reference);
    switch (message) {
    case WM_MOUSEMOVE: {
        const int hot = HeaderItemAt(header, lParam);
        if (hot != state->hotItem) {
            state->hotItem = hot;
            InvalidateRect(header, nullptr, FALSE);
        }
        if (!state->tracking) {
            state->tracking = true;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, header, 0 };
            TrackMouseEvent(&track);
        }
        break;
    }
    case WM_MOUSELEAVE:
        state->tracking = false;
        state->hotItem = -1;
        InvalidateRect(header, nullptr, FALSE);
        break;
    case WM_LBUTTONDOWN:
        state->pressedItem = HeaderItemAt(header, lParam);
        InvalidateRect(header, nullptr, FALSE);
        break;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        state->pressedItem = -1;
        InvalidateRect(header, nullptr, FALSE);
        break;
    case WM_PAINT:
        PaintHeader(header, *state);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_ENABLE:
    case WM_THEMECHANGED:
    case WM_SETFONT: {
        const LRESULT result = DefSubclassProc(header, message, wParam, lParam);
        InvalidateRect(header, nullptr, FALSE);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(header, HeaderSubclassProc, kHeaderSubclassId);
        delete state;
        return DefSubclassProc(header, message, wParam, lParam);
    }
    return DefSubclassProc(header, message, wParam, lParam);
}

BOOL AttachListHeaderInternal(HWND listWindow, HWND themeWindow)
{
    HWND header = FindHeaderWindow(listWindow);
    if (!header || !FindData(themeWindow)) return FALSE;
    DWORD_PTR existing = 0;
    if (GetWindowSubclass(header, HeaderSubclassProc, kHeaderSubclassId, &existing)) return TRUE;
    auto* state = new (std::nothrow) HeaderData();
    if (!state) return FALSE;
    state->themeWindow = themeWindow;
    SetWindowTheme(header, L"", L"");
    if (!SetWindowSubclass(header, HeaderSubclassProc, kHeaderSubclassId,
        reinterpret_cast<DWORD_PTR>(state))) {
        delete state;
        return FALSE;
    }
    InvalidateRect(header, nullptr, TRUE);
    return TRUE;
}

LRESULT CALLBACK ClarionHeaderWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<ClarionListData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        data = reinterpret_cast<ClarionListData*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_PAINT:
        PaintClarionHeader(window, *data);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wParam == 1) {
            PositionClarionHeader(*data);
            RedrawWindow(window, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
            if (++data->startupTicks >= 12) KillTimer(window, 1);
            return 0;
        }
        break;
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        if (data->resizingColumn >= 0) {
            const int width = std::max(36, data->dragStartWidth + x - data->dragStartX);
            data->columns[static_cast<size_t>(data->resizingColumn)].width = width;
            SendMessage(data->listWindow, WM_MOUSEMOVE, MK_LBUTTON,
                OverlayPointForList(*data, lParam));
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        SetCursor(LoadCursorW(nullptr,
            ClarionDividerAt(*data, x) >= 0 ? IDC_SIZEWE : IDC_ARROW));
        const int hot = ClarionColumnAt(*data, GET_X_LPARAM(lParam));
        if (hot != data->hotColumn) {
            data->hotColumn = hot;
            InvalidateRect(window, nullptr, FALSE);
        }
        if (!data->tracking) {
            data->tracking = true;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, window, 0 };
            TrackMouseEvent(&track);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        data->tracking = false;
        data->hotColumn = -1;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        data->resizingColumn = ClarionDividerAt(*data, GET_X_LPARAM(lParam));
        if (data->resizingColumn >= 0) {
            data->dragStartX = GET_X_LPARAM(lParam);
            data->dragStartWidth = data->columns[static_cast<size_t>(data->resizingColumn)].width;
            SetCapture(window);
            SendMessage(data->listWindow, WM_LBUTTONDOWN, wParam,
                OverlayPointForList(*data, lParam));
            return 0;
        }
        data->pressedColumn = ClarionColumnAt(*data, GET_X_LPARAM(lParam));
        SetCapture(window);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        if (data->resizingColumn >= 0) {
            SendMessage(data->listWindow, WM_LBUTTONUP, wParam,
                OverlayPointForList(*data, lParam));
            data->lastResizedColumn = data->resizingColumn + 1;
            data->resizingColumn = -1;
            if (GetCapture() == window) ReleaseCapture();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        const int released = ClarionColumnAt(*data, GET_X_LPARAM(lParam));
        if (released >= 0 && released == data->pressedColumn) {
            data->lastClick = released + 1;
            ThemeData* theme = FindData(data->themeWindow);
            if (theme && IsWindow(theme->notifyButton)) {
                HWND owner = GetParent(theme->notifyButton);
                if (IsWindow(owner)) {
                    PostMessageW(owner, WM_COMMAND,
                        MAKEWPARAM(static_cast<WORD>(
                            GetDlgCtrlID(theme->notifyButton)), BN_CLICKED),
                        reinterpret_cast<LPARAM>(theme->notifyButton));
                }
            }
        }
        data->pressedColumn = -1;
        if (GetCapture() == window) ReleaseCapture();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_CAPTURECHANGED:
        data->resizingColumn = -1;
        data->pressedColumn = -1;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK ClarionListSubclassProc(HWND listWindow, UINT message, WPARAM wParam,
    LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    auto* data = reinterpret_cast<ClarionListData*>(reference);
    switch (message) {
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        // Clarion repaints LIST controls during DISPLAY and window layout.
        // Let it finish, then restore the sibling overlay in the same message.
        const LRESULT result = DefSubclassProc(listWindow, message, wParam, lParam);
        PositionClarionHeader(*data);
        if (IsWindow(data->overlay))
            RedrawWindow(data->overlay, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
        return result;
    }
    case WM_NCPAINT: {
        const LRESULT result = DefSubclassProc(listWindow, message, wParam, lParam);
        PositionClarionHeader(*data);
        if (IsWindow(data->overlay)) InvalidateRect(data->overlay, nullptr, FALSE);
        return result;
    }
    case WM_WINDOWPOSCHANGED:
    case WM_SIZE:
    case WM_SHOWWINDOW: {
        const LRESULT result = DefSubclassProc(listWindow, message, wParam, lParam);
        PositionClarionHeader(*data);
        return result;
    }
    case WM_ENABLE: {
        const LRESULT result = DefSubclassProc(listWindow, message, wParam, lParam);
        if (IsWindow(data->overlay)) InvalidateRect(data->overlay, nullptr, FALSE);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(listWindow, ClarionListSubclassProc, kClarionListSubclassId);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_clarionLists.erase(data);
        }
        if (IsWindow(data->overlay)) DestroyWindow(data->overlay);
        delete data;
        return DefSubclassProc(listWindow, message, wParam, lParam);
    }
    return DefSubclassProc(listWindow, message, wParam, lParam);
}

ClarionListData* FindClarionListData(HWND listWindow)
{
    DWORD_PTR reference = 0;
    if (!GetWindowSubclass(listWindow, ClarionListSubclassProc,
        kClarionListSubclassId, &reference)) return nullptr;
    return reinterpret_cast<ClarionListData*>(reference);
}

struct ControlContext
{
    ThemeData* data;
    HWND themeWindow;
    int count;
};

BOOL CALLBACK ApplyControlTheme(HWND control, LPARAM parameter)
{
    auto* context = reinterpret_cast<ControlContext*>(parameter);

    wchar_t className[96]{};
    GetClassNameW(control, className, static_cast<int>(std::size(className)));
    CharLowerBuffW(className, lstrlenW(className));
    const bool editLike = wcsstr(className, L"edit") || wcsstr(className, L"entry") ||
        wcsstr(className, L"richedit");
    const bool comboLike = wcsstr(className, L"combo") != nullptr;
    const bool listLike = wcsstr(className, L"list") != nullptr;
    const bool treeLike = wcsstr(className, L"tree") != nullptr;
    const bool labelLike = wcsstr(className, L"static") || wcsstr(className, L"string") ||
        wcsstr(className, L"prompt") || wcsstr(className, L"label");

    if (editLike || listLike || treeLike) {
        SetWindowTheme(control,
            context->data->mode == CHTHEME_LIGHT ? L"Explorer" : L"DarkMode_Explorer", nullptr);
    }
    if (comboLike) {
        AttachComboTheme(control, *context->data);
    }
    if (editLike) {
        SendMessage(control, EM_SETBKGNDCOLOR, 0, context->data->input);
        const LONG_PTR exStyle = GetWindowLongPtr(control, GWL_EXSTYLE);
        SetWindowLongPtr(control, GWL_EXSTYLE,
            exStyle & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        SetWindowPos(control, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        AttachEntryFrame(control, GetAncestor(control, GA_ROOT));
    }
    if (lstrcmpiW(className, WC_LISTVIEWW) == 0 || wcsstr(className, L"listview")) {
        ListView_SetBkColor(control, context->data->input);
        ListView_SetTextBkColor(control, context->data->input);
        ListView_SetTextColor(control, context->data->text);
        AttachListHeaderInternal(control, context->themeWindow);
    }
    if (lstrcmpiW(className, WC_TREEVIEWW) == 0 || treeLike) {
        TreeView_SetBkColor(control, context->data->input);
        TreeView_SetTextColor(control, context->data->text);
    }
    if (labelLike) {
        SetWindowTheme(control, L"", L"");
    }
    InvalidateRect(control, nullptr, TRUE);
    ++context->count;
    return TRUE;
}

void RefreshWindow(HWND window, ThemeData& data)
{
    ApplyTitleBarMode(window, data.mode != CHTHEME_LIGHT);
    ControlContext context{ &data, window, 0 };
    EnumChildWindows(window, ApplyControlTheme, reinterpret_cast<LPARAM>(&context));
    std::vector<ClarionListData*> themedLists;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto* list : g_clarionLists)
            if (list && list->themeWindow == window) themedLists.push_back(list);
    }
    for (auto* list : themedLists) {
        ApplyClarionListTheme(*list, data);
        PositionClarionHeader(*list);
        if (IsWindow(list->overlay)) {
            // Clarion applications commonly call DISPLAY immediately after
            // CHTheme_SetMode. Reassert/repaint after that deferred LIST paint.
            list->startupTicks = 0;
            SetTimer(list->overlay, 1, 50, nullptr);
            RedrawWindow(list->overlay, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
        }
    }
    RedrawWindow(window, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
        RDW_FRAME | RDW_UPDATENOW);
}

void PaintWindowBackground(HWND window, ThemeData& data)
{
    HDC dc = GetDCEx(window, nullptr,
        DCX_CACHE | DCX_CLIPCHILDREN | DCX_CLIPSIBLINGS);
    if (!dc) return;
    RECT client{};
    GetClientRect(window, &client);
    FillRect(dc, &client, data.backgroundBrush);
    ReleaseDC(window, dc);
}

void DrawMenuBottomBorder(HWND window, const ThemeData& data)
{
    if (!data.menuAttached || !GetMenu(window)) return;

    RECT windowRect{};
    RECT clientRect{};
    POINT clientOrigin{};
    if (!GetWindowRect(window, &windowRect) || !GetClientRect(window, &clientRect) ||
        !ClientToScreen(window, &clientOrigin)) return;

    const int left = clientOrigin.x - windowRect.left;
    const int top = clientOrigin.y - windowRect.top;
    if (top <= 0 || clientRect.right <= 0) return;

    HDC dc = GetWindowDC(window);
    if (!dc) return;
    const COLORREF border = data.mode == CHTHEME_LIGHT
        ? RGB(183, 198, 214) : RGB(43, 65, 85);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, left, top - 1, nullptr);
    LineTo(dc, left + clientRect.right, top - 1);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    ReleaseDC(window, dc);
}

LRESULT CALLBACK MdiClientSubclassProc(HWND window, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    HWND frame = reinterpret_cast<HWND>(reference);
    switch (message) {
    case WM_MDISETMENU: {
        // Clarion supplies the merged frame menu in wParam. The lParam menu
        // is the optional MDI "Window" submenu to which USER32 appends the
        // automatic numbered child list. CompuHost-style full-screen MDI
        // children use the merged menu but not that synthetic child list.
        const LRESULT result = DefSubclassProc(window, message, wParam, 0);
        if (IsWindow(frame))
            PostMessageW(frame, kRefreshMergedMenuMessage, 0, 0);
        return result;
    }
    case WM_MDIACTIVATE:
    case WM_MDIMAXIMIZE:
    case WM_MDIRESTORE:
    case WM_MDICREATE:
    case WM_MDIDESTROY: {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        if (IsWindow(frame))
            PostMessageW(frame, kRefreshMergedMenuMessage, 0, 0);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(window, MdiClientSubclassProc, kMdiClientSubclassId);
        if (ThemeData* data = FindData(frame)) data->mdiClient = nullptr;
        break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

BOOL CALLBACK FindMdiClientProc(HWND child, LPARAM parameter)
{
    wchar_t className[64]{};
    GetClassNameW(child, className, static_cast<int>(std::size(className)));
    if (lstrcmpiW(className, L"MDIClient") != 0) return TRUE;
    *reinterpret_cast<HWND*>(parameter) = child;
    return FALSE;
}

void AttachMdiClient(HWND frame, ThemeData& data)
{
    HWND mdiClient = nullptr;
    EnumChildWindows(frame, FindMdiClientProc,
        reinterpret_cast<LPARAM>(&mdiClient));
    if (!mdiClient) return;
    data.mdiClient = mdiClient;
    SetWindowSubclass(mdiClient, MdiClientSubclassProc,
        kMdiClientSubclassId, reinterpret_cast<DWORD_PTR>(frame));
}

LRESULT CALLBACK WindowSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR reference)
{
    auto* data = reinterpret_cast<ThemeData*>(reference);
    switch (message) {
    case WM_WINDOWPOSCHANGING: {
        auto* position = reinterpret_cast<WINDOWPOS*>(lParam);
        if (position && (position->flags & SWP_SHOWWINDOW)) {
            bool startupHidden = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                const auto found = g_updateStates.find(window);
                startupHidden =
                    found != g_updateStates.end() &&
                    found->second.restoreVisible &&
                    (found->second.depth > 0 ||
                        found->second.pendingReveal);
            }
            if (startupHidden) {
                // Clarion can issue SHOW/SWP_SHOWWINDOW requests during later
                // application initialization. Keep the window hidden until
                // the theme DLL's one final startup reveal.
                position->flags &= ~SWP_SHOWWINDOW;
                position->flags |= SWP_NOACTIVATE;
            }
        }
        break;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure && measure->CtlType == ODT_MENU && IsThemedMenuItem(measure->itemData)) {
            MeasureMenuItem(*measure);
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlType == ODT_MENU && IsThemedMenuItem(draw->itemData)) {
            DrawMenuItem(*draw);
            return TRUE;
        }
        break;
    }
    case WM_INITMENU:
    case WM_INITMENUPOPUP:
        // Clarion may rebuild menu entries at runtime. Reapply the owner-draw
        // metadata before Windows measures or paints the opened menu.
        if (GetMenu(window))
            RefreshMergedMenu(window, *data, true);
        if (data->menuAttached) {
            HMENU menu = GetMenu(window);
            ApplyMenuBackground(menu, data->menuBrush);
            DrawMenuBar(window);
        }
        break;
    case kRefreshMergedMenuMessage:
        RefreshMergedMenu(window, *data, true);
        // Refreshing a merged menu only changes the non-client menu bar.
        // Erasing the whole MDI frame here causes visible flicker and also
        // disturbs owner-drawn controls in the maximized child.
        DrawMenuBar(window);
        return 0;
    case kApplyApplicationThemeMessage:
        CHTheme_SetMode(window, static_cast<int>(wParam));
        if (IsWindow(data->notifyButton)) {
            // Do not simulate BM_CLICK: Windows can suppress/defer it for a
            // hidden button in an inactive threaded MDI window. Deliver the
            // BN_CLICKED command directly to the Clarion window instead.
            HWND owner = GetParent(data->notifyButton);
            const int controlId = GetDlgCtrlID(data->notifyButton);
            if (IsWindow(owner)) {
                PostMessageW(owner, WM_COMMAND,
                    MAKEWPARAM(static_cast<WORD>(controlId), BN_CLICKED),
                    reinterpret_cast<LPARAM>(data->notifyButton));
            }
        }
        return 0;
    case kForceApplicationRedrawMessage:
        RedrawWindow(window, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
            RDW_FRAME | RDW_UPDATENOW);
        return 0;
    case kEnsureWindowVisibleMessage:
        EnsureWindowVisible(window);
        return 0;
    case WM_TIMER:
        if (wParam == kStartupRevealTimerId) {
            KillTimer(window, kStartupRevealTimerId);
            bool restoreVisible = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                const auto found = g_updateStates.find(window);
                if (found != g_updateStates.end() &&
                    found->second.depth == 0 &&
                    found->second.pendingReveal) {
                    restoreVisible = found->second.restoreVisible;
                    g_updateStates.erase(found);
                }
            }
            if (!restoreVisible) return 0;

            DrawMenuBar(window);
            RedrawWindow(window, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
                RDW_FRAME | RDW_UPDATENOW);
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
            RedrawWindow(window, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
                RDW_FRAME | RDW_UPDATENOW);
            return 0;
        }
        if (wParam == kDeferredThemeRedrawTimerId) {
            KillTimer(window, kDeferredThemeRedrawTimerId);
            RedrawWindow(window, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
                RDW_FRAME | RDW_UPDATENOW);
            PaintWindowBackground(window, *data);
            return 0;
        }
        break;
    case WM_NCPAINT:
    case WM_NCACTIVATE: {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        DrawMenuBottomBorder(window, *data);
        return result;
    }
    case WM_PAINT: {
        // Clarion can fill the client area from its WM_PAINT handler after
        // WM_ERASEBKGND. Paint the final themed surface, clipped around every
        // child HWND, so controls and custom child renderers remain untouched.
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        PaintWindowBackground(window, *data);
        RestoreSliderChildren(window);
        return result;
    }
    case WM_PRINTCLIENT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, data->backgroundBrush);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, data->backgroundBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, IsWindowEnabled(control) ? data->text : data->disabledText);
        return reinterpret_cast<LRESULT>(data->backgroundBrush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, data->input);
        SetTextColor(dc, IsWindowEnabled(control) ? data->text : data->disabledText);
        return reinterpret_cast<LRESULT>(data->inputBrush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, data->text);
        return reinterpret_cast<LRESULT>(data->backgroundBrush);
    }
    case WM_THEMECHANGED:
    case WM_SETTINGCHANGE: {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        RefreshWindow(window, *data);
        return result;
    }
    case WM_DPICHANGED: {
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        CreateResources(window, *data);
        RefreshWindow(window, *data);
        return result;
    }
    case WM_NCDESTROY:
        KillTimer(window, kStartupRevealTimerId);
        if (IsWindow(data->mdiClient))
            RemoveWindowSubclass(data->mdiClient, MdiClientSubclassProc,
                kMdiClientSubclassId);
        if (data->menuAttached) RestoreMenu(window, *data);
        RemoveWindowSubclass(window, WindowSubclassProc, kWindowSubclassId);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_windows.erase(window);
            g_updateStates.erase(window);
        }
        DeleteResources(*data);
        delete data;
        return DefSubclassProc(window, message, wParam, lParam);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

ThemeData* FindData(HWND window)
{
    DWORD_PTR reference = 0;
    if (!GetWindowSubclass(window, WindowSubclassProc, kWindowSubclassId, &reference)) return nullptr;
    return reinterpret_cast<ThemeData*>(reference);
}
}

HWND __stdcall CHSlider_Create(HWND parentWindow, int x, int y, int width, int height,
    int orientation, int minimum, int maximum, int value, HWND notifyButton)
{
    if (!IsWindow(parentWindow) || width < 20 || height < 20 ||
        (orientation != CHSLIDER_HORIZONTAL && orientation != CHSLIDER_VERTICAL) ||
        minimum >= maximum || !EnsureSliderClass())
        return nullptr;
    if (notifyButton && !IsWindow(notifyButton)) return nullptr;

    auto* data = new (std::nothrow) SliderData();
    if (!data) return nullptr;
    data->parent = parentWindow;
    data->notifyButton = notifyButton;
    data->orientation = orientation;
    data->minimum = minimum;
    data->maximum = maximum;
    data->value = std::clamp(value, minimum, maximum);

    HWND slider = CreateWindowExW(WS_EX_LAYERED, kSliderClass, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        x, y, width, height, parentWindow, nullptr, GetModuleHandleW(nullptr), data);
    if (!slider) {
        delete data;
        return nullptr;
    }
    SetLayeredWindowAttributes(slider, kSliderTransparencyKey, 255, LWA_COLORKEY);
    // Clarion may keep LIST, SHEET and other native controls above newly
    // created sibling HWNDs. Explicitly surface the slider so it is not
    // concealed when its rectangle intersects one of those controls.
    SetWindowPos(slider, HWND_TOP, x, y, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(slider, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
    return slider;
}

BOOL __stdcall CHSlider_Destroy(HWND sliderWindow)
{
    return GetSliderData(sliderWindow) && DestroyWindow(sliderWindow);
}

BOOL __stdcall CHSlider_SetRange(HWND sliderWindow, int minimum, int maximum)
{
    SliderData* data = GetSliderData(sliderWindow);
    if (!data || minimum >= maximum) return FALSE;
    data->minimum = minimum;
    data->maximum = maximum;
    data->value = std::clamp(data->value, minimum, maximum);
    InvalidateRect(sliderWindow, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHSlider_SetValue(HWND sliderWindow, int value)
{
    SliderData* data = GetSliderData(sliderWindow);
    if (!data) return FALSE;
    data->value = ClampSliderValue(*data, value);
    InvalidateRect(sliderWindow, nullptr, FALSE);
    return TRUE;
}

int __stdcall CHSlider_GetValue(HWND sliderWindow)
{
    SliderData* data = GetSliderData(sliderWindow);
    return data ? data->value : 0;
}

BOOL __stdcall CHSlider_SetStep(HWND sliderWindow, int step)
{
    SliderData* data = GetSliderData(sliderWindow);
    if (!data || step < 1) return FALSE;
    data->step = step;
    return TRUE;
}

BOOL __stdcall CHSlider_SetEnabled(HWND sliderWindow, BOOL enabled)
{
    if (!GetSliderData(sliderWindow)) return FALSE;
    EnableWindow(sliderWindow, enabled);
    return TRUE;
}

HWND __stdcall CHTabStrip_Create(HWND parentWindow, int x, int y, int width, int height,
    const char* labels, int selected, HWND notifyButton)
{
    if (!IsWindow(parentWindow) || (notifyButton && !IsWindow(notifyButton)) ||
        width <= 0 || height <= 0 || !EnsureTabStripClass()) return nullptr;
    std::vector<std::wstring> parsed = ParseTabLabels(labels);
    if (parsed.empty()) return nullptr;

    auto* data = new (std::nothrow) TabStripData();
    if (!data) return nullptr;
    data->parent = parentWindow;
    data->notifyButton = notifyButton;
    data->labels = std::move(parsed);
    data->enabled.assign(data->labels.size(), true);
    data->visible.assign(data->labels.size(), true);
    data->selected = std::clamp(selected, 1, static_cast<int>(data->labels.size()));

    HWND window = CreateWindowExW(0, kTabStripClass, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        x, y, width, height, parentWindow, nullptr, GetModuleHandleW(nullptr), data);
    if (!window) {
        delete data;
        return nullptr;
    }
    SetWindowPos(window, HWND_TOP, x, y, width, height,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    RedrawWindow(window, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    return window;
}

BOOL __stdcall CHTabStrip_Destroy(HWND tabWindow)
{
    return GetTabStripData(tabWindow) ? DestroyWindow(tabWindow) : FALSE;
}

BOOL __stdcall CHTabStrip_SetSelected(HWND tabWindow, int selected)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data || selected < 1 ||
        selected > static_cast<int>(data->labels.size()) ||
        !data->enabled[static_cast<size_t>(selected - 1)]) return FALSE;
    data->selected = selected;
    InvalidateRect(tabWindow, nullptr, FALSE);
    return TRUE;
}

int __stdcall CHTabStrip_GetSelected(HWND tabWindow)
{
    TabStripData* data = GetTabStripData(tabWindow);
    return data ? data->selected : 0;
}

BOOL __stdcall CHTabStrip_SetLabels(HWND tabWindow, const char* labels)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data) return FALSE;
    std::vector<std::wstring> parsed = ParseTabLabels(labels);
    if (parsed.empty()) return FALSE;
    data->labels = std::move(parsed);
    data->enabled.assign(data->labels.size(), true);
    data->visible.assign(data->labels.size(), true);
    data->selected = std::clamp(data->selected, 1,
        static_cast<int>(data->labels.size()));
    data->hover = 0;
    InvalidateRect(tabWindow, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHTabStrip_SetEnabled(HWND tabWindow, int tab, BOOL enabled)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data || tab < 1 || tab > static_cast<int>(data->labels.size()))
        return FALSE;
    data->enabled[static_cast<size_t>(tab - 1)] = enabled != FALSE;
    InvalidateRect(tabWindow, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHTabStrip_SetTabVisible(HWND tabWindow, int tab, BOOL visible)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data || tab < 1 || tab > static_cast<int>(data->labels.size()))
        return FALSE;
    data->visible[static_cast<size_t>(tab - 1)] = visible != FALSE;
    if (!visible && data->hover == tab) data->hover = 0;
    InvalidateRect(tabWindow, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHTabStrip_SetVisible(HWND tabWindow, BOOL visible)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data) return FALSE;
    data->stripVisible = visible != FALSE;
    const int command = data->stripVisible ? SW_SHOWNA : SW_HIDE;
    ShowWindow(tabWindow, command);
    if (IsWindow(data->leftEdge)) ShowWindow(data->leftEdge, command);
    if (IsWindow(data->rightEdge)) ShowWindow(data->rightEdge, command);
    if (IsWindow(data->bottomEdge)) ShowWindow(data->bottomEdge, command);
    return TRUE;
}

BOOL __stdcall CHTabStrip_SetSheetBounds(HWND tabWindow, int x, int y,
    int width, int height)
{
    TabStripData* data = GetTabStripData(tabWindow);
    if (!data || width <= 2 || height <= 2 || !EnsureSheetEdgeClass())
        return FALSE;

    const UINT dpi = GetDpiForWindow(data->parent);
    const int edge = std::max(2, MulDiv(2, dpi ? static_cast<int>(dpi) : 96, 96));
    auto createEdge = [&](HWND& window) {
        if (!IsWindow(window)) {
            window = CreateWindowExW(0, kSheetEdgeClass, L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 1, 1, data->parent, nullptr, GetModuleHandleW(nullptr), nullptr);
        }
        return IsWindow(window);
    };
    if (!createEdge(data->leftEdge) || !createEdge(data->rightEdge) ||
        !createEdge(data->bottomEdge)) return FALSE;

    const UINT visibilityFlag =
        data->stripVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    SetWindowPos(data->leftEdge, HWND_TOP, x, y, edge, height,
        SWP_NOACTIVATE | visibilityFlag);
    SetWindowPos(data->rightEdge, HWND_TOP, x + width - edge, y, edge, height,
        SWP_NOACTIVATE | visibilityFlag);
    SetWindowPos(data->bottomEdge, HWND_TOP, x, y + height - edge, width, edge,
        SWP_NOACTIVATE | visibilityFlag);
    RECT tabRectangle{};
    GetClientRect(tabWindow, &tabRectangle);
    const int tabHeight = std::max(1,
        static_cast<int>(tabRectangle.bottom - tabRectangle.top));
    SetWindowPos(tabWindow, HWND_TOP, x, y, width, tabHeight,
        SWP_NOACTIVATE | visibilityFlag);
    return TRUE;
}

HWND __stdcall CHTheme_CreateFlatOptionMask(HWND parentWindow, int x, int y,
    int width, int height)
{
    if (!IsWindow(parentWindow) || width <= 4 || height <= 4 ||
        !EnsureOptionEdgeClass() || !EnsureFlatMaskClass()) return nullptr;
    auto* data = new (std::nothrow) FlatMaskData();
    if (!data) return nullptr;
    data->parent = parentWindow;
    HWND window = CreateWindowExW(0, kFlatMaskClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 1, 1, parentWindow, nullptr, GetModuleHandleW(nullptr), data);
    if (!window) {
        delete data;
        return nullptr;
    }
    if (!CHTheme_SetFlatOptionBounds(window, x, y, width, height)) {
        DestroyWindow(window);
        return nullptr;
    }
    return window;
}

BOOL __stdcall CHTheme_SetFlatOptionBounds(HWND maskWindow, int x, int y,
    int width, int height)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data || width <= 4 || height <= 4 || !EnsureOptionEdgeClass())
        return FALSE;
    data->optionX = x;
    data->optionY = y;
    data->optionWidth = width;
    data->optionHeight = height;
    auto createEdge = [&](HWND& window, int identifier) {
        if (!IsWindow(window)) {
            window = CreateWindowExW(0, kOptionEdgeClass, L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, 1, 1, data->parent,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
                GetModuleHandleW(nullptr), nullptr);
        }
        return IsWindow(window);
    };
    if (!createEdge(data->leftEdge, 1) || !createEdge(data->rightEdge, 2) ||
        !createEdge(data->bottomEdge, 3)) return FALSE;

    const UINT visibilityFlag =
        data->visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    // Use one opaque surface for the complete OPTION. This reliably covers
    // Clarion's native BOXED frame without leaving differently colored edge
    // strips that can look like overlapping controls.
    SetWindowPos(maskWindow, HWND_TOP, x, y, width, height,
        SWP_NOACTIVATE | SWP_HIDEWINDOW);
    ShowWindow(data->leftEdge, SW_HIDE);
    ShowWindow(data->rightEdge, SW_HIDE);
    ShowWindow(data->bottomEdge, SW_HIDE);
    if (IsWindow(data->choiceWindow)) {
        SetWindowPos(data->choiceWindow, HWND_TOP, 0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE |
            (HasChoiceBounds(*data) ? visibilityFlag : SWP_HIDEWINDOW));
    }
    return TRUE;
}

BOOL __stdcall CHTheme_SetFlatOptionVisible(HWND maskWindow, BOOL visible)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data) return FALSE;
    data->visible = visible != FALSE;
    const int command = data->visible ? SW_SHOWNA : SW_HIDE;
    ShowWindow(maskWindow, SW_HIDE);
    if (IsWindow(data->leftEdge)) ShowWindow(data->leftEdge, SW_HIDE);
    if (IsWindow(data->rightEdge)) ShowWindow(data->rightEdge, SW_HIDE);
    if (IsWindow(data->bottomEdge)) ShowWindow(data->bottomEdge, SW_HIDE);
    if (IsWindow(data->choiceWindow)) ShowWindow(data->choiceWindow, command);
    return TRUE;
}

BOOL __stdcall CHTheme_SetFlatOptionCaption(HWND maskWindow, const char* caption)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data) return FALSE;
    data->caption = AnsiToWide(caption ? caption : "");
    InvalidateRect(maskWindow, nullptr, TRUE);
    UpdateWindow(maskWindow);
    return TRUE;
}

BOOL __stdcall CHTheme_SetFlatOptionChoices(HWND maskWindow, const char* labels,
    int selected)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data || !EnsureOptionChoiceClass()) return FALSE;
    std::vector<std::wstring> parsed = ParseTabLabels(labels);
    if (parsed.empty()) return FALSE;
    data->choiceLabels = std::move(parsed);
    data->choiceBounds.assign(data->choiceLabels.size(), RECT{});
    data->choiceBoundsDirty = true;
    data->selectedChoice = std::clamp(selected, 1,
        static_cast<int>(data->choiceLabels.size()));
    if (!IsWindow(data->choiceWindow)) {
        data->choiceWindow = CreateWindowExW(0, kOptionChoiceClass, L"",
            WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS,
            0, 0, 1, 1, data->parent, nullptr, GetModuleHandleW(nullptr), data);
    }
    if (!IsWindow(data->choiceWindow)) return FALSE;
    CHTheme_SetFlatOptionBounds(maskWindow, data->optionX, data->optionY,
        data->optionWidth, data->optionHeight);
    InvalidateRect(data->choiceWindow, nullptr, TRUE);
    return TRUE;
}

BOOL __stdcall CHTheme_SetFlatOptionChoiceBounds(HWND maskWindow, int choice,
    int x, int y, int width, int height)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data || choice < 1 ||
        choice > static_cast<int>(data->choiceBounds.size()) ||
        width <= 0 || height <= 0) return FALSE;
    const RECT bounds{ x, y, x + width, y + height };
    RECT& stored = data->choiceBounds[static_cast<size_t>(choice - 1)];
    if (!EqualRect(&stored, &bounds)) {
        stored = bounds;
        data->choiceBoundsDirty = true;
    }
    // Clarion supplies the choices in order. Batch the complete geometry update
    // and move/repaint only once, after the final choice, and only if something
    // actually changed.
    if (choice == static_cast<int>(data->choiceBounds.size()) &&
        data->choiceBoundsDirty && HasChoiceBounds(*data)) {
        PositionChoiceWindow(*data);
        data->choiceBoundsDirty = false;
    }
    return TRUE;
}

BOOL __stdcall CHTheme_SetFlatOptionChoice(HWND maskWindow, int selected)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    if (!data || selected < 1 ||
        selected > static_cast<int>(data->choiceLabels.size())) return FALSE;
    data->selectedChoice = selected;
    RepaintOptionChoice(data->choiceWindow);
    return TRUE;
}

int __stdcall CHTheme_GetFlatOptionChoice(HWND maskWindow)
{
    FlatMaskData* data = GetFlatMaskData(maskWindow);
    return data ? data->selectedChoice : 0;
}

BOOL __stdcall CHTheme_DestroyFlatOptionMask(HWND maskWindow)
{
    return GetFlatMaskData(maskWindow) ? DestroyWindow(maskWindow) : FALSE;
}

BOOL __stdcall CHTheme_AttachWindow(HWND window)
{
    if (!IsWindow(window)) return FALSE;
    EnsureWindowVisible(window);
    if (FindData(window)) return TRUE;
    auto* data = new (std::nothrow) ThemeData();
    if (!data) return FALSE;
    CreateResources(window, *data);
    if (!SetWindowSubclass(window, WindowSubclassProc, kWindowSubclassId,
        reinterpret_cast<DWORD_PTR>(data))) {
        DeleteResources(*data);
        delete data;
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_windows.insert(window);
        data->mode = g_applicationMode;
    }
    AttachMdiClient(window, *data);
    AttachMenu(window, *data);
    CHTheme_SetMode(window, data->mode);
    // Clarion position-manager templates can restore an INI position after
    // %AfterWindowOpening extensions have run. Defer the final visibility
    // check until that initialization has returned to the message queue.
    PostMessageW(window, kEnsureWindowVisibleMessage, 0, 0);
    return TRUE;
}

BOOL __stdcall CHTheme_DetachWindow(HWND window)
{
    ThemeData* data = FindData(window);
    if (!data) return FALSE;
    if (data->menuAttached) RestoreMenu(window, *data);
    RemoveWindowSubclass(window, WindowSubclassProc, kWindowSubclassId);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_windows.erase(window);
        g_updateStates.erase(window);
    }
    DeleteResources(*data);
    delete data;
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    return TRUE;
}

int __stdcall CHTheme_AttachAllControls(HWND window)
{
    ThemeData* data = FindData(window);
    if (!data && !CHTheme_AttachWindow(window)) return 0;
    data = FindData(window);
    ControlContext context{ data, window, 0 };
    EnumChildWindows(window, ApplyControlTheme, reinterpret_cast<LPARAM>(&context));
    return context.count;
}

BOOL __stdcall CHTheme_AttachDropList(HWND listWindow)
{
    if (!IsWindow(listWindow)) return FALSE;

    HWND themeWindow = GetAncestor(listWindow, GA_ROOT);
    ThemeData* data = FindData(themeWindow);
    if (!data && (!IsWindow(themeWindow) || !CHTheme_AttachWindow(themeWindow)))
        return FALSE;
    data = FindData(themeWindow);
    if (!data) return FALSE;

    // Clarion LIST controls with DROP() are not reliably discoverable through
    // EnumChildWindows.  The template passes their PROP:Handle explicitly.
    AttachComboTheme(listWindow, *data);
    // ClaDrop is a proprietary Clarion-painted control.  Its closed-field
    // frame and arrow are drawn directly by the runtime during hover/open/
    // restore operations, outside a dependable Win32 paint sequence.  Do not
    // overlay that chrome: doing so causes visible contention and flicker.
    // The template-applied PROPLIST colours below remain authoritative for
    // the closed text and popup rows.
    if (data->font) {
        SendMessageW(listWindow, WM_SETFONT,
            reinterpret_cast<WPARAM>(data->font), TRUE);
    }
    RedrawWindow(listWindow, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN |
        RDW_UPDATENOW);
    return TRUE;
}

BOOL __stdcall CHTheme_SetMode(HWND window, int mode)
{
    ThemeData* data = FindData(window);
    if (!data || (mode != CHTHEME_DARK && mode != CHTHEME_DARK_HIGH_CONTRAST &&
        mode != CHTHEME_LIGHT)) return FALSE;
    data->mode = mode;
    if (mode == CHTHEME_LIGHT) {
        data->background = data->lightBackground;
        data->surface = RGB(255, 255, 255);
        data->menuSurface = RGB(255, 255, 255);
        data->input = RGB(255, 255, 255);
        data->text = RGB(25, 38, 52);
        data->secondaryText = RGB(91, 106, 121);
        data->disabledText = RGB(137, 147, 158);
    } else if (mode == CHTHEME_DARK_HIGH_CONTRAST) {
        data->background = data->darkBackground; data->surface = RGB(10, 20, 30);
        data->menuSurface = RGB(16, 29, 42);
        data->input = RGB(3, 10, 17); data->text = RGB(255, 255, 255);
        data->secondaryText = RGB(190, 204, 218);
        data->disabledText = RGB(104, 118, 132);
    } else {
        data->background = data->darkBackground; data->surface = RGB(13, 23, 33);
        data->menuSurface = RGB(18, 30, 42);
        data->input = RGB(8, 16, 24); data->text = RGB(235, 241, 248);
        data->secondaryText = RGB(158, 173, 189);
        data->disabledText = RGB(104, 118, 132);
    }
    // Keep the menu bar and popup menu surfaces visually connected to the
    // configurable application window background.
    data->menuSurface = data->background;
    CreateResources(window, *data);
    if (data->menuAttached) {
        ApplyMenuBackground(GetMenu(window), data->menuBrush);
        DrawMenuBar(window);
    }
    RefreshWindow(window, *data);
    // Clarion may defer or suppress the WM_PAINT generated by RedrawWindow
    // while processing an ACCEPTED event. Paint the client surface directly
    // so the first theme selection (and initial per-window override) is
    // visible immediately; the regular paint path remains authoritative for
    // subsequent expose, restore, and resize operations.
    PaintWindowBackground(window, *data);
    // Clarion can draw directly to the window after returning from this DLL
    // call, without issuing another WM_PAINT. Coalesce a final repaint after
    // the current ACCEPTED/window-open cycle has yielded to the message loop.
    SetTimer(window, kDeferredThemeRedrawTimerId, 10, nullptr);
    return TRUE;
}

BOOL __stdcall CHTheme_SetBackgroundColors(HWND window, COLORREF lightBackground,
    COLORREF darkBackground)
{
    ThemeData* data = FindData(window);
    if (!data) return FALSE;
    data->lightBackground = lightBackground;
    data->darkBackground = darkBackground;
    const BOOL changed = CHTheme_SetMode(window, data->mode);
    if (changed) {
        // Clarion performs an additional startup paint after the
        // AfterWindowOpening embeds have completed. Queue the final repaint so
        // the configured per-window background wins that first paint without
        // requiring activation, resizing, or another user action.
        PostMessageW(window, kForceApplicationRedrawMessage, 0, 0);
    }
    return changed;
}

BOOL __stdcall CHTheme_SetFont(HWND window, const char* faceName, int pointSize,
    BOOL bold, BOOL italic)
{
    ThemeData* data = FindData(window);
    if (!data || !faceName || !*faceName) return FALSE;
    std::wstring face = AnsiToWide(faceName);
    if (face.empty()) return FALSE;
    data->fontFace = std::move(face);
    data->fontPointSize = std::max(1, pointSize);
    data->fontWeight = bold ? FW_BOLD : FW_NORMAL;
    data->fontItalic = italic ? TRUE : FALSE;
    CreateResources(window, *data);
    if (data->menuAttached) {
        DrawMenuBar(window);
    }
    InvalidateRect(window, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHTheme_SetNotifyButton(HWND window, HWND notifyButton)
{
    ThemeData* data = FindData(window);
    if (!data || (notifyButton && !IsWindow(notifyButton))) return FALSE;
    data->notifyButton = notifyButton;
    return TRUE;
}

BOOL __stdcall CHTheme_ConsumeFlatOptionNotify(HWND window)
{
    ThemeData* data = FindData(window);
    if (!data || data->flatOptionNotifyCount == 0) return FALSE;
    --data->flatOptionNotifyCount;
    return TRUE;
}

BOOL __stdcall CHSlider_SetColors(HWND sliderWindow, COLORREF lightTrack,
    COLORREF lightFill, COLORREF darkTrack, COLORREF darkFill)
{
    SliderData* data = GetSliderData(sliderWindow);
    if (!data) return FALSE;
    data->lightTrack = lightTrack;
    data->lightFill = lightFill;
    data->darkTrack = darkTrack;
    data->darkFill = darkFill;
    InvalidateRect(sliderWindow, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHTheme_SetApplicationMode(HWND window, int mode)
{
    if (mode != CHTHEME_DARK &&
        mode != CHTHEME_DARK_HIGH_CONTRAST && mode != CHTHEME_LIGHT) {
        return FALSE;
    }

    if (!window) {
        std::unordered_set<HWND> themedWindows;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_applicationMode = mode;
            themedWindows = g_windows;
        }

        BOOL queued = FALSE;
        const DWORD callingThread = GetCurrentThreadId();
        for (HWND candidate : themedWindows) {
            if (!IsWindow(candidate)) continue;
            const DWORD windowThread =
                GetWindowThreadProcessId(candidate, nullptr);
            if (windowThread == callingThread) {
                // Complete the initiating thread before returning so the
                // Clarion after-field hook sees the new mode during this same
                // ACCEPTED event.
                SendMessageW(candidate, kApplyApplicationThemeMessage,
                    static_cast<WPARAM>(mode), 0);
                queued = TRUE;
            } else if (PostMessageW(candidate,
                kApplyApplicationThemeMessage,
                static_cast<WPARAM>(mode), 0)) {
                queued = TRUE;
            }
        }
        return queued;
    }

    if (!IsWindow(window)) return FALSE;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_applicationMode = mode;
    }

    HWND frame = FindMenuOwner(window);
    if (!frame) frame = GetAncestor(window, GA_ROOT);
    if (!frame) frame = window;

    if (!FindData(frame)) CHTheme_AttachWindow(frame);
    if (window != frame && !FindData(window)) CHTheme_AttachWindow(window);

    std::unordered_set<HWND> themedWindows;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        themedWindows = g_windows;
    }

    BOOL changed = FALSE;
    for (HWND candidate : themedWindows) {
        if (!IsWindow(candidate)) continue;
        HWND candidateFrame = FindMenuOwner(candidate);
        if (!candidateFrame) candidateFrame = GetAncestor(candidate, GA_ROOT);
        if (!candidateFrame) candidateFrame = candidate;
        if (candidate == window || candidate == frame || candidateFrame == frame) {
            if (CHTheme_SetMode(candidate, mode)) changed = TRUE;
        }
    }

    DrawMenuBar(frame);
    RedrawWindow(frame, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    return changed;
}

BOOL __stdcall CHTheme_ForceRedraw(HWND window)
{
    if (!window) {
        std::unordered_set<HWND> themedWindows;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            themedWindows = g_windows;
        }
        BOOL queued = FALSE;
        for (HWND candidate : themedWindows) {
            if (IsWindow(candidate) &&
                PostMessageW(candidate, kForceApplicationRedrawMessage, 0, 0)) {
                queued = TRUE;
            }
        }
        return queued;
    }
    if (!IsWindow(window)) return FALSE;
    return RedrawWindow(window, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
        RDW_FRAME | RDW_UPDATENOW);
}

BOOL __stdcall CHTheme_BeginUpdate(HWND window)
{
    if (!IsWindow(window)) return FALSE;

    bool hideWindow = false;
    bool cancelPendingReveal = false;
    HWND freezeMdiClient = nullptr;
    HWND freezeMdiFrame = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        UpdateState& state = g_updateStates[window];
        if (state.depth == 0) {
            if (state.pendingReveal) {
                // A nested initialization phase began before the deferred
                // reveal. Keep the original visibility state and restart the
                // delay when the matching EndUpdate arrives.
                state.pendingReveal = false;
                cancelPendingReveal = true;
            } else {
                // Preserve an intentionally hidden Clarion window. Testing the
                // WS_VISIBLE style directly also works when an owner or MDI
                // parent is temporarily hidden during application startup.
                state.restoreVisible =
                    (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
                hideWindow = state.restoreVisible;
            }
            if ((GetWindowLongPtrW(window, GWL_EXSTYLE) &
                    WS_EX_MDICHILD) != 0) {
                freezeMdiClient = GetParent(window);
                freezeMdiFrame = IsWindow(freezeMdiClient)
                    ? GetParent(freezeMdiClient) : nullptr;
                state.frozenMdiClient = IsWindow(freezeMdiClient)
                    ? freezeMdiClient : nullptr;
                state.frozenMdiFrame = IsWindow(freezeMdiFrame)
                    ? freezeMdiFrame : nullptr;
            }
        }
        ++state.depth;
    }

    if (cancelPendingReveal)
        KillTimer(window, kStartupRevealTimerId);

    // Freeze only the visible MDI parents. The hidden child remains free to
    // lay itself out, preserving WindowResizeClass geometry capture.
    if (IsWindow(freezeMdiClient))
        SendMessageW(freezeMdiClient, WM_SETREDRAW, FALSE, 0);
    if (IsWindow(freezeMdiFrame))
        SendMessageW(freezeMdiFrame, WM_SETREDRAW, FALSE, 0);

    if (hideWindow)
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
            SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    return TRUE;
}

BOOL __stdcall CHTheme_EndUpdate(HWND window)
{
    bool finishUpdate = false;
    HWND frozenMdiClient = nullptr;
    HWND frozenMdiFrame = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto found = g_updateStates.find(window);
        if (found == g_updateStates.end()) return FALSE;
        if (found->second.depth > 0)
            --found->second.depth;
        if (found->second.depth == 0) {
            // The Clarion extension owns the final PROP:Hide transition.
            // Release the native show guard before returning so that its
            // one final reveal cannot be blocked by the DLL.
            frozenMdiClient = found->second.frozenMdiClient;
            frozenMdiFrame = found->second.frozenMdiFrame;
            g_updateStates.erase(found);
            finishUpdate = true;
        }
    }

    if (!finishUpdate) return TRUE;
    if (!IsWindow(window)) return FALSE;

    if (IsWindow(frozenMdiClient))
        SendMessageW(frozenMdiClient, WM_SETREDRAW, TRUE, 0);
    if (IsWindow(frozenMdiFrame))
        SendMessageW(frozenMdiFrame, WM_SETREDRAW, TRUE, 0);

    // Complete the final hidden rendering pass. The extension immediately
    // clears PROP:Hide and issues DISPLAY after this call returns.
    DrawMenuBar(window);
    const BOOL result = RedrawWindow(window, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN |
        RDW_FRAME | RDW_UPDATENOW);
    // Invalidate the visible parents without forcing an intermediate paint.
    // Clarion reveals the child immediately after this call returns.
    if (IsWindow(frozenMdiClient))
        RedrawWindow(frozenMdiClient, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    if (IsWindow(frozenMdiFrame))
        RedrawWindow(frozenMdiFrame, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    return result;
}

BOOL __stdcall CHTheme_AttachMenu(HWND window)
{
    HWND owner = FindMenuOwner(window);
    if (!owner) return FALSE;
    ThemeData* data = FindData(owner);
    if (!data && !CHTheme_AttachWindow(owner)) return FALSE;
    data = FindData(owner);
    return data ? AttachMenu(owner, *data) : FALSE;
}

BOOL __stdcall CHTheme_DetachMenu(HWND window)
{
    HWND owner = FindMenuOwner(window);
    ThemeData* data = owner ? FindData(owner) : nullptr;
    if (!data || !data->menuAttached) return FALSE;
    RestoreMenu(owner, *data);
    data->menuCaptions.clear();
    data->menuIcons.clear();
    data->hiddenMenuPaths.clear();
    data->menuCaptionsById.clear();
    data->menuIconsById.clear();
    data->hiddenMenuIds.clear();
    return TRUE;
}

BOOL __stdcall CHTheme_AttachClarionMenu(HWND window, const char* definition)
{
    if (!definition || !*definition) return FALSE;
    HWND owner = FindMenuOwner(window);
    if (!owner) return FALSE;

    ThemeData* sourceTheme = FindData(window);
    ThemeData* data = FindData(owner);
    if (!data && !CHTheme_AttachWindow(owner)) return FALSE;
    data = FindData(owner);
    if (!data) return FALSE;

    if (sourceTheme && sourceTheme != data) {
        CHTheme_SetMode(owner, sourceTheme->mode);
        CHTheme_SetAccent(owner, sourceTheme->accent);
        data = FindData(owner);
        if (!data) return FALSE;
    }

    MenuSourceData source;
    ParseMenuDefinition(definition, source.captions, source.icons,
        source.hiddenPaths, source.captionsById, source.iconsById,
        source.hiddenIds);
    if (source.captions.empty() && source.hiddenPaths.empty() &&
        source.captionsById.empty() && source.hiddenIds.empty()) return FALSE;
    data->menuSources[window] = std::move(source);
    data->activeMenuSource = window;
    AttachMdiClient(owner, *data);
    const BOOL attached = RefreshMergedMenu(owner, *data, true);

    // Clarion can call the child extension while it is still constructing the
    // frame's merged HMENU.  Recheck once the current Clarion event has
    // returned to the Windows message queue; this is a one-shot notification,
    // not a polling timer.
    if (window != owner)
        PostMessageW(owner, kRefreshMergedMenuMessage, 0, 0);
    return attached;
}

int __stdcall CHTheme_GetMode(HWND window)
{
    ThemeData* data = FindData(window);
    return data ? data->mode : -1;
}

BOOL __stdcall CHTheme_SetAccent(HWND window, COLORREF accent)
{
    ThemeData* data = FindData(window);
    if (!data) return FALSE;
    data->accent = accent;
    RefreshWindow(window, *data);
    return TRUE;
}

HWND __stdcall CHTheme_GetListHeader(HWND listWindow)
{
    return FindHeaderWindow(listWindow);
}

BOOL __stdcall CHTheme_AttachListHeader(HWND listWindow)
{
    if (!IsWindow(listWindow)) return FALSE;
    HWND themeWindow = GetAncestor(listWindow, GA_ROOT);
    if (!FindData(themeWindow) && !CHTheme_AttachWindow(themeWindow)) return FALSE;
    return AttachListHeaderInternal(listWindow, themeWindow);
}

BOOL __stdcall CHTheme_DetachListHeader(HWND listWindow)
{
    HWND header = FindHeaderWindow(listWindow);
    if (!header) return FALSE;
    DWORD_PTR reference = 0;
    if (!GetWindowSubclass(header, HeaderSubclassProc, kHeaderSubclassId, &reference)) return FALSE;
    RemoveWindowSubclass(header, HeaderSubclassProc, kHeaderSubclassId);
    delete reinterpret_cast<HeaderData*>(reference);
    SetWindowTheme(header, L"Explorer", nullptr);
    InvalidateRect(header, nullptr, TRUE);
    return TRUE;
}

BOOL __stdcall CHTheme_AttachClarionListHeader(HWND listWindow, int headerHeight)
{
    if (!IsWindow(listWindow) || headerHeight < 16 || headerHeight > 80) return FALSE;
    if (FindClarionListData(listWindow)) return TRUE;
    // Clarion procedure windows may be children of a frame/MDI root. Bind the
    // overlay to the nearest ancestor already attached to CHTheme, which is the
    // HWND Clarion passes to CHTheme_SetMode when switching this procedure.
    HWND themeWindow = listWindow;
    while (themeWindow && !FindData(themeWindow)) themeWindow = GetParent(themeWindow);
    if (!themeWindow) {
        themeWindow = GetAncestor(listWindow, GA_ROOT);
        if (!FindData(themeWindow) && !CHTheme_AttachWindow(themeWindow)) return FALSE;
    }
    if (!EnsureClarionHeaderClass()) return FALSE;

    auto* data = new (std::nothrow) ClarionListData();
    if (!data) return FALSE;
    data->listWindow = listWindow;
    data->themeWindow = themeWindow;
    const UINT dpi = GetDpiForWindow(listWindow);
    const int nativeHeaderHeight = std::max(16, MulDiv(18, dpi ? dpi : 96, 96));
    data->height = std::min(headerHeight, nativeHeaderHeight);
    data->originalStyle = GetWindowLongPtrW(listWindow, GWL_STYLE);
    data->originalExStyle = GetWindowLongPtrW(listWindow, GWL_EXSTYLE);
    const LONG_PTR flatStyle = data->originalStyle & ~static_cast<LONG_PTR>(WS_BORDER);
    const LONG_PTR flatExStyle = data->originalExStyle &
        ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
    if (flatStyle != data->originalStyle || flatExStyle != data->originalExStyle) {
        SetWindowLongPtrW(listWindow, GWL_STYLE, flatStyle);
        SetWindowLongPtrW(listWindow, GWL_EXSTYLE, flatExStyle);
        SetWindowPos(listWindow, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        data->frameFlattened = true;
    }
    HWND parent = GetParent(listWindow);
    data->overlay = CreateWindowExW(0, kClarionHeaderClass, L"", WS_CHILD | WS_VISIBLE,
        0, 0, 1, data->height, parent, nullptr, GetModuleHandleW(nullptr), data);
    if (!data->overlay || !SetWindowSubclass(listWindow, ClarionListSubclassProc,
        kClarionListSubclassId, reinterpret_cast<DWORD_PTR>(data))) {
        if (IsWindow(data->overlay)) DestroyWindow(data->overlay);
        delete data;
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_clarionLists.insert(data);
    }
    if (ThemeData* theme = FindData(themeWindow))
        ApplyClarionListTheme(*data, *theme);
    PositionClarionHeader(*data);
    // Clarion performs additional LIST layout/painting after EVENT:OpenWindow.
    // Briefly reassert the overlay's sibling z-order until that startup work ends.
    SetTimer(data->overlay, 1, 50, nullptr);
    RedrawWindow(data->overlay, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
    return TRUE;
}

BOOL __stdcall CHTheme_SetClarionListColumns(HWND listWindow, const char* definition)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || !definition) return FALSE;
    data->columns.clear();
    std::string all(definition);
    const bool clarionFormat =
        all.find('~') != std::string::npos &&
        all.find('@') != std::string::npos;
    // The optional advanced definition uses "Caption|Width;...".  Native
    // Clarion FORMAT strings also contain vertical bars, but use picture
    // tokens such as @s255@ and grouping characters.  Do not interpret a
    // headerless native FORMAT as one enormous advanced caption.
    const bool simpleDefinition =
        !clarionFormat &&
        all.find('@') == std::string::npos &&
        all.find('[') == std::string::npos &&
        all.find(']') == std::string::npos;
    size_t start = 0;
    while (simpleDefinition && start <= all.size()) {
        const size_t end = all.find(';', start);
        const std::string token = all.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        const size_t divider = token.rfind('|');
        if (divider != std::string::npos && divider > 0) {
            const std::string captionA = token.substr(0, divider);
            const int width = std::max(20, std::atoi(token.substr(divider + 1).c_str()));
            const int chars = MultiByteToWideChar(CP_ACP, 0, captionA.c_str(), -1, nullptr, 0);
            if (chars > 1) {
                std::wstring caption(static_cast<size_t>(chars), L'\0');
                MultiByteToWideChar(CP_ACP, 0, captionA.c_str(), -1,
                    caption.data(), chars);
                caption.resize(static_cast<size_t>(chars - 1));
                data->columns.push_back({ caption, width, width, 0 });
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    // The template can pass Clarion's native FORMAT string directly.  When
    // the simple "Caption|Width;..." form produced no columns, derive the
    // captions and relative widths from FORMAT's ~heading~ segments.
    if (data->columns.empty() && clarionFormat) {
        size_t searchFrom = 0;
        size_t columnStart = 0;
        std::vector<int> relativeWidths;
        while (searchFrom < all.size()) {
            const size_t captionStart = all.find('~', searchFrom);
            if (captionStart == std::string::npos) break;
            const size_t captionEnd = all.find('~', captionStart + 1);
            if (captionEnd == std::string::npos) break;

            size_t numberStart = columnStart;
            while (numberStart < captionStart &&
                !std::isdigit(static_cast<unsigned char>(all[numberStart]))) {
                ++numberStart;
            }
            size_t numberEnd = numberStart;
            while (numberEnd < captionStart &&
                std::isdigit(static_cast<unsigned char>(all[numberEnd]))) {
                ++numberEnd;
            }
            const int relativeWidth = numberEnd > numberStart
                ? std::max(1, std::atoi(all.substr(numberStart,
                    numberEnd - numberStart).c_str()))
                : 80;
            const std::string captionA =
                all.substr(captionStart + 1, captionEnd - captionStart - 1);
            const int chars = MultiByteToWideChar(
                CP_ACP, 0, captionA.c_str(), -1, nullptr, 0);
            if (chars > 1) {
                std::wstring caption(static_cast<size_t>(chars), L'\0');
                MultiByteToWideChar(CP_ACP, 0, captionA.c_str(), -1,
                    caption.data(), chars);
                caption.resize(static_cast<size_t>(chars - 1));
                data->columns.push_back({ caption, relativeWidth,
                    relativeWidth, 0 });
                relativeWidths.push_back(relativeWidth);
            }

            const size_t pictureStart = all.find('@', captionEnd + 1);
            const size_t pictureEnd = pictureStart == std::string::npos
                ? std::string::npos
                : all.find('@', pictureStart + 1);
            columnStart = pictureEnd == std::string::npos
                ? captionEnd + 1
                : pictureEnd + 1;
            searchFrom = columnStart;
        }

        if (!data->columns.empty()) {
            RECT bounds{};
            GetClientRect(listWindow, &bounds);
            const int available = std::max(
                20 * static_cast<int>(data->columns.size()),
                static_cast<int>(bounds.right - bounds.left));
            int totalRelative = 0;
            for (const int width : relativeWidths) totalRelative += width;
            int assigned = 0;
            for (size_t index = 0; index < data->columns.size(); ++index) {
                const int width = index + 1 == data->columns.size()
                    ? std::max(20, available - assigned)
                    : std::max(20, available * relativeWidths[index] /
                        std::max(1, totalRelative));
                data->columns[index].width = width;
                data->columns[index].pixelBasis = width;
                data->columns[index].clarionBasis = relativeWidths[index];
                assigned += width;
            }
        }
    }
    PositionClarionHeader(*data);
    RedrawWindow(data->overlay, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
    return data->columns.empty() ? FALSE : TRUE;
}

BOOL __stdcall CHTheme_SetClarionListSort(HWND listWindow, int column, int direction)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || column < 0 || column > static_cast<int>(data->columns.size()) ||
        direction < -1 || direction > 1) return FALSE;
    data->sortColumn = column == 0 ? -1 : column - 1;
    data->sortDirection = direction;
    InvalidateRect(data->overlay, nullptr, FALSE);
    return TRUE;
}

int __stdcall CHTheme_GetClarionListHeaderClick(HWND listWindow)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data) return 0;
    const int click = data->lastClick;
    data->lastClick = 0;
    return click;
}

BOOL __stdcall CHTheme_DetachClarionListHeader(HWND listWindow)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data) return FALSE;
    RemoveWindowSubclass(listWindow, ClarionListSubclassProc, kClarionListSubclassId);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_clarionLists.erase(data);
    }
    if (IsWindow(data->overlay)) DestroyWindow(data->overlay);
    if (data->frameFlattened && IsWindow(listWindow)) {
        SetWindowLongPtrW(listWindow, GWL_STYLE, data->originalStyle);
        SetWindowLongPtrW(listWindow, GWL_EXSTYLE, data->originalExStyle);
        SetWindowPos(listWindow, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    delete data;
    InvalidateRect(listWindow, nullptr, TRUE);
    return TRUE;
}

int __stdcall CHTheme_GetClarionListResizedColumn(HWND listWindow)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data) return 0;
    const int column = data->lastResizedColumn;
    data->lastResizedColumn = 0;
    return column;
}

int __stdcall CHTheme_GetClarionListColumnWidth(HWND listWindow, int column)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || column < 1 || column > static_cast<int>(data->columns.size())) return 0;
    const ClarionColumn& item = data->columns[static_cast<size_t>(column - 1)];
    if (data->controlClarionBasis > 0 && data->controlPixelBasis > 0)
        return MulDiv(item.width, data->controlClarionBasis,
            data->controlPixelBasis);
    // Clarion FORMAT widths depend on the LIST's own metrics. When calibrated,
    // preserve the exact ratio between the initial overlay and Clarion widths.
    if (item.clarionBasis > 0 && item.pixelBasis > 0)
        return MulDiv(item.width, item.clarionBasis, item.pixelBasis);
    // Compatibility fallback for callers that have not supplied a basis.
    const UINT dpi = GetDpiForWindow(listWindow);
    return MulDiv(item.width, 96, dpi ? static_cast<int>(dpi) : 96);
}

BOOL __stdcall CHTheme_SetClarionListColumnBasis(HWND listWindow, int column,
    int clarionWidth)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || column < 1 || column > static_cast<int>(data->columns.size()) ||
        clarionWidth <= 0) return FALSE;
    ClarionColumn& item = data->columns[static_cast<size_t>(column - 1)];
    item.pixelBasis = item.width;
    item.clarionBasis = clarionWidth;
    return TRUE;
}

BOOL __stdcall CHTheme_SetClarionListControlBasis(HWND listWindow,
    int clarionControlWidth)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || clarionControlWidth <= 0 || data->columns.empty()) return FALSE;
    RECT bounds{};
    if (!GetClientRect(listWindow, &bounds)) return FALSE;
    const int pixelWidth = static_cast<int>(bounds.right - bounds.left);
    if (pixelWidth <= 0) return FALSE;
    data->controlPixelBasis = pixelWidth;
    data->controlClarionBasis = clarionControlWidth;
    for (ClarionColumn& item : data->columns) {
        if (item.clarionBasis > 0) {
            item.width = std::max(20,
                MulDiv(item.clarionBasis, pixelWidth, clarionControlWidth));
            item.pixelBasis = item.width;
        }
    }
    PositionClarionHeader(*data);
    RedrawWindow(data->overlay, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
    return TRUE;
}

BOOL __stdcall CHTheme_SetClarionListColumnResizable(HWND listWindow, int column,
    BOOL resizable)
{
    ClarionListData* data = FindClarionListData(listWindow);
    if (!data || column < 1 ||
        column > static_cast<int>(data->columns.size())) return FALSE;
    data->columns[static_cast<size_t>(column - 1)].resizable =
        resizable != FALSE;
    return TRUE;
}

DWORD __stdcall CHTheme_GetWindowsBuild()
{
    struct VersionInfo
    {
        ULONG size;
        ULONG major;
        ULONG minor;
        ULONG build;
        ULONG platform;
        WCHAR servicePack[128];
        USHORT servicePackMajor;
        USHORT servicePackMinor;
        USHORT suiteMask;
        BYTE productType;
        BYTE reserved;
    };

    using RtlGetVersionProc = LONG(WINAPI*)(VersionInfo*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionProc>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) return 0;

    VersionInfo version{};
    version.size = sizeof(version);
    if (rtlGetVersion(&version) != 0 || version.major < 10) return 0;
    return version.build;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
