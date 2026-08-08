# Test_Setup Clarion 10 Integration

This is the initial integration recipe for `F:\Invicion Software Code\Clarion 10 Projects\Test_App\Test_Setup\Test_Setup.app`. It uses artificial test values only; nothing from the reference `Setup.TXA` is required.

Do not edit generated `Test_Setup.clw` or `Test_Setup001.clw` directly. Add the following through Clarion embeds/window design so regeneration preserves it.

## 1. Install the shared type include

Copy `clarion\CHStructuredDialogTypes.inc` to a Clarion include search directory. Add this immediately after the existing global INCLUDE statements, before the global MAP:

```clarion
   INCLUDE('CHStructuredDialogTypes.inc')
```

## 2. Global MAP

Place inside the application's global `MAP`, alongside the generated member modules:

```clarion
     MODULE('CHTheme.dll')
CHUI_GetAbiVersion(),ULONG,PASCAL,RAW,NAME('CHUI_GetAbiVersion')
CHUI_GetHeaderSize(),ULONG,PASCAL,RAW,NAME('CHUI_GetHeaderSize')
CHUI_GetEntrySize(),ULONG,PASCAL,RAW,NAME('CHUI_GetEntrySize')
CHUI_GetPathRecordSize(),ULONG,PASCAL,RAW,NAME('CHUI_GetPathRecordSize')
CHUI_ValidateDialog(LONG HeaderAddress,LONG EntriesAddress),LONG,PASCAL,RAW,NAME('CHUI_ValidateDialog')
CHUI_ValidateDialogEx(LONG HeaderAddress,LONG EntriesAddress,LONG PathsAddress,ULONG PathCount),LONG,PASCAL,RAW,NAME('CHUI_ValidateDialogEx')
CHUI_OpenDialog(LONG OwnerHwnd,LONG HeaderAddress,LONG EntriesAddress,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHUI_OpenDialog')
CHUI_OpenDialogEx(LONG OwnerHwnd,LONG HeaderAddress,LONG EntriesAddress,LONG PathsAddress,ULONG PathCount,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHUI_OpenDialogEx')
CHUI_ConsumeCompletion(LONG CompletionButtonHwnd,*ULONG InstanceID,*LONG DialogResult),LONG,PASCAL,RAW,NAME('CHUI_ConsumeCompletion')
CHUI_ConsumeChange(LONG CompletionButtonHwnd,*ULONG InstanceID,*ULONG EntryID),LONG,PASCAL,RAW,NAME('CHUI_ConsumeChange')
CHUI_ConsumeAction(LONG CompletionButtonHwnd,*ULONG InstanceID,*ULONG EntryID),LONG,PASCAL,RAW,NAME('CHUI_ConsumeAction')
CHUI_SetEntryValue(ULONG InstanceID,ULONG EntryID,*CSTRING Value),LONG,PASCAL,RAW,NAME('CHUI_SetEntryValue')
     END
```

Create the Clarion-compatible `CHTheme.LIB` from the candidate DLL before linking, as with the project's other CHTheme exports. In the Test_Setup folder, use this protected copy as LIBMaker's input:

```text
F:\Invicion Software Code\Clarion 10 Projects\Test_App\Test_Setup\StructuredDialogCandidate\CHTheme.dll
```

Do not use the DLL that Clarion copies from the accessory directory during generation: until the candidate is promoted, that copy is the older production DLL and contains no `CHUI_*` exports. Clarion generation/build may overwrite the DLL beside `Test_Setup.exe`; after compiling and before running, restore the protected candidate DLL beside the EXE. The current candidate SHA-256 is recorded in `docs/HASHES.md` and must match the protected copy before testing.

## 3. Main window controls

In the Main procedure's WINDOW designer, add:

```clarion
BUTTON('Open Structured Setup'),AT(12,12,110,18),USE(?OpenStructuredSetup)
BUTTON(''),AT(0,0,1,1),USE(?CHUIComplete),HIDE
```

The second button is the one-shot completion bridge. It remains hidden.

## 4. Main procedure Data section

Place in `Main procedure / Local Data` before the WINDOW declaration:

