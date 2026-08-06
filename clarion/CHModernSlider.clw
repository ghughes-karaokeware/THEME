                     MEMBER()

                     INCLUDE('CHModernSlider.inc'),ONCE

                     MAP
                       MODULE('CHTheme.dll')
CHSlider_Create         PROCEDURE(LONG ParentHwnd,LONG x,LONG y,LONG Width,LONG Height,LONG Orientation,LONG Minimum,LONG Maximum,LONG Value,LONG NotifyButtonHwnd),LONG,PASCAL,RAW,NAME('CHSlider_Create')
CHSlider_Destroy        PROCEDURE(LONG SliderHwnd),LONG,PASCAL,RAW,NAME('CHSlider_Destroy')
CHSlider_SetRange       PROCEDURE(LONG SliderHwnd,LONG Minimum,LONG Maximum),LONG,PASCAL,RAW,NAME('CHSlider_SetRange')
CHSlider_SetValue       PROCEDURE(LONG SliderHwnd,LONG Value),LONG,PASCAL,RAW,NAME('CHSlider_SetValue')
CHSlider_GetValue       PROCEDURE(LONG SliderHwnd),LONG,PASCAL,RAW,NAME('CHSlider_GetValue')
CHSlider_SetStep        PROCEDURE(LONG SliderHwnd,LONG Step),LONG,PASCAL,RAW,NAME('CHSlider_SetStep')
CHSlider_SetEnabled     PROCEDURE(LONG SliderHwnd,LONG Enabled),LONG,PASCAL,RAW,NAME('CHSlider_SetEnabled')
CHSlider_SetColors      PROCEDURE(LONG SliderHwnd,LONG LightTrack,LONG LightFill,LONG DarkTrack,LONG DarkFill),LONG,PASCAL,RAW,NAME('CHSlider_SetColors')
                       END
                       MODULE('Windows API')
MoveWindow               PROCEDURE(LONG Hwnd,LONG x,LONG y,LONG Width,LONG Height,LONG Repaint),LONG,PASCAL,RAW,NAME('MoveWindow')
ShowWindow               PROCEDURE(LONG Hwnd,LONG CmdShow),LONG,PASCAL,RAW,NAME('ShowWindow')
                       END
                     END

CHModernSlider.Init    PROCEDURE(LONG RegionFEQ,LONG Orientation,LONG Minimum,LONG Maximum,LONG Value,LONG NotifyFEQ,LONG Step)
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
 IF RegionFEQ = 0 OR Minimum >= Maximum
   RETURN FALSE
 END

 ParentHwnd = 0{PROP:Handle}
 IF NotifyFEQ
   SELF.NotifyFEQ = NotifyFEQ
 END
 IF NotifyFEQ
   NotifyHwnd = NotifyFEQ{PROP:Handle}
 END
 IF ParentHwnd = 0 OR (NotifyFEQ AND NotifyHwnd = 0)
   SELF.Kill()
   RETURN FALSE
 END

 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(RegionFEQ,x,y,w,h)
 0{PROP:Pixels} = WasPixels
 IF w <= 0 OR h <= 0
   RETURN FALSE
 END

 SELF.SliderHwnd = CHSlider_Create(ParentHwnd,x,y,w,h,Orientation,Minimum,Maximum,Value,NotifyHwnd)
 IF SELF.SliderHwnd = 0
   SELF.Kill()
   RETURN FALSE
 END
 ! The SHEET extension has not necessarily applied its initial TAB visibility
 ! yet. Keep the native overlay hidden until the first post-sheet TakeEvent.
 Result = ShowWindow(SELF.SliderHwnd,0)

 SELF.RegionFEQ = RegionFEQ
 SELF.LastValue = CHSlider_GetValue(SELF.SliderHwnd)
 SELF.LastVisible = FALSE
 SELF.Active = TRUE
 IF Step > 0
   Result = CHSlider_SetStep(SELF.SliderHwnd,Step)
 END
 RETURN TRUE

CHModernSlider.SyncPosition PROCEDURE()
x                       LONG
y                       LONG
w                       LONG
h                       LONG
WasPixels               LONG
 CODE
 IF NOT SELF.Active OR SELF.SliderHwnd = 0 OR SELF.RegionFEQ = 0
   RETURN FALSE
 END
 WasPixels = 0{PROP:Pixels}
 0{PROP:Pixels} = TRUE
 GETPOSITION(SELF.RegionFEQ,x,y,w,h)
 0{PROP:Pixels} = WasPixels
 IF w <= 0 OR h <= 0
   RETURN FALSE
 END
 RETURN MoveWindow(SELF.SliderHwnd,x,y,w,h,TRUE)

