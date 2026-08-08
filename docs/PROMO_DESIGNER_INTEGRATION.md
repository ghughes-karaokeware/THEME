# Promo Trailer Designer — Clarion 10 integration

Use the existing `Test_Setup.app`. Do not create another application and do not edit generated `.clw` files directly; place each block in the named Clarion embed so regeneration preserves it.

This guide is tailored to the current Test_Setup procedure. It keeps the existing Structured Setup implementation, reuses `?CHUIComplete`, and adds only one visible test button.

## 1. Global includes

In **Global Properties > Embeds > Global Data**, immediately after the existing `CHStructuredDialogTypes.inc` include:

```clarion
  INCLUDE('CHPromoDesignerTypes.inc')
```

## 2. Global MAP

In **Global Properties > Embeds > Inside the Global Map**, add these declarations to the existing `MODULE('CHTheme.dll')` block. Do not create a second module:

```clarion
CHPT_GetAbiVersion(),ULONG,PASCAL,RAW,NAME('CHPT_GetAbiVersion')
CHPT_GetDataSize(),ULONG,PASCAL,RAW,NAME('CHPT_GetDataSize')
CHPT_ValidateData(LONG DataAddress),LONG,PASCAL,RAW,NAME('CHPT_ValidateData')
CHPT_OpenDesigner(LONG OwnerHwnd,LONG DataAddress,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHPT_OpenDesigner')
CHPT_ConsumeCompletion(LONG CompletionButtonHwnd,*ULONG InstanceID,*LONG DesignerResult),LONG,PASCAL,RAW,NAME('CHPT_ConsumeCompletion')
```

## 3. Main procedure data

In **Main > Data**, add these declarations before the window declaration:

```clarion
PromoData               GROUP
Version                    ULONG
StructureSize              ULONG
Flags                      ULONG
BufferCapacity             ULONG
MaximumLength              ULONG
InstanceID                 ULONG
Result                     LONG
InputLength                ULONG
OutputLength               ULONG
Text                       CSTRING(4096)
Reserved                   STRING(124)
                        END
PromoDesignerInstance   ULONG
PromoCompletedInstance  ULONG
PromoDesignerResult     LONG
PromoDesignerStatus     LONG
PromoAcceptedText       CSTRING(4096)
```

`PromoAcceptedText` is the test application's last accepted value. Cancel never changes it.

The ABI group is intentionally declared inline. `Main` is generated into the separate `Test_Setup001.clw` MEMBER module, so a named `GROUP,TYPE` declared only by a global include is not visible there in Clarion 10. The inline layout is exactly 4256 bytes and matches the DLL.

## 4. Main window

Open the Main window formatter and add this button beside `?OpenStructuredSetup`. Reuse the existing hidden `?CHUIComplete` button; do not add another notification control.

```clarion
BUTTON('Test Promo Trailer Designer'),AT(126,12,126,18),USE(?OpenPromoDesigner)
```

## 5. Initialization routine

In **Main > Local Routines**, add:

```clarion
PreparePromoDesigner ROUTINE
  CLEAR(PromoData)
  PromoData.Version = CHPT_ABI_VERSION
  PromoData.StructureSize = SIZE(PromoData)
  PromoData.BufferCapacity = CHPT_TEXT_CAPACITY
  PromoData.MaximumLength = 1027
  PromoData.Flags = CHPT_FLAG_DEBUG_LOG

  IF PromoAcceptedText = ''
    PromoData.Text = 'Welcome to <<3><<B>Friday Night Karaoke<</B><<0>!<13,10>' & |
                     'Please visit the bar for tonight''s drink specials.<13,10>' & |
                     '<<2><<B>Download Songbooks Online<</B><<0> and send your request from your phone!'
  ELSE
    PromoData.Text = PromoAcceptedText
  END
```

Clarion doubles `<` inside source literals. At runtime the buffer contains normal `<3>`, `<B>`, and other tags.

## 6. Test button — EVENT:Accepted

In **Main > ?OpenPromoDesigner > EVENT:Accepted**, insert the complete block below:

