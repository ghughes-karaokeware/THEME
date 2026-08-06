#include "ButtonSubclass.h"

#include <commctrl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <new>
#include <string>
#include <unordered_set>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")

namespace
{
constexpr UINT_PTR kSubclassId = 0x43484254; // CHBT
constexpr UINT_PTR kParentSubclassId = 0x43484250; // CHBP
constexpr UINT kDeferredRedrawMessage = WM_APP + 0x425;
constexpr UINT kParentDeferredRedrawMessage = WM_APP + 0x426;
constexpr UINT_PTR kStartupRedrawTimer = 0x43485244; // CHRD

enum class ControlKind
{
    PushButton,
    RadioButton,
    CheckBox,
    GroupBox
};

struct ButtonData
{
    COLORREF normalTop = RGB(64, 74, 91);
    COLORREF normalBottom = RGB(38, 46, 60);
    COLORREF hoverTop = RGB(72, 139, 249);
    COLORREF hoverBottom = RGB(38, 99, 210);
    COLORREF pressedTop = RGB(34, 86, 174);
    COLORREF pressedBottom = RGB(25, 61, 128);
    COLORREF text = RGB(255, 255, 255);
    COLORREF hoverText = RGB(255, 255, 255);
    COLORREF border = RGB(28, 35, 47);
    COLORREF disabledTop = RGB(28, 38, 50);
    COLORREF disabledBottom = RGB(18, 26, 35);
    COLORREF disabledText = RGB(105, 119, 134);
    COLORREF disabledBorder = RGB(39, 53, 68);
    int radius = 6;
    int borderWidth = 1;
    bool gradient = true;
    bool tracking = false;
    int startupRedrawTicks = 0;
    HICON icon = nullptr; // Borrowed; caller retains ownership.
    HBITMAP bitmap = nullptr; // Borrowed; caller retains ownership.
    UINT imageType = 0;
    bool ownsIcon = false;
    int iconWidth = 16;
    int iconGap = 6;
    LONG_PTR originalStyle = 0;
    int horizontalAlignment = 0; // -1 left, 0 native/center, 1 right
    int verticalAlignment = 0; // -1 top, 0 native/center, 1 bottom
    bool horizontalAlignmentSet = false;
    bool verticalAlignmentSet = false;
    int role = CHBUTTON_ROLE_STANDARD;
    ControlKind kind = ControlKind::PushButton;
    bool pushLike = false;

    ~ButtonData()
    {
        if (ownsIcon && icon) DestroyIcon(icon);
    }
};

std::mutex g_mutex;
std::unordered_set<HWND> g_buttons;
int g_theme = CHBUTTON_THEME_DARK;

ButtonData* FindData(HWND hwnd);

HICON LoadRasterImageAsIcon(const std::string& fileName)
{
    static ULONG_PTR gdiplusToken = 0;
    static std::once_flag gdiplusOnce;
    std::call_once(gdiplusOnce, [] {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&gdiplusToken, &input, nullptr);
    });
    if (!gdiplusToken) return nullptr;

    const int wideLength = MultiByteToWideChar(CP_ACP, 0, fileName.c_str(),
        -1, nullptr, 0);
    if (wideLength <= 1) return nullptr;
    std::wstring wideName(static_cast<size_t>(wideLength), L'\0');
    if (!MultiByteToWideChar(CP_ACP, 0, fileName.c_str(), -1,
        wideName.data(), wideLength)) return nullptr;

    Gdiplus::Bitmap bitmap(wideName.c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok ||
        bitmap.GetWidth() == 0 || bitmap.GetHeight() == 0) return nullptr;

    HICON icon = nullptr;
    return bitmap.GetHICON(&icon) == Gdiplus::Ok ? icon : nullptr;
}

void CaptureNativeImage(HWND button, ButtonData& data)
{
    if (data.ownsIcon && data.icon) DestroyIcon(data.icon);
    data.ownsIcon = false;
    data.icon = reinterpret_cast<HICON>(
        SendMessage(button, BM_GETIMAGE, IMAGE_ICON, 0));
    data.bitmap = reinterpret_cast<HBITMAP>(
        SendMessage(button, BM_GETIMAGE, IMAGE_BITMAP, 0));
    if (data.icon) {
        data.imageType = IMAGE_ICON;
    } else if (data.bitmap) {
        data.imageType = IMAGE_BITMAP;
    } else {
        data.imageType = 0;
    }
}

