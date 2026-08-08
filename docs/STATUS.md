# Current Status

Updated: 2026-08-08

## Stable baseline

- `CHTheme.dll` is the latest verified C++ build.
- The C++ source `dll/CHTheme.cpp` matches the latest staged list-overlay visibility revision.
- `ButtonSubclass` is preserved in the complete Visual C++ project.
- Current Clarion extension sources are under `clarion/`.
- The maintained HTML guide is under `docs/` and is installed in the Clarion accessory documentation directory.

## Recently verified corrections

- Native sliders follow their assigned REGION and Clarion parent hierarchy.
- Clarion exclusively controls ordinary TAB-content visibility.
- A zero `PROP:ChoiceFEQ` is not forced to the first TAB.
- Programmatically restored Clarion TAB choices update the custom strip highlight.
- Custom strip clicks update Clarion selection without manually hiding ordinary controls.
- Test_CDG and CompuHost V4 Win32 Release builds succeeded after the TAB corrections.

## Current testing phase

Continue broad bug testing of the Theme and ButtonSubclass DLL integration. Preserve the TAB-content and TAB-highlight ownership rules above.

## Structured Dialog development

- The legacy `Setup.TXA` has been reviewed as an analysis-only source.
- The Structured Dialog ABI, ownership rules, completion mechanism, dependency scope, and Test_Setup proof-of-concept boundary are recorded in `STRUCTURED_DIALOG_DESIGN.md`.
- The legacy control and behavior inventory is recorded in `SETUP_TXA_ANALYSIS.md`.
- The Structured Dialog is implemented in `dll/CHStructuredDialog.cpp`; later exports extend its original six-function surface.
- The current ABI is version `00020000H`, with a 256-byte header and a 5,376-byte entry record. Version 2 enlarges the single entry `Value` field to 4,096 bytes.
- Win32 Release builds with deployment disabled and zero compiler warnings.
- The 32-bit `/W4 /WX` harness verifies ABI sizes, valid and invalid record walking, Cancel preservation, OK write-back to the same structure, and completion consumption.
- `Test_Setup.app` itself remains unmodified. The test-only DLL, shared include, and paste-ready integration guide are staged beside it for manual Clarion integration.
- The proof-of-concept DLL has not been installed into CompuHost or the Clarion accessory directories.
- The first visual-refinement pass preserves ABI `00010000H` while enabling native dark-window/control styling, preventing value-label clipping, correcting the advanced-panel heading, and adding a functional Back button that returns from the third panel to the selected page.
- Value-label columns are measured from the active panel's captions using the dialog font, remain aligned within each panel, and preserve a minimum 100-pixel input width.
- After using Back from a nested detail panel, the selected page exposes an `Advanced Settings...` button that reopens that same third panel.
- The four DLL-managed command buttons use dark owner-drawn styling with a blue primary OK action and explicit pressed, disabled, and focus states.
- Category and page records accept 18 built-in CompuHost-oriented `IconID` equates. The DLL renders their scalable monochrome glyphs using the active navigation colors without changing ABI version or record size.
- Nested-panel launchers inherit the target panel's caption and built-in icon, so the second-panel action and third-panel destination remain visually connected.
- The window now uses distinct background, raised panel, navigation, and input colors with bordered panel cards and higher-contrast selected navigation rows.
- Win32 Release and the 32-bit ABI/round-trip harness both pass after the visual-refinement changes.
- `CHUI_COLOR` now provides an owner-drawn color swatch and native Windows color-selection dialog. The selected Clarion `LONG`/`COLORREF` value is transactional and is returned only after the setup dialog is accepted.
- Entries can opt into real-time Clarion notification with `CHUI_FLAG_LIVE_NOTIFY`; `CHUI_ConsumeChange` distinguishes those events from final completion and returns the dialog instance plus changed entry ID.
- Categories with no selected second-level page no longer interpret root categories as third-level detail panels, preventing empty `Advanced - Audio` views.
- The structured setup window is resizable and maximizable. Navigation, content/detail cards, dynamic controls, and command buttons follow the client area, with separate practical minimum widths for two- and three-panel views.
- Minimum-height footer placement now uses the real client rectangle, keeping OK and Cancel fully visible. `CHUI_DROPDOWN` uses a rounded, shaded DLL-owned selector face with ellipsis, focus, pressed, disabled, and current-selection states.
- `CHUI_DROPDOWN` is again a true owner-drawn combo/drop-list rather than a context menu. Category/page selections use inset rounded and shaded cards, with optional `HelpText` rendered as the target-style subtitle.
- Closed dropdown faces now receive complete DLL-owned dark rounded painting, including focus border, selected caption, and chevron. Checkboxes retain native input behavior while using compact rounded blue theme glyphs and matching focus/disabled text.
- DLL-owned mouse tracking now supplies distinct hover feedback for category/page cards, command/detail/color buttons, dropdown faces, and checkboxes without weakening selected or pressed-state contrast.
- The footer now includes a dirty-state-aware Apply button. Apply validates and writes all current values, signals the existing Clarion completion event with `CHUI_RESULT_APPLY`, remains open, and establishes the new Cancel baseline; OK uses the same value-application routine and then closes.
- `CHUI_ACTION` entries render as themed, hover-aware buttons and signal the existing hidden Clarion notification control without closing the dialog or changing its dirty state. `CHUI_ConsumeAction` returns the dialog instance and action entry ID so CompuHost can invoke its existing device, file-management, and advanced-settings procedures.
- The target-style `Reset All Settings` footer action asks for confirmation, loads every value entry's declared default into the dialog working copy, refreshes dependencies, and participates in normal Apply/OK/Cancel transactions without persisting immediately.
- `CHUI_SetEntryValue` lets an action handler safely return a bounded UTF-8 value to any value entry in the still-open dialog. The DLL validates dropdown and Boolean replacements, refreshes visible controls and dependencies, and updates the Apply dirty state without prematurely committing caller memory.
- All DLL-created interactive controls now participate in explicit modeless keyboard routing: Tab and Shift+Tab move through the dialog, Escape cancels (or first closes an open dropdown), and Enter accepts from edit/slider fields while native buttons, lists, combos, and checkboxes retain their normal keys.
- `CHUI_FILE` and `CHUI_FOLDER` entries render an integrated read-only path field and themed Browse button, use the Windows `IFileDialog` selector, and transact through their ordinary 4,096-byte `DialogEntries[].Value`. They use the same `CHUI_ValidateDialog` and `CHUI_OpenDialog` APIs as all other entries.
- `CHUI_FONT` opens a conventional Windows font popup with installed faces, style selection, integrated color, and populated preview while omitting font size. Its `Face name|style mask|color` value directly preserves CDE style flags outside Bold/Italic/Underline, and remains transactional under Apply/OK/Cancel.
- Font entries can associate with an existing text entry by ID and render as an inline `FONT...` button on the same row, allowing a compact selector for every CompuHost heading without separate color rows.
- The font popup retains an internal 12-point size solely to drive the Windows Sample preview; size controls remain hidden and the selected size is never returned or applied.
- Font-size and unsupported Strikeout controls are hidden only after the native font dialog completes initialization, preserving the internal selection needed to populate Sample.
- The preview-corrected DLL is staged in Test_Setup's `StructuredDialogCandidate` folder and verified byte-for-byte by SHA-256; CompuHost and Clarion accessory deployment remain untouched.
- Win32 Release and the 32-bit ABI/validation/round-trip harness pass with zero warnings after the color-entry addition; deployment remains pending manual test approval.

## Known build caveat

The CompuHost project reports existing redirection-copy warnings for legacy DLL dependencies. These warnings predate the latest sheet corrections and are not emitted by `CHModernSheet`.
