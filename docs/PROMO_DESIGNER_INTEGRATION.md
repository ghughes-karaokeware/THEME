# Promo Trailer Designer — Clarion 10 integration

Use the existing `Test_Setup.app`. Do not create another application and do not edit generated `.clw` files directly; place each block in the named Clarion embed so regeneration preserves it.

## 1. Global includes

Immediately after the existing global includes:

```clarion
  INCLUDE('CHPromoDesignerTypes.inc')
```

## 2. Global MAP

Add these declarations to the existing `MODULE('CHTheme.dll')` block:

```clarion
CHPT_GetAbiVersion(),ULONG,PASCAL,RAW,NAME('CHPT_GetAbiVersion')
CHPT_GetDataSize(),ULONG,PASCAL,RAW,NAME('CHPT_GetDataSize')
CHPT_ValidateData(LONG DataAddress),LONG,PASCAL,RAW,NAME('CHPT_ValidateData')
CHPT_OpenDesigner(LONG OwnerHwnd,LONG DataAddress,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHPT_OpenDesigner')
CHPT_ConsumeCompletion(LONG CompletionButtonHwnd,*ULONG InstanceID,*LONG DesignerResult),LONG,PASCAL,RAW,NAME('CHPT_ConsumeCompletion')
```

## 3. Main procedure data

Add before the window declaration:

```clarion
PromoData               CHPT_PROMO_DATA
PromoDesignerInstance   ULONG
PromoCompletedInstance  ULONG
PromoDesignerResult     LONG
PromoDesignerStatus     LONG
PromoAcceptedText       CSTRING(CHPT_TEXT_CAPACITY)
```

## 4. Main window

Add beside `?OpenStructuredSetup`. Reuse the existing hidden `?CHUIComplete` button.

```clarion
BUTTON('Test Promo Trailer Designer'),AT(126,12,126,18),USE(?OpenPromoDesigner)
```

## 5. Initialization routine

Add to Local Routines:

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

Place this **before** the existing Structured Setup consume calls:

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

ELSIF CHUI_ConsumeAction(?CHUIComplete{PROP:Handle},ActionInstance,ActionEntryID)
  ! Existing Structured Setup action handling.
ELSIF CHUI_ConsumeCompletion(?CHUIComplete{PROP:Handle},CompletedInstance,DialogResult)
  ! Existing Structured Setup Apply/OK/Cancel handling.
END
```

Keep the existing `HandleStructuredChange` call where it currently belongs. The two dialogs share the notification button but have separate consume queues.

## 8. Owner close protection

Replace the current close guard with:

```clarion
IF DialogInstance OR PromoDesignerInstance
  MESSAGE('Close the Setup or Promo Trailer Designer before closing the test application.')
  CYCLE
END
```

## 9. Test sequence

1. Open Structured Setup and verify it still works.
2. Open Promo Designer and confirm the supplied tags are hidden and formatting is visible.
3. Exercise selection, Bold, Italic, Underline, colors, Undo/Redo, Enter, joining, and multi-line paste.
4. Press Cancel and confirm `PromoAcceptedText` is unchanged.
5. Reopen, edit, press OK, and inspect the returned encoded text.
6. Reopen again and confirm the accepted markup reconstructs visually.
7. Confirm blank lines are omitted on OK, matching the legacy editor.
