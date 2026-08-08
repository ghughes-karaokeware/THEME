#include "CHTheme.h"

#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <deque>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr DWORD kAbiVersion = 0x00010000;
constexpr DWORD kMaximumEntries = 512;
constexpr wchar_t kDialogClass[] = L"CHTheme.StructuredDialog";
constexpr int kCategoryListId = 100;
constexpr int kPageListId = 101;
constexpr int kBackButtonId = 102;
constexpr int kDetailButtonId = 103;
constexpr int kApplyButtonId = 104;
constexpr int kResetAllButtonId = 105;
constexpr int kFirstDynamicId = 1000;
constexpr wchar_t kHoverProperty[] = L"CHTheme.StructuredHover";
constexpr UINT kSetEntryValueMessage = WM_APP + 0x351;

struct Option { std::string value; std::wstring caption; };
struct Completion { DWORD instanceId; LONG result; };
struct Change { DWORD instanceId; DWORD entryId; };
struct Action { DWORD instanceId; DWORD entryId; };
struct SetEntryValueRequest { DWORD entryId; std::string value; };

struct RuntimeEntry
{
    CHUI_ENTRY_RECORD definition{};
    size_t sourceIndex = 0;
    std::string workingValue;
    std::string baselineValue;
    std::string cancelValue;
    std::string defaultWorkingValue;
    size_t pathIndex = static_cast<size_t>(-1);
    HWND control = nullptr;
    HWND valueLabel = nullptr;
    HWND pathDisplay = nullptr;
};

struct DialogData
{
    HWND window = nullptr;
    HWND owner = nullptr;
    HWND completionButton = nullptr;
    CHUI_ENTRY_RECORD* callerEntries = nullptr;
    CHUI_PATH_RECORD* callerPaths = nullptr;
    DWORD pathCount = 0;
    DWORD instanceId = 0;
    bool committed = false;
    bool rendering = false;
    bool detailSuppressed = false;
    DWORD selectedCategory = 0;
    DWORD selectedPage = 0;
    DWORD selectedDetail = 0;
    int hoveredCategory = -1;
    int hoveredPage = -1;
    std::wstring title;
    std::vector<RuntimeEntry> entries;
    std::unordered_map<DWORD, size_t> byId;
    std::unordered_map<int, size_t> byControlId;
    std::vector<HWND> dynamicWindows;
    HWND categories = nullptr;
    HWND pages = nullptr;
    HFONT font = nullptr;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH surfaceBrush = nullptr;
    HBRUSH navigationBrush = nullptr;
    HBRUSH inputBrush = nullptr;
    HBRUSH borderBrush = nullptr;
};

std::mutex g_mutex;
std::unordered_map<HWND, HWND> g_byOwner;
std::unordered_map<DWORD, HWND> g_byInstance;
std::unordered_map<HWND, std::deque<Completion>> g_completions;
std::unordered_map<HWND, std::deque<Change>> g_changes;
std::unordered_map<HWND, std::deque<Action>> g_actions;
std::atomic<DWORD> g_nextInstance{ 1 };

bool IsTerminated(const char* value, size_t capacity)
{
    return value && std::find(value, value + capacity, '\0') != value + capacity;
}

std::wstring Utf8ToWide(const char* value)
{
    if (!value || !*value) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value, -1, nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
        result.data(), count);
    result.pop_back();
    return result;
}