```clarion
DialogHeader             GROUP
Version                    ULONG
HeaderSize                 ULONG
EntrySize                  ULONG
EntryCount                 ULONG
Flags                      ULONG
InstanceID                 ULONG
Title                      CSTRING(128)
Reserved                   STRING(104)
                         END
DialogEntries            GROUP,DIM(13)
Type                       ULONG
ID                         ULONG
ParentID                   ULONG
Flags                      ULONG
DependencyID               ULONG
DependencyOperator         ULONG
Minimum                    LONG
Maximum                    LONG
Step                       LONG
IconID                     ULONG
Caption                    CSTRING(128)
Value                      CSTRING(128)
DefaultValue               CSTRING(128)
DependencyValue            CSTRING(128)
Options                    CSTRING(512)
HelpText                   CSTRING(256)
Reserved                   STRING(88)
                         END
DialogPaths              CHUI_PATH_RECORD,DIM(2)
DialogInstance           ULONG
CompletedInstance        ULONG
DialogResult             LONG
DialogStatus             LONG

TestAutoCrossfade        BYTE(1)
TestCrossfadeSeconds     LONG(5)
TestAudioMode            CSTRING(32)
TestPreviewVolume        LONG(70)
TestSecondaryOutput      BYTE(1)
TestSampleRate           CSTRING(32)
TestExclusiveMode        BYTE(0)
TestBackgroundImage      CSTRING(CHUI_PATH_CAPACITY)
TestRecordingFolder      CSTRING(CHUI_PATH_CAPACITY)
```

## 5. Procedure Setup embed

Place in `Main procedure / Procedure Setup`, before opening the window:

```clarion
TestAudioMode = 'WASAPI_SHARED'
TestSampleRate = '48000'
```

## 6. Open button — EVENT:Accepted

Place in `?OpenStructuredSetup / Accepted`:

```clarion
IF DialogInstance
  MESSAGE('The Structured Setup dialog is already open.')
  CYCLE
END

DO PrepareStructuredDialog

IF CHUI_GetAbiVersion() <> CHUI_ABI_VERSION
  MESSAGE('CHTheme.dll Structured Dialog ABI version mismatch.')
  CYCLE
END
IF CHUI_GetHeaderSize() <> SIZE(DialogHeader)
  MESSAGE('Header size mismatch. Clarion=' & SIZE(DialogHeader) & |
          ' DLL=' & CHUI_GetHeaderSize())
  CYCLE
END
IF CHUI_GetEntrySize() <> SIZE(DialogEntries[1])
  MESSAGE('Entry size mismatch. Clarion=' & SIZE(DialogEntries[1]) & |
          ' DLL=' & CHUI_GetEntrySize())
  CYCLE
END
IF CHUI_GetPathRecordSize() <> SIZE(DialogPaths[1])
  MESSAGE('Path-record size mismatch. Clarion=' & SIZE(DialogPaths[1]) & |
          ' DLL=' & CHUI_GetPathRecordSize())
  CYCLE
END

DialogStatus = CHUI_ValidateDialogEx(ADDRESS(DialogHeader), |
                                     ADDRESS(DialogEntries[1]), |
                                     ADDRESS(DialogPaths[1]),2)
IF DialogStatus <> CHUI_STATUS_OK
  MESSAGE('Structured Dialog validation failed: ' & DialogStatus)
  CYCLE
END

DialogStatus = CHUI_OpenDialogEx(0{PROP:Handle},ADDRESS(DialogHeader), |
                                 ADDRESS(DialogEntries[1]), |
                                 ADDRESS(DialogPaths[1]),2, |
                                 ?CHUIComplete{PROP:Handle})
IF DialogStatus > 0
  DialogInstance = DialogStatus
ELSE
  MESSAGE('Unable to open Structured Dialog: ' & DialogStatus)
END
```

## 7. Completion button — EVENT:Accepted

Place in `?CHUIComplete / Accepted`:

