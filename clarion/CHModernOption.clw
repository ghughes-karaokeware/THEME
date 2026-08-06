                     MEMBER()

                     INCLUDE('CHModernOption.inc'),ONCE

                     MAP
                       MODULE('CHTheme.dll')
CHTheme_CreateFlatOptionMask PROCEDURE(LONG ParentHwnd,LONG x,LONG y,LONG Width,LONG Height),LONG,PASCAL,RAW,NAME('CHTheme_CreateFlatOptionMask')
CHTheme_SetFlatOptionBounds  PROCEDURE(LONG MaskHwnd,LONG x,LONG y,LONG Width,LONG Height),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionBounds')
CHTheme_SetFlatOptionVisible PROCEDURE(LONG MaskHwnd,LONG Visible),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionVisible')
CHTheme_SetFlatOptionCaption PROCEDURE(LONG MaskHwnd,*CSTRING Caption),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionCaption')
CHTheme_SetFlatOptionChoices PROCEDURE(LONG MaskHwnd,*CSTRING Labels,LONG Selected),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionChoices')
CHTheme_SetFlatOptionChoiceBounds PROCEDURE(LONG MaskHwnd,LONG Choice,LONG x,LONG y,LONG Width,LONG Height),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionChoiceBounds')
CHTheme_SetFlatOptionChoice PROCEDURE(LONG MaskHwnd,LONG Selected),LONG,PASCAL,RAW,NAME('CHTheme_SetFlatOptionChoice')
CHTheme_GetFlatOptionChoice PROCEDURE(LONG MaskHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetFlatOptionChoice')
CHTheme_DestroyFlatOptionMask PROCEDURE(LONG MaskHwnd),LONG,PASCAL,RAW,NAME('CHTheme_DestroyFlatOptionMask')
                       END
                     END

CHModernOption.Init  PROCEDURE(LONG OptionFEQ,LONG OwnerFEQ,LONG OwnerSheetFEQ)
ParentHwnd              LONG
x                       LONG
y                       LONG
w                       LONG
h                       LONG
WasPixels               LONG
Caption                 CSTRING(256)
Result                  LONG
 CODE
 SELF.Kill()
 IF OptionFEQ = 0
   RETURN FALSE
 END
 OptionFEQ{PROP:Boxed} = FALSE
 OptionFEQ{PROP:Trn} = TRUE
 ParentHwnd = 0{PROP:Handle}
 IF ParentHwnd = 0
   RETURN FALSE
 END
 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(OptionFEQ,x,y,w,h)
 0{PROP:Pixels} = WasPixels
 IF w <= 0 OR h <= 0
   RETURN FALSE
 END
 SELF.MaskHwnd = CHTheme_CreateFlatOptionMask(ParentHwnd,x,y,w,h)
 IF SELF.MaskHwnd = 0
   RETURN FALSE
 END
 Caption = OptionFEQ{PROP:Text}
 Result = CHTheme_SetFlatOptionCaption(SELF.MaskHwnd,Caption)
 SELF.OptionFEQ = OptionFEQ
 SELF.OwnerFEQ = OwnerFEQ
 SELF.OwnerSheetFEQ = OwnerSheetFEQ
 SELF.LastVisible = -1
 SELF.Active = TRUE
 Result = SELF.SyncVisibility()
 RETURN TRUE

CHModernOption.SetRadioFEQ PROCEDURE(LONG Choice,LONG RadioFEQ)
ControlFEQ              LONG
RadioX                  LONG
RadioY                  LONG
RadioW                  LONG
RadioH                  LONG
ControlX                LONG
ControlY                LONG
ControlW                LONG
ControlH                LONG
WasPixels               LONG
 CODE
 IF Choice < 1 OR Choice > 20 OR RadioFEQ = 0
   RETURN FALSE
 END
 SELF.RadioFEQ[Choice] = RadioFEQ
 IF Choice > SELF.ChoiceCount
   SELF.ChoiceCount = Choice
 END
 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(RadioFEQ,RadioX,RadioY,RadioW,RadioH)
 ControlFEQ = 0
 LOOP
   ControlFEQ = 0{PROP:NextField,ControlFEQ}
   IF ControlFEQ = 0
     BREAK
   END
   IF (ControlFEQ{PROP:Type} + 0) = CREATE:Panel
     GETPOSITION(ControlFEQ,ControlX,ControlY,ControlW,ControlH)
     IF ControlX = RadioX AND ControlY = RadioY AND |
        ControlW = RadioW AND ControlH = RadioH
       SELF.CoverFEQ[Choice] = ControlFEQ
       SELF.CoverWasHidden[Choice] = ControlFEQ{PROP:Hide}
       HIDE(ControlFEQ)
       BREAK
     END
   END
 END
 0{PROP:Pixels} = WasPixels
 HIDE(RadioFEQ)
 RETURN TRUE

CHModernOption.SetChoices PROCEDURE(*CSTRING Labels)
Choice                  LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.ChoiceCount = 0
   RETURN FALSE
 END
 SELF.Selected = 1
 LOOP Choice = 1 TO SELF.ChoiceCount
   IF SELF.RadioFEQ[Choice] AND SELF.RadioFEQ[Choice]{PROP:Checked}
     SELF.Selected = Choice
     BREAK
   END
 END
 Result = CHTheme_SetFlatOptionChoices(SELF.MaskHwnd,Labels,SELF.Selected)
 IF Result
   Result = SELF.SyncPosition()
 END
 RETURN Result

CHModernOption.SyncPosition PROCEDURE()
x                       LONG
y                       LONG
w                       LONG
h                       LONG
RadioX                  LONG
RadioY                  LONG
RadioW                  LONG
RadioH                  LONG
Choice                  LONG
WasPixels               LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.MaskHwnd = 0 OR SELF.OptionFEQ = 0
   RETURN FALSE
 END
 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(SELF.OptionFEQ,x,y,w,h)
 IF w <= 0 OR h <= 0
   0{PROP:Pixels} = WasPixels
   RETURN FALSE
 END
 Result = CHTheme_SetFlatOptionBounds(SELF.MaskHwnd,x,y,w,h)
 LOOP Choice = 1 TO SELF.ChoiceCount
   IF SELF.RadioFEQ[Choice]
     GETPOSITION(SELF.RadioFEQ[Choice],RadioX,RadioY,RadioW,RadioH)
     IF RadioW > 0 AND RadioH > 0
       Result = CHTheme_SetFlatOptionChoiceBounds(SELF.MaskHwnd,Choice,RadioX,RadioY,RadioW,RadioH)
     END
   END
 END
 0{PROP:Pixels} = WasPixels
 RETURN Result

CHModernOption.SyncVisibility PROCEDURE()
ControlFEQ              LONG
ParentFEQ               LONG
Visible                 LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.MaskHwnd = 0 OR SELF.OptionFEQ = 0
   RETURN FALSE
 END
 Visible = TRUE
 IF SELF.OptionFEQ{PROP:Hide}
   Visible = FALSE
 END
 IF SELF.OwnerFEQ AND SELF.OwnerSheetFEQ
   IF (SELF.OwnerSheetFEQ{PROP:ChoiceFEQ} + 0) <> SELF.OwnerFEQ
     Visible = FALSE
   END
 END
 ControlFEQ = SELF.OwnerFEQ
 IF ControlFEQ = 0
   ControlFEQ = SELF.OptionFEQ{PROP:Parent}
 END
 LOOP WHILE ControlFEQ
   IF ControlFEQ{PROP:Hide}
     Visible = FALSE
     BREAK
   END
   ParentFEQ = ControlFEQ{PROP:Parent}
   IF (ControlFEQ{PROP:Type} + 0) = CREATE:Tab
     IF ParentFEQ AND (ParentFEQ{PROP:ChoiceFEQ} + 0) <> ControlFEQ
       Visible = FALSE
       BREAK
     END
   END
   ControlFEQ = ParentFEQ
 END
 IF SELF.LastVisible <> Visible
   Result = CHTheme_SetFlatOptionVisible(SELF.MaskHwnd,Visible)
   SELF.LastVisible = Visible
   RETURN Result
 END
 RETURN TRUE

CHModernOption.TakeEvent PROCEDURE()
Result                  LONG
NativeSelected          LONG
Choice                  LONG
ClarionSelected         LONG
 CODE
 ! The window-resize template may reposition the OPTION after EVENT:Sized has
 ! already passed this extension's embed. Re-read the FEQ bounds on each event
 ! loop pass; SetFlatOptionBounds is inexpensive and avoids embed-order coupling.
 IF SELF.Active
   Result = SELF.SyncPosition()
   Result = SELF.SyncVisibility()
   NativeSelected = CHTheme_GetFlatOptionChoice(SELF.MaskHwnd)
   IF NativeSelected >= 1 AND NativeSelected <= SELF.ChoiceCount AND NativeSelected <> SELF.Selected
     SELF.Selected = NativeSelected
     IF SELF.RadioFEQ[NativeSelected]
       SELF.RadioFEQ[NativeSelected]{PROP:Checked} = TRUE
       POST(EVENT:Accepted,SELF.RadioFEQ[NativeSelected])
     END
   ELSE
     ClarionSelected = 0
     LOOP Choice = 1 TO SELF.ChoiceCount
       IF SELF.RadioFEQ[Choice] AND SELF.RadioFEQ[Choice]{PROP:Checked}
         ClarionSelected = Choice
         BREAK
       END
     END
     IF ClarionSelected AND ClarionSelected <> SELF.Selected
       SELF.Selected = ClarionSelected
       Result = CHTheme_SetFlatOptionChoice(SELF.MaskHwnd,SELF.Selected)
     END
   END
 END

CHModernOption.Kill  PROCEDURE()
Result                  LONG
Choice                  LONG
 CODE
 IF SELF.Active AND SELF.MaskHwnd
   Result = CHTheme_DestroyFlatOptionMask(SELF.MaskHwnd)
 END
 SELF.Active = FALSE
 SELF.MaskHwnd = 0
 SELF.OptionFEQ = 0
 SELF.OwnerFEQ = 0
 SELF.OwnerSheetFEQ = 0
 SELF.ChoiceCount = 0
 SELF.Selected = 0
 SELF.LastVisible = -1
 LOOP Choice = 1 TO 20
   IF SELF.CoverFEQ[Choice]
     IF NOT SELF.CoverWasHidden[Choice]
       UNHIDE(SELF.CoverFEQ[Choice])
     END
     SELF.CoverFEQ[Choice] = 0
     SELF.CoverWasHidden[Choice] = FALSE
   END
   IF SELF.RadioFEQ[Choice]
     UNHIDE(SELF.RadioFEQ[Choice])
     SELF.RadioFEQ[Choice] = 0
   END
 END