std::string WideToUtf8(const wchar_t* value)
{
    if (!value || !*value) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
        result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::vector<Option> ParseOptions(const char* definition)
{
    std::vector<Option> result;
    const std::string source = definition ? definition : "";
    size_t start = 0;
    while (start <= source.size()) {
        const size_t end = source.find('|', start);
        const std::string item = source.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) {
            const size_t separator = item.find('=');
            if (!separator || separator == std::string::npos ||
                separator + 1 >= item.size()) return {};
            const std::wstring caption = Utf8ToWide(item.substr(separator + 1).c_str());
            if (caption.empty()) return {};
            result.push_back({ item.substr(0, separator), caption });
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

bool IsValueType(DWORD type)
{
    return type >= CHUI_ENTRY && type <= CHUI_FOLDER;
}

bool IsEntryType(DWORD type)
{
    return IsValueType(type) || type == CHUI_ACTION;
}

LONG Validate(const CHUI_DIALOG_HEADER* header, const CHUI_ENTRY_RECORD* entries)
{
    if (!header || (!entries && header->entryCount)) return CHUI_ERROR_ARGUMENT;
    if (header->version != kAbiVersion) return CHUI_ERROR_VERSION;
    if (header->headerSize != sizeof(CHUI_DIALOG_HEADER)) return CHUI_ERROR_HEADER_SIZE;
    if (header->entrySize != sizeof(CHUI_ENTRY_RECORD)) return CHUI_ERROR_ENTRY_SIZE;
    if (!header->entryCount || header->entryCount > kMaximumEntries)
        return CHUI_ERROR_ENTRY_COUNT;
    if (!IsTerminated(header->title, sizeof(header->title))) return CHUI_ERROR_STRING;

    std::unordered_map<DWORD, DWORD> types;
    for (DWORD index = 0; index < header->entryCount; ++index) {
        const CHUI_ENTRY_RECORD& entry = entries[index];
        if (entry.type < CHUI_PANEL ||
            (entry.type > CHUI_SEPARATOR && !IsEntryType(entry.type)))
            return CHUI_ERROR_TYPE;
        if (!entry.id || types.find(entry.id) != types.end()) return CHUI_ERROR_ID;
        if (!IsTerminated(entry.caption, sizeof(entry.caption)) ||
            !IsTerminated(entry.value, sizeof(entry.value)) ||
            !IsTerminated(entry.defaultValue, sizeof(entry.defaultValue)) ||
            !IsTerminated(entry.dependencyValue, sizeof(entry.dependencyValue)) ||
            !IsTerminated(entry.options, sizeof(entry.options)) ||
            !IsTerminated(entry.helpText, sizeof(entry.helpText))) return CHUI_ERROR_STRING;
        if (entry.type == CHUI_DROPDOWN && ParseOptions(entry.options).empty())
            return CHUI_ERROR_OPTIONS;
        types.emplace(entry.id, entry.type);
    }
    for (DWORD index = 0; index < header->entryCount; ++index) {
        const CHUI_ENTRY_RECORD& entry = entries[index];
        if (!entry.parentId) {
            if (entry.type != CHUI_PANEL) return CHUI_ERROR_PARENT;
        } else {
            auto parent = types.find(entry.parentId);
            if (parent == types.end() ||
                (parent->second != CHUI_PANEL && parent->second != CHUI_GROUP))
                return CHUI_ERROR_PARENT;
        }
        if (entry.dependencyId && types.find(entry.dependencyId) == types.end())
            return CHUI_ERROR_PARENT;
        if (entry.dependencyOperator > CHUI_DEPEND_NOT_EQUAL)
            return CHUI_ERROR_ARGUMENT;
    }
    return CHUI_STATUS_OK;
}

LONG ValidatePaths(const CHUI_DIALOG_HEADER* header,
    const CHUI_ENTRY_RECORD* entries, const CHUI_PATH_RECORD* paths,
    DWORD pathCount)
{
    const LONG base = Validate(header, entries);
    if (base != CHUI_STATUS_OK) return base;
    DWORD required = 0;
    std::unordered_map<DWORD, DWORD> pathTypes;
    for (DWORD index = 0; index < header->entryCount; ++index) {
        if (entries[index].type == CHUI_FILE || entries[index].type == CHUI_FOLDER) {
            ++required;
            pathTypes.emplace(entries[index].id, entries[index].type);
        }
    }
    if (pathCount != required || (pathCount && !paths)) return CHUI_ERROR_PATH_COUNT;
    std::unordered_map<DWORD, bool> seen;
    for (DWORD index = 0; index < pathCount; ++index) {
        const auto target = pathTypes.find(paths[index].entryId);
        if (target == pathTypes.end() || seen.find(paths[index].entryId) != seen.end())
            return CHUI_ERROR_PATH_RECORD;
        if (!IsTerminated(paths[index].value, sizeof(paths[index].value)) ||
            !IsTerminated(paths[index].defaultValue,
                sizeof(paths[index].defaultValue))) return CHUI_ERROR_STRING;
        seen.emplace(paths[index].entryId, true);
    }
    return CHUI_STATUS_OK;
}

DialogData* GetData(HWND window)
{
    return reinterpret_cast<DialogData*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

RuntimeEntry* FindEntry(DialogData& data, DWORD id)
{
    auto found = data.byId.find(id);
    return found == data.byId.end() ? nullptr : &data.entries[found->second];
}

void DrawBuiltInIcon(HDC dc, const RECT& bounds, DWORD iconId, COLORREF color)
{
    if (!iconId || iconId > CHUI_ICON_INFORMATION) return;
    const int left = bounds.left + 2;
    const int top = bounds.top + 2;
    const int right = bounds.right - 2;
    const int bottom = bounds.bottom - 2;
    const int middleX = (left + right) / 2;
    const int middleY = (top + bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    switch (iconId) {
    case CHUI_ICON_AUDIO: {
        POINT speaker[] = { {left, middleY - 3}, {left + 4, middleY - 3},
            {middleX, top + 2}, {middleX, bottom - 2},
            {left + 4, middleY + 3}, {left, middleY + 3} };
        SelectObject(dc, brush);
        Polygon(dc, speaker, static_cast<int>(std::size(speaker)));
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, middleX - 2, top + 3, right - 1, bottom - 3,
            middleX + 1, top + 4, middleX + 1, bottom - 4);
        break;
    }
    case CHUI_ICON_KARAOKE:
    case CHUI_ICON_MICROPHONE:
        RoundRect(dc, middleX - 3, top, middleX + 4, middleY + 4, 5, 5);
        Arc(dc, middleX - 7, top + 3, middleX + 8, bottom - 3,
            left, middleY, right, middleY);
        MoveToEx(dc, middleX, middleY + 4, nullptr);
        LineTo(dc, middleX, bottom);
        MoveToEx(dc, middleX - 4, bottom, nullptr);
        LineTo(dc, middleX + 4, bottom);
        break;
    case CHUI_ICON_DISPLAY:
    case CHUI_ICON_DEVICE:
        Rectangle(dc, left, top + 1, right, bottom - 4);
        MoveToEx(dc, middleX, bottom - 4, nullptr);
        LineTo(dc, middleX, bottom);
        MoveToEx(dc, middleX - 5, bottom, nullptr);
        LineTo(dc, middleX + 5, bottom);
        break;
    case CHUI_ICON_SONG_QUEUE:
    case CHUI_ICON_SONGBOOK:
    case CHUI_ICON_FILE:
        Rectangle(dc, left + 2, top, right - 1, bottom);
        MoveToEx(dc, left + 5, top + 5, nullptr);
        LineTo(dc, right - 4, top + 5);
        MoveToEx(dc, left + 5, middleY, nullptr);
        LineTo(dc, right - 4, middleY);
        MoveToEx(dc, left + 5, bottom - 5, nullptr);
        LineTo(dc, right - 4, bottom - 5);
        break;
    case CHUI_ICON_AUTOMATION: {
        POINT bolt[] = { {middleX + 1, top}, {left + 3, middleY + 2},
            {middleX - 1, middleY + 2}, {middleX - 2, bottom},
            {right - 2, middleY - 2}, {middleX + 2, middleY - 2} };
        SelectObject(dc, brush);
        Polygon(dc, bolt, static_cast<int>(std::size(bolt)));
        break;
    }
    case CHUI_ICON_KEYBOARD:
        Rectangle(dc, left, top + 3, right, bottom - 2);
        for (int x = left + 4; x < right - 2; x += 4) {
            MoveToEx(dc, x, top + 6, nullptr);
            LineTo(dc, x, top + 9);
        }
        MoveToEx(dc, left + 4, bottom - 5, nullptr);
        LineTo(dc, right - 4, bottom - 5);
        break;
    case CHUI_ICON_PLAY: {
        POINT play[] = { {left + 3, top}, {right - 1, middleY},
            {left + 3, bottom} };
        SelectObject(dc, brush);
        Polygon(dc, play, static_cast<int>(std::size(play)));
        break;
    }
    case CHUI_ICON_STOP:
        SelectObject(dc, brush);
        Rectangle(dc, left + 2, top + 2, right - 2, bottom - 2);
        break;
    case CHUI_ICON_WARNING: {
        POINT warning[] = { {middleX, top}, {right, bottom}, {left, bottom} };
        Polygon(dc, warning, static_cast<int>(std::size(warning)));
        MoveToEx(dc, middleX, top + 5, nullptr);
        LineTo(dc, middleX, bottom - 5);
        SetPixel(dc, middleX, bottom - 2, color);
        break;
    }
    case CHUI_ICON_INFORMATION:
        Ellipse(dc, left, top, right, bottom);
        MoveToEx(dc, middleX, middleY - 1, nullptr);
        LineTo(dc, middleX, bottom - 4);
        SetPixel(dc, middleX, top + 4, color);
        break;
    case CHUI_ICON_APPEARANCE:
        Ellipse(dc, left, top, right, bottom);
        SelectObject(dc, brush);
        Ellipse(dc, left + 4, top + 4, left + 7, top + 7);
        Ellipse(dc, right - 7, top + 4, right - 4, top + 7);
        Ellipse(dc, left + 4, bottom - 7, left + 7, bottom - 4);
        break;
    default:
        Ellipse(dc, left + 2, top + 2, right - 2, bottom - 2);
        MoveToEx(dc, middleX, top, nullptr);
        LineTo(dc, middleX, top + 4);
        MoveToEx(dc, middleX, bottom - 4, nullptr);
        LineTo(dc, middleX, bottom);
        MoveToEx(dc, left, middleY, nullptr);
        LineTo(dc, left + 4, middleY);
        MoveToEx(dc, right - 4, middleY, nullptr);
        LineTo(dc, right, middleY);
        break;
    }

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawNavigationItem(DialogData& data, const DRAWITEMSTRUCT& item)
{
    if (item.itemID == static_cast<UINT>(-1)) return;
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const bool hovered = item.CtlID == kCategoryListId
        ? data.hoveredCategory == static_cast<int>(item.itemID)
        : data.hoveredPage == static_cast<int>(item.itemID);
    const COLORREF foreground = selected ? RGB(255, 255, 255) : RGB(218, 229, 239);
    HBRUSH brush = CreateSolidBrush(RGB(16, 31, 46));
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    if (selected || hovered) {
        RECT card = item.rcItem;
        InflateRect(&card, -3, -2);
        HBRUSH selectedBrush = CreateSolidBrush(selected
            ? RGB(9, 76, 164) : RGB(25, 48, 69));
        HPEN selectedPen = CreatePen(PS_SOLID, 1, selected
            ? RGB(25, 119, 229) : RGB(55, 82, 105));
        HGDIOBJ oldBrush = SelectObject(item.hDC, selectedBrush);
        HGDIOBJ oldPen = SelectObject(item.hDC, selectedPen);
        RoundRect(item.hDC, card.left, card.top, card.right, card.bottom, 7, 7);
        SelectObject(item.hDC, oldPen);
        SelectObject(item.hDC, oldBrush);
        DeleteObject(selectedPen);
        DeleteObject(selectedBrush);
        RECT shade = card;
        shade.left += 2;
        shade.right -= 2;
        shade.top += 2;
        shade.bottom = shade.top + 3;
        HBRUSH shadeBrush = CreateSolidBrush(selected
            ? RGB(18, 94, 190) : RGB(31, 59, 83));
        FillRect(item.hDC, &shade, shadeBrush);
        DeleteObject(shadeBrush);
    }

    RuntimeEntry* entry = FindEntry(data, static_cast<DWORD>(item.itemData));
    RECT iconRect = item.rcItem;
    iconRect.left += 10;
    iconRect.right = iconRect.left + 18;
    iconRect.top += (item.rcItem.bottom - item.rcItem.top - 18) / 2;
    iconRect.bottom = iconRect.top + 18;
    const DWORD iconId = entry ? entry->definition.iconId : CHUI_ICON_NONE;
    DrawBuiltInIcon(item.hDC, iconRect, iconId, foreground);

    wchar_t caption[256]{};
    SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID,
        reinterpret_cast<LPARAM>(caption));
    RECT textRect = item.rcItem;
    textRect.left += iconId ? 38 : 12;
    textRect.right -= 10;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, foreground);
    const std::wstring help = entry ? Utf8ToWide(entry->definition.helpText) : L"";
    if (help.empty()) {
        DrawTextW(item.hDC, caption, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        RECT captionRect = textRect;
        captionRect.top += 5;
        captionRect.bottom = captionRect.top + 18;
        DrawTextW(item.hDC, caption, -1, &captionRect,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT helpRect = textRect;
        helpRect.top += 23;
        SetTextColor(item.hDC, selected ? RGB(171, 207, 247) : RGB(139, 164, 187));
        DrawTextW(item.hDC, help.c_str(), -1, &helpRect,
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (item.itemState & ODS_FOCUS) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -5, -4);
        DrawFocusRect(item.hDC, &focus);
    }
}

void SetControlFont(HWND control, HFONT font)
{
    if (IsWindow(control)) SendMessageW(control, WM_SETFONT,
        reinterpret_cast<WPARAM>(font), TRUE);
}

bool IsCommandButton(int id)
{
    return id == IDOK || id == IDCANCEL || id == kBackButtonId ||
        id == kDetailButtonId || id == kApplyButtonId ||
        id == kResetAllButtonId;
}

void DrawCommandButton(DialogData& data, const DRAWITEMSTRUCT& item)
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hovered = GetPropW(item.hwndItem, kHoverProperty) != nullptr;
    const bool primary = item.CtlID == IDOK;
    const COLORREF fill = disabled ? RGB(25, 36, 48) :
        pressed ? (primary ? RGB(0, 78, 158) : RGB(24, 47, 69)) :
        hovered ? (primary ? RGB(12, 119, 228) : RGB(27, 52, 75)) :
        (primary ? RGB(0, 105, 210) : RGB(18, 37, 55));
    const COLORREF border = disabled ? RGB(48, 61, 74) :
        (primary ? RGB(30, 132, 239) : RGB(66, 88, 108));
    const COLORREF text = disabled ? RGB(113, 128, 142) : RGB(240, 246, 252);

    RECT bounds = item.rcItem;
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(item.hDC, brush);
    HGDIOBJ oldPen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, bounds.left, bounds.top, bounds.right, bounds.bottom, 5, 5);
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    DWORD iconId = CHUI_ICON_NONE;
    if (item.CtlID == kDetailButtonId) {
        RuntimeEntry* detail = FindEntry(data, data.selectedDetail);
        if (detail) iconId = detail->definition.iconId;
    }
    if (iconId) {
        RECT iconRect = bounds;
        iconRect.left += 12;
        iconRect.right = iconRect.left + 18;
        iconRect.top += 5;
        iconRect.bottom = iconRect.top + 18;
        DrawBuiltInIcon(item.hDC, iconRect, iconId, text);
        bounds.left += 34;
    }

    wchar_t caption[128]{};
    GetWindowTextW(item.hwndItem, caption, static_cast<int>(std::size(caption)));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, text);
    if (pressed) OffsetRect(&bounds, 1, 1);
    DrawTextW(item.hDC, caption, -1, &bounds,
        (iconId ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE |
        DT_END_ELLIPSIS);
    if ((item.itemState & ODS_FOCUS) && !disabled) {
        InflateRect(&bounds, -4, -4);
        DrawFocusRect(item.hDC, &bounds);
    }
}

void DrawColorButton(RuntimeEntry& entry, const DRAWITEMSTRUCT& item)
{
    unsigned long numeric = 0;
    std::from_chars(entry.workingValue.data(),
        entry.workingValue.data() + entry.workingValue.size(), numeric);
    RECT bounds = item.rcItem;
    const bool hovered = GetPropW(item.hwndItem, kHoverProperty) != nullptr;
    HBRUSH background = CreateSolidBrush(hovered
        ? RGB(27, 52, 75) : RGB(18, 37, 55));
    FillRect(item.hDC, &bounds, background);
    DeleteObject(background);
    HBRUSH border = CreateSolidBrush(RGB(66, 88, 108));
    FrameRect(item.hDC, &bounds, border);
    DeleteObject(border);
    RECT swatch = bounds;
    swatch.left += 7;
    swatch.top += 5;
    swatch.right = swatch.left + 38;
    swatch.bottom -= 5;
    HBRUSH color = CreateSolidBrush(static_cast<COLORREF>(numeric));
    FillRect(item.hDC, &swatch, color);
    DeleteObject(color);
    FrameRect(item.hDC, &swatch, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    RECT text = bounds;
    text.left = swatch.right + 10;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, RGB(240, 246, 252));
    DrawTextW(item.hDC, L"Choose...", -1, &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (item.itemState & ODS_FOCUS) {
        InflateRect(&bounds, -3, -3);
        DrawFocusRect(item.hDC, &bounds);
    }
}

void DrawDropdownItem(const DRAWITEMSTRUCT& item)
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF fill = disabled ? RGB(20, 31, 42) :
        selected ? RGB(10, 92, 184) : RGB(18, 34, 49);
    const COLORREF textColor = disabled ? RGB(113, 128, 142) : RGB(235, 241, 248);
    RECT bounds = item.rcItem;
    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(item.hDC, &bounds, brush);
    DeleteObject(brush);
    wchar_t caption[512]{};
    if (item.itemID != static_cast<UINT>(-1))
        SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID,
            reinterpret_cast<LPARAM>(caption));
    RECT text = bounds;
    text.left += 9;
    text.right -= 8;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, textColor);
    DrawTextW(item.hDC, caption, -1, &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if ((item.itemState & ODS_FOCUS) && !disabled) {
        InflateRect(&bounds, -2, -2);
        DrawFocusRect(item.hDC, &bounds);
    }
}

bool ChooseEntryColor(DialogData& data, RuntimeEntry& entry)
{
    unsigned long numeric = 0;
    std::from_chars(entry.workingValue.data(),
        entry.workingValue.data() + entry.workingValue.size(), numeric);
    static COLORREF customColors[16]{};
    CHOOSECOLORW picker{ sizeof(picker) };
    picker.hwndOwner = data.window;
    picker.rgbResult = static_cast<COLORREF>(numeric);
    picker.lpCustColors = customColors;
    picker.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&picker)) return false;
    entry.workingValue = std::to_string(static_cast<unsigned long>(picker.rgbResult));
    InvalidateRect(entry.control, nullptr, TRUE);
    return true;
}

