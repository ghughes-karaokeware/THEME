#include "CHTheme.h"

#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
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
constexpr int kFirstDynamicId = 1000;

struct Option { std::string value; std::wstring caption; };
struct Completion { DWORD instanceId; LONG result; };
struct Change { DWORD instanceId; DWORD entryId; };

struct RuntimeEntry
{
    CHUI_ENTRY_RECORD definition{};
    size_t sourceIndex = 0;
    std::string workingValue;
    HWND control = nullptr;
    HWND valueLabel = nullptr;
};

struct DialogData
{
    HWND window = nullptr;
    HWND owner = nullptr;
    HWND completionButton = nullptr;
    CHUI_ENTRY_RECORD* callerEntries = nullptr;
    DWORD instanceId = 0;
    bool committed = false;
    bool rendering = false;
    bool detailSuppressed = false;
    DWORD selectedCategory = 0;
    DWORD selectedPage = 0;
    DWORD selectedDetail = 0;
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
std::unordered_map<HWND, std::deque<Completion>> g_completions;
std::unordered_map<HWND, std::deque<Change>> g_changes;
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
    return type >= CHUI_ENTRY && type <= CHUI_COLOR;
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
            (entry.type > CHUI_SEPARATOR && !IsValueType(entry.type)))
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
    const COLORREF background = selected ? RGB(10, 92, 184) : RGB(16, 31, 46);
    const COLORREF foreground = selected ? RGB(255, 255, 255) : RGB(218, 229, 239);
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    if (selected) {
        HBRUSH border = CreateSolidBrush(RGB(31, 132, 239));
        FrameRect(item.hDC, &item.rcItem, border);
        DeleteObject(border);
    }

    RuntimeEntry* entry = FindEntry(data, static_cast<DWORD>(item.itemData));
    RECT iconRect = item.rcItem;
    iconRect.left += 8;
    iconRect.right = iconRect.left + 18;
    iconRect.top += 5;
    iconRect.bottom = iconRect.top + 18;
    const DWORD iconId = entry ? entry->definition.iconId : CHUI_ICON_NONE;
    DrawBuiltInIcon(item.hDC, iconRect, iconId, foreground);

    wchar_t caption[256]{};
    SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID,
        reinterpret_cast<LPARAM>(caption));
    RECT textRect = item.rcItem;
    textRect.left += iconId ? 34 : 9;
    textRect.right -= 6;
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, foreground);
    DrawTextW(item.hDC, caption, -1, &textRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item.itemState & ODS_FOCUS) DrawFocusRect(item.hDC, &item.rcItem);
}

void SetControlFont(HWND control, HFONT font)
{
    if (IsWindow(control)) SendMessageW(control, WM_SETFONT,
        reinterpret_cast<WPARAM>(font), TRUE);
}

bool IsCommandButton(int id)
{
    return id == IDOK || id == IDCANCEL || id == kBackButtonId ||
        id == kDetailButtonId;
}