```clarion
IF CHUI_ConsumeAction(?CHUIComplete{PROP:Handle}, |
                      ChangedInstance,ChangedEntryID)
  IF ChangedInstance = DialogInstance
    CASE ChangedEntryID
    OF 401
      SelectAudioDevice
      ! Feed the returned stable value back into the still-open working dialog.
      CHUI_SetEntryValue(DialogInstance,AudioDeviceEntryID,SelectedDeviceValue)
    END
  END
END

IF CHUI_ConsumeCompletion(?CHUIComplete{PROP:Handle}, |
                          CompletedInstance,DialogResult)
  IF CompletedInstance = DialogInstance
    IF DialogResult = CHUI_RESULT_OK OR DialogResult = CHUI_RESULT_APPLY
      DO ApplyStructuredDialog
      MESSAGE('Accepted values:|' & |
              'Auto crossfade=' & TestAutoCrossfade & '|' & |
              'Seconds=' & TestCrossfadeSeconds & '|' & |
              'Audio mode=' & TestAudioMode & '|' & |
              'Preview volume=' & TestPreviewVolume & '|' & |
              'Secondary output=' & TestSecondaryOutput & '|' & |
              'Sample rate=' & TestSampleRate & '|' & |
              'Exclusive=' & TestExclusiveMode)
    ELSIF DialogResult = CHUI_RESULT_CANCEL
      MESSAGE('Cancelled. The original Clarion values were preserved.')
    END
    ! Apply uses this same event but leaves the native dialog open.
    ! Clear the instance only after OK or Cancel closes it.
    IF DialogResult <> CHUI_RESULT_APPLY
      DialogInstance = 0
    END
  END
END
```

An entry with `Type = CHUI_ACTION` is rendered as a themed button. Activating it
posts the same hidden-button `EVENT:Accepted`, but it is consumed through
`CHUI_ConsumeAction`. It neither closes the setup dialog nor changes the Apply
state. Use the returned entry ID to call the existing Clarion procedure.

## 8. PrepareStructuredDialog ROUTINE

Place in `Main procedure / Local Routines`:

```clarion
PrepareStructuredDialog ROUTINE
  CLEAR(DialogHeader)
  CLEAR(DialogEntries)
  CLEAR(DialogPaths)

  DialogHeader.Version = CHUI_ABI_VERSION
  DialogHeader.HeaderSize = SIZE(DialogHeader)
  DialogHeader.EntrySize = SIZE(DialogEntries[1])
  DialogHeader.EntryCount = 13
  DialogHeader.Title = 'CompuHost V4 - Structured Setup Test'

  DialogEntries[1].Type = CHUI_PANEL
  DialogEntries[1].ID = 100
  DialogEntries[1].IconID = CHUI_ICON_AUDIO
  DialogEntries[1].Caption = 'Audio'

  DialogEntries[2].Type = CHUI_PANEL
  DialogEntries[2].ID = 110
  DialogEntries[2].ParentID = 100
  DialogEntries[2].IconID = CHUI_ICON_GENERAL
  DialogEntries[2].Caption = 'General'

  DialogEntries[3].Type = CHUI_CHECKBOX
  DialogEntries[3].ID = 111
  DialogEntries[3].ParentID = 110
  DialogEntries[3].Caption = 'Enable Auto Crossfade'
  DialogEntries[3].Value = TestAutoCrossfade
  DialogEntries[3].DefaultValue = '1'

  DialogEntries[4].Type = CHUI_NUMBER
  DialogEntries[4].ID = 112
  DialogEntries[4].ParentID = 110
  DialogEntries[4].Flags = CHUI_FLAG_DEPEND_DISABLE
  DialogEntries[4].DependencyID = 111
  DialogEntries[4].DependencyOperator = CHUI_DEPEND_EQUAL
  DialogEntries[4].DependencyValue = '1'
  DialogEntries[4].Minimum = 0
  DialogEntries[4].Maximum = 30
  DialogEntries[4].Step = 1
  DialogEntries[4].Caption = 'Crossfade duration (seconds)'
  DialogEntries[4].Value = TestCrossfadeSeconds
  DialogEntries[4].DefaultValue = '5'

  DialogEntries[5].Type = CHUI_DROPDOWN
  DialogEntries[5].ID = 113
  DialogEntries[5].ParentID = 110
  DialogEntries[5].Caption = 'Default Audio Mode'
  DialogEntries[5].Value = TestAudioMode
  DialogEntries[5].DefaultValue = 'WASAPI_SHARED'
  DialogEntries[5].Options = 'WASAPI_SHARED=Shared|WASAPI_EXCLUSIVE=Exclusive|DIRECTSOUND=DirectSound'

  DialogEntries[6].Type = CHUI_SLIDER
  DialogEntries[6].ID = 114
  DialogEntries[6].ParentID = 110
  DialogEntries[6].Minimum = 0
  DialogEntries[6].Maximum = 100
  DialogEntries[6].Step = 5
  DialogEntries[6].Caption = 'Preview volume'
  DialogEntries[6].Value = TestPreviewVolume
  DialogEntries[6].DefaultValue = '70'

  DialogEntries[7].Type = CHUI_PANEL
  DialogEntries[7].ID = 120
  DialogEntries[7].ParentID = 100
  DialogEntries[7].IconID = CHUI_ICON_DEVICE
  DialogEntries[7].Caption = 'Output Devices'

  DialogEntries[8].Type = CHUI_CHECKBOX
  DialogEntries[8].ID = 121
  DialogEntries[8].ParentID = 120
  DialogEntries[8].Caption = 'Enable secondary output'
  DialogEntries[8].Value = TestSecondaryOutput
  DialogEntries[8].DefaultValue = '0'

  DialogEntries[9].Type = CHUI_PANEL
  DialogEntries[9].ID = 130
  DialogEntries[9].ParentID = 120
  DialogEntries[9].IconID = CHUI_ICON_ADVANCED
  DialogEntries[9].Caption = 'Device Advanced Settings'

  DialogEntries[10].Type = CHUI_DROPDOWN
  DialogEntries[10].ID = 131
  DialogEntries[10].ParentID = 130
  DialogEntries[10].Caption = 'Sample rate'
  DialogEntries[10].Value = TestSampleRate
  DialogEntries[10].DefaultValue = '48000'
  DialogEntries[10].Options = '44100=44100 Hz|48000=48000 Hz|96000=96000 Hz'

  DialogEntries[11].Type = CHUI_CHECKBOX
  DialogEntries[11].ID = 132
  DialogEntries[11].ParentID = 130
  DialogEntries[11].Caption = 'Allow exclusive control'
  DialogEntries[11].Value = TestExclusiveMode
  DialogEntries[11].DefaultValue = '0'

  DialogEntries[12].Type = CHUI_FILE
  DialogEntries[12].ID = 140
  DialogEntries[12].ParentID = 110
  DialogEntries[12].Caption = 'Background image'
  DialogEntries[12].Options = '*.png=PNG images|*.jpg;*.jpeg=JPEG images|*.*=All files'
  DialogPaths[1].EntryID = 140
  DialogPaths[1].Value = TestBackgroundImage
  DialogPaths[1].DefaultValue = ''

  DialogEntries[13].Type = CHUI_FOLDER
  DialogEntries[13].ID = 141
  DialogEntries[13].ParentID = 110
  DialogEntries[13].Caption = 'Recording folder'
  DialogPaths[2].EntryID = 141
  DialogPaths[2].Value = TestRecordingFolder
  DialogPaths[2].DefaultValue = ''
```

