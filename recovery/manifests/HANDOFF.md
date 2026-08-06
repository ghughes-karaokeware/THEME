# Clarion Theme Extension Recovery Handoff

Created: 2026-08-06  
Original Codex task: `Continue Clarion theme extension`  
Original task ID: `019f99dc-11ee-7be0-a5a4-e6d90da22b54`

## Purpose

This package preserves the theming project after the original Codex task entered a repeated context-compaction loop. No build, installation, deployment, reversion, or application-source edit was performed while creating this recovery.

## Original task workspace

`C:\Users\Glen\Documents\Codex\2026-07-25\task-title-continue-compuhost-v4-clarion`

The workspace is not a usable Git repository. Although a `.git` directory exists, `git status` reports that it is not a repository. Recovery therefore relies on full copies, SHA-256 manifests, timestamps, existing backups, and direct file comparisons.

## Important current state

The workspace contains the active extension sources:

- `CHModernSheet.clw`
- `CHModernSheet.inc`
- `CHModernSlider.clw`
- `CHModernSlider.inc`
- `CHModernTheme.tpl`
- staged `CHTheme` C++ sources
- timestamped pre-change `CHModernSheet.clw` backups

The latest workspace-only change is in `CHModernSheet.clw`. It removes the previously introduced fallback that wrote the first registered TAB into `PROP:ChoiceFEQ` when Clarion reported zero. The working file now leaves a zero `ChoiceFEQ` untouched so Clarion remains solely responsible for the initial TAB selection.

That latest workspace change was **not deployed** during recovery. The installed Clarion and CompuHost copies retain the earlier forced-first-tab fallback.

## Unresolved issue

The native Win32 slider overlays must follow the visibility of their assigned Clarion REGION without changing the SHEET selection or hiding ordinary TAB contents.

Reported regression:

- If a non-slider TAB is restored at application startup, its contents can be missing.
- If the slider TAB is restored, its contents and sliders appear.
- Each slider already has an assigned Clarion REGION that should remain the visibility authority.

## Known deployed hashes at recovery time

- `CHTheme.dll`: `5BB4392793980C793BF3C8083E5F2B4250794099B8F9C2AEAFBCC3340BAA3007`
- Installed `CHModernTheme.tpl`: `9A747DE8F2E1E8EBB5903A738531BA7B76FAB35F5060D68C799305F6D0C99182`
- Installed `CHModernSlider.clw`: `9ACE4A34B4BD302A84B44B4A81F314596A4DD49769CB633E115A3A330BD8B5C1`
- Installed `CHModernSlider.inc`: `6426CBED44387900F783213AE5F6327FB9D12E49B8C51F6D7A1B133CE0B82DF5`
- Installed `CHModernSheet.clw`: `A5DB7B6487D20CAD74E6A849DE9DDC5C2F48E521FDD8DB607C2D256B6CD50EE5`

## Safe continuation procedure

1. Treat `workspace-snapshot` and `deployed-snapshot` as immutable evidence.
2. Review `DIFF-installed-vs-working-CHModernSheet.txt`.
3. Inspect `CHModernSlider.SyncVisibility` and confirm what `RegionFEQ{PROP:Hide}` reports when its parent TAB is inactive.
4. Do not let slider code write `SHEET{PROP:ChoiceFEQ}` or hide/show ordinary TAB contents.
5. Make the smallest source-only change first.
6. Show and validate the diff before copying files to Clarion or CompuHost.
7. Build Win32 Release only if the C++ DLL changes; require zero errors and warnings.
8. Confirm the relevant executable is closed before replacing any DLL.
9. Hash every deployed copy after installation.

## Package contents

- `workspace-snapshot`: full copy of the original task workspace.
- `deployed-snapshot`: readable deployed Clarion and CompuHost files, kept in source-specific subdirectories.
- `WORKSPACE-SHA256.csv`: hashes, sizes, and timestamps for the workspace snapshot.
- `DEPLOYED-SHA256.csv`: hashes, sizes, and timestamps for deployed files.
- `DIFF-installed-vs-working-CHModernSheet.txt`: exact comparison showing the undeployed removal of the forced first-TAB fallback.
- `Clarion-Theme-Recovery-20260806.zip`: portable archive of the recovery directory.