```clarion
IF PromoDesignerInstance
  MESSAGE('The Promo Trailer Designer is already open.')
  CYCLE
END

DO PreparePromoDesigner

IF CHPT_GetAbiVersion() <> CHPT_ABI_VERSION
  MESSAGE('Promo Designer ABI version mismatch.')
  CYCLE
END
IF CHPT_GetDataSize() <> SIZE(PromoData)
  MESSAGE('Promo structure mismatch. Clarion=' & SIZE(PromoData) & |
          ' DLL=' & CHPT_GetDataSize())
  CYCLE
END

PromoDesignerStatus = CHPT_ValidateData(ADDRESS(PromoData))
IF PromoDesignerStatus <> CHPT_STATUS_OK
  MESSAGE('Promo data validation failed: ' & PromoDesignerStatus)
  CYCLE
END

PromoDesignerStatus = CHPT_OpenDesigner(0{PROP:Handle}, |
    ADDRESS(PromoData),?CHUIComplete{PROP:Handle})
IF PromoDesignerStatus > 0
  PromoDesignerInstance = PromoDesignerStatus
ELSE
  MESSAGE('Unable to open Promo Designer: ' & PromoDesignerStatus)
END
```

## 7. Shared hidden callback — EVENT:Accepted

In the current **Main > ?CHUIComplete > EVENT:Accepted** embed, place this block at the very beginning, before `DO HandleStructuredChange` and before either Structured Setup consume call:

```clarion
IF CHPT_ConsumeCompletion(?CHUIComplete{PROP:Handle}, |
    PromoCompletedInstance,PromoDesignerResult)

  IF PromoCompletedInstance = PromoDesignerInstance
    CASE PromoDesignerResult
    OF CHPT_RESULT_OK
      PromoAcceptedText = PromoData.Text
      MESSAGE('Accepted encoded Promo Trailer text:|' & PromoAcceptedText)
    OF CHPT_RESULT_CANCEL
      MESSAGE('Promo Trailer changes cancelled.|Original text remains unchanged.')
    END
    PromoDesignerInstance = 0
  END
END
```

Then leave all existing Structured Setup code exactly where it is, beginning with:

```clarion
DO HandleStructuredChange

IF CHUI_ConsumeAction(?CHUIComplete{PROP:Handle}, |
    ActionInstance,ActionEntryID)
  ! Existing action handling remains unchanged.
ELSIF CHUI_ConsumeCompletion(?CHUIComplete{PROP:Handle}, |
    CompletedInstance,DialogResult)
  ! Existing Apply/OK/Cancel handling remains unchanged.
END
```

The Promo and Structured Setup completion queues are independent. Calling the Promo consumer first does not remove a Structured Setup notification.

## 8. Owner close protection

In **Main > EVENT:CloseWindow**, replace only the current `IF DialogInstance` guard with:

```clarion
IF DialogInstance OR PromoDesignerInstance
  MESSAGE('Close the Setup or Promo Trailer Designer before closing the test application.')
  CYCLE
END
```

## 9. Regenerate the Clarion import library

The staged DLL contains the exports, but the current Clarion-format `CHTheme.lib` predates them. With the test application closed:

1. Open Clarion's **Library Maker**.
2. Open the staged `CHTheme.dll` from the Test_Setup folder.
3. Confirm these five undecorated names are listed: `CHPT_GetAbiVersion`, `CHPT_GetDataSize`, `CHPT_ValidateData`, `CHPT_OpenDesigner`, and `CHPT_ConsumeCompletion`.
4. Save the Clarion import library as `CHTheme.lib` in the Test_Setup folder, replacing the old test copy.
5. Rebuild the application. Unresolved `CHPT_...` externals indicate that the old library is still being linked.

## 10. Test sequence

1. Open Structured Setup and verify it still works.
2. Open Promo Designer and confirm the supplied tags are hidden and formatting is visible.
3. Exercise selection, Bold, Italic, Underline, colors, Undo/Redo, Enter, joining, and multi-line paste.
4. Press Cancel and confirm `PromoAcceptedText` is unchanged.
5. Reopen, edit, press OK, and inspect the returned encoded text.
6. Reopen again and confirm the accepted markup reconstructs visually.
7. Confirm blank lines are omitted on OK, matching the legacy editor.

## 11. Expected ownership and return behavior

- The designer is modeless, so the Test_Setup ACCEPT loop continues running.
- Only one Promo Designer instance may be open.
- OK serializes the edited rich text into `PromoData.Text` and reports `CHPT_RESULT_OK`.
- Cancel reports `CHPT_RESULT_CANCEL` and leaves `PromoData.Text` and `PromoAcceptedText` unchanged.
- Closing the designer with the title-bar X is treated as Cancel.
- `PromoDesignerInstance` must be cleared only after consuming the matching completion.