bool ChooseEntryPath(DialogData& data, RuntimeEntry& entry)
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileOpenDialog* picker = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker));
    if (FAILED(result) || !picker) {
        if (SUCCEEDED(initialized)) CoUninitialize();
        return false;
    }
    DWORD options = 0;
    picker->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (entry.definition.type == CHUI_FOLDER)
        options |= FOS_PICKFOLDERS;
    else
        options |= FOS_FILEMUSTEXIST;
    picker->SetOptions(options);
    picker->SetTitle(Utf8ToWide(entry.definition.caption).c_str());

    std::vector<std::wstring> filterNames;
    std::vector<std::wstring> filterPatterns;
    std::vector<COMDLG_FILTERSPEC> filters;
    if (entry.definition.type == CHUI_FILE) {
        const auto parsed = ParseOptions(entry.definition.options);
        filterNames.reserve(parsed.size());
        filterPatterns.reserve(parsed.size());
        filters.reserve(parsed.size());
        for (const auto& option : parsed) {
            filterNames.push_back(option.caption);
            filterPatterns.push_back(Utf8ToWide(option.value.c_str()));
        }
        for (size_t index = 0; index < filterNames.size(); ++index)
            filters.push_back({ filterNames[index].c_str(),
                filterPatterns[index].c_str() });
        if (!filters.empty())
            picker->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
    }

    result = picker->Show(data.window);
    IShellItem* selected = nullptr;
    PWSTR selectedPath = nullptr;
    if (SUCCEEDED(result)) result = picker->GetResult(&selected);
    if (SUCCEEDED(result) && selected)
        result = selected->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath);
    bool changed = false;
    if (SUCCEEDED(result) && selectedPath) {
        const std::string utf8 = WideToUtf8(selectedPath);
        if (utf8.size() < sizeof(CHUI_PATH_RECORD::value)) {
            entry.workingValue = utf8;
            if (IsWindow(entry.pathDisplay))
                SetWindowTextW(entry.pathDisplay, selectedPath);
            changed = true;
        } else {
            MessageBoxW(data.window, L"The selected path is too long.",
                data.title.c_str(), MB_OK | MB_ICONWARNING);
        }
    }
    if (selectedPath) CoTaskMemFree(selectedPath);
    if (selected) selected->Release();
    picker->Release();
    if (SUCCEEDED(initialized)) CoUninitialize();
    return changed;
}

void PaintModernCombo(HWND control, HDC dc)
{
    RECT bounds{};
    GetClientRect(control, &bounds);
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const bool focused = GetFocus() == control;
    const bool hovered = GetPropW(control, kHoverProperty) != nullptr;
    HBRUSH brush = CreateSolidBrush(enabled
        ? (hovered ? RGB(24, 45, 63) : RGB(18, 34, 49)) : RGB(20, 31, 42));
    HPEN pen = CreatePen(PS_SOLID, 1,
        focused ? RGB(31, 132, 239) :
        hovered ? RGB(75, 105, 130) : RGB(55, 78, 98));
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, 6, 6);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t caption[512]{};
    const int selected = static_cast<int>(SendMessageW(control, CB_GETCURSEL, 0, 0));
    if (selected >= 0)
        SendMessageW(control, CB_GETLBTEXT, selected,
            reinterpret_cast<LPARAM>(caption));
    RECT text = bounds;
    text.left += 10;
    text.right -= 31;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? RGB(235, 241, 248) : RGB(113, 128, 142));
    DrawTextW(dc, caption, -1, &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int centerX = bounds.right - 16;
    const int centerY = (bounds.top + bounds.bottom) / 2;
    HPEN arrow = CreatePen(PS_SOLID, 2,
        enabled ? RGB(205, 220, 233) : RGB(95, 109, 122));
    HGDIOBJ oldArrow = SelectObject(dc, arrow);
    MoveToEx(dc, centerX - 4, centerY - 2, nullptr);
    LineTo(dc, centerX, centerY + 2);
    LineTo(dc, centerX + 4, centerY - 2);
    SelectObject(dc, oldArrow);
    DeleteObject(arrow);
}