bool DrawDarkIconAdaptation(HDC destination, int x, int y, int width,
    int height, HICON icon)
{
    if (!destination || !icon || width <= 0 || height <= 0) return false;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* colorBits = nullptr;
    void* maskBits = nullptr;
    HBITMAP colorBitmap = CreateDIBSection(destination, &info, DIB_RGB_COLORS,
        &colorBits, nullptr, 0);
    HBITMAP maskBitmap = CreateDIBSection(destination, &info, DIB_RGB_COLORS,
        &maskBits, nullptr, 0);
    if (!colorBitmap || !maskBitmap || !colorBits || !maskBits) {
        if (colorBitmap) DeleteObject(colorBitmap);
        if (maskBitmap) DeleteObject(maskBitmap);
        return false;
    }

    HDC colorDc = CreateCompatibleDC(destination);
    HDC maskDc = CreateCompatibleDC(destination);
    if (!colorDc || !maskDc) {
        if (colorDc) DeleteDC(colorDc);
        if (maskDc) DeleteDC(maskDc);
        DeleteObject(colorBitmap);
        DeleteObject(maskBitmap);
        return false;
    }

    HGDIOBJ oldColor = SelectObject(colorDc, colorBitmap);
    HGDIOBJ oldMask = SelectObject(maskDc, maskBitmap);
    std::fill_n(static_cast<DWORD*>(colorBits),
        static_cast<size_t>(width) * height, 0u);
    std::fill_n(static_cast<DWORD*>(maskBits),
        static_cast<size_t>(width) * height, 0x00FFFFFFu);
    DrawIconEx(colorDc, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL);
    DrawIconEx(maskDc, 0, 0, icon, width, height, 0, nullptr, DI_MASK);

    DWORD* colors = static_cast<DWORD*>(colorBits);
    const DWORD* masks = static_cast<const DWORD*>(maskBits);
    bool adapted = false;
    const size_t count = static_cast<size_t>(width) * height;
    for (size_t index = 0; index < count; ++index) {
        const BYTE blue = static_cast<BYTE>(colors[index] & 0xFF);
        const BYTE green = static_cast<BYTE>((colors[index] >> 8) & 0xFF);
        const BYTE red = static_cast<BYTE>((colors[index] >> 16) & 0xFF);
        BYTE alpha = static_cast<BYTE>((colors[index] >> 24) & 0xFF);
        const BYTE maskBlue = static_cast<BYTE>(masks[index] & 0xFF);
        const BYTE maskGreen = static_cast<BYTE>((masks[index] >> 8) & 0xFF);
        const BYTE maskRed = static_cast<BYTE>((masks[index] >> 16) & 0xFF);
        const bool maskOpaque =
            (static_cast<int>(maskRed) + maskGreen + maskBlue) < 384;
        if (alpha == 0 && maskOpaque) alpha = 255;

        const BYTE maximum = std::max({ red, green, blue });
        const BYTE minimum = std::min({ red, green, blue });
        const int luminance =
            (static_cast<int>(red) * 30 + static_cast<int>(green) * 59 +
                static_cast<int>(blue) * 11) / 100;
        if (alpha == 0 || maximum > 96 || maximum - minimum > 28) {
            colors[index] = 0;
            continue;
        }

        // Darker pixels receive the strongest lift. This retains antialiased
        // edges while leaving colored and already-light icon pixels intact.
        const int strength = std::clamp(112 - luminance, 0, 112);
        const BYTE overlayAlpha = static_cast<BYTE>(
            static_cast<int>(alpha) * strength / 112);
        constexpr BYTE targetRed = 232;
        constexpr BYTE targetGreen = 238;
        constexpr BYTE targetBlue = 245;
        colors[index] =
            (static_cast<DWORD>(overlayAlpha) << 24) |
            (static_cast<DWORD>(targetRed * overlayAlpha / 255) << 16) |
            (static_cast<DWORD>(targetGreen * overlayAlpha / 255) << 8) |
            static_cast<DWORD>(targetBlue * overlayAlpha / 255);
        adapted = true;
    }

    if (adapted) {
        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(destination, x, y, width, height, colorDc, 0, 0,
            width, height, blend);
    }

    SelectObject(colorDc, oldColor);
    SelectObject(maskDc, oldMask);
    DeleteDC(colorDc);
    DeleteDC(maskDc);
    DeleteObject(colorBitmap);
    DeleteObject(maskBitmap);
    return adapted;
}

void RestartButtonRedraws(HWND parent)
{
    std::unordered_set<HWND> buttons;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        buttons = g_buttons;
    }
    for (HWND button : buttons) {
        if (!IsWindow(button) || !IsChild(parent, button)) continue;
        if (ButtonData* data = FindData(button)) {
            data->startupRedrawTicks = 0;
            SetTimer(button, kStartupRedrawTimer, 50, nullptr);
        }
        RedrawWindow(button, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    }
}

LRESULT CALLBACK ParentSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
    LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    switch (message) {
    case kParentDeferredRedrawMessage:
        RestartButtonRedraws(hwnd);
        return 0;
    case WM_SIZE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
            PostMessageW(hwnd, kParentDeferredRedrawMessage, 0, 0);
        }
        return result;
    }
    case WM_WINDOWPOSCHANGED: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        const auto* position = reinterpret_cast<const WINDOWPOS*>(lParam);
        if (position && (position->flags & SWP_SHOWWINDOW)) {
            PostMessageW(hwnd, kParentDeferredRedrawMessage, 0, 0);
        }
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ParentSubclassProc, kParentSubclassId);
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

COLORREF Blend(COLORREF color, COLORREF target, int targetPercent)
{
    const int sourcePercent = 100 - targetPercent;
    return RGB(
        (GetRValue(color) * sourcePercent + GetRValue(target) * targetPercent) / 100,
        (GetGValue(color) * sourcePercent + GetGValue(target) * targetPercent) / 100,
        (GetBValue(color) * sourcePercent + GetBValue(target) * targetPercent) / 100);
}

void FillGradient(HDC dc, const RECT& rect, COLORREF top, COLORREF bottom)
{
    TRIVERTEX vertices[2] = {
        { rect.left, rect.top, static_cast<COLOR16>(GetRValue(top) << 8), static_cast<COLOR16>(GetGValue(top) << 8), static_cast<COLOR16>(GetBValue(top) << 8), 0xffff },
        { rect.right, rect.bottom, static_cast<COLOR16>(GetRValue(bottom) << 8), static_cast<COLOR16>(GetGValue(bottom) << 8), static_cast<COLOR16>(GetBValue(bottom) << 8), 0xffff }
    };
    GRADIENT_RECT gradient = { 0, 1 };
    GradientFill(dc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
}

void PaintParentBackground(HWND hwnd, HDC dc, const RECT& client)
{
    HWND parent = GetParent(hwnd);
    HBRUSH brush = nullptr;
    if (parent) {
        brush = reinterpret_cast<HBRUSH>(SendMessage(parent, WM_CTLCOLORBTN,
            reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(hwnd)));
    }
    if (!brush) brush = GetSysColorBrush(COLOR_BTNFACE);
    FillRect(dc, &client, brush);

    // Let custom Clarion backgrounds paint the portion behind this control.
    // PRF_CHILDREN is intentionally omitted to avoid painting/recursing into
    // the button itself.
    if (parent) {
        POINT origin{ 0, 0 };
        MapWindowPoints(hwnd, parent, &origin, 1);
        const int saved = SaveDC(dc);
        SetViewportOrgEx(dc, -origin.x, -origin.y, nullptr);
        SendMessage(parent, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc),
            PRF_CLIENT | PRF_ERASEBKGND);
        RestoreDC(dc, saved);
    }
}

