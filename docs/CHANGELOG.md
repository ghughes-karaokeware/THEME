# Changelog

## 2026-08-07

- Added ABI-compatible `CHUI_COLOR` structured-dialog entries with a current-color swatch, native Windows color picker, decimal Clarion `LONG` write-back, and Cancel preservation.
- Added `CHUI_COLOR EQUATE(16)` to the Clarion include and linked the Windows common-dialog library for Win32 builds.
- Extended the ABI harness to validate color-entry records and rebuilt Win32 Release with zero warnings and zero errors.
- Re-normalized `CHStructuredDialogTypes.inc` to ASCII CRLF after adding the color equate and verified zero bare linefeeds in both the repository and staged Test_Setup copies.
- Added opt-in `CHUI_FLAG_LIVE_NOTIFY` entries and the `CHUI_ConsumeChange` export so selected controls can trigger distinguishable Clarion events for real-time preview without making every setting live.
- Guarded rendering when a category has no selected page, preventing a root category from being misidentified as an empty advanced/detail panel.
- Made the structured setup window resizable/maximizable, removed forced fixed widths, added responsive two/three-panel layout and minimum tracking sizes, and extended the harness to verify real resizing.
- Corrected minimum-size footer clipping by positioning against the actual client height, and replaced square native dropdown faces with rounded shaded owner-drawn selectors while preserving the existing dropdown ABI and live notifications.
- Restored real attached dropdown-list behavior with dark owner-drawn items, and moved the rounded/shaded card treatment to category/page navigation where it appears in the target design; navigation `HelpText` now renders as a subtitle.
- Subclassed the closed combo face and checkbox paint path so Windows no longer exposes classic white arrow buttons or checkbox squares, while preserving native interaction and existing structured-dialog ABI behavior.
- Verified the Structured Dialog proof of concept from Clarion in both two-panel and three-panel configurations.
- Preserved ABI version `00010000H` and all existing Clarion integration declarations.
- Enabled Windows dark styling for the native title bar and child controls.
- Corrected the advanced-panel heading so it cannot display a misdecoded UTF-8 dash.
- Added a functional third-panel Back button; changing category or page makes the applicable detail panel available again.
- Increased value-label height and enabled end ellipsis to prevent multi-line clipping into adjacent controls.
- Replaced the fixed value-label width with font-aware per-panel measurement, keeping each panel aligned while reserving at least 100 pixels for input controls.
- Added an `Advanced Settings...` button after returning from a nested detail panel so the third panel can be reopened without changing the selected page.
- Replaced the standard white command buttons with DLL-owned dark rounded rendering, including blue primary, secondary, pressed, disabled, and keyboard-focus states.
- Added 18 built-in CompuHost-oriented icon equates, reused the reserved entry-number slot as ABI-compatible `IconID`, and added scalable theme-colored icon rendering to category and page navigation.
- Made a hidden nested panel's launcher use that panel's full caption and `IconID`, with a 230-pixel layout that displays `Device Advanced Settings` without truncation.
- Increased visual separation with distinct near-black window, raised navigation/content, and input surfaces; added subtle panel borders and a stronger bordered selection blue based on the target CompuHost design.
- Rebuilt Win32 Release with zero warnings and zero errors and passed the 32-bit ABI/validation/round-trip harness.

## 2026-08-06

- Consolidated the complete Theme and ButtonSubclass C++ projects into a permanent Codex workspace.
- Consolidated the latest verified Clarion wrapper and template sources.
- Preserved staged C++ milestones and immutable recovery manifests.
- Removed the forced first-TAB fallback.
- Returned ordinary TAB-content visibility ownership to Clarion.
- Added two-way selection synchronization between restored Clarion choices and the custom strip.
- Preserved REGION/ancestor-based visibility for native slider overlays.
- Updated the installed Modern Theme Extensions HTML documentation with a concise CompuHost V4 implementation guide, current SHEET ownership rules, slider visibility behavior, verification steps, and troubleshooting guidance.
- Audited the legacy Setup TXA and documented the finalized Structured Dialog ABI, memory ownership, completion notification, proof-of-concept scope, migration inventory, and behaviors requiring explicit actions.
- Implemented the isolated modeless Structured Dialog proof of concept, versioned packed ABI, hierarchy validation, two/three-level navigation, initial native controls, declarative dependency handling, transactional OK/Cancel behavior, one-shot completion queue, Clarion type include, exact Test_Setup embed guide, and 32-bit ABI/round-trip harness.
- Corrected `CHStructuredDialogTypes.inc` to Clarion-compatible ASCII/Windows CRLF line endings and added Git attributes that preserve CRLF for Clarion source formats.
- Corrected the Test_Setup external `MODULE('CHTheme.dll')` examples to use Clarion MAP prototype syntax without the invalid `PROCEDURE` keyword.
- Simplified the Clarion test ABI declarations to equates plus procedure-local GROUPs passed through `ADDRESS()` and replaced illegal button-embed `EXIT` statements with `CYCLE`.
- Documented that Clarion generation can overwrite the test DLL with the older accessory copy and established a protected `StructuredDialogCandidate` DLL as the LIBMaker and post-build runtime source.