CHModernSlider.SyncVisibility PROCEDURE()
ControlFEQ              LONG
ParentFEQ               LONG
Visible                 LONG
Result                  LONG
 CODE
 IF NOT SELF.Active OR SELF.SliderHwnd = 0 OR SELF.RegionFEQ = 0
   RETURN FALSE
 END
 Visible = TRUE
 ControlFEQ = SELF.RegionFEQ
 LOOP WHILE ControlFEQ
   IF ControlFEQ{PROP:Hide}
     Visible = FALSE
     BREAK
   END
   ParentFEQ = ControlFEQ{PROP:Parent}
   ! A native slider is a top-level HWND overlay, so Clarion cannot clip it
   ! automatically to the selected TAB.  During initial window setup the
   ! inactive TAB's PROP:Hide can also lag behind the SHEET choice.  Treat
   ! the enclosing SHEET's ChoiceFEQ as authoritative.
   IF (ControlFEQ{PROP:Type} + 0) = CREATE:Tab
     IF ParentFEQ AND (ParentFEQ{PROP:ChoiceFEQ} + 0) <> ControlFEQ
       Visible = FALSE
       BREAK
     END
   END
   ControlFEQ = ParentFEQ
 END
 IF SELF.LastVisible <> Visible
   IF Visible
     Result = ShowWindow(SELF.SliderHwnd,5)
   ELSE
     Result = ShowWindow(SELF.SliderHwnd,0)
   END
   SELF.LastVisible = Visible
 END
 RETURN TRUE

CHModernSlider.SetRange PROCEDURE(LONG Minimum,LONG Maximum)
Result                  LONG
 CODE
 IF NOT SELF.Active OR Minimum >= Maximum
   RETURN FALSE
 END
 Result = CHSlider_SetRange(SELF.SliderHwnd,Minimum,Maximum)
 IF Result
   SELF.LastValue = CHSlider_GetValue(SELF.SliderHwnd)
 END
 RETURN Result

CHModernSlider.SetValue PROCEDURE(LONG Value)
Result                  LONG
 CODE
 IF NOT SELF.Active
   RETURN FALSE
 END
 Result = CHSlider_SetValue(SELF.SliderHwnd,Value)
 IF Result
   SELF.LastValue = CHSlider_GetValue(SELF.SliderHwnd)
 END
 RETURN Result

CHModernSlider.GetValue PROCEDURE()
 CODE
 IF NOT SELF.Active
   RETURN 0
 END
 RETURN CHSlider_GetValue(SELF.SliderHwnd)

CHModernSlider.SetStep PROCEDURE(LONG Step)
 CODE
 IF NOT SELF.Active OR Step <= 0
   RETURN FALSE
 END
 RETURN CHSlider_SetStep(SELF.SliderHwnd,Step)

CHModernSlider.SetEnabled PROCEDURE(BYTE Enabled)
 CODE
 IF NOT SELF.Active
   RETURN FALSE
 END
 RETURN CHSlider_SetEnabled(SELF.SliderHwnd,Enabled)

CHModernSlider.SetColors PROCEDURE(LONG LightTrack,LONG LightFill,LONG DarkTrack,LONG DarkFill)
 CODE
 IF NOT SELF.Active
   RETURN FALSE
 END
 RETURN CHSlider_SetColors(SELF.SliderHwnd,LightTrack,LightFill,DarkTrack,DarkFill)

CHModernSlider.TakeEvent PROCEDURE()
Result                  LONG
CurrentValue            LONG
 CODE
 ! Resize extensions can move the hidden REGION after EVENT:Sized has already
 ! passed this extension's embed. Re-read its final position on every ACCEPT
 ! loop pass so the native slider follows the Clarion layout.
 IF SELF.Active
   Result = SELF.SyncPosition()
   Result = SELF.SyncVisibility()
   ! The shared hidden BUTTON is notified only when a slider value changes.
   IF EVENT() = EVENT:Accepted AND FIELD() = SELF.NotifyFEQ
     CurrentValue = CHSlider_GetValue(SELF.SliderHwnd)
     IF CurrentValue <> SELF.LastValue
       SELF.LastValue = CurrentValue
       POST(EVENT:Accepted,SELF.RegionFEQ)
     END
   END
 END

CHModernSlider.Kill    PROCEDURE()
Result                  LONG
 CODE
 IF SELF.Active AND SELF.SliderHwnd
   Result = CHSlider_Destroy(SELF.SliderHwnd)
 END
 IF SELF.OwnNotify AND SELF.NotifyFEQ
   DESTROY(SELF.NotifyFEQ)
 END
 SELF.Active = FALSE
 SELF.SliderHwnd = 0
 SELF.RegionFEQ = 0
 SELF.NotifyFEQ = 0
 SELF.OwnNotify = FALSE
 SELF.LastValue = 0
 SELF.LastVisible = -1
