# Current Status

Updated: 2026-08-06

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

## Known build caveat

The CompuHost project reports existing redirection-copy warnings for legacy DLL dependencies. These warnings predate the latest sheet corrections and are not emitted by `CHModernSheet`.
