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
    using Open = LONG(__stdcall*)(HWND, CHUI_DIALOG_HEADER*, CHUI_ENTRY_RECORD*, HWND);
    using Consume = LONG(__stdcall*)(HWND, DWORD*, LONG*);
    const GetValue getVersion = Load<GetValue>(module, "CHUI_GetAbiVersion");
    const GetValue getHeaderSize = Load<GetValue>(module, "CHUI_GetHeaderSize");
    const GetValue getEntrySize = Load<GetValue>(module, "CHUI_GetEntrySize");
    const Validate validate = Load<Validate>(module, "CHUI_ValidateDialog");
    const Open openDialog = Load<Open>(module, "CHUI_OpenDialog");
    const Consume consume = Load<Consume>(module, "CHUI_ConsumeCompletion");
    if (!getVersion || !getHeaderSize || !getEntrySize || !validate ||
        !openDialog || !consume) return 4;

    if (getVersion() != 0x00010000 ||
        getHeaderSize() != sizeof(CHUI_DIALOG_HEADER) ||
        getEntrySize() != sizeof(CHUI_ENTRY_RECORD)) return 5;

    CHUI_DIALOG_HEADER header{};
    CHUI_ENTRY_RECORD entries[3]{};
    header.version = getVersion();
    header.headerSize = sizeof(header);
    header.entrySize = sizeof(entries[0]);
    header.entryCount = 3;
    strcpy_s(header.title, "ABI test");

    entries[0].type = CHUI_PANEL;
    entries[0].id = 100;
    strcpy_s(entries[0].caption, "Audio");
    entries[1].type = CHUI_PANEL;
    entries[1].id = 110;
    entries[1].parentId = 100;
    strcpy_s(entries[1].caption, "General");
    entries[2].type = CHUI_DROPDOWN;
    entries[2].id = 111;
    entries[2].parentId = 110;
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
    SendMessageW(dialog, WM_CLOSE, 0, 0);
    DWORD completedInstance = 0;
    LONG result = -1;
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(cancelInstance) ||
        result != CHUI_RESULT_CANCEL || std::strcmp(entries[2].value, "SHARED")) return 12;

    entries[2].type = CHUI_CHECKBOX;
    strcpy_s(entries[2].caption, "Enabled");
    strcpy_s(entries[2].value, "1");
    entries[2].options[0] = '\0';
    const LONG okInstance = openDialog(owner, &header, entries, notification);
    if (okInstance <= 0) return 13;
    dialog = FindWindowW(L"CHTheme.StructuredDialog", L"ABI test");
    if (!dialog) return 14;
    HWND checkbox = GetDlgItem(dialog, 1002);
    if (!checkbox) return 15;
    SendMessageW(checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
    HWND okButton = GetDlgItem(dialog, IDOK);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
        reinterpret_cast<LPARAM>(okButton));
    if (!consume(notification, &completedInstance, &result) ||
        completedInstance != static_cast<DWORD>(okInstance) ||
        result != CHUI_RESULT_OK || std::strcmp(entries[2].value, "0")) return 16;
    DestroyWindow(owner);

    std::printf("ABI=%08lX header=%lu entry=%lu validation=PASS roundtrip=PASS\n",
        static_cast<unsigned long>(getVersion()),
        static_cast<unsigned long>(getHeaderSize()),
        static_cast<unsigned long>(getEntrySize()));
    FreeLibrary(module);
    return 0;
}
