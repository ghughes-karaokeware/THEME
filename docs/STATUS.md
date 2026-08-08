# Current Status

Updated: 2026-08-07

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
- The isolated version-1 proof of concept is implemented in `dll/CHStructuredDialog.cpp` with six `CHUI_*` exports.
- The ABI is fixed at version `00010000H`, a 256-byte header, and a 1,408-byte entry record.
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
- Win32 Release and the 32-bit ABI/validation/round-trip harness pass with zero warnings after the color-entry addition; deployment remains pending manual test approval.

## Known build caveat

The CompuHost project reports existing redirection-copy warnings for legacy DLL dependencies. These warnings predate the latest sheet corrections and are not emitted by `CHModernSheet`.
