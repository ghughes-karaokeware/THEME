# Changelog

## 2026-08-07

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