LRESULT CALLBACK ModernComboProc(HWND control, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(control, &paint);
        PaintModernCombo(control, dc);
        EndPaint(control, &paint);
        return 0;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        InvalidateRect(control, nullptr, TRUE);
        break;
    case WM_MOUSEMOVE: {
        SetPropW(control, kHoverProperty, reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, control, 0 };
        TrackMouseEvent(&tracking);
        InvalidateRect(control, nullptr, TRUE);
        break;
    }
    case WM_MOUSELEAVE:
        RemovePropW(control, kHoverProperty);
        InvalidateRect(control, nullptr, TRUE);
        break;
    case WM_NCDESTROY:
        RemovePropW(control, kHoverProperty);
        RemoveWindowSubclass(control, ModernComboProc, 1);
        break;
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

void PaintModernCheckbox(HWND control, HDC dc)
{
    RECT bounds{};
    GetClientRect(control, &bounds);
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const bool checked = SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool hovered = GetPropW(control, kHoverProperty) != nullptr;
    HBRUSH background = CreateSolidBrush(RGB(13, 27, 40));
    FillRect(dc, &bounds, background);
    DeleteObject(background);

    RECT box{ 1, (bounds.bottom - 16) / 2, 17, (bounds.bottom - 16) / 2 + 16 };
    HBRUSH boxBrush = CreateSolidBrush(checked
        ? (enabled ? RGB(10, 105, 215) : RGB(40, 67, 93)) : RGB(15, 29, 42));
    HPEN boxPen = CreatePen(PS_SOLID, 1,
        enabled ? (checked ? RGB(45, 143, 245) :
            hovered ? RGB(92, 125, 151) : RGB(68, 91, 111)) : RGB(48, 61, 74));
    HGDIOBJ oldBrush = SelectObject(dc, boxBrush);
    HGDIOBJ oldPen = SelectObject(dc, boxPen);
    RoundRect(dc, box.left, box.top, box.right, box.bottom, 4, 4);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(boxPen);
    DeleteObject(boxBrush);
    if (checked) {
        HPEN checkPen = CreatePen(PS_SOLID, 2,
            enabled ? RGB(255, 255, 255) : RGB(144, 157, 169));
        HGDIOBJ oldCheck = SelectObject(dc, checkPen);
        MoveToEx(dc, box.left + 4, box.top + 8, nullptr);
        LineTo(dc, box.left + 7, box.top + 11);
        LineTo(dc, box.left + 13, box.top + 5);
        SelectObject(dc, oldCheck);
        DeleteObject(checkPen);
    }

    wchar_t caption[512]{};
    GetWindowTextW(control, caption, static_cast<int>(std::size(caption)));
    RECT text = bounds;
    text.left = 25;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? RGB(235, 241, 248) : RGB(113, 128, 142));
    DrawTextW(dc, caption, -1, &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (GetFocus() == control) {
        RECT focus = text;
        InflateRect(&focus, -1, -3);
        DrawFocusRect(dc, &focus);
    }
}

LRESULT CALLBACK ModernCheckboxProc(HWND control, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(control, &paint);
        PaintModernCheckbox(control, dc);
        EndPaint(control, &paint);
        return 0;
    }
    case BM_SETCHECK: {
        const LRESULT result = DefSubclassProc(control, message, wParam, lParam);
        InvalidateRect(control, nullptr, TRUE);
        return result;
    }
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        InvalidateRect(control, nullptr, TRUE);
        break;
    case WM_MOUSEMOVE: {
        SetPropW(control, kHoverProperty, reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, control, 0 };
        TrackMouseEvent(&tracking);
        InvalidateRect(control, nullptr, TRUE);
        break;
    }
    case WM_MOUSELEAVE:
        RemovePropW(control, kHoverProperty);
        InvalidateRect(control, nullptr, TRUE);
        break;
    case WM_NCDESTROY:
        RemovePropW(control, kHoverProperty);
        RemoveWindowSubclass(control, ModernCheckboxProc, 1);
        break;
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

LRESULT CALLBACK ButtonHoverProc(HWND control, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_MOUSEMOVE) {
        if (!GetPropW(control, kHoverProperty)) {
            SetPropW(control, kHoverProperty, reinterpret_cast<HANDLE>(1));
            InvalidateRect(control, nullptr, TRUE);
        }
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, control, 0 };
        TrackMouseEvent(&tracking);
    } else if (message == WM_MOUSELEAVE) {
        RemovePropW(control, kHoverProperty);
        InvalidateRect(control, nullptr, TRUE);
    } else if (message == WM_NCDESTROY) {
        RemovePropW(control, kHoverProperty);
        RemoveWindowSubclass(control, ButtonHoverProc, 1);
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

LRESULT CALLBACK DialogKeyboardProc(HWND control, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_KEYDOWN) {
        HWND parent = GetParent(control);
        if (wParam == VK_ESCAPE) {
            wchar_t className[32]{};
            GetClassNameW(control, className, static_cast<int>(std::size(className)));
            if (lstrcmpiW(className, WC_COMBOBOXW) == 0 &&
                SendMessageW(control, CB_GETDROPPEDSTATE, 0, 0)) {
                SendMessageW(control, CB_SHOWDROPDOWN, FALSE, 0);
            } else if (IsWindow(parent)) {
                SendMessageW(parent, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED),
                    reinterpret_cast<LPARAM>(GetDlgItem(parent, IDCANCEL)));
            }
            return 0;
        }
        if (wParam == VK_TAB && IsWindow(parent)) {
            const BOOL previous = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            HWND next = GetNextDlgTabItem(parent, control, previous);
            if (IsWindow(next)) SetFocus(next);
            return 0;
        }
        if (wParam == VK_RETURN && IsWindow(parent)) {
            wchar_t className[32]{};
            GetClassNameW(control, className, static_cast<int>(std::size(className)));
            if (lstrcmpiW(className, L"EDIT") == 0 ||
                lstrcmpiW(className, TRACKBAR_CLASSW) == 0) {
                HWND ok = GetDlgItem(parent, IDOK);
                SendMessageW(parent, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
                    reinterpret_cast<LPARAM>(ok));
                return 0;
            }
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(control, DialogKeyboardProc, 1);
    }
    return DefSubclassProc(control, message, wParam, lParam);
}

HWND AddWindow(DialogData& data, DWORD exStyle, const wchar_t* className,
    const wchar_t* caption, DWORD style, int x, int y, int width, int height, int id)
{
    HWND control = CreateWindowExW(exStyle, className, caption,
        WS_CHILD | WS_VISIBLE | style, x, y, width, height, data.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(data.window, GWLP_HINSTANCE)), nullptr);
    SetControlFont(control, data.font);
    if (control) SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
    if (control) SetWindowSubclass(control, DialogKeyboardProc, 1, 0);
    if (control && lstrcmpW(className, L"BUTTON") == 0 &&
        (style & BS_TYPEMASK) == BS_OWNERDRAW)
        SetWindowSubclass(control, ButtonHoverProc, 1, 0);
    if (control && id >= kFirstDynamicId) data.dynamicWindows.push_back(control);
    return control;
}

std::vector<size_t> Children(DialogData& data, DWORD parentId, bool panels)
{
    std::vector<size_t> result;
    for (size_t index = 0; index < data.entries.size(); ++index) {
        const auto& definition = data.entries[index].definition;
        if (definition.parentId == parentId &&
            (definition.type == CHUI_PANEL) == panels) result.push_back(index);
    }
    return result;
}

void PopulateList(HWND list, DialogData& data, DWORD parentId)
{
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (size_t index : Children(data, parentId, true)) {
        const auto& entry = data.entries[index].definition;
        const std::wstring caption = Utf8ToWide(entry.caption);
        const int item = static_cast<int>(SendMessageW(list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(caption.c_str())));
        SendMessageW(list, LB_SETITEMDATA, item, entry.id);
    }
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) > 0)
        SendMessageW(list, LB_SETCURSEL, 0, 0);
}

DWORD SelectedListId(HWND list)
{
    const LRESULT selected = SendMessageW(list, LB_GETCURSEL, 0, 0);
    return selected == LB_ERR ? 0 : static_cast<DWORD>(
        SendMessageW(list, LB_GETITEMDATA, selected, 0));
}

