                     MEMBER()

                     INCLUDE('CHModernSheet.inc'),ONCE

                     MAP
                       MODULE('CHTheme.dll')
CHTabStrip_Create       PROCEDURE(LONG ParentHwnd,LONG x,LONG y,LONG Width,LONG Height,*CSTRING Labels,LONG Selected,LONG NotifyButtonHwnd),LONG,PASCAL,RAW,NAME('CHTabStrip_Create')
CHTabStrip_Destroy      PROCEDURE(LONG TabHwnd),LONG,PASCAL,RAW,NAME('CHTabStrip_Destroy')
CHTabStrip_SetSelected  PROCEDURE(LONG TabHwnd,LONG Selected),LONG,PASCAL,RAW,NAME('CHTabStrip_SetSelected')
CHTabStrip_GetSelected  PROCEDURE(LONG TabHwnd),LONG,PASCAL,RAW,NAME('CHTabStrip_GetSelected')
CHTabStrip_SetLabels    PROCEDURE(LONG TabHwnd,*CSTRING Labels),LONG,PASCAL,RAW,NAME('CHTabStrip_SetLabels')
CHTabStrip_SetEnabled   PROCEDURE(LONG TabHwnd,LONG Tab,LONG Enabled),LONG,PASCAL,RAW,NAME('CHTabStrip_SetEnabled')
CHTabStrip_SetTabVisible PROCEDURE(LONG TabHwnd,LONG Tab,LONG Visible),LONG,PASCAL,RAW,NAME('CHTabStrip_SetTabVisible')
CHTabStrip_SetVisible   PROCEDURE(LONG TabHwnd,LONG Visible),LONG,PASCAL,RAW,NAME('CHTabStrip_SetVisible')
CHTabStrip_SetSheetBounds PROCEDURE(LONG TabHwnd,LONG x,LONG y,LONG Width,LONG Height),LONG,PASCAL,RAW,NAME('CHTabStrip_SetSheetBounds')
CHTheme_GetMode          PROCEDURE(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetMode')
CHTheme_ForceRedraw      PROCEDURE(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_ForceRedraw')
                       END
                       MODULE('Windows API')
MoveWindow               PROCEDURE(LONG Hwnd,LONG x,LONG y,LONG Width,LONG Height,LONG Repaint),LONG,PASCAL,RAW,NAME('MoveWindow')
                       END
                     END

CHModernSheet.Init    PROCEDURE(LONG SheetFEQ,*CSTRING Labels,LONG NotifyFEQ,LONG HeaderHeight)
ParentHwnd              LONG
NotifyHwnd              LONG
x                       LONG
y                       LONG
w                       LONG
h                       LONG
WasPixels               LONG
Result                  LONG
 CODE
 SELF.Kill()
 IF SheetFEQ = 0 OR HeaderHeight < 18
   RETURN FALSE
 END
 ParentHwnd = 0{PROP:Handle}
 NotifyHwnd = 0
 IF NotifyFEQ = 0
   SELF.NotifyFEQ = CREATE(0,CREATE:Button)
   IF SELF.NotifyFEQ
     SELF.OwnNotify = TRUE
     SELF.NotifyFEQ{PROP:XPos} = 0
     SELF.NotifyFEQ{PROP:YPos} = 0
     SELF.NotifyFEQ{PROP:Width} = 1
     SELF.NotifyFEQ{PROP:Height} = 1
     SELF.NotifyFEQ{PROP:Hide} = TRUE
   END
 ELSE
   SELF.NotifyFEQ = NotifyFEQ
 END
 IF SELF.NotifyFEQ
   NotifyHwnd = SELF.NotifyFEQ{PROP:Handle}
 END
 IF ParentHwnd = 0 OR NotifyHwnd = 0
   SELF.Kill()
   RETURN FALSE
 END

 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(SheetFEQ,x,y,w,h)
 0{PROP:Pixels} = WasPixels
 IF w <= 0 OR h <= 0
   RETURN FALSE
 END

 SELF.StripHwnd = CHTabStrip_Create(ParentHwnd,x,y,w,HeaderHeight,Labels,1,NotifyHwnd)
 IF SELF.StripHwnd = 0
   SELF.Kill()
   RETURN FALSE
 END
 SELF.SheetFEQ = SheetFEQ
 SELF.HeaderHeight = HeaderHeight
 SELF.Selected = 1
 SELF.LastVisible = -1
 SELF.Active = TRUE
 Result = CHTabStrip_SetSheetBounds(SELF.StripHwnd,x,y,w,h)
 Result = SELF.SyncVisibility()
 RETURN TRUE

CHModernSheet.SetTabFEQ PROCEDURE(LONG Tab,LONG TabFEQ)
Result                  LONG
Visible                 LONG
Selected                LONG
ChoiceFEQ               LONG
ScanTab                 LONG
 CODE
 IF Tab < 1 OR Tab > 20
   RETURN FALSE
 END
 SELF.TabFEQ[Tab] = TabFEQ
 IF SELF.Active AND SELF.StripHwnd AND TabFEQ
   Visible = TRUE
   IF TabFEQ{PROP:Hide}
     Visible = FALSE
    END
    Result = CHTabStrip_SetTabVisible(SELF.StripHwnd,Tab,Visible)
  END
  ! Clarion can establish the SHEET's initial choice before the generated
  ! SetTabFEQ calls register its TABs. Adopt a nonzero Clarion choice as soon
  ! as its TAB is registered. A zero ChoiceFEQ is left untouched: only Clarion
  ! may choose the initial TAB or determine which TAB contents are visible.
  IF SELF.Active AND SELF.StripHwnd AND SELF.SheetFEQ
    ChoiceFEQ = SELF.SheetFEQ{PROP:ChoiceFEQ}
    LOOP ScanTab = 1 TO 20
      IF SELF.TabFEQ[ScanTab] AND SELF.TabFEQ[ScanTab] = ChoiceFEQ
        Selected = ScanTab
        BREAK
      END
    END
    IF Selected
      SELF.Selected = Selected
      Result = CHTabStrip_SetSelected(SELF.StripHwnd,Selected)
    END
  END
 RETURN TRUE

CHModernSheet.SetSelected PROCEDURE(LONG Tab)
Result                  LONG
 CODE
 IF NOT SELF.Active OR Tab < 1 OR Tab > 20
   RETURN FALSE
 END
 Result = CHTabStrip_SetSelected(SELF.StripHwnd,Tab)
 IF Result AND SELF.TabFEQ[Tab]
   SELF.Selected = Tab
   SELF.SheetFEQ{PROP:ChoiceFEQ} = SELF.TabFEQ[Tab]
   SELECT(SELF.TabFEQ[Tab])
   DISPLAY(SELF.SheetFEQ)
 END
 RETURN Result

CHModernSheet.GetSelected PROCEDURE()
 CODE
 IF NOT SELF.Active
   RETURN 0
 END
 RETURN CHTabStrip_GetSelected(SELF.StripHwnd)

CHModernSheet.SetEnabled PROCEDURE(LONG Tab,BYTE Enabled)
 CODE
 IF NOT SELF.Active
   RETURN FALSE
 END
 RETURN CHTabStrip_SetEnabled(SELF.StripHwnd,Tab,Enabled)

CHModernSheet.SetLabels PROCEDURE(*CSTRING Labels)
 CODE
 IF NOT SELF.Active
   RETURN FALSE
 END
 RETURN CHTabStrip_SetLabels(SELF.StripHwnd,Labels)

CHModernSheet.SetBackgroundColors PROCEDURE(LONG LightBackground,LONG DarkBackground)
 CODE
 SELF.LightBackground = LightBackground
 SELF.DarkBackground = DarkBackground
 SELF.LastBackground = -1
 RETURN SELF.SyncBackground()

CHModernSheet.SyncPosition PROCEDURE()
x                       LONG
y                       LONG
w                       LONG
h                       LONG
WasPixels               LONG
 CODE
 IF NOT SELF.Active OR SELF.StripHwnd = 0 OR SELF.SheetFEQ = 0
   RETURN FALSE
 END
 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(SELF.SheetFEQ,x,y,w,h)
 0{PROP:Pixels} = WasPixels
 IF w <= 0 OR h <= 0
   RETURN FALSE
 END
 RETURN CHTabStrip_SetSheetBounds(SELF.StripHwnd,x,y,w,h)

CHModernSheet.SyncVisibility PROCEDURE()
ControlFEQ              LONG
Visible                 LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.StripHwnd = 0 OR SELF.SheetFEQ = 0
   RETURN FALSE
 END
 Visible = TRUE
 ControlFEQ = SELF.SheetFEQ
 LOOP WHILE ControlFEQ
   IF ControlFEQ{PROP:Hide}
     Visible = FALSE
     BREAK
   END
   ControlFEQ = ControlFEQ{PROP:Parent}
 END
 IF SELF.LastVisible <> Visible
   Result = CHTabStrip_SetVisible(SELF.StripHwnd,Visible)
   SELF.LastVisible = Visible
   RETURN Result
 END
 RETURN TRUE

CHModernSheet.SyncTabVisibility PROCEDURE()
Tab                     LONG
Result                  LONG
SyncResult              LONG
Visible                 LONG
 CODE
 IF NOT SELF.Active OR SELF.StripHwnd = 0
   RETURN FALSE
 END
 Result = TRUE
 LOOP Tab = 1 TO 20
   IF SELF.TabFEQ[Tab]
     Visible = TRUE
     IF SELF.TabFEQ[Tab]{PROP:Hide}
       Visible = FALSE
     END
     SyncResult = CHTabStrip_SetTabVisible(SELF.StripHwnd,Tab,Visible)
     IF NOT SyncResult
       Result = FALSE
     END
   END
 END
 RETURN Result

CHModernSheet.SyncBackground PROCEDURE()
Mode                    LONG
Background              LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.SheetFEQ = 0
   RETURN FALSE
 END
 IF SELF.LightBackground = 0 AND SELF.DarkBackground = 0
   RETURN TRUE
 END
 Mode = CHTheme_GetMode(0{PROP:Handle})
 IF Mode = 2
   Background = SELF.LightBackground
 ELSE
   Background = SELF.DarkBackground
 END
 IF SELF.LastBackground <> Background OR SELF.SheetFEQ{PROP:Color} <> Background
   SELF.SheetFEQ{PROP:Color} = Background
   SELF.LastBackground = Background
   ! A SHEET background is cached in Clarion's window surface. A control-only
   ! DISPLAY updates the property but leaves those pixels unchanged until a
   ! later resize or tab selection. Rebuild the complete Clarion surface now.
   DISPLAY
   Result = CHTheme_ForceRedraw(0{PROP:Handle})
 END
 RETURN TRUE

CHModernSheet.SyncTabContents PROCEDURE(LONG SelectedTab)
ControlFEQ              LONG
ParentFEQ               LONG
OwnerTab                LONG
Tab                     LONG
 CODE
 IF NOT SELF.Active OR SelectedTab < 1 OR SelectedTab > 20
   RETURN FALSE
 END
 ControlFEQ = 0
 LOOP
   ControlFEQ = 0{PROP:NextField,ControlFEQ}
   IF ControlFEQ = 0
     BREAK
   END
   OwnerTab = 0
   ParentFEQ = ControlFEQ{PROP:Parent}
   LOOP WHILE ParentFEQ
     LOOP Tab = 1 TO 20
       IF SELF.TabFEQ[Tab] AND ParentFEQ = SELF.TabFEQ[Tab]
         OwnerTab = Tab
         BREAK
       END
     END
     IF OwnerTab
       BREAK
     END
     ParentFEQ = ParentFEQ{PROP:Parent}
   END
   IF OwnerTab
     IF OwnerTab = SelectedTab
       ControlFEQ{PROP:Hide} = FALSE
     ELSE
       ControlFEQ{PROP:Hide} = TRUE
     END
   END
 END
 DISPLAY(SELF.SheetFEQ)
 RETURN TRUE

CHModernSheet.TakeEvent PROCEDURE()
Selected                LONG
ChoiceFEQ               LONG
ScanTab                 LONG
Result                  LONG
SyncResult              LONG
 CODE
 IF NOT SELF.Active
   RETURN
 END
 ! Resize extensions can move the SHEET after EVENT:Sized has passed this
 ! embed. Re-read its final bounds on every ACCEPT loop pass.
  Result = SELF.SyncPosition()
  SyncResult = SELF.SyncVisibility()
  SyncResult = SELF.SyncTabVisibility()
  SyncResult = SELF.SyncBackground()
  IF EVENT() = EVENT:Accepted AND FIELD() = SELF.NotifyFEQ
    ! A click on the native strip is the only time the strip drives Clarion.
    Selected = SELF.GetSelected()
    IF Selected >= 1 AND Selected <= 20 AND Selected <> SELF.Selected
      SELF.Selected = Selected
      IF SELF.TabFEQ[Selected]
        SELF.SheetFEQ{PROP:ChoiceFEQ} = SELF.TabFEQ[Selected]
        SELECT(SELF.TabFEQ[Selected])
        DISPLAY(SELF.SheetFEQ)
      END
    END
  ELSE
    ! Application code can restore or change the Clarion TAB directly. Mirror
    ! that authoritative choice to the native strip without changing contents.
    ChoiceFEQ = SELF.SheetFEQ{PROP:ChoiceFEQ}
    LOOP ScanTab = 1 TO 20
      IF SELF.TabFEQ[ScanTab] AND SELF.TabFEQ[ScanTab] = ChoiceFEQ
        Selected = ScanTab
        BREAK
      END
    END
    IF Selected AND Selected <> SELF.Selected
      SELF.Selected = Selected
      Result = CHTabStrip_SetSelected(SELF.StripHwnd,Selected)
    END
  END

CHModernSheet.Kill    PROCEDURE()
Result                  LONG
Tab                     LONG
 CODE
 IF SELF.Active AND SELF.StripHwnd
   Result = CHTabStrip_Destroy(SELF.StripHwnd)
 END
 IF SELF.OwnNotify AND SELF.NotifyFEQ
   DESTROY(SELF.NotifyFEQ)
 END
 LOOP Tab = 1 TO 20
   SELF.TabFEQ[Tab] = 0
 END
 SELF.Active = FALSE
 SELF.StripHwnd = 0
 SELF.SheetFEQ = 0
 SELF.NotifyFEQ = 0
 SELF.OwnNotify = FALSE
 SELF.HeaderHeight = 0
 SELF.Selected = 0
 SELF.LightBackground = 0
 SELF.DarkBackground = 0
 SELF.LastBackground = -1
 SELF.LastVisible = -1
