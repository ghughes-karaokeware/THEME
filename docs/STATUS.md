# Current Status

Updated: 2026-08-06

## Stable baseline

- `CHTheme.dll` is the latest verified C++ build.
- The C++ source `dll/CHTheme.cpp` matches the latest staged list-overlay visibility revision.
- `ButtonSubclass` is preserved in the complete Visual C++ project.
- Current Clarion extension sources are under `clarion/`.

## Recently verified corrections

- Native sliders follow their assigned REGION and Clarion parent hierarchy.
- Clarion exclusively controls ordinary TAB-content visibility.
- A zero `PROP:ChoiceFEQ` is not forced to the first TAB.
- Programmatically restored Clarion TAB choices update the custom strip highlight.
- Custom strip clicks update Clarion selection without manually hiding ordinary controls.
- Test_CDG and CompuHost V4 Win32 Release builds succeeded after the TAB corrections.

## Current testing phase

Continue broad bug testing of the Theme and ButtonSubclass DLL integration. Preserve the TAB-content and TAB-highlight ownership rules above.

## Known build caveat

The CompuHost project reports existing redirection-copy warnings for legacy DLL dependencies. These warnings predate the latest sheet corrections and are not emitted by `CHModernSheet`.