LRESULT CALLBACK NavigationHoverProc(HWND list, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
    auto* data = reinterpret_cast<DialogData*>(reference);
    if (!data) return DefSubclassProc(list, message, wParam, lParam);
    int& hovered = list == data->categories
        ? data->hoveredCategory : data->hoveredPage;
    if (message == WM_MOUSEMOVE) {
        const DWORD hit = static_cast<DWORD>(SendMessageW(list, LB_ITEMFROMPOINT,
            0, lParam));
        const int next = HIWORD(hit) ? -1 : static_cast<int>(LOWORD(hit));
        if (next != hovered) {
            hovered = next;
            InvalidateRect(list, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{ sizeof(tracking), TME_LEAVE, list, 0 };
        TrackMouseEvent(&tracking);
    } else if (message == WM_MOUSELEAVE) {
        if (hovered != -1) {
            hovered = -1;
            InvalidateRect(list, nullptr, FALSE);
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(list, NavigationHoverProc, 1);
    }
    return DefSubclassProc(list, message, wParam, lParam);
}

void ClearDynamic(DialogData& data)
{
    data.byControlId.clear();
    for (HWND control : data.dynamicWindows)
        if (IsWindow(control)) DestroyWindow(control);
    data.dynamicWindows.clear();
    for (auto& entry : data.entries) {
        entry.control = nullptr;
        entry.valueLabel = nullptr;
        entry.pathDisplay = nullptr;
    }
}

void ReadControl(RuntimeEntry& entry)
{
    if (!IsWindow(entry.control)) return;
    switch (entry.definition.type) {
    case CHUI_COLOR:
    case CHUI_FILE:
    case CHUI_FOLDER:
        break;
    case CHUI_DROPDOWN: {
        const int selected = static_cast<int>(SendMessageW(entry.control,
            CB_GETCURSEL, 0, 0));
        const auto options = ParseOptions(entry.definition.options);
        if (selected >= 0 && selected < static_cast<int>(options.size()))
            entry.workingValue = options[static_cast<size_t>(selected)].value;
        break;
    }
    case CHUI_CHECKBOX:
    case CHUI_RADIO:
        entry.workingValue = SendMessageW(entry.control, BM_GETCHECK, 0, 0) == BST_CHECKED
            ? "1" : "0";
        break;
    case CHUI_SLIDER:
        entry.workingValue = std::to_string(static_cast<int>(
            SendMessageW(entry.control, TBM_GETPOS, 0, 0)));
        break;
    default: {
        wchar_t value[512]{};
        GetWindowTextW(entry.control, value, static_cast<int>(std::size(value)));
        entry.workingValue = WideToUtf8(value);
        break;
    }
    }
}

void ReadAllControls(DialogData& data)
{
    for (auto& entry : data.entries) ReadControl(entry);
}

bool DependencyMatches(DialogData& data, const RuntimeEntry& entry)
{
    if (!entry.definition.dependencyId ||
        entry.definition.dependencyOperator == CHUI_DEPEND_NONE) return true;
    RuntimeEntry* source = FindEntry(data, entry.definition.dependencyId);
    if (!source) return false;
    const bool equal = source->workingValue == entry.definition.dependencyValue;
    return entry.definition.dependencyOperator == CHUI_DEPEND_EQUAL ? equal : !equal;
}

void ApplyDependencies(DialogData& data)
{
    for (auto& entry : data.entries) {
        if (!IsWindow(entry.control)) continue;
        const bool match = DependencyMatches(data, entry);
        const bool enabled = !(entry.definition.flags & CHUI_FLAG_DISABLED) &&
            (match || !(entry.definition.flags & CHUI_FLAG_DEPEND_DISABLE));
        EnableWindow(entry.control, enabled);
        if (IsWindow(entry.pathDisplay)) EnableWindow(entry.pathDisplay, enabled);
        if (entry.definition.flags & CHUI_FLAG_DEPEND_HIDE) {
            ShowWindow(entry.control, match ? SW_SHOWNA : SW_HIDE);
            if (IsWindow(entry.valueLabel))
                ShowWindow(entry.valueLabel, match ? SW_SHOWNA : SW_HIDE);
            if (IsWindow(entry.pathDisplay))
                ShowWindow(entry.pathDisplay, match ? SW_SHOWNA : SW_HIDE);
        }
    }
}

void NotifyLiveChange(DialogData& data, RuntimeEntry& entry)
{
    if (!(entry.definition.flags & CHUI_FLAG_LIVE_NOTIFY)) return;
    strncpy_s(data.callerEntries[entry.sourceIndex].value,
        entry.workingValue.c_str(), _TRUNCATE);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_changes[data.completionButton].push_back({ data.instanceId,
            entry.definition.id });
    }
    HWND parent = GetParent(data.completionButton);
    if (IsWindow(parent) && IsWindow(data.completionButton))
        PostMessageW(parent, WM_COMMAND,
            MAKEWPARAM(GetDlgCtrlID(data.completionButton), BN_CLICKED),
            reinterpret_cast<LPARAM>(data.completionButton));
}

void NotifyAction(DialogData& data, const RuntimeEntry& entry)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_actions[data.completionButton].push_back({ data.instanceId,
            entry.definition.id });
    }
    HWND parent = GetParent(data.completionButton);
    if (IsWindow(parent) && IsWindow(data.completionButton))
        PostMessageW(parent, WM_COMMAND,
            MAKEWPARAM(GetDlgCtrlID(data.completionButton), BN_CLICKED),
            reinterpret_cast<LPARAM>(data.completionButton));
}

int LabelWidth(DialogData& data, const std::vector<size_t>& children,
    int minimum, int panelWidth)
{
    int measured = minimum;
    HDC dc = GetDC(data.window);
    HFONT oldFont = dc && data.font
        ? static_cast<HFONT>(SelectObject(dc, data.font)) : nullptr;
    if (dc) {
        for (size_t index : children) {
            const RuntimeEntry& entry = data.entries[index];
            if (!IsValueType(entry.definition.type) ||
                entry.definition.type == CHUI_CHECKBOX ||
                entry.definition.type == CHUI_RADIO) continue;
            const std::wstring caption = Utf8ToWide(entry.definition.caption);
            SIZE extent{};
            if (GetTextExtentPoint32W(dc, caption.c_str(),
                static_cast<int>(caption.size()), &extent))
                measured = std::max(measured, static_cast<int>(extent.cx) + 16);
        }
        if (oldFont) SelectObject(dc, oldFont);
        ReleaseDC(data.window, dc);
    }
    return std::min(measured, panelWidth - 118);
}

void AddValueControl(DialogData& data, RuntimeEntry& entry, int& y,
    int left, int width, int labelWidth)
{
    const int id = kFirstDynamicId + static_cast<int>(entry.sourceIndex);
    const std::wstring caption = Utf8ToWide(entry.definition.caption);
    const std::wstring value = Utf8ToWide(entry.workingValue.c_str());
    const int controlX = left + labelWidth;
    const int controlWidth = std::max(100, width - labelWidth - 18);

    if (entry.definition.type == CHUI_HEADING) {
        AddWindow(data, 0, L"STATIC", caption.c_str(), SS_LEFT,
            left, y, width - 18, 24, id);
        y += 32;
        return;
    }
    if (entry.definition.type == CHUI_SEPARATOR) {
        AddWindow(data, 0, L"STATIC", L"", SS_ETCHEDHORZ,
            left, y + 8, width - 18, 2, id);
        y += 20;
        return;
    }
    if (entry.definition.type == CHUI_GROUP) {
        AddWindow(data, 0, L"BUTTON", caption.c_str(), BS_GROUPBOX,
            left, y, width - 18, 42, id);
        y += 50;
        return;
    }
    if (entry.definition.type == CHUI_ACTION) {
        entry.control = AddWindow(data, 0, L"BUTTON", caption.c_str(),
            WS_TABSTOP | BS_OWNERDRAW, left, y, std::min(230, width - 18), 28, id);
        data.byControlId[id] = entry.sourceIndex;
        y += 36;
        return;
    }
    if (entry.definition.type == CHUI_CHECKBOX || entry.definition.type == CHUI_RADIO) {
        const DWORD style = entry.definition.type == CHUI_CHECKBOX
            ? BS_AUTOCHECKBOX : BS_AUTORADIOBUTTON;
        entry.control = AddWindow(data, 0, L"BUTTON", caption.c_str(),
            WS_TABSTOP | style, left, y, width - 18, 24, id);
        SendMessageW(entry.control, BM_SETCHECK,
            entry.workingValue == "1" ? BST_CHECKED : BST_UNCHECKED, 0);
        if (entry.definition.type == CHUI_CHECKBOX)
            SetWindowSubclass(entry.control, ModernCheckboxProc, 1, 0);
    } else {
        AddWindow(data, 0, L"STATIC", caption.c_str(), SS_LEFT,
            left, y + 5, labelWidth - 8, 24, id + 600);
        if (entry.definition.type == CHUI_COLOR) {
            entry.control = AddWindow(data, 0, L"BUTTON", L"Choose...",
                WS_TABSTOP | BS_OWNERDRAW, controlX, y, controlWidth, 26, id);
        } else if (entry.definition.type == CHUI_FILE ||
            entry.definition.type == CHUI_FOLDER) {
            const int buttonWidth = 82;
            entry.pathDisplay = AddWindow(data, WS_EX_CLIENTEDGE, L"EDIT",
                value.c_str(), ES_READONLY | ES_AUTOHSCROLL,
                controlX, y, std::max(60, controlWidth - buttonWidth - 6), 26,
                id + 2000);
            entry.control = AddWindow(data, 0, L"BUTTON", L"Browse...",
                WS_TABSTOP | BS_OWNERDRAW,
                controlX + std::max(60, controlWidth - buttonWidth), y,
                buttonWidth, 26, id);
        } else if (entry.definition.type == CHUI_DROPDOWN) {
            const auto options = ParseOptions(entry.definition.options);
            entry.control = AddWindow(data, 0, WC_COMBOBOXW, L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
                CBS_HASSTRINGS | WS_VSCROLL,
                controlX, y, controlWidth, 240, id);
            int selected = 0;
            for (size_t option = 0; option < options.size(); ++option) {
                SendMessageW(entry.control, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(options[option].caption.c_str()));
                if (options[option].value == entry.workingValue)
                    selected = static_cast<int>(option);
            }
            SendMessageW(entry.control, CB_SETCURSEL, selected, 0);
            SendMessageW(entry.control, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 24);
            SendMessageW(entry.control, CB_SETITEMHEIGHT, 0, 24);
            SetWindowSubclass(entry.control, ModernComboProc, 1, 0);
        } else if (entry.definition.type == CHUI_SLIDER) {
            entry.control = AddWindow(data, 0, TRACKBAR_CLASSW, L"",
                WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                controlX, y, controlWidth - 44, 26, id);
            SendMessageW(entry.control, TBM_SETRANGEMIN, FALSE, entry.definition.minimum);
            SendMessageW(entry.control, TBM_SETRANGEMAX, FALSE, entry.definition.maximum);
            SendMessageW(entry.control, TBM_SETLINESIZE, 0,
                std::max<LONG>(1, entry.definition.step));
            int numeric = entry.definition.minimum;
            std::from_chars(entry.workingValue.data(),
                entry.workingValue.data() + entry.workingValue.size(), numeric);
            SendMessageW(entry.control, TBM_SETPOS, TRUE, numeric);
            entry.valueLabel = AddWindow(data, 0, L"STATIC", value.c_str(), SS_RIGHT,
                controlX + controlWidth - 40, y + 5, 38, 20, id + 700);
        } else {
            entry.control = AddWindow(data, WS_EX_CLIENTEDGE, L"EDIT", value.c_str(),
                WS_TABSTOP | ES_AUTOHSCROLL, controlX, y, controlWidth, 24, id);
        }
    }
    data.byControlId[id] = entry.sourceIndex;
    y += 34;
}

