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
CHUI_ValidateDialog(*CHUI_DIALOG_HEADER Header,*CHUI_ENTRY_RECORD Entries),LONG,PASCAL,RAW,NAME('CHUI_ValidateDialog')
CHUI_OpenDialog(LONG OwnerHwnd,*CHUI_DIALOG_HEADER Header,*CHUI_ENTRY_RECORD Entries,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHUI_OpenDialog')
CHUI_ConsumeCompletion(LONG CompletionButtonHwnd,*ULONG InstanceID,*LONG DialogResult),LONG,PASCAL,RAW,NAME('CHUI_ConsumeCompletion')
     END
```

Create the Clarion-compatible `CHTheme.LIB` from the new DLL exports before linking, as with the project's other CHTheme exports.

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
DialogHeader             CHUI_DIALOG_HEADER
DialogEntries            CHUI_ENTRY_RECORD,DIM(11)
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
  EXIT
END

DO PrepareStructuredDialog

IF CHUI_GetAbiVersion() <> CHUI_ABI_VERSION
  MESSAGE('CHTheme.dll Structured Dialog ABI version mismatch.')
  EXIT
END
IF CHUI_GetHeaderSize() <> SIZE(DialogHeader)
  MESSAGE('Header size mismatch. Clarion=' & SIZE(DialogHeader) & |
          ' DLL=' & CHUI_GetHeaderSize())
  EXIT
END
IF CHUI_GetEntrySize() <> SIZE(DialogEntries[1])
  MESSAGE('Entry size mismatch. Clarion=' & SIZE(DialogEntries[1]) & |
          ' DLL=' & CHUI_GetEntrySize())
  EXIT
END

DialogStatus = CHUI_ValidateDialog(DialogHeader,DialogEntries[1])
IF DialogStatus <> CHUI_STATUS_OK
  MESSAGE('Structured Dialog validation failed: ' & DialogStatus)
  EXIT
END

DialogStatus = CHUI_OpenDialog(0{PROP:Handle},DialogHeader,DialogEntries[1], |
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
IF CHUI_ConsumeCompletion(?CHUIComplete{PROP:Handle}, |
                          CompletedInstance,DialogResult)
  IF CompletedInstance = DialogInstance
    IF DialogResult = CHUI_RESULT_OK
      DO ApplyStructuredDialog
      MESSAGE('Accepted values:|' & |
              'Auto crossfade=' & TestAutoCrossfade & '|' & |
              'Seconds=' & TestCrossfadeSeconds & '|' & |
              'Audio mode=' & TestAudioMode & '|' & |
              'Preview volume=' & TestPreviewVolume & '|' & |
              'Secondary output=' & TestSecondaryOutput & '|' & |
              'Sample rate=' & TestSampleRate & '|' & |
              'Exclusive=' & TestExclusiveMode)
    ELSE
      MESSAGE('Cancelled. The original Clarion values were preserved.')
    END
    DialogInstance = 0
  END
END
```

## 8. PrepareStructuredDialog ROUTINE

Place in `Main procedure / Local Routines`:

```clarion
PrepareStructuredDialog ROUTINE
  CLEAR(DialogHeader)
  CLEAR(DialogEntries)

  DialogHeader.Version = CHUI_ABI_VERSION
  DialogHeader.HeaderSize = SIZE(DialogHeader)
  DialogHeader.EntrySize = SIZE(DialogEntries[1])
  DialogHeader.EntryCount = 11
  DialogHeader.Title = 'CompuHost V4 - Structured Setup Test'

  DialogEntries[1].Type = CHUI_PANEL
  DialogEntries[1].ID = 100
  DialogEntries[1].Caption = 'Audio'

  DialogEntries[2].Type = CHUI_PANEL
  DialogEntries[2].ID = 110
  DialogEntries[2].ParentID = 100
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
```

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
