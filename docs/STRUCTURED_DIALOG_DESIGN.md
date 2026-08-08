# Structured Dialog Design

Updated: 2026-08-07

## Scope

`CHTheme.dll` will provide a generic, modeless native Structured Dialog. Clarion supplies a fixed contiguous description and current values; the DLL owns creation, layout, navigation, interaction, validation, working values, and theming. The first integration target is `Test_Setup.app`, not CompuHost.

## Interoperability contract

- Win32 only, using the project's established `__stdcall` / Clarion `PASCAL,RAW,NAME(...)` convention.
- Shared records contain fixed-width 32-bit integers and fixed UTF-8 byte arrays only.
- C++ records use explicit one-byte packing; the exported validation function also checks ABI version, header size, entry size, count, capacity, string termination, IDs, parents, and bounds.
- No Clarion QUEUE metadata, references, callbacks, C++ pointers, or application-variable addresses cross the DLL boundary.
- Clarion owns the memory and keeps it alive until the single completion notification.
- The DLL copies definitions and working values internally. Cancel and title-bar close leave Clarion memory untouched. OK copies accepted values only into the original entries' `Value` fields.
- The DLL releases every caller pointer before posting completion.

## Initial entry model

Structural types: `PANEL`, `GROUP`, `HEADING`, and `SEPARATOR`.

Initial value types: `ENTRY`, `NUMBER`, `DROPDOWN`, `CHECKBOX`, `RADIO`, and `SLIDER`.

Reserved type values permit later `PASSWORD`, `FILE`, `FOLDER`, `COLOR`, and `HOTKEY` additions without changing the version-1 record.

Each entry contains version/size information, type, ID, parent ID, flags, dependency fields, numeric limits, a built-in icon ID, caption, current value, default value, dropdown options, help text, and reserved bytes. Dropdown options use bounded `stored-value=Display caption` pairs; stored values therefore remain stable when captions change.

The original reserved 32-bit number at byte offset 36 is now named `IconID`. Values `CHUI_ICON_NONE` through `CHUI_ICON_INFORMATION` select DLL-owned, theme-colored scalable glyphs. This consumes an existing reserved slot, so ABI version `00010000H`, the 1,408-byte record size, and every subsequent field offset remain unchanged. Unknown icon IDs render as no icon for forward compatibility.

## Minimal API

```cpp
DWORD __stdcall CHUI_GetAbiVersion(void);
DWORD __stdcall CHUI_GetHeaderSize(void);
DWORD __stdcall CHUI_GetEntrySize(void);
LONG  __stdcall CHUI_ValidateDialog(const CHUI_DIALOG_HEADER*, const CHUI_ENTRY*);
LONG  __stdcall CHUI_OpenDialog(HWND owner, CHUI_DIALOG_HEADER*, CHUI_ENTRY*, HWND completionButton);
LONG  __stdcall CHUI_ConsumeCompletion(HWND completionButton, DWORD* instanceId, LONG* result);
```

The validation function must be usable before any UI is created so the first Clarion test can prove structure size, offsets, strings, and consecutive-record walking independently.

## Completion mechanism

Use the existing hidden Clarion notification-button convention. The DLL posts one `WM_COMMAND` notification when the dialog finishes; Clarion receives the button's `EVENT:Accepted` and calls `CHUI_ConsumeCompletion`. This avoids replacing the Clarion window procedure or maintaining a Clarion callback while preserving modeless operation.

## Dependencies

Version 1 supports a deliberately small rule: an entry may depend on another entry ID being equal or unequal to a supplied value. This is sufficient for enable/disable and show/hide behavior such as enabling Crossfade Duration only when Auto Crossfade is enabled. More complex expressions are intentionally excluded.

## Clarion variable mapping

Application-variable metadata remains Clarion-only. A later helper may map Entry ID to `ADDRESS()`, datatype, and size, but the DLL will never dereference arbitrary CompuHost globals. The proof of concept uses explicit prepare/apply assignments before generic BYTE, SHORT, LONG, STRING, and CSTRING mappings are considered.

## Proof-of-concept content

- Audio primary category.
- General level-2 page containing checkbox, number, dropdown, and slider controls.
- Output Devices level-2 page that exposes an Advanced Settings third panel.
- Current values, defaults, stable dropdown values, one dependency, OK commit, Cancel preservation, and one completion event.
- Optional diagnostics for ABI validation, parsed entries, bad parents/options, window creation, result, committed values, and notification delivery.

## Deferred beyond the current proof of concept

Search, application-supplied icon registration, file/folder pickers, tables, actual CompuHost globals, and migration of the production Setup dialog. Color selection, callback-free action notifications, and transactional reset-all have since been implemented without changing the version-1 record layout.