void Render(DialogData& data)
{
    data.rendering = true;
    ClearDynamic(data);
    const auto details = data.selectedPage
        ? Children(data, data.selectedPage, true) : std::vector<size_t>{};
    data.selectedDetail = details.empty() ? 0 : data.entries[details.front()].definition.id;
    const bool hasDetail = data.selectedDetail != 0 && !data.detailSuppressed;

    RECT client{};
    GetClientRect(data.window, &client);
    if (hasDetail && client.right < 1100) {
        RECT windowBounds{};
        GetWindowRect(data.window, &windowBounds);
        SetWindowPos(data.window, nullptr, 0, 0, 1220,
            windowBounds.bottom - windowBounds.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetClientRect(data.window, &client);
    }
    const int clientWidth = std::max(850, static_cast<int>(client.right));
    const int clientHeight = std::max(360, static_cast<int>(client.bottom));
    const int panelBottom = clientHeight - 59;
    const int navigationHeight = std::max(180, panelBottom - 48);
    SetWindowPos(data.categories, nullptr, 18, 48, 220, navigationHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(data.pages, nullptr, 254, 48, 202, navigationHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    constexpr int contentLeft = 466;
    const int availableWidth = std::max(360, clientWidth - contentLeft - 14);
    const int contentWidth = hasDetail
        ? std::max(300, (availableWidth - 8) / 2) : availableWidth;
    const int detailLeft = contentLeft + contentWidth + 8;
    const int detailWidth = std::max(300, clientWidth - detailLeft - 14);

    const auto pageChildren = data.selectedPage
        ? Children(data, data.selectedPage, false) : std::vector<size_t>{};
    const int pageControlWidth = std::max(280, contentWidth - 16);
    const int pageLabelWidth = LabelWidth(data, pageChildren, 158,
        pageControlWidth);
    int y = 82;
    for (size_t index : pageChildren)
        AddValueControl(data, data.entries[index], y, contentLeft + 8,
            pageControlWidth, pageLabelWidth);
    HWND detailButton = GetDlgItem(data.window, kDetailButtonId);
    RuntimeEntry* availableDetail = FindEntry(data, data.selectedDetail);
    if (availableDetail) SetWindowTextW(detailButton,
        Utf8ToWide(availableDetail->definition.caption).c_str());
    SetWindowPos(detailButton, nullptr, contentLeft + 8, y + 4,
        std::min(230, pageControlWidth), 28,
        SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(detailButton, data.selectedDetail && data.detailSuppressed
        ? SW_SHOWNA : SW_HIDE);
    if (hasDetail) {
        RuntimeEntry* detail = FindEntry(data, data.selectedDetail);
        if (detail) {
            const std::wstring heading = L"Advanced - " +
                Utf8ToWide(detail->definition.caption);
            AddWindow(data, 0, L"STATIC", heading.c_str(), SS_LEFT,
                detailLeft + 8, 48, detailWidth - 16, 24,
                kFirstDynamicId + 550);
        }
        const auto detailChildren = Children(data, data.selectedDetail, false);
        const int detailControlWidth = std::max(280, detailWidth - 16);
        const int detailLabelWidth = LabelWidth(data, detailChildren, 138,
            detailControlWidth);
        y = 82;
        for (size_t index : detailChildren)
            AddValueControl(data, data.entries[index], y, detailLeft + 8,
                detailControlWidth,
                detailLabelWidth);
    }
    const int buttonY = clientHeight - 45;
    SetWindowPos(GetDlgItem(data.window, IDOK), nullptr, clientWidth - 260, buttonY,
        76, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(data.window, IDCANCEL), nullptr, clientWidth - 176,
        buttonY,
        76, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(data.window, kApplyButtonId), nullptr, clientWidth - 92,
        buttonY, 76, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(data.window, kResetAllButtonId), nullptr, 14,
        buttonY, 150, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    HWND back = GetDlgItem(data.window, kBackButtonId);
    SetWindowPos(back, nullptr, detailLeft + 8, buttonY, 76, 30,
        SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(back, hasDetail ? SW_SHOWNA : SW_HIDE);
    data.rendering = false;
    ApplyDependencies(data);
    InvalidateRect(data.window, nullptr, TRUE);
}

bool ValidateWorkingValues(DialogData& data)
{
    for (const auto& entry : data.entries) {
        if ((entry.definition.flags & CHUI_FLAG_REQUIRED) && entry.workingValue.empty()) {
            MessageBoxW(data.window, L"A required value is missing.", data.title.c_str(),
                MB_OK | MB_ICONWARNING);
            return false;
        }
        if (entry.definition.type == CHUI_NUMBER || entry.definition.type == CHUI_SLIDER) {
            LONG number = 0;
            const auto parsed = std::from_chars(entry.workingValue.data(),
                entry.workingValue.data() + entry.workingValue.size(), number);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != entry.workingValue.data() + entry.workingValue.size() ||
                number < entry.definition.minimum || number > entry.definition.maximum) {
                MessageBoxW(data.window, L"A numeric value is outside its permitted range.",
                    data.title.c_str(), MB_OK | MB_ICONWARNING);
                return false;
            }
        }
        if (entry.definition.type == CHUI_COLOR) {
            unsigned long color = 0;
            const auto parsed = std::from_chars(entry.workingValue.data(),
                entry.workingValue.data() + entry.workingValue.size(), color);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != entry.workingValue.data() + entry.workingValue.size() ||
                color > 0x00FFFFFFUL) {
                MessageBoxW(data.window, L"A color value is invalid.",
                    data.title.c_str(), MB_OK | MB_ICONWARNING);
                return false;
            }
        }
    }
    return true;
}

bool HasChanges(const DialogData& data)
{
    for (const auto& entry : data.entries)
        if (IsValueType(entry.definition.type) &&
            entry.workingValue != entry.baselineValue) return true;
    return false;
}

void UpdateApplyButton(DialogData& data)
{
    HWND apply = GetDlgItem(data.window, kApplyButtonId);
    if (IsWindow(apply)) EnableWindow(apply, HasChanges(data));
}

void ResetAllValues(DialogData& data)
{
    ReadAllControls(data);
    for (auto& entry : data.entries) {
        if (!IsValueType(entry.definition.type)) continue;
        entry.workingValue = entry.defaultWorkingValue;
    }
    Render(data);
    UpdateApplyButton(data);
}

bool CommitWorkingValues(DialogData& data, bool establishBaseline)
{
    if (!ValidateWorkingValues(data)) return false;
    for (auto& entry : data.entries) {
        if (!IsValueType(entry.definition.type)) continue;
        if (entry.pathIndex != static_cast<size_t>(-1))
            strncpy_s(data.callerPaths[entry.pathIndex].value,
                entry.workingValue.c_str(), _TRUNCATE);
        else
            strncpy_s(data.callerEntries[entry.sourceIndex].value,
                entry.workingValue.c_str(), _TRUNCATE);
        if (establishBaseline) {
            entry.baselineValue = entry.workingValue;
            entry.cancelValue = entry.workingValue;
        }
    }
    return true;
}

void QueueCompletion(DialogData& data, LONG result)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_completions[data.completionButton].push_back({ data.instanceId, result });
    }
    HWND parent = GetParent(data.completionButton);
    if (IsWindow(parent) && IsWindow(data.completionButton))
        PostMessageW(parent, WM_COMMAND,
            MAKEWPARAM(GetDlgCtrlID(data.completionButton), BN_CLICKED),
            reinterpret_cast<LPARAM>(data.completionButton));
}

void Complete(DialogData& data, LONG result)
{
    ReadAllControls(data);
    if (result == CHUI_RESULT_OK) {
        if (!CommitWorkingValues(data, false)) return;
        data.committed = true;
    } else {
        for (const auto& entry : data.entries) {
            if (!IsValueType(entry.definition.type)) continue;
            if (entry.pathIndex != static_cast<size_t>(-1))
                strncpy_s(data.callerPaths[entry.pathIndex].value,
                    entry.cancelValue.c_str(), _TRUNCATE);
            else
                strncpy_s(data.callerEntries[entry.sourceIndex].value,
                    entry.cancelValue.c_str(), _TRUNCATE);
        }
    }
    DestroyWindow(data.window);
}

void NotifyCompletion(DialogData& data)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_byOwner.erase(data.owner);
        g_byInstance.erase(data.instanceId);
    }
    QueueCompletion(data, data.committed ? CHUI_RESULT_OK : CHUI_RESULT_CANCEL);
}

