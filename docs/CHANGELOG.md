# Changelog

- Corrected Promo Designer header/toolbar spacing and removed native owner-draw background/focus artifacts from its modern buttons and color swatches.
- Added internal `.PRM` Load, Save, and Clear commands plus transactional Auto-load-on-startup flag round-tripping.
- Redesigned the Promo Designer with a gradient title area, formatting band, bordered editor card, hover-responsive controls, an unclipped Auto-load option, and a balanced footer command bar.
- Reordered Promo formatting commands to place color swatches beside B/I/U, moved Undo/Redo/Clear to a separated trailing group, removed the Default button, and enabled the immersive dark Windows title bar.

## 2026-08-08

- Added the first native Promo Trailer Designer proof of concept: packed Clarion ABI, modeless transactional editor, legacy tag parser/canonical serializer, per-line isolation, color palette, Bold/Italic/Underline, Undo/Redo, keyboard formatting, CP1252/length validation, and shared hidden-button completion flow.
- Added automated Promo markup, ABI, and modeless OK/Cancel tests while retaining a passing Structured Setup regression harness and zero-warning Win32 Release build.
- Added `CHPromoDesignerTypes.inc` and exact Test_Setup Clarion embed instructions; the legacy `PROMOTRAILEREDITOR.TXA` remains analysis-only and unchanged.
- Corrected the Structured Setup guide's code-block contrast and added paste-ready Clarion supporting routines for loading, preparing, accepting/decoding, applying, live changes, actions, and close protection.
- Rebuilt the Structured Setup Win32 Release candidate with zero warnings/errors and passed the 32-bit ABI validation/round-trip harness.
- Backed up every active deployed DLL/library/include/document target, then hash-verified the RC DLL across ten runtime locations and the Clarion import library across nine linker locations.
- Added a dedicated hand-holding Structured Setup integration guide and linked it from the main Modern Theme Extensions guide; installed both guides and the paste-ready Clarion sample in the Karaokeware accessory documentation folder.
- Deferred hiding the font-size controls until after the Windows common font dialog completes native initialization, allowing it to create the Sample preview before removing size from the user interface.
- Restored the Windows font dialog's populated Sample preview by retaining an internal 12-point preview selection while continuing to hide the size label and selector from users.
- Integrated color into the single `CHUI_FONT` popup and composite value, now returned as `Face name|CDE style mask|COLORREF`; a separate `CHUI_COLOR` entry is no longer required for each heading.
- Added optional inline font-button association: a `CHUI_FONT` entry linked to a text entry by `DependencyID` renders a compact `FONT...` button on the same row.
- Corrected font-preview initialization to use an internal 12-point preview while keeping font size unavailable to the user, and hid the unsupported Strikeout control.
- Added `CHUI_FONT`, a single familiar Windows font popup that selects the installed font face plus Bold, Italic, and Underline attributes while hiding and disabling font-size selection.
- Defined the font value as `Face name|CDE style mask`; the popup updates only CDE font bits `0001H`, `0002H`, and `0004H`, preserving alignment, shadow, and future non-font style bits.
- Extended the Win32 ABI harness to validate and round-trip a font selection through Apply and Cancel; Win32 Release builds with zero warnings and errors.
- Added integrated `CHUI_FILE` and `CHUI_FOLDER` controls with a read-only path display, themed Browse button, modern Windows file/folder picker, and FILE filter definitions.
- Advanced the structured-dialog ABI to `00020000H` and enlarged every entry's single `Value` field to 4,096 bytes, allowing FILE/FOLDER settings to use the same declaration, validation, open, Apply, and OK flow as every other setting.
- Implemented transactional path defaults, Reset All, dirty detection, Apply/OK write-back, and Cancel-to-last-Apply behavior.
- Extended the 32-bit harness to update a FILE path longer than 128 bytes directly through its ordinary entry and verify Apply/Cancel round trips.
- Added ABI-compatible `CHUI_ACTION` entries and the `CHUI_ConsumeAction` export for invoking existing Clarion procedures from themed structured-dialog buttons.
- Extended the harness to verify action validation, ID delivery, dialog persistence, isolation from the completion queue, and subsequent Cancel completion.
- Added a themed `Reset All Settings` footer button with confirmation and transactional restoration of declared defaults; Cancel can still discard the reset.
- Added `CHUI_SetEntryValue` so Clarion action handlers can refresh a value in the open native dialog; inputs are bounded to the ABI field, dropdown/Boolean values are checked, and successful updates participate in Apply/Cancel.
- Extended the harness to verify live control refresh and rejection of an invalid programmatic Boolean value.
- Added explicit keyboard routing required by the custom modeless window, including Tab/Shift+Tab traversal, Escape cancellation/dropdown dismissal, and Enter acceptance from text and slider controls.
- Extended the harness to verify Escape cancellation from both fixed footer buttons and dynamically generated action buttons.
- Added a themed Apply button that is enabled only while structured-dialog values differ from the last accepted baseline.
- Apply validates and writes all values, queues `CHUI_RESULT_APPLY` through the existing completion notification, keeps the dialog open, and establishes a new Cancel rollback baseline.
- Extended the ABI harness to verify the Apply result, caller write-back, persistent dialog window, and disabled clean state.
- Updated and staged the Clarion include and integration guide; the ABI version and exported-function set remain unchanged.

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
- Added explicit `TrackMouseEvent` hover tracking and repaint states across navigation cards, owner-drawn buttons, dropdowns, color controls, and checkboxes.
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