## 9. ApplyStructuredDialog ROUTINE

Place immediately after the prepare routine:

```clarion
ApplyStructuredDialog ROUTINE
  TestAutoCrossfade = DialogEntries[3].Value
  TestCrossfadeSeconds = DialogEntries[4].Value
  TestAudioMode = DialogEntries[5].Value
  TestPreviewVolume = DialogEntries[6].Value
  TestSecondaryOutput = DialogEntries[8].Value
  TestSampleRate = DialogEntries[10].Value
  TestExclusiveMode = DialogEntries[11].Value
  TestBackgroundImage = DialogPaths[1].Value
  TestRecordingFolder = DialogPaths[2].Value
```

`CHUI_FILE` and `CHUI_FOLDER` render a read-only path field plus a themed
`Browse...` button. The DLL opens the native Windows selector and updates its
working copy immediately. `DialogPaths[].Value` is written only by Apply or OK;
Cancel restores the value accepted by the most recent Apply. FILE options use
`pattern=caption` pairs separated by `|`, as shown above.

## 10. Host-window close protection

In the Main window `EVENT:CloseWindow` embed, prevent the Clarion-owned memory from disappearing while the modeless native dialog is open:

```clarion
IF DialogInstance
  MESSAGE('Close the Structured Setup dialog before closing the test application.')
  CYCLE
END
```

## Test sequence

1. Open the Structured Setup dialog and confirm the Clarion host remains responsive.
2. Confirm General uses the two-level presentation and current values are visible.
3. Clear Auto Crossfade and confirm Crossfade Duration disables without a Clarion event.
4. Select Output Devices and confirm the advanced third panel appears.
5. Change values and Cancel; reopen and confirm the original Clarion values remain.
6. Change values and press OK; confirm the completion message reports the new values.
7. Reopen and confirm the accepted values are supplied back as current values.
8. Attempt a second open while already open and confirm only one instance exists.