LRESULT CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    DialogData* data = GetData(window);
    if (message == WM_NCCREATE) {
        data = reinterpret_cast<DialogData*>(
            reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        data->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    }
    if (!data) return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case kSetEntryValueMessage: {
        auto* request = reinterpret_cast<SetEntryValueRequest*>(lParam);
        if (!request) return FALSE;
        RuntimeEntry* entry = FindEntry(*data, request->entryId);
        if (!entry || !IsValueType(entry->definition.type)) return FALSE;
        if (entry->pathIndex == static_cast<size_t>(-1) &&
            request->value.size() >= sizeof(CHUI_ENTRY_RECORD::value)) return FALSE;
        if (entry->definition.type == CHUI_DROPDOWN) {
            const auto options = ParseOptions(entry->definition.options);
            if (std::none_of(options.begin(), options.end(),
                [&](const Option& option) { return option.value == request->value; }))
                return FALSE;
        }
        if ((entry->definition.type == CHUI_CHECKBOX ||
            entry->definition.type == CHUI_RADIO) &&
            request->value != "0" && request->value != "1") return FALSE;
        ReadAllControls(*data);
        entry->workingValue = request->value;
        Render(*data);
        UpdateApplyButton(*data);
        return TRUE;
    }
    case WM_CREATE: {
        data->backgroundBrush = CreateSolidBrush(RGB(5, 13, 22));
        data->surfaceBrush = CreateSolidBrush(RGB(13, 27, 40));
        data->navigationBrush = CreateSolidBrush(RGB(16, 31, 46));
        data->inputBrush = CreateSolidBrush(RGB(18, 34, 49));
        data->borderBrush = CreateSolidBrush(RGB(39, 59, 77));
        const UINT dpi = GetDpiForWindow(window);
        data->font = CreateFontW(-MulDiv(9, dpi ? static_cast<int>(dpi) : 96, 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        data->categories = AddWindow(*data, 0, L"LISTBOX", L"",
            WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            18, 48, 220, 430, kCategoryListId);
        data->pages = AddWindow(*data, 0, L"LISTBOX", L"",
            WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            254, 48, 202, 430, kPageListId);
        SetWindowSubclass(data->categories, NavigationHoverProc, 1,
            reinterpret_cast<DWORD_PTR>(data));
        SetWindowSubclass(data->pages, NavigationHoverProc, 1,
            reinterpret_cast<DWORD_PTR>(data));
        SendMessageW(data->categories, LB_SETITEMHEIGHT, 0, 44);
        SendMessageW(data->pages, LB_SETITEMHEIGHT, 0, 44);
        AddWindow(*data, 0, L"BUTTON", L"OK", WS_TABSTOP | BS_OWNERDRAW,
            674, 500, 76, 30, IDOK);
        AddWindow(*data, 0, L"BUTTON", L"Cancel", WS_TABSTOP | BS_OWNERDRAW,
            758, 500, 76, 30, IDCANCEL);
        AddWindow(*data, 0, L"BUTTON", L"Apply", WS_TABSTOP | BS_OWNERDRAW,
            758, 500, 76, 30, kApplyButtonId);
        AddWindow(*data, 0, L"BUTTON", L"Reset All Settings",
            WS_TABSTOP | BS_OWNERDRAW, 14, 500, 150, 30, kResetAllButtonId);
        AddWindow(*data, 0, L"BUTTON", L"< Back", WS_TABSTOP | BS_OWNERDRAW,
            842, 500, 76, 30, kBackButtonId);
        AddWindow(*data, 0, L"BUTTON", L"Advanced Settings...",
            WS_TABSTOP | BS_OWNERDRAW, 474, 120, 150, 28, kDetailButtonId);
        PopulateList(data->categories, *data, 0);
        data->selectedCategory = SelectedListId(data->categories);
        PopulateList(data->pages, *data, data->selectedCategory);
        data->selectedPage = SelectedListId(data->pages);
        Render(*data);
        UpdateApplyButton(*data);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (id == IDOK && notification == BN_CLICKED) {
            Complete(*data, CHUI_RESULT_OK);
            return 0;
        }
        if (id == IDCANCEL && notification == BN_CLICKED) {
            Complete(*data, CHUI_RESULT_CANCEL);
            return 0;
        }
        if (id == kApplyButtonId && notification == BN_CLICKED) {
            ReadAllControls(*data);
            if (CommitWorkingValues(*data, true)) {
                UpdateApplyButton(*data);
                QueueCompletion(*data, CHUI_RESULT_APPLY);
            }
            return 0;
        }
        if (id == kResetAllButtonId && notification == BN_CLICKED) {
            if (MessageBoxW(data->window,
                L"Reset all settings in this dialog to their declared defaults?",
                data->title.c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
                ResetAllValues(*data);
            return 0;
        }
        if (id == kBackButtonId && notification == BN_CLICKED) {
            ReadAllControls(*data);
            data->detailSuppressed = true;
            Render(*data);
            return 0;
        }
        if (id == kDetailButtonId && notification == BN_CLICKED) {
            ReadAllControls(*data);
            data->detailSuppressed = false;
            Render(*data);
            return 0;
        }
        if (id == kCategoryListId && notification == LBN_SELCHANGE) {
            ReadAllControls(*data);
            data->selectedCategory = SelectedListId(data->categories);
            data->detailSuppressed = false;
            PopulateList(data->pages, *data, data->selectedCategory);
            data->selectedPage = SelectedListId(data->pages);
            Render(*data);
            return 0;
        }
        if (id == kPageListId && notification == LBN_SELCHANGE) {
            ReadAllControls(*data);
            data->selectedPage = SelectedListId(data->pages);
            data->detailSuppressed = false;
            Render(*data);
            return 0;
        }
        const auto dynamic = data->byControlId.find(id);
        if (!data->rendering && dynamic != data->byControlId.end() &&
            notification == BN_CLICKED) {
            RuntimeEntry& entry = data->entries[dynamic->second];
            if (entry.definition.type == CHUI_FILE ||
                entry.definition.type == CHUI_FOLDER) {
                if (ChooseEntryPath(*data, entry)) UpdateApplyButton(*data);
                ApplyDependencies(*data);
                return 0;
            }
        }
        if (!data->rendering && dynamic != data->byControlId.end() &&
            notification == BN_CLICKED &&
            data->entries[dynamic->second].definition.type == CHUI_ACTION) {
            NotifyAction(*data, data->entries[dynamic->second]);
            return 0;
        }
        if (!data->rendering && dynamic != data->byControlId.end() &&
            notification == BN_CLICKED) {
            RuntimeEntry& entry = data->entries[dynamic->second];
            if (entry.definition.type == CHUI_COLOR) {
                if (ChooseEntryColor(*data, entry)) {
                    NotifyLiveChange(*data, entry);
                    UpdateApplyButton(*data);
                }
                ApplyDependencies(*data);
                return 0;
            }
        }
        if (!data->rendering && id >= kFirstDynamicId &&
            (notification == BN_CLICKED || notification == CBN_SELCHANGE ||
             notification == EN_CHANGE)) {
            ReadAllControls(*data);
            const auto changed = data->byControlId.find(id);
            if (changed != data->byControlId.end())
                NotifyLiveChange(*data, data->entries[changed->second]);
            ApplyDependencies(*data);
            UpdateApplyButton(*data);
        }
        return 0;
    }
    case WM_HSCROLL:
        if (!data->rendering) {
            ReadAllControls(*data);
            for (auto& entry : data->entries) {
                if (entry.control == reinterpret_cast<HWND>(lParam) &&
                    IsWindow(entry.valueLabel)) {
                    SetWindowTextW(entry.valueLabel,
                        Utf8ToWide(entry.workingValue.c_str()).c_str());
                    NotifyLiveChange(*data, entry);
                }
            }
            ApplyDependencies(*data);
            UpdateApplyButton(*data);
        }
        return 0;
    case WM_SIZE:
        if (!data->rendering && data->font) Render(*data);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize.x = data->selectedDetail && !data->detailSuppressed
            ? 1100 : 850;
        limits->ptMinTrackSize.y = 500;
        return 0;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item && item->CtlType == ODT_LISTBOX &&
            (item->CtlID == kCategoryListId || item->CtlID == kPageListId)) {
            DrawNavigationItem(*data, *item);
            return TRUE;
        }
        if (item && item->CtlType == ODT_BUTTON &&
            IsCommandButton(static_cast<int>(item->CtlID))) {
            DrawCommandButton(*data, *item);
            return TRUE;
        }
        if (item && item->CtlType == ODT_BUTTON) {
            const auto dynamic = data->byControlId.find(static_cast<int>(item->CtlID));
            if (dynamic != data->byControlId.end()) {
                RuntimeEntry& entry = data->entries[dynamic->second];
                if (entry.definition.type == CHUI_COLOR) {
                    DrawColorButton(entry, *item);
                    return TRUE;
                }
                if (entry.definition.type == CHUI_ACTION ||
                    entry.definition.type == CHUI_FILE ||
                    entry.definition.type == CHUI_FOLDER) {
                    DrawCommandButton(*data, *item);
                    return TRUE;
                }
            }
        }
        if (item && item->CtlType == ODT_COMBOBOX) {
            DrawDropdownItem(*item);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        Complete(*data, CHUI_RESULT_CANCEL);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(235, 241, 248));
        return reinterpret_cast<LRESULT>(data->surfaceBrush);
    }
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, RGB(16, 31, 46));
        SetTextColor(dc, RGB(235, 241, 248));
        return reinterpret_cast<LRESULT>(data->navigationBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, RGB(18, 34, 49));
        SetTextColor(dc, RGB(235, 241, 248));
        return reinterpret_cast<LRESULT>(data->inputBrush);
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        HDC dc = reinterpret_cast<HDC>(wParam);
        FillRect(dc, &client, data->backgroundBrush);
        const int panelBottom = std::max(301L, client.bottom - 59);
        const bool hasDetail = data->selectedDetail && !data->detailSuppressed;
        const int availableWidth = std::max(360L, client.right - 466 - 14);
        const int contentWidth = hasDetail
            ? std::max(300, (availableWidth - 8) / 2) : availableWidth;
        const int detailLeft = 466 + contentWidth + 8;
        const RECT categoryPanel{ 14, 40, 242, panelBottom };
        const RECT pagePanel{ 250, 40, 462, panelBottom };
        const RECT contentPanel{ 466, 40, 466 + contentWidth, panelBottom };
        FillRect(dc, &categoryPanel, data->surfaceBrush);
        FillRect(dc, &pagePanel, data->surfaceBrush);
        FillRect(dc, &contentPanel, data->surfaceBrush);
        FrameRect(dc, &categoryPanel, data->borderBrush);
        FrameRect(dc, &pagePanel, data->borderBrush);
        FrameRect(dc, &contentPanel, data->borderBrush);
        if (hasDetail) {
            const RECT detailPanel{ detailLeft, 40, client.right - 14, panelBottom };
            FillRect(dc, &detailPanel, data->surfaceBrush);
            FrameRect(dc, &detailPanel, data->borderBrush);
        }
        return 1;
    }
    case WM_NCDESTROY:
        NotifyCompletion(*data);
        DeleteObject(data->backgroundBrush);
        DeleteObject(data->surfaceBrush);
        DeleteObject(data->navigationBrush);
        DeleteObject(data->inputBrush);
        DeleteObject(data->borderBrush);
        DeleteObject(data->font);
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        delete data;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool EnsureDialogClass()
{
    static ATOM atom = 0;
    if (atom) return true;
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&DialogProc), &module);
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = module;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kDialogClass;
    atom = RegisterClassExW(&wc);
    return atom || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
}

