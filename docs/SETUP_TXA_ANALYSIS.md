# Legacy Setup TXA Analysis

Source: `F:\Invicion Software Code\Clarion 10 Projects\Test_App\Test_Setup\Setup.TXA`

SHA-256: `A62B0B72BE0052D40C85EF4E071CB17151FF6A78C374FDC1CE3DF506E21AAB0D`

The TXA is analysis-only. Its referenced variables and procedures do not exist in `Test_Setup.app` and must not be added to that test application.

## Stored settings suited to declarative entries

| Existing setting/control | Existing value | Proposed V4 location | Structured type | Notes |
|---|---|---|---|---|
| Display Heading Lines 1-3 | `CON:DisplayLine1`, `CON:DisplayLine3`, `CON:DisplayLine4` | Displays / Performer Display / Text | ENTRY | Caption strings; preserve current/default value. |
| Display Line 4-5 | `CON:DisplayLine5`, `CON:DisplayLine6` | Displays / Performer Display / Text | ENTRY | Existing fields allow longer strings. |
| Hide current performer | `GLO:HidePerfDisp` via `LOC:HidePerfDispl` | Displays / Performer Display / Text | CHECKBOX | Temporary local copy already demonstrates prepare/apply behavior. |
| Secondary-screen size/mode | `CON:ScreenBSize`, `GLO:CDGScreenSize` | Displays / Secondary Display / Layout | RADIO or DROPDOWN | Stable numeric values 0-3; legacy options include inactive, 640x480, 800x600, 1024x768. |
| Override automatic screen positioning | `GLO:OverrideAutoScreenPos` | Displays / Secondary Display / Position | CHECKBOX | Existing handler also writes the registry immediately; V4 should apply on OK unless immediate persistence is explicitly required. |
| CDG playback synchronization | `CON:CDGPlayBackDelay`, `GLO:CDGPlayBackDelay` | Karaoke Player / Playback / Synchronization | NUMBER or SLIDER | Legacy range is normally -3000..3000 step 20, narrowed to -300..300 for one timing mode. |
| Spectrum display | `CON:SpectrumStatus`, `GLO:SpectrumStatus` | Displays / Host Display / Meters | CHECKBOX | Restart currently required. |
| VU meter display | `CON:VUStatus`, `GLO:VUStatus` | Displays / Host Display / Meters | CHECKBOX | Restart currently required. |
| Oscilloscope display | `CON:OscilliscopeStatus`, `GLO:OscilliscopeStatus` | Displays / Host Display / Meters | CHECKBOX | Restart currently required. |
| Host lyrics display | `CON:HostCDGScreenStatus`, `GLO:HostCDGScreenStatus` | Displays / Host Display / Lyrics | CHECKBOX | Restart currently required. |
| Disable theme support | `GLO:DisableTheme` | Appearance / Theme / Compatibility | CHECKBOX | Existing handler writes registry immediately; mark restart required. |
| Audio device | `CON:SoundCardIndex`, `GLO:SoundCardIndex` | Audio / Output Devices | DROPDOWN | Legacy selection is delegated to `SelectAudioDevice`; V4 should use stable device identifiers rather than display captions/index where available. |
| Recording device/format | `CON:INPIndex`, `CON:RECIndex`, `CON:RecordSampleRate`, `CON:RecordFormat`, `CON:RecordFolder`, `CON:RecordChannels` | Audio / Record & Archive | DROPDOWN/NUMBER/FOLDER | A third panel is appropriate for advanced format and channel properties. |
| Filler audio output | `CON:FillerSoundCardIndex`, `GLO:FillerSoundCardIndex` | Audio / Output Devices | DROPDOWN | Keep separate from primary output. |

## Existing defaults and initialization

When no Config record is available, Setup assigns defaults including spectrum/VU/oscilloscope/host-display enabled, screen size 2, sound-card index 0, fade durations 5, fade time 10, shuffle disabled, empty playlist filename, and maximum volume 0. It reads the Config record otherwise and uses a local `isthere` flag to select ADD versus PUT.

The current Setup also derives screen dimensions from `GetSystemMetrics`, conditionally narrows the CDG timing range, enables position controls only when the auxiliary window is available, and initializes preview colors from global font/color variables.

## Behavior that must not be modeled as passive values

- Font and color selection calls `GetFonts`, then immediately updates the live Scroller window and posts `EVENT:ResizeAuxFonts`.
- Auxiliary-screen arrow controls modify offsets repeatedly from timer/mouse handling and post reposition events to another thread/window.
- Save Current Setup and Recall Setup perform file dialogs, close/open/copy CFG and button files, and reload many globals.
- Audio, recording, scrolling, background-image, capture-device, and hardware-mode buttons launch separate procedures.
- Advanced support opens `system2.ini` in Notepad after a warning.
- Registry writes currently occur immediately for theme and positioning options.
- Save and Exit writes Config, registry values, globals, restart reminders, and cross-component state.

These belong to explicit future `ACTION` handling or application-side completion processing. They are intentionally excluded from the callback-free proof of concept.

## Migration risks discovered

- The legacy dialog mixes editing, persistence, live preview, file management, device discovery, cross-thread window manipulation, and restart notifications.
- Some fields are copied through several layers (`CON`, `GLO`, locals, Config), so the accepted destination must be documented per setting before migration.
- Cancel currently copies `LOC:HidePerfDispl` back to `GLO:HidePerfDisp`, which is unusual and should be reviewed rather than reproduced blindly.
- Several settings write the registry immediately, meaning legacy Cancel may not fully roll them back.
- Device indices are unstable compared with persistent device IDs.
- The TXA contains legacy hidden controls and absolute resource paths; neither should shape the new declarative layout.

## Recommended V4 categories

General; Karaoke Player; Audio; Displays; Song Queue; Filler Music; Automation; Songbooks Online; File Management; Appearance; Keyboard & Shortcuts; Advanced.

The supplied TXA content primarily belongs under Audio, Displays, Appearance, and Advanced. Production migration must wait for the Test_Setup ABI and OK/Cancel round trip to be proven.

