#include "../dll/CHTheme.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

template <typename T>
T Load(HMODULE module, const char* name)
{
    return reinterpret_cast<T>(GetProcAddress(module, name));
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) return 3;

    using GetValue = DWORD(__stdcall*)();
    using Validate = LONG(__stdcall*)(const CHUI_DIALOG_HEADER*,
        const CHUI_ENTRY_RECORD*);
    using ValidateEx = LONG(__stdcall*)(const CHUI_DIALOG_HEADER*,
        const CHUI_ENTRY_RECORD*, const CHUI_PATH_RECORD*, DWORD);
    using Open = LONG(__stdcall*)(HWND, CHUI_DIALOG_HEADER*, CHUI_ENTRY_RECORD*, HWND);
    using OpenEx = LONG(__stdcall*)(HWND, CHUI_DIALOG_HEADER*, CHUI_ENTRY_RECORD*,
        CHUI_PATH_RECORD*, DWORD, HWND);
    using Consume = LONG(__stdcall*)(HWND, DWORD*, LONG*);
    using ConsumeChange = LONG(__stdcall*)(HWND, DWORD*, DWORD*);
    using SetEntryValue = LONG(__stdcall*)(DWORD, DWORD, const char*);
    const GetValue getVersion = Load<GetValue>(module, "CHUI_GetAbiVersion");
    const GetValue getHeaderSize = Load<GetValue>(module, "CHUI_GetHeaderSize");
    const GetValue getEntrySize = Load<GetValue>(module, "CHUI_GetEntrySize");
    const GetValue getPathSize = Load<GetValue>(module, "CHUI_GetPathRecordSize");
    const Validate validate = Load<Validate>(module, "CHUI_ValidateDialog");
    const ValidateEx validateEx = Load<ValidateEx>(module, "CHUI_ValidateDialogEx");
    const Open openDialog = Load<Open>(module, "CHUI_OpenDialog");
    const OpenEx openDialogEx = Load<OpenEx>(module, "CHUI_OpenDialogEx");
    const Consume consume = Load<Consume>(module, "CHUI_ConsumeCompletion");
    const ConsumeChange consumeChange = Load<ConsumeChange>(module,
        "CHUI_ConsumeChange");
    const ConsumeChange consumeAction = Load<ConsumeChange>(module,
        "CHUI_ConsumeAction");
    const SetEntryValue setEntryValue = Load<SetEntryValue>(module,
        "CHUI_SetEntryValue");
    if (!getVersion || !getHeaderSize || !getEntrySize || !getPathSize ||
        !validate || !validateEx || !openDialog || !openDialogEx || !consume ||
        !consumeChange || !consumeAction || !setEntryValue) return 4;

    if (getVersion() != 0x00010000 ||
        getHeaderSize() != sizeof(CHUI_DIALOG_HEADER) ||
        getEntrySize() != sizeof(CHUI_ENTRY_RECORD) ||
        getPathSize() != sizeof(CHUI_PATH_RECORD)) return 5;

    CHUI_DIALOG_HEADER header{};
    CHUI_ENTRY_RECORD entries[3]{};
    header.version = getVersion();
    header.headerSize = sizeof(header);
    header.entrySize = sizeof(entries[0]);
    header.entryCount = 3;
    strcpy_s(header.title, "ABI test");

    entries[0].type = CHUI_PANEL;
    entries[0].id = 100;
    entries[0].iconId = CHUI_ICON_AUDIO;
    strcpy_s(entries[0].caption, "Audio");
    entries[1].type = CHUI_PANEL;
    entries[1].id = 110;
    entries[1].parentId = 100;
    strcpy_s(entries[1].caption, "General");
    entries[2].type = CHUI_DROPDOWN;
    entries[2].id = 111;
    entries[2].parentId = 110;
    entries[2].type = CHUI_COLOR;
    entries[2].options[0] = '\0';
    strcpy_s(entries[2].value, "16711680");
    if (validate(&header, entries) != CHUI_STATUS_OK) return 17;
    entries[2].type = CHUI_DROPDOWN;
    strcpy_s(entries[2].value, "SHARED");
    strcpy_s(entries[2].options, "SHARED=Shared|EXCLUSIVE=Exclusive");
    strcpy_s(entries[2].caption, "Mode");
    strcpy_s(entries[2].value, "SHARED");
    strcpy_s(entries[2].defaultValue, "SHARED");
    strcpy_s(entries[2].options, "SHARED=Shared|EXCLUSIVE=Exclusive");

    if (validate(&header, entries) != CHUI_STATUS_OK) return 6;
    const DWORD savedSize = header.entrySize;
    header.entrySize = savedSize - 1;
    if (validate(&header, entries) != CHUI_ERROR_ENTRY_SIZE) return 7;
    header.entrySize = savedSize;
    entries[2].parentId = 999;
    if (validate(&header, entries) != CHUI_ERROR_PARENT) return 8;
    entries[2].parentId = 110;

    WNDCLASSW ownerClass{};
    ownerClass.lpfnWndProc = DefWindowProcW;
    ownerClass.hInstance = GetModuleHandleW(nullptr);
    ownerClass.lpszClassName = L"CHUI.AbiTestOwner";
    RegisterClassW(&ownerClass);
    HWND owner = CreateWindowW(ownerClass.lpszClassName, L"ABI owner",
        WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr,
        ownerClass.hInstance, nullptr);
    HWND notification = CreateWindowW(L"BUTTON", L"", WS_CHILD,
        0, 0, 1, 1, owner, reinterpret_cast<HMENU>(900),
        ownerClass.hInstance, nullptr);
    if (!owner || !notification) return 9;

    const LONG cancelInstance = openDialog(owner, &header, entries, notification);
    if (cancelInstance <= 0) return 10;
    HWND dialog = FindWindowW(L"CHTheme.StructuredDialog", L"ABI test");
    if (!dialog) return 11;
    const LONG_PTR style = GetWindowLongPtrW(dialog, GWL_STYLE);
    if (!(style & WS_THICKFRAME) || !(style & WS_MAXIMIZEBOX)) return 19;
    if (!GetDlgItem(dialog, 105)) return 28;
    SetWindowPos(dialog, nullptr, 0, 0, 850, 500,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RECT clientBounds{};
    GetClientRect(dialog, &clientBounds);
    HWND minimumOk = GetDlgItem(dialog, IDOK);
    RECT okBounds{};
    GetWindowRect(minimumOk, &okBounds);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&okBounds), 2);
    if (okBounds.top < 0 || okBounds.bottom > clientBounds.bottom) return 21;
    RECT beforeResize{};
    GetWindowRect(dialog, &beforeResize);
    SetWindowPos(dialog, nullptr, 0, 0,
        (beforeResize.right - beforeResize.left) + 180,
        (beforeResize.bottom - beforeResize.top) + 100,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RECT afterResize{};
    GetWindowRect(dialog, &afterResize);
    if (afterResize.right - afterResize.left <=
        beforeResize.right - beforeResize.left) return 20;
    SendMessageW(minimumOk, WM_KEYDOWN, VK_ESCAPE, 0);
    DWORD completedInstance = 0;
    LONG result = -1;
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(cancelInstance) ||
        result != CHUI_RESULT_CANCEL || std::strcmp(entries[2].value, "SHARED")) return 12;

    entries[2].type = CHUI_CHECKBOX;
    entries[2].flags = CHUI_FLAG_LIVE_NOTIFY;
    strcpy_s(entries[2].caption, "Enabled");
    strcpy_s(entries[2].value, "1");
    entries[2].options[0] = '\0';
    const LONG okInstance = openDialog(owner, &header, entries, notification);
    if (okInstance <= 0) return 13;
    dialog = FindWindowW(L"CHTheme.StructuredDialog", L"ABI test");
    if (!dialog) return 14;
    HWND checkbox = GetDlgItem(dialog, 1002);
    if (!checkbox) return 15;
    if (!setEntryValue(static_cast<DWORD>(okInstance), 111, "0") ||
        setEntryValue(static_cast<DWORD>(okInstance), 111, "invalid")) return 29;
    checkbox = GetDlgItem(dialog, 1002);
    if (!checkbox || SendMessageW(checkbox, BM_GETCHECK, 0, 0) != BST_UNCHECKED)
        return 30;
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED),
        reinterpret_cast<LPARAM>(checkbox));
    DWORD changedInstance = 0;
    DWORD changedEntry = 0;
    if (!consumeChange(notification, &changedInstance, &changedEntry) ||
        changedInstance != static_cast<DWORD>(okInstance) || changedEntry != 111 ||
        std::strcmp(entries[2].value, "0")) return 18;
    HWND applyButton = GetDlgItem(dialog, 104);
    if (!applyButton || !IsWindowEnabled(applyButton)) return 21;
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(104, BN_CLICKED),
        reinterpret_cast<LPARAM>(applyButton));
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(okInstance) ||
        result != CHUI_RESULT_APPLY || std::strcmp(entries[2].value, "0") ||
        !IsWindow(dialog) || IsWindowEnabled(applyButton)) return 22;
    HWND okButton = GetDlgItem(dialog, IDOK);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
        reinterpret_cast<LPARAM>(okButton));
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(okInstance) ||
        result != CHUI_RESULT_OK || std::strcmp(entries[2].value, "0")) return 16;

    entries[2].type = CHUI_ACTION;
    entries[2].flags = 0;
    strcpy_s(entries[2].caption, "Select audio device...");
    entries[2].value[0] = '\0';
    if (validate(&header, entries) != CHUI_STATUS_OK) return 23;
    const LONG actionInstance = openDialog(owner, &header, entries, notification);
    if (actionInstance <= 0) return 24;
    dialog = FindWindowW(L"CHTheme.StructuredDialog", L"ABI test");
    HWND actionButton = dialog ? GetDlgItem(dialog, 1002) : nullptr;
    if (!actionButton) return 25;
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED),
        reinterpret_cast<LPARAM>(actionButton));
    DWORD actionInstanceResult = 0;
    DWORD actionEntry = 0;
    if (!consumeAction(notification, &actionInstanceResult, &actionEntry) ||
        actionInstanceResult != static_cast<DWORD>(actionInstance) ||
        actionEntry != 111 || !IsWindow(dialog) ||
        consume(notification, &completedInstance, &result)) return 26;
    SendMessageW(actionButton, WM_KEYDOWN, VK_ESCAPE, 0);
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(actionInstance) ||
        result != CHUI_RESULT_CANCEL) return 27;

    entries[2].type = CHUI_FILE;
    strcpy_s(entries[2].caption, "Background image");
    strcpy_s(entries[2].options, "*.png=PNG images|*.*=All files");
    CHUI_PATH_RECORD path{};
    path.entryId = 111;
    strcpy_s(path.value, "C:\\Images\\original.png");
    strcpy_s(path.defaultValue, "C:\\Images\\default.png");
    if (validateEx(&header, entries, &path, 1) != CHUI_STATUS_OK ||
        openDialog(owner, &header, entries, notification) != CHUI_ERROR_PATH_COUNT)
        return 31;
    const LONG pathInstance = openDialogEx(owner, &header, entries, &path, 1,
        notification);
    if (pathInstance <= 0) return 32;
    dialog = FindWindowW(L"CHTheme.StructuredDialog", L"ABI test");
    HWND browseButton = dialog ? GetDlgItem(dialog, 1002) : nullptr;
    HWND pathDisplay = dialog ? GetDlgItem(dialog, 3002) : nullptr;
    if (!browseButton || !pathDisplay) return 33;
    const char* updatedPath =
        "C:\\A deliberately long folder name used to prove that the companion "
        "path record is not limited by the ordinary 128-byte entry value field\\"
        "selected-background-image.png";
    if (!setEntryValue(static_cast<DWORD>(pathInstance), 111, updatedPath)) return 34;
    applyButton = GetDlgItem(dialog, 104);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(104, BN_CLICKED),
        reinterpret_cast<LPARAM>(applyButton));
    if (!consume(notification, &completedInstance, &result) ||
        result != CHUI_RESULT_APPLY || std::strcmp(path.value, updatedPath) ||
        !IsWindow(dialog)) return 35;
    SendMessageW(dialog, WM_CLOSE, 0, 0);
    if (!consume(notification, &completedInstance, &result) ||
        result != CHUI_RESULT_CANCEL || std::strcmp(path.value, updatedPath)) return 36;
    DestroyWindow(owner);

    std::printf("ABI=%08lX header=%lu entry=%lu path=%lu validation=PASS roundtrip=PASS\n",
        static_cast<unsigned long>(getVersion()),
        static_cast<unsigned long>(getHeaderSize()),
        static_cast<unsigned long>(getEntrySize()),
        static_cast<unsigned long>(getPathSize()));
    FreeLibrary(module);
    return 0;
}