void PaintButton(HWND hwnd, HDC target, ButtonData& data)
{
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    const bool enabled = IsWindowEnabled(hwnd) != FALSE;
    const LRESULT state = SendMessage(hwnd, BM_GETSTATE, 0, 0);
    const bool pressed = enabled && ((state & BST_PUSHED) != 0);
    const bool focused = enabled && (GetFocus() == hwnd || (state & BST_FOCUS) != 0);
    const bool selectedChoice = data.pushLike &&
        SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;

    COLORREF top = data.normalTop;
    COLORREF bottom = data.normalBottom;
    if (pressed) { top = data.pressedTop; bottom = data.pressedBottom; }
    else if (selectedChoice) { top = data.hoverTop; bottom = data.hoverBottom; }
    else if (data.tracking && enabled) { top = data.hoverTop; bottom = data.hoverBottom; }
    else if (focused && enabled) {
        top = Blend(top, data.hoverTop, 12);
        bottom = Blend(bottom, data.hoverBottom, 12);
    }
    if (!enabled) { top = data.disabledTop; bottom = data.disabledBottom; }

    PaintParentBackground(hwnd, dc, client);

    // Clarion FLAT CHECK/RADIO controls can supply their own rectangular
    // background color (and legacy cover panels may sit directly beneath).
    // Painting rounded corners exposes those pixels as a pale rectangular
    // halo. Keep push-like choices flat and fully opaque.
    const int radius = data.pushLike ? 0 :
        std::clamp(data.radius, 0, std::min(width, height) / 2);
    HRGN clip = radius > 0
        ? CreateRoundRectRgn(client.left, client.top, client.right + 1,
            client.bottom + 1, radius * 2 + 1, radius * 2 + 1)
        : CreateRectRgn(client.left, client.top, client.right, client.bottom);
    SelectClipRgn(dc, clip);
    if (data.gradient) FillGradient(dc, client, top, bottom);
    else {
        HBRUSH fill = CreateSolidBrush(top);
        FillRect(dc, &client, fill);
        DeleteObject(fill);
    }
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);

    if (data.borderWidth > 0) {
        const COLORREF outlineColor = !enabled
            ? data.disabledBorder
            : focused
            ? Blend(data.border, data.hoverTop, 40)
            : data.border;
        const int outlineWidth = data.borderWidth;
        HPEN pen = CreatePen(PS_SOLID, outlineWidth, outlineColor);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        if (radius > 0) {
            RoundRect(dc, client.left, client.top, client.right, client.bottom,
                radius * 2 + 1, radius * 2 + 1);
        } else {
            Rectangle(dc, client.left, client.top, client.right, client.bottom);
        }
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    wchar_t text[512]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    HFONT font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled
        ? ((selectedChoice || (data.tracking && !pressed)) ? data.hoverText : data.text)
        : data.disabledText);

    RECT content = client;
    InflateRect(&content, -8, -3);
    SIZE textSize{};
    const bool multilineText = std::wcschr(text, L'\r') != nullptr ||
        std::wcschr(text, L'\n') != nullptr;
    if (multilineText) {
        RECT measured{ 0, 0, 0, 0 };
        DrawTextW(dc, text, -1, &measured,
            DT_LEFT | DT_CALCRECT);
        textSize.cx = measured.right - measured.left;
        textSize.cy = measured.bottom - measured.top;
    } else {
        GetTextExtentPoint32W(dc, text, lstrlenW(text), &textSize);
    }
    int imageWidth = 0;
    int imageHeight = 0;
    if (data.imageType == IMAGE_ICON && data.icon) {
        const int maximumHeight = std::max(1, height - 8);
        const int requestedWidth = data.iconWidth > 1
            ? data.iconWidth
            : std::clamp(height - 12, 16, 32);
        imageWidth = imageHeight =
            std::max(1, std::min(requestedWidth, maximumHeight));
    } else if (data.imageType == IMAGE_BITMAP && data.bitmap) {
        BITMAP details{};
        if (GetObject(data.bitmap, sizeof(details), &details)) {
            const int maximumHeight = std::max(1, height - 8);
            imageHeight = std::min(static_cast<int>(details.bmHeight), maximumHeight);
            imageWidth = details.bmHeight > 0
                ? std::max(1, static_cast<int>(
                    details.bmWidth * imageHeight / details.bmHeight)) : 0;
        }
    }
    const bool hasImage = imageWidth > 0 && imageHeight > 0;
    // Clarion frequently represents an icon-only BUTTON with a single-space
    // caption.  Treat whitespace-only captions as empty; otherwise the
    // renderer incorrectly lays the button out as a stacked icon+caption and
    // pushes the visible icon to the top of the button.
    bool hasText = false;
    for (const wchar_t* character = text; *character; ++character) {
        if (!iswspace(*character)) {
            hasText = true;
            break;
        }
    }
    const LONG_PTR horizontalStyle =
        data.originalStyle & (BS_LEFT | BS_RIGHT | BS_CENTER);
    const int horizontalAlignment = data.horizontalAlignmentSet
        ? data.horizontalAlignment
        : (horizontalStyle == BS_LEFT ? -1 :
            horizontalStyle == BS_RIGHT ? 1 : 0);
    // Match Clarion's designer semantics. Centered image/caption content is
    // vertical (image above caption); LEFT and RIGHT are horizontal and put
    // the image on the corresponding outer edge. Runtime caption changes do
    // not alter this orientation.
    const bool stackedContent =
        hasImage && hasText && horizontalAlignment == 0;
    const int groupWidth = stackedContent
        ? std::max(imageWidth, static_cast<int>(textSize.cx))
        : (hasText ? textSize.cx : 0) +
            (hasImage ? imageWidth + (hasText ? data.iconGap : 0) : 0);
    const int groupHeight = stackedContent
        ? imageHeight + data.iconGap + textSize.cy
        : std::max(imageHeight, static_cast<int>(textSize.cy));
    int x = content.left;
    if (horizontalAlignment > 0) {
        x = std::max(static_cast<int>(content.left),
            static_cast<int>(content.right) - groupWidth);
    } else if (horizontalAlignment == 0) {
        x += std::max(0,
            (static_cast<int>(content.right - content.left) - groupWidth) / 2);
    }
    const int offset = pressed ? 1 : 0;
    const LONG_PTR verticalStyle =
        data.originalStyle & (BS_TOP | BS_BOTTOM | BS_VCENTER);
    const int verticalAlignment = data.verticalAlignmentSet
        ? data.verticalAlignment
        : (verticalStyle == BS_TOP ? -1 : verticalStyle == BS_BOTTOM ? 1 : 0);
    int groupY = content.top;
    if (verticalAlignment > 0) {
        groupY = std::max(static_cast<int>(content.top),
            static_cast<int>(content.bottom) - groupHeight);
    } else if (verticalAlignment == 0) {
        const int freeHeight =
            static_cast<int>(content.bottom - content.top) - groupHeight;
        // When the remaining height is odd, put the extra pixel above the
        // content. This matches the optical centre of the rounded button face
        // instead of leaving every icon one pixel high.
        groupY += std::max(0, (freeHeight + 1) / 2);
    }
    const bool imageOnRight =
        !stackedContent && hasImage && hasText && horizontalAlignment > 0;
    int imageX = imageOnRight
        ? x + groupWidth - imageWidth + offset
        : x + offset;
    int imageY = stackedContent
        ? groupY + offset
        : (height - imageHeight + 1) / 2 + offset;
    if (stackedContent) {
        imageX += std::max(0, (groupWidth - imageWidth) / 2);
    } else if (verticalAlignment < 0) {
        imageY = content.top + offset;
    } else if (verticalAlignment > 0) {
        imageY = std::max(static_cast<int>(content.top),
            static_cast<int>(content.bottom) - imageHeight) + offset;
    }
    if (hasImage) {
        if (data.imageType == IMAGE_ICON && data.icon) {
            if (enabled) {
                DrawIconEx(dc, imageX, imageY, data.icon,
                    imageWidth, imageHeight, 0, nullptr, DI_NORMAL);
                if (g_theme == CHBUTTON_THEME_DARK) {
                    DrawDarkIconAdaptation(dc, imageX, imageY, imageWidth,
                        imageHeight, data.icon);
                }
            } else {
                DrawStateW(dc, nullptr, nullptr,
                    reinterpret_cast<LPARAM>(data.icon), 0,
                    imageX, imageY, imageWidth, imageHeight,
                    DST_ICON | DSS_DISABLED);
            }
        } else if (data.imageType == IMAGE_BITMAP && data.bitmap) {
            if (enabled) {
                HDC imageDc = CreateCompatibleDC(dc);
                HGDIOBJ oldImage = SelectObject(imageDc, data.bitmap);
                BITMAP details{};
                GetObject(data.bitmap, sizeof(details), &details);
                SetStretchBltMode(dc, HALFTONE);
                StretchBlt(dc, imageX, imageY, imageWidth, imageHeight,
                    imageDc, 0, 0, details.bmWidth, details.bmHeight, SRCCOPY);
                SelectObject(imageDc, oldImage);
                DeleteDC(imageDc);
            } else {
                DrawStateW(dc, nullptr, nullptr,
                    reinterpret_cast<LPARAM>(data.bitmap), 0,
                    imageX, imageY, imageWidth, imageHeight,
                    DST_BITMAP | DSS_DISABLED);
            }
        }
    }
    RECT textRect{};
    if (stackedContent) {
        textRect = RECT{ x + offset, imageY + imageHeight + data.iconGap,
            x + groupWidth + offset, content.bottom + offset };
    } else if (imageOnRight) {
        textRect = RECT{ x + offset, content.top + offset,
            imageX - data.iconGap, content.bottom + offset };
    } else {
        const int textLeft = x +
            (hasImage ? imageWidth + (hasText ? data.iconGap : 0) : 0);
        textRect = RECT{ textLeft + offset, content.top + offset,
            x + groupWidth + offset, content.bottom + offset };
    }
    if (hasText) {
        UINT textFlags = stackedContent
            ? DT_CENTER
            : (horizontalAlignment > 0 ? DT_RIGHT : DT_LEFT);
        if (multilineText) {
            if (stackedContent) {
                // The group itself is already vertically positioned.
            } else if (verticalAlignment > 0) {
                textRect.top = std::max<LONG>(textRect.top,
                    textRect.bottom - static_cast<LONG>(textSize.cy));
            } else if (verticalAlignment == 0) {
                textRect.top += std::max<LONG>(0,
                    (textRect.bottom - textRect.top -
                        static_cast<LONG>(textSize.cy)) / 2);
            }
        } else {
            textFlags |= DT_SINGLELINE | DT_END_ELLIPSIS;
            if (verticalAlignment < 0) textFlags |= DT_TOP;
            else if (verticalAlignment > 0) textFlags |= DT_BOTTOM;
            else textFlags |= DT_VCENTER;
        }
        DrawTextW(dc, text, -1, &textRect,
            textFlags);
    }
    SelectObject(dc, oldFont);

    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void PaintChoiceControl(HWND hwnd, HDC target, ButtonData& data)
{
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    PaintParentBackground(hwnd, dc, client);

    const bool enabled = IsWindowEnabled(hwnd) != FALSE;
    const bool focused = enabled && GetFocus() == hwnd;
    const bool checked = SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const COLORREF accent = g_theme == CHBUTTON_THEME_LIGHT
        ? RGB(20, 109, 218) : RGB(48, 139, 249);
    const COLORREF outline = g_theme == CHBUTTON_THEME_LIGHT
        ? RGB(148, 163, 181) : RGB(73, 94, 114);
    const COLORREF hoverOutline = (data.tracking || focused) && enabled
        ? accent : outline;
    const COLORREF textColor = enabled
        ? (g_theme == CHBUTTON_THEME_LIGHT ? RGB(28, 39, 52) : RGB(226, 233, 241))
        : (g_theme == CHBUTTON_THEME_LIGHT ? RGB(145, 154, 165) : RGB(91, 107, 123));

    HFONT font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, textColor);

    wchar_t text[512]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    // Captionless Clarion CHECK controls are commonly declared as a compact
    // square. Preserve that complete designer rectangle instead of applying
    // the captioned-control padding and shrinking the indicator inside it.
    const bool hasCaption = text[0] != 0;
    const int glyphSize = hasCaption
        ? std::clamp(height - 6, 7, 16)
        : std::max(1, std::min(width, height));
    const int glyphLeft = hasCaption ? 2 : std::max(0, (width - glyphSize) / 2);
    const int glyphTop = std::max(0, (height - glyphSize) / 2);
    RECT glyph{ glyphLeft, glyphTop, glyphLeft + glyphSize, glyphTop + glyphSize };

    HPEN pen = CreatePen(PS_SOLID, 1, hoverOutline);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HBRUSH fill = CreateSolidBrush(g_theme == CHBUTTON_THEME_LIGHT
        ? RGB(255, 255, 255) : RGB(10, 18, 27));
    HGDIOBJ oldBrush = SelectObject(dc, fill);

    if (data.kind == ControlKind::RadioButton) {
        Ellipse(dc, glyph.left, glyph.top, glyph.right, glyph.bottom);
        if (checked) {
            const int inset = std::max(3, glyphSize / 4);
            HBRUSH dot = CreateSolidBrush(enabled ? accent : textColor);
            SelectObject(dc, dot);
            SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, glyph.left + inset, glyph.top + inset,
                glyph.right - inset, glyph.bottom - inset);
            SelectObject(dc, fill);
            SelectObject(dc, pen);
            DeleteObject(dot);
        }
    } else {
        HBRUSH checkedFill = nullptr;
        if (checked) {
            checkedFill = CreateSolidBrush(enabled ? accent : textColor);
            SelectObject(dc, checkedFill);
        }
        RoundRect(dc, glyph.left, glyph.top, glyph.right, glyph.bottom, 3, 3);
        if (checked) {
            const int checkWidth = glyphSize >= 11 ? 2 : 1;
            const int edge = std::max(1, glyphSize / 5);
            HPEN checkPen = CreatePen(PS_SOLID, checkWidth, RGB(255, 255, 255));
            SelectObject(dc, checkPen);
            MoveToEx(dc, glyph.left + edge,
                glyph.top + glyphSize / 2, nullptr);
            LineTo(dc, glyph.left + glyphSize / 2 - 1,
                glyph.bottom - edge - 1);
            LineTo(dc, glyph.right - edge - 1,
                glyph.top + edge);
            SelectObject(dc, pen);
            DeleteObject(checkPen);
        }
        if (checkedFill) {
            SelectObject(dc, fill);
            DeleteObject(checkedFill);
        }
    }

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(fill);
    DeleteObject(pen);

    if (hasCaption) {
        RECT textRect{ glyph.right + 7, 0, client.right, client.bottom };
        DrawTextW(dc, text, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(dc, oldFont);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void PaintGroupBox(HWND hwnd, HDC target)
{
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    PaintParentBackground(hwnd, dc, client);

    HFONT font = reinterpret_cast<HFONT>(SendMessage(hwnd, WM_GETFONT, 0, 0));
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    const bool enabled = IsWindowEnabled(hwnd) != FALSE;
    SetTextColor(dc, enabled
        ? (g_theme == CHBUTTON_THEME_LIGHT ? RGB(47, 61, 77) : RGB(184, 198, 212))
        : (g_theme == CHBUTTON_THEME_LIGHT ? RGB(145, 154, 165) : RGB(91, 107, 123)));

    wchar_t text[512]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    SIZE extent{};
    GetTextExtentPoint32W(dc, text, lstrlenW(text), &extent);
    const int lineY = std::max(7, static_cast<int>(extent.cy) / 2 + 1);
    const COLORREF border = g_theme == CHBUTTON_THEME_LIGHT
        ? RGB(205, 214, 224) : RGB(43, 60, 76);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    MoveToEx(dc, 0, lineY, nullptr);
    LineTo(dc, text[0] ? 8 : client.right, lineY);
    if (text[0]) {
        MoveToEx(dc, 14 + extent.cx, lineY, nullptr);
        LineTo(dc, client.right - 1, lineY);
    }
    LineTo(dc, client.right - 1, client.bottom - 1);
    LineTo(dc, 0, client.bottom - 1);
    LineTo(dc, 0, lineY);

    RECT textRect{ 10, 0, std::min(client.right, 12 + extent.cx), extent.cy + 2 };
    DrawTextW(dc, text, -1, &textRect, DT_LEFT | DT_SINGLELINE);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldFont);
    DeleteObject(pen);
    BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

LRESULT CALLBACK ButtonSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    auto* data = reinterpret_cast<ButtonData*>(reference);
    switch (message) {
    case kDeferredRedrawMessage:
        RedrawWindow(hwnd, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
        return 0;
    case WM_TIMER:
        if (wParam == kStartupRedrawTimer) {
            if (IsWindowVisible(hwnd)) {
                RedrawWindow(hwnd, nullptr, nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
            }
            if (++data->startupRedrawTicks >= 12) {
                KillTimer(hwnd, kStartupRedrawTimer);
            }
            return 0;
        }
        break;
    case WM_SHOWWINDOW:
        if (wParam) {
            data->startupRedrawTicks = 0;
            SetTimer(hwnd, kStartupRedrawTimer, 50, nullptr);
        }
        break;
    case WM_MOUSEMOVE:
        if (!data->tracking) {
            data->tracking = true;
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        data->tracking = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case BM_SETSTATE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_SIZE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_ENABLE: {
        if (!wParam) data->tracking = false;
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case BM_SETIMAGE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        CaptureNativeImage(hwnd, *data);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case BM_SETSTYLE: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        data->originalStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_STYLECHANGED: {
        const STYLESTRUCT* changed =
            reinterpret_cast<const STYLESTRUCT*>(lParam);
        const LONG_PTR oldStyle = changed
            ? static_cast<LONG_PTR>(changed->styleOld)
            : data->originalStyle;
        const LONG_PTR newStyle = changed
            ? static_cast<LONG_PTR>(changed->styleNew)
            : GetWindowLongPtr(hwnd, GWL_STYLE);
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        if (wParam == GWL_STYLE) {
            // Clarion implements runtime properties such as PROP:Left,
            // PROP:Center and PROP:Right by changing the native style.
            data->originalStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
            const LONG_PTR horizontalMask = BS_LEFT | BS_RIGHT | BS_CENTER;
            if ((oldStyle & horizontalMask) != (newStyle & horizontalMask)) {
                // A runtime Clarion property now owns horizontal alignment;
                // release the designer-time override installed at attachment.
                data->horizontalAlignment = 0;
                data->horizontalAlignmentSet = false;
            }
            // Keep an explicit vertical alignment supplied by the Clarion
            // extension. Clarion can reapply BS_TOP while updating unrelated
            // button styles after attachment; allowing that transient native
            // style to clear our override places every icon at the top edge.
            // Controls without an explicit override continue to follow their
            // native BS_TOP/BS_BOTTOM/BS_VCENTER style in PaintButton.
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return result;
    }
    case WM_SETTEXT: {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NCCALCSIZE:
        // Keep the complete button HWND as client area. Some Clarion button
        // classes reserve a rectangular default/focus frame here.
        if (data->kind != ControlKind::GroupBox) return 0;
        break;
    case WM_NCPAINT:
        // The renderer owns the complete visual, including border and focus.
        if (data->kind != ControlKind::GroupBox) return 0;
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        if (data->kind == ControlKind::PushButton || data->pushLike) PaintButton(hwnd, dc, *data);
        else if (data->kind == ControlKind::GroupBox) PaintGroupBox(hwnd, dc);
        else PaintChoiceControl(hwnd, dc, *data);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_PRINTCLIENT:
        if (data->kind == ControlKind::PushButton || data->pushLike) PaintButton(hwnd, reinterpret_cast<HDC>(wParam), *data);
        else if (data->kind == ControlKind::GroupBox) PaintGroupBox(hwnd, reinterpret_cast<HDC>(wParam));
        else PaintChoiceControl(hwnd, reinterpret_cast<HDC>(wParam), *data);
        return 0;
    case WM_NCDESTROY:
        KillTimer(hwnd, kStartupRedrawTimer);
        RemoveWindowSubclass(hwnd, ButtonSubclassProc, kSubclassId);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_buttons.erase(hwnd);
        }
        delete data;
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

ButtonData* FindData(HWND hwnd)
{
    DWORD_PTR reference = 0;
    if (!GetWindowSubclass(hwnd, ButtonSubclassProc, kSubclassId, &reference)) return nullptr;
    return reinterpret_cast<ButtonData*>(reference);
}

bool ApplyRole(ButtonData& data, int role, int theme)
{
    data.gradient = true;
    data.borderWidth = 1;

    if (theme == CHBUTTON_THEME_LIGHT) {
        data.text = RGB(28, 39, 52);
        data.hoverText = RGB(255, 255, 255);
        data.disabledTop = RGB(238, 241, 245);
        data.disabledBottom = RGB(225, 230, 236);
        data.disabledText = RGB(137, 147, 158);
        data.disabledBorder = RGB(197, 205, 214);

        switch (role) {
        case CHBUTTON_ROLE_STANDARD:
            data.normalTop = RGB(255, 255, 255);
            data.normalBottom = RGB(235, 239, 244);
            data.hoverTop = RGB(71, 158, 255);
            data.hoverBottom = RGB(22, 105, 211);
            data.pressedTop = RGB(213, 225, 239);
            data.pressedBottom = RGB(195, 212, 231);
            data.border = RGB(169, 181, 194);
            data.radius = 5;
            break;
        case CHBUTTON_ROLE_PRIMARY:
        case CHBUTTON_ROLE_SELECTED:
            data.normalTop = RGB(42, 137, 244);
            data.normalBottom = RGB(10, 91, 194);
            data.hoverTop = RGB(67, 155, 255);
            data.hoverBottom = RGB(20, 109, 218);
            data.pressedTop = RGB(14, 98, 204);
            data.pressedBottom = RGB(7, 71, 158);
            data.text = RGB(255, 255, 255);
            data.border = RGB(20, 109, 218);
            data.radius = role == CHBUTTON_ROLE_SELECTED ? 4 : 5;
            break;
        case CHBUTTON_ROLE_DESTRUCTIVE:
            data.normalTop = RGB(239, 71, 75);
            data.normalBottom = RGB(190, 30, 36);
            data.hoverTop = RGB(255, 91, 94);
            data.hoverBottom = RGB(216, 42, 48);
            data.pressedTop = RGB(196, 31, 38);
            data.pressedBottom = RGB(150, 19, 25);
            data.text = RGB(255, 255, 255);
            data.border = RGB(207, 42, 48);
            data.radius = 5;
            break;
        case CHBUTTON_ROLE_SUCCESS:
            data.normalTop = RGB(42, 181, 104);
            data.normalBottom = RGB(18, 130, 70);
            data.hoverTop = RGB(65, 204, 126);
            data.hoverBottom = RGB(24, 151, 82);
            data.pressedTop = RGB(19, 138, 73);
            data.pressedBottom = RGB(11, 101, 51);
            data.text = RGB(255, 255, 255);
            data.border = RGB(24, 151, 82);
            data.radius = 5;
            break;
        case CHBUTTON_ROLE_TOOLBAR:
            data.normalTop = RGB(248, 250, 252);
            data.normalBottom = RGB(228, 233, 239);
            data.hoverTop = RGB(71, 158, 255);
            data.hoverBottom = RGB(22, 105, 211);
            data.pressedTop = RGB(207, 220, 235);
            data.pressedBottom = RGB(188, 205, 224);
            data.border = RGB(176, 187, 199);
            data.radius = 4;
            break;
        default:
            return false;
        }
        data.role = role;
        return true;
    }

    data.text = RGB(245, 248, 252);
    data.hoverText = RGB(255, 255, 255);
    data.disabledTop = RGB(28, 38, 50);
    data.disabledBottom = RGB(18, 26, 35);
    data.disabledText = RGB(105, 119, 134);
    data.disabledBorder = RGB(39, 53, 68);

    switch (role) {
    case CHBUTTON_ROLE_STANDARD:
        data.normalTop = RGB(38, 51, 66);
        data.normalBottom = RGB(22, 32, 43);
        data.hoverTop = RGB(48, 139, 249);
        data.hoverBottom = RGB(19, 91, 203);
        data.pressedTop = RGB(20, 31, 43);
        data.pressedBottom = RGB(13, 22, 31);
        data.border = RGB(49, 69, 88);
        data.radius = 5;
        break;
    case CHBUTTON_ROLE_PRIMARY:
        data.normalTop = RGB(28, 122, 235);
        data.normalBottom = RGB(8, 77, 174);
        data.hoverTop = RGB(52, 145, 255);
        data.hoverBottom = RGB(14, 94, 206);
        data.pressedTop = RGB(8, 82, 183);
        data.pressedBottom = RGB(5, 55, 130);
        data.border = RGB(36, 137, 255);
        data.radius = 5;
        break;
    case CHBUTTON_ROLE_DESTRUCTIVE:
        data.normalTop = RGB(236, 55, 59);
        data.normalBottom = RGB(180, 22, 29);
        data.hoverTop = RGB(255, 76, 79);
        data.hoverBottom = RGB(207, 30, 37);
        data.pressedTop = RGB(186, 24, 31);
        data.pressedBottom = RGB(132, 13, 19);
        data.border = RGB(247, 71, 75);
        data.radius = 5;
        break;
    case CHBUTTON_ROLE_SUCCESS:
        data.normalTop = RGB(34, 177, 95);
        data.normalBottom = RGB(12, 112, 56);
        data.hoverTop = RGB(54, 207, 116);
        data.hoverBottom = RGB(17, 139, 70);
        data.pressedTop = RGB(14, 126, 63);
        data.pressedBottom = RGB(7, 83, 40);
        data.text = RGB(255, 255, 255);
        data.border = RGB(45, 201, 108);
        data.radius = 5;
        break;
    case CHBUTTON_ROLE_TOOLBAR:
        data.normalTop = RGB(27, 40, 53);
        data.normalBottom = RGB(15, 25, 35);
        data.hoverTop = RGB(43, 126, 225);
        data.hoverBottom = RGB(18, 76, 169);
        data.pressedTop = RGB(13, 24, 34);
        data.pressedBottom = RGB(8, 16, 24);
        data.border = RGB(45, 64, 82);
        data.radius = 4;
        break;
    case CHBUTTON_ROLE_SELECTED:
        data.normalTop = RGB(20, 108, 220);
        data.normalBottom = RGB(5, 66, 153);
        data.hoverTop = RGB(38, 132, 247);
        data.hoverBottom = RGB(8, 84, 187);
        data.pressedTop = RGB(7, 75, 165);
        data.pressedBottom = RGB(3, 49, 116);
        data.border = RGB(31, 132, 255);
        data.radius = 4;
        break;
    default:
        return false;
    }
    data.role = role;
    return true;
}

struct AttachAllContext
{
    int attached = 0;
};

BOOL CALLBACK AttachAllCallback(HWND child, LPARAM parameter)
{
    auto* context = reinterpret_cast<AttachAllContext*>(parameter);
    const LRESULT dialogCode = SendMessage(child, WM_GETDLGCODE, 0, 0);
    wchar_t className[128]{};
    GetClassNameW(child, className, static_cast<int>(std::size(className)));
    CharLowerBuffW(className, lstrlenW(className));
    const bool buttonClass = std::wcsstr(className, L"button") != nullptr;

    // Some Clarion button classes return no DLGC_BUTTON flags while disabled.
    // Their class name still identifies them without enabling the control.
    if (((dialogCode & DLGC_BUTTON) != 0 || buttonClass) && CHButton_Attach(child)) {
        ++context->attached;
    }
    return TRUE;
}
}

BOOL __stdcall CHButton_Attach(HWND button)
{
    if (!IsWindow(button)) return FALSE;
    if (FindData(button)) return TRUE;

    // A native BS_DEFPUSHBUTTON paints a square outer default frame. Clarion's
    // dialog manager still owns the default control ID and Enter-key action, so
    // normalize only the visual button type before installing our renderer.
    LONG_PTR style = GetWindowLongPtr(button, GWL_STYLE);
    const LONG_PTR buttonType = style & BS_TYPEMASK;
    ControlKind kind = ControlKind::PushButton;
    switch (buttonType) {
    case BS_PUSHBUTTON:
    case BS_DEFPUSHBUTTON:
    case BS_OWNERDRAW:
        kind = ControlKind::PushButton;
        break;
    case BS_RADIOBUTTON:
    case BS_AUTORADIOBUTTON:
        kind = ControlKind::RadioButton;
        break;
    case BS_CHECKBOX:
    case BS_AUTOCHECKBOX:
    case BS_3STATE:
    case BS_AUTO3STATE:
        kind = ControlKind::CheckBox;
        break;
    case BS_GROUPBOX:
        kind = ControlKind::GroupBox;
        break;
    default:
        return FALSE;
    }
    const bool pushLikeChoice = (style & BS_PUSHLIKE) != 0 &&
        (kind == ControlKind::RadioButton || kind == ControlKind::CheckBox);
    if (buttonType == BS_DEFPUSHBUTTON) {
        style = (style & ~static_cast<LONG_PTR>(BS_TYPEMASK)) | BS_PUSHBUTTON;
    }
    const bool ownsCompleteFrame = kind != ControlKind::GroupBox;
    if (ownsCompleteFrame) {
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
    }
    SetWindowLongPtr(button, GWL_STYLE, style);

    LONG_PTR extendedStyle = GetWindowLongPtr(button, GWL_EXSTYLE);
    if (ownsCompleteFrame) {
        extendedStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
    }
    SetWindowLongPtr(button, GWL_EXSTYLE, extendedStyle);
    auto* data = new (std::nothrow) ButtonData();
    if (!data) return FALSE;
    data->kind = kind;
    data->pushLike = pushLikeChoice;
    data->originalStyle = style;
    CaptureNativeImage(button, *data);
    ApplyRole(*data, CHBUTTON_ROLE_STANDARD, g_theme);
    if (!SetWindowSubclass(button, ButtonSubclassProc, kSubclassId, reinterpret_cast<DWORD_PTR>(data))) {
        delete data;
        return FALSE;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_buttons.insert(button);
    }
    if (HWND parent = GetParent(button)) {
        SetWindowSubclass(parent, ParentSubclassProc, kParentSubclassId, 0);
    }
    // Recalculate the frame only after our subclass is active so Clarion
    // cannot reserve or repaint its old square non-client focus border.
    SetWindowPos(button, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (kind != ControlKind::GroupBox) {
        SendMessage(button, WM_UPDATEUISTATE,
            MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
    }
    SetWindowRgn(button, nullptr, TRUE);
    RedrawWindow(button, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
    PostMessageW(button, kDeferredRedrawMessage, 0, 0);
    SetTimer(button, kStartupRedrawTimer, 50, nullptr);
    return TRUE;
}

int __stdcall CHButton_AttachAll(HWND parent)
{
    if (!IsWindow(parent)) return 0;
    AttachAllContext context;
    EnumChildWindows(parent, AttachAllCallback, reinterpret_cast<LPARAM>(&context));
    return context.attached;
}

BOOL __stdcall CHButton_Detach(HWND button)
{
    ButtonData* data = FindData(button);
    if (!data) return FALSE;
    RemoveWindowSubclass(button, ButtonSubclassProc, kSubclassId);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_buttons.erase(button);
    }
    delete data;
    SetWindowRgn(button, nullptr, TRUE);
    InvalidateRect(button, nullptr, TRUE);
    return TRUE;
}

void __stdcall CHButton_DetachAll(void)
{
    std::unordered_set<HWND> buttons;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        buttons = g_buttons;
    }
    for (HWND button : buttons) if (IsWindow(button)) CHButton_Detach(button);
}

BOOL __stdcall CHButton_SetColors(HWND button, COLORREF normalTop, COLORREF normalBottom, COLORREF hoverTop, COLORREF hoverBottom, COLORREF pressedTop, COLORREF pressedBottom, COLORREF textColor, COLORREF borderColor)
{
    ButtonData* data = FindData(button);
    if (!data) return FALSE;
    data->normalTop = normalTop; data->normalBottom = normalBottom;
    data->hoverTop = hoverTop; data->hoverBottom = hoverBottom;
    data->pressedTop = pressedTop; data->pressedBottom = pressedBottom;
    data->text = textColor; data->hoverText = textColor; data->border = borderColor;
    data->role = -1; // Preserve explicit per-button colors across theme changes.
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetDisabledColors(HWND button, COLORREF topColor, COLORREF bottomColor, COLORREF textColor, COLORREF borderColor)
{
    ButtonData* data = FindData(button);
    if (!data) return FALSE;
    data->disabledTop = topColor;
    data->disabledBottom = bottomColor;
    data->disabledText = textColor;
    data->disabledBorder = borderColor;
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetRole(HWND button, int role)
{
    ButtonData* data = FindData(button);
    if (!data || !ApplyRole(*data, role, g_theme)) return FALSE;
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetTheme(int theme)
{
    if (theme != CHBUTTON_THEME_DARK && theme != CHBUTTON_THEME_LIGHT) return FALSE;
    g_theme = theme;

    std::unordered_set<HWND> buttons;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        buttons = g_buttons;
    }
    for (HWND button : buttons) {
        if (!IsWindow(button)) continue;
        ButtonData* data = FindData(button);
        if (data && data->role >= CHBUTTON_ROLE_STANDARD) {
            ApplyRole(*data, data->role, theme);
            RedrawWindow(button, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
            PostMessageW(button, kDeferredRedrawMessage, 0, 0);
        }
    }
    return TRUE;
}

BOOL __stdcall CHButton_SetMetrics(HWND button, int cornerRadius, int borderWidth, BOOL gradient)
{
    ButtonData* data = FindData(button);
    if (!data) return FALSE;
    data->radius = std::max(0, cornerRadius);
    data->borderWidth = std::clamp(borderWidth, 0, 8);
    data->gradient = gradient != FALSE;
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetIcon(HWND button, HICON icon, int iconWidth, int gap)
{
    ButtonData* data = FindData(button);
    if (!data) return FALSE;
    if (data->ownsIcon && data->icon) DestroyIcon(data->icon);
    data->icon = icon;
    data->bitmap = nullptr;
    data->imageType = icon ? IMAGE_ICON : 0;
    data->ownsIcon = false;
    data->iconWidth = std::max(1, iconWidth);
    data->iconGap = std::max(0, gap);
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetIconFile(HWND button, const char* iconName,
    int iconWidth, int gap)
{
    ButtonData* data = FindData(button);
    if (!data || !iconName || !*iconName) return FALSE;

    HICON icon = nullptr;
    bool ownsIcon = false;
    const auto* clarionIcon =
        reinterpret_cast<const unsigned char*>(iconName);
    if (clarionIcon[0] == 0xff && clarionIcon[1] == 0x01 &&
        clarionIcon[2] >= 0x01 && clarionIcon[2] <= 0x05 &&
        clarionIcon[3] == 0x7f) {
        icon = LoadIconA(nullptr,
            MAKEINTRESOURCEA(32512 + clarionIcon[2] - 1));
    }
    // Clarion's ICON:New is a proprietary token rather than a filename or
    // Win32 resource identifier. Use the corresponding Windows document icon.
    if (!icon && clarionIcon[0] == 0xff && clarionIcon[1] == 0x02 &&
        clarionIcon[2] == 0x06 && clarionIcon[3] == 0x7f) {
        SHSTOCKICONINFO stock{ sizeof(stock) };
        if (SUCCEEDED(SHGetStockIconInfo(SIID_DOCNOASSOC,
            SHGSI_ICON | SHGSI_SMALLICON, &stock))) {
            icon = stock.hIcon;
            ownsIcon = icon != nullptr;
        }
    }

    std::string name(iconName);
    while (!name.empty() && std::isspace(
        static_cast<unsigned char>(name.front()))) name.erase(name.begin());
    while (!name.empty() && std::isspace(
        static_cast<unsigned char>(name.back()))) name.pop_back();

    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    LPCSTR standard = nullptr;
    if (upper == "ICON:APPLICATION") standard = MAKEINTRESOURCEA(32512);
    else if (upper == "ICON:HAND" || upper == "ICON:ERROR") standard = MAKEINTRESOURCEA(32513);
    else if (upper == "ICON:QUESTION") standard = MAKEINTRESOURCEA(32514);
    else if (upper == "ICON:EXCLAMATION" || upper == "ICON:WARNING") standard = MAKEINTRESOURCEA(32515);
    else if (upper == "ICON:ASTERISK" || upper == "ICON:INFORMATION") standard = MAKEINTRESOURCEA(32516);
    else if (upper == "ICON:WINLOGO") standard = MAKEINTRESOURCEA(32517);
    else if (upper == "ICON:SHIELD") standard = MAKEINTRESOURCEA(32518);

    if (!icon && standard) {
        icon = LoadIconA(nullptr, standard);
    } else if (!icon) {
        char* end = nullptr;
        const unsigned long numeric = std::strtoul(name.c_str(), &end, 0);
        if (end && *end == 0 && numeric > 0 && numeric <= 0xffff) {
            icon = LoadIconA(nullptr, MAKEINTRESOURCEA(numeric));
        }
        if (!icon) {
            icon = reinterpret_cast<HICON>(LoadImageA(nullptr, name.c_str(),
                IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
            ownsIcon = icon != nullptr;
        }
        // Clarion accepts GIF and other raster formats for BUTTON ICON
        // attributes. Win32 LoadImage does not, so decode them with GDI+ and
        // convert the result to an HICON to preserve transparent pixels.
        if (!icon && upper.size() >= 4 &&
            (upper.compare(upper.size() - 4, 4, ".GIF") == 0 ||
             upper.compare(upper.size() - 4, 4, ".PNG") == 0 ||
             upper.compare(upper.size() - 4, 4, ".JPG") == 0 ||
             upper.compare(upper.size() - 4, 4, ".BMP") == 0)) {
            icon = LoadRasterImageAsIcon(name);
            ownsIcon = icon != nullptr;
        }
        // Clarion accepts PNG button images, while LoadImage only accepts
        // ICO/CUR for IMAGE_ICON. Prefer an adjacent same-name ICO when one is
        // available; this also preserves transparent multi-resolution artwork.
        if (!icon && upper.size() >= 4 &&
            upper.compare(upper.size() - 4, 4, ".PNG") == 0) {
            std::string icoName = name.substr(0, name.size() - 4) + ".ico";
            icon = reinterpret_cast<HICON>(LoadImageA(nullptr, icoName.c_str(),
                IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
            ownsIcon = icon != nullptr;
        }
    }
    if (!icon) return FALSE;

    if (data->ownsIcon && data->icon) DestroyIcon(data->icon);
    data->icon = icon;
    data->bitmap = nullptr;
    data->imageType = IMAGE_ICON;
    data->ownsIcon = ownsIcon;
    data->iconWidth = std::max(1, iconWidth);
    data->iconGap = std::max(0, gap);
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL __stdcall CHButton_SetContentAlignment(HWND button, int horizontal,
    int vertical)
{
    ButtonData* data = FindData(button);
    if (!data || horizontal < -1 || horizontal > 1 ||
        vertical < -1 || vertical > 1) return FALSE;
    data->horizontalAlignment = horizontal;
    data->verticalAlignment = vertical;
    data->horizontalAlignmentSet = true;
    data->verticalAlignmentSet = true;
    InvalidateRect(button, nullptr, FALSE);
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