DWORD __stdcall CHUI_GetAbiVersion() { return kAbiVersion; }
DWORD __stdcall CHUI_GetHeaderSize() { return sizeof(CHUI_DIALOG_HEADER); }
DWORD __stdcall CHUI_GetEntrySize() { return sizeof(CHUI_ENTRY_RECORD); }
DWORD __stdcall CHUI_GetPathRecordSize() { return sizeof(CHUI_PATH_RECORD); }

LONG __stdcall CHUI_ValidateDialog(const CHUI_DIALOG_HEADER* header,
    const CHUI_ENTRY_RECORD* entries)
{
    return Validate(header, entries);
}

LONG __stdcall CHUI_ValidateDialogEx(const CHUI_DIALOG_HEADER* header,
    const CHUI_ENTRY_RECORD* entries, const CHUI_PATH_RECORD* paths,
    DWORD pathCount)
{
    return ValidatePaths(header, entries, paths, pathCount);
}

LONG OpenDialogCore(HWND ownerWindow, CHUI_DIALOG_HEADER* header,
    CHUI_ENTRY_RECORD* entries, CHUI_PATH_RECORD* paths, DWORD pathCount,
    HWND completionButton)
{
    const LONG valid = ValidatePaths(header, entries, paths, pathCount);
    if (valid != CHUI_STATUS_OK) return valid;
    if (!IsWindow(ownerWindow) || !IsWindow(completionButton)) return CHUI_ERROR_ARGUMENT;
    if (!EnsureDialogClass()) return CHUI_ERROR_WINDOW;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto existing = g_byOwner.find(ownerWindow);
        if (existing != g_byOwner.end() && IsWindow(existing->second)) {
            SetForegroundWindow(existing->second);
            return CHUI_ERROR_ALREADY_OPEN;
        }
    }

    auto* data = new (std::nothrow) DialogData();
    if (!data) return CHUI_ERROR_WINDOW;
    data->owner = ownerWindow;
    data->completionButton = completionButton;
    data->callerEntries = entries;
    data->callerPaths = paths;
    data->pathCount = pathCount;
    data->instanceId = g_nextInstance.fetch_add(1);
    if (!data->instanceId) data->instanceId = g_nextInstance.fetch_add(1);
    data->title = Utf8ToWide(header->title);
    if (data->title.empty()) data->title = L"Structured Dialog";
    data->entries.reserve(header->entryCount);
    std::unordered_map<DWORD, size_t> pathsByEntry;
    for (DWORD index = 0; index < pathCount; ++index)
        pathsByEntry.emplace(paths[index].entryId, index);
    for (DWORD index = 0; index < header->entryCount; ++index) {
        RuntimeEntry runtime{};
        runtime.definition = entries[index];
        runtime.sourceIndex = index;
        const auto path = pathsByEntry.find(entries[index].id);
        if (path != pathsByEntry.end()) {
            runtime.pathIndex = path->second;
            runtime.workingValue = paths[path->second].value[0]
                ? paths[path->second].value : paths[path->second].defaultValue;
            runtime.defaultWorkingValue = paths[path->second].defaultValue;
            runtime.cancelValue = paths[path->second].value;
        } else {
            runtime.workingValue = entries[index].value[0]
                ? entries[index].value : entries[index].defaultValue;
            runtime.defaultWorkingValue = entries[index].defaultValue;
            runtime.cancelValue = entries[index].value;
        }
        runtime.baselineValue = runtime.workingValue;
        data->byId.emplace(runtime.definition.id, index);
        data->entries.push_back(std::move(runtime));
    }
    header->instanceId = data->instanceId;

    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&DialogProc), &module);
    HWND window = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_CONTROLPARENT,
        kDialogClass, data->title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 590, ownerWindow, nullptr, module, data);
    if (!window) {
        delete data;
        return CHUI_ERROR_WINDOW;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_byOwner[ownerWindow] = window;
        g_byInstance[data->instanceId] = window;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    return static_cast<LONG>(data->instanceId);
}

LONG __stdcall CHUI_OpenDialog(HWND ownerWindow, CHUI_DIALOG_HEADER* header,
    CHUI_ENTRY_RECORD* entries, HWND completionButton)
{
    return OpenDialogCore(ownerWindow, header, entries, nullptr, 0,
        completionButton);
}

LONG __stdcall CHUI_OpenDialogEx(HWND ownerWindow, CHUI_DIALOG_HEADER* header,
    CHUI_ENTRY_RECORD* entries, CHUI_PATH_RECORD* paths, DWORD pathCount,
    HWND completionButton)
{
    return OpenDialogCore(ownerWindow, header, entries, paths, pathCount,
        completionButton);
}

LONG __stdcall CHUI_ConsumeCompletion(HWND completionButton,
    DWORD* instanceId, LONG* result)
{
    if (!completionButton || !instanceId || !result) return FALSE;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_completions.find(completionButton);
    if (found == g_completions.end() || found->second.empty()) return FALSE;
    const Completion completion = found->second.front();
    found->second.pop_front();
    if (found->second.empty()) g_completions.erase(found);
    *instanceId = completion.instanceId;
    *result = completion.result;
    return TRUE;
}

LONG __stdcall CHUI_ConsumeChange(HWND completionButton,
    DWORD* instanceId, DWORD* entryId)
{
    if (!IsWindow(completionButton) || !instanceId || !entryId) return FALSE;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_changes.find(completionButton);
    if (found == g_changes.end() || found->second.empty()) return FALSE;
    const Change change = found->second.front();
    found->second.pop_front();
    if (found->second.empty()) g_changes.erase(found);
    *instanceId = change.instanceId;
    *entryId = change.entryId;
    return TRUE;
}

LONG __stdcall CHUI_ConsumeAction(HWND completionButton,
    DWORD* instanceId, DWORD* entryId)
{
    if (!IsWindow(completionButton) || !instanceId || !entryId) return FALSE;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto found = g_actions.find(completionButton);
    if (found == g_actions.end() || found->second.empty()) return FALSE;
    const Action action = found->second.front();
    found->second.pop_front();
    if (found->second.empty()) g_actions.erase(found);
    *instanceId = action.instanceId;
    *entryId = action.entryId;
    return TRUE;
}

LONG __stdcall CHUI_SetEntryValue(DWORD instanceId, DWORD entryId,
    const char* value)
{
    if (!instanceId || !entryId || !value) return FALSE;
    const size_t length = strnlen_s(value, sizeof(CHUI_PATH_RECORD::value));
    if (length >= sizeof(CHUI_PATH_RECORD::value)) return FALSE;
    HWND window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto found = g_byInstance.find(instanceId);
        if (found == g_byInstance.end()) return FALSE;
        window = found->second;
    }
    if (!IsWindow(window)) return FALSE;
    SetEntryValueRequest request{ entryId, std::string(value, length) };
    return static_cast<LONG>(SendMessageW(window, kSetEntryValueMessage, 0,
        reinterpret_cast<LPARAM>(&request)) != FALSE);
}