void DrawCommandButton(DialogData& data, const DRAWITEMSTRUCT& item)
{
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool primary = item.CtlID == IDOK;
    const COLORREF fill = disabled ? RGB(25, 36, 48) :
        pressed ? (primary ? RGB(0, 78, 158) : RGB(24, 47, 69)) :
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
    HBRUSH background = CreateSolidBrush(RGB(18, 37, 55));
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

HWND AddWindow(DialogData& data, DWORD exStyle, const wchar_t* className,
    const wchar_t* caption, DWORD style, int x, int y, int width, int height, int id)
{
    HWND control = CreateWindowExW(exStyle, className, caption,
        WS_CHILD | WS_VISIBLE | style, x, y, width, height, data.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(data.window, GWLP_HINSTANCE)), nullptr);
    SetControlFont(control, data.font);
    if (control) SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
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

void ClearDynamic(DialogData& data)
{
    data.byControlId.clear();
    for (HWND control : data.dynamicWindows)
        if (IsWindow(control)) DestroyWindow(control);
    data.dynamicWindows.clear();
    for (auto& entry : data.entries) {
        entry.control = nullptr;
        entry.valueLabel = nullptr;
    }
}

void ReadControl(RuntimeEntry& entry)
{
    if (!IsWindow(entry.control)) return;
    switch (entry.definition.type) {
    case CHUI_COLOR:
        break;
    case CHUI_CHECKBOX:
    case CHUI_RADIO:
        entry.workingValue = SendMessageW(entry.control, BM_GETCHECK, 0, 0) == BST_CHECKED
            ? "1" : "0";
        break;
    case CHUI_DROPDOWN: {
        const int selected = static_cast<int>(SendMessageW(entry.control, CB_GETCURSEL, 0, 0));
        const auto options = ParseOptions(entry.definition.options);
        if (selected >= 0 && selected < static_cast<int>(options.size()))
            entry.workingValue = options[static_cast<size_t>(selected)].value;
        break;
    }
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
        if (entry.definition.flags & CHUI_FLAG_DEPEND_HIDE) {
            ShowWindow(entry.control, match ? SW_SHOWNA : SW_HIDE);
            if (IsWindow(entry.valueLabel))
                ShowWindow(entry.valueLabel, match ? SW_SHOWNA : SW_HIDE);
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
    if (entry.definition.type == CHUI_CHECKBOX || entry.definition.type == CHUI_RADIO) {
        const DWORD style = entry.definition.type == CHUI_CHECKBOX
            ? BS_AUTOCHECKBOX : BS_AUTORADIOBUTTON;
        entry.control = AddWindow(data, 0, L"BUTTON", caption.c_str(),
            WS_TABSTOP | style, left, y, width - 18, 24, id);
        SendMessageW(entry.control, BM_SETCHECK,
            entry.workingValue == "1" ? BST_CHECKED : BST_UNCHECKED, 0);
    } else {
        AddWindow(data, 0, L"STATIC", caption.c_str(), SS_LEFT,
            left, y + 5, labelWidth - 8, 24, id + 600);
        if (entry.definition.type == CHUI_COLOR) {
            entry.control = AddWindow(data, 0, L"BUTTON", L"Choose...",
                WS_TABSTOP | BS_OWNERDRAW, controlX, y, controlWidth, 26, id);
        } else if (entry.definition.type == CHUI_DROPDOWN) {
            entry.control = AddWindow(data, 0, WC_COMBOBOXW, L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                controlX, y, controlWidth, 220, id);
            const auto options = ParseOptions(entry.definition.options);
            int selected = 0;
            for (size_t option = 0; option < options.size(); ++option) {
                SendMessageW(entry.control, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(options[option].caption.c_str()));
                if (options[option].value == entry.workingValue)
                    selected = static_cast<int>(option);
            }
            SendMessageW(entry.control, CB_SETCURSEL, selected, 0);
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

    const auto pageChildren = data.selectedPage
        ? Children(data, data.selectedPage, false) : std::vector<size_t>{};
    const int pageLabelWidth = LabelWidth(data, pageChildren, 158, 350);
    int y = 82;
    for (size_t index : pageChildren)
        AddValueControl(data, data.entries[index], y, 474, 350, pageLabelWidth);
    HWND detailButton = GetDlgItem(data.window, kDetailButtonId);
    RuntimeEntry* availableDetail = FindEntry(data, data.selectedDetail);
    if (availableDetail) SetWindowTextW(detailButton,
        Utf8ToWide(availableDetail->definition.caption).c_str());
    SetWindowPos(detailButton, nullptr, 474, y + 4, 230, 28,
        SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(detailButton, data.selectedDetail && data.detailSuppressed
        ? SW_SHOWNA : SW_HIDE);
    if (hasDetail) {
        RuntimeEntry* detail = FindEntry(data, data.selectedDetail);
        if (detail) {
            const std::wstring heading = L"Advanced - " +
                Utf8ToWide(detail->definition.caption);
            AddWindow(data, 0, L"STATIC", heading.c_str(), SS_LEFT,
                842, 48, 336, 24, kFirstDynamicId + 550);
        }
        const auto detailChildren = Children(data, data.selectedDetail, false);
        const int detailLabelWidth = LabelWidth(data, detailChildren, 138, 340);
        y = 82;
        for (size_t index : detailChildren)
            AddValueControl(data, data.entries[index], y, 842, 340,
                detailLabelWidth);
    }
    SetWindowPos(data.window, nullptr, 0, 0, hasDetail ? 1220 : 850, 590,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    const int buttonOffset = hasDetail ? 370 : 0;
    SetWindowPos(GetDlgItem(data.window, IDOK), nullptr, 674 + buttonOffset, 500,
        76, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(GetDlgItem(data.window, IDCANCEL), nullptr, 758 + buttonOffset, 500,
        76, 30, SWP_NOZORDER | SWP_NOACTIVATE);
    HWND back = GetDlgItem(data.window, kBackButtonId);
    SetWindowPos(back, nullptr, 842, 500, 76, 30,
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

void Complete(DialogData& data, LONG result)
{
    ReadAllControls(data);
    if (result == CHUI_RESULT_OK) {
        if (!ValidateWorkingValues(data)) return;
        for (const auto& entry : data.entries) {
            if (IsValueType(entry.definition.type))
                strncpy_s(data.callerEntries[entry.sourceIndex].value,
                    entry.workingValue.c_str(), _TRUNCATE);
        }
        data.committed = true;
    } else {
        for (const auto& entry : data.entries) {
            if (IsValueType(entry.definition.type))
                strncpy_s(data.callerEntries[entry.sourceIndex].value,
                    entry.definition.value, _TRUNCATE);
        }
    }
    DestroyWindow(data.window);
}

void NotifyCompletion(DialogData& data)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_byOwner.erase(data.owner);
        g_completions[data.completionButton].push_back({ data.instanceId,
            data.committed ? CHUI_RESULT_OK : CHUI_RESULT_CANCEL });
    }
    HWND parent = GetParent(data.completionButton);
    if (IsWindow(parent) && IsWindow(data.completionButton))
        PostMessageW(parent, WM_COMMAND,
            MAKEWPARAM(GetDlgCtrlID(data.completionButton), BN_CLICKED),
            reinterpret_cast<LPARAM>(data.completionButton));
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
        SendMessageW(data->categories, LB_SETITEMHEIGHT, 0, 28);
        SendMessageW(data->pages, LB_SETITEMHEIGHT, 0, 28);
        AddWindow(*data, 0, L"BUTTON", L"OK", WS_TABSTOP | BS_OWNERDRAW,
            674, 500, 76, 30, IDOK);
        AddWindow(*data, 0, L"BUTTON", L"Cancel", WS_TABSTOP | BS_OWNERDRAW,
            758, 500, 76, 30, IDCANCEL);
        AddWindow(*data, 0, L"BUTTON", L"< Back", WS_TABSTOP | BS_OWNERDRAW,
            842, 500, 76, 30, kBackButtonId);
        AddWindow(*data, 0, L"BUTTON", L"Advanced Settings...",
            WS_TABSTOP | BS_OWNERDRAW, 474, 120, 150, 28, kDetailButtonId);
        PopulateList(data->categories, *data, 0);
        data->selectedCategory = SelectedListId(data->categories);
        PopulateList(data->pages, *data, data->selectedCategory);
        data->selectedPage = SelectedListId(data->pages);
        Render(*data);
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
            if (entry.definition.type == CHUI_COLOR) {
                if (ChooseEntryColor(*data, entry))
                    NotifyLiveChange(*data, entry);
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
        }
        return 0;
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
            }
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
        const RECT categoryPanel{ 14, 40, 242, 486 };
        const RECT pagePanel{ 250, 40, 462, 486 };
        const RECT contentPanel{ 466, 40, 832, 486 };
        FillRect(dc, &categoryPanel, data->surfaceBrush);
        FillRect(dc, &pagePanel, data->surfaceBrush);
        FillRect(dc, &contentPanel, data->surfaceBrush);
        FrameRect(dc, &categoryPanel, data->borderBrush);
        FrameRect(dc, &pagePanel, data->borderBrush);
        FrameRect(dc, &contentPanel, data->borderBrush);
        if (client.right > 900) {
            const RECT detailPanel{ 834, 40, client.right - 14, 486 };
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

LONG __stdcall CHUI_ValidateDialog(const CHUI_DIALOG_HEADER* header,
    const CHUI_ENTRY_RECORD* entries)
{
    return Validate(header, entries);
}

LONG __stdcall CHUI_OpenDialog(HWND ownerWindow, CHUI_DIALOG_HEADER* header,
    CHUI_ENTRY_RECORD* entries, HWND completionButton)
{
    const LONG valid = Validate(header, entries);
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
    data->instanceId = g_nextInstance.fetch_add(1);
    if (!data->instanceId) data->instanceId = g_nextInstance.fetch_add(1);
    data->title = Utf8ToWide(header->title);
    if (data->title.empty()) data->title = L"Structured Dialog";
    data->entries.reserve(header->entryCount);
    for (DWORD index = 0; index < header->entryCount; ++index) {
        RuntimeEntry runtime{};
        runtime.definition = entries[index];
        runtime.sourceIndex = index;
        runtime.workingValue = entries[index].value[0]
            ? entries[index].value : entries[index].defaultValue;
        data->byId.emplace(runtime.definition.id, index);
        data->entries.push_back(std::move(runtime));
    }
    header->instanceId = data->instanceId;

    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&DialogProc), &module);
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, kDialogClass, data->title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 590, ownerWindow, nullptr, module, data);
    if (!window) {
        delete data;
        return CHUI_ERROR_WINDOW;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_byOwner[ownerWindow] = window;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    return static_cast<LONG>(data->instanceId);
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
