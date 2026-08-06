                     INCLUDE('EQUATES.CLW'),ONCE

CHModernSheet       CLASS,TYPE,MODULE('CHModernSheet.clw')
SheetFEQ              SIGNED
NotifyFEQ             SIGNED
OwnNotify             BYTE
StripHwnd             LONG
HeaderHeight          LONG
Selected              LONG
LightBackground       LONG
DarkBackground        LONG
LastBackground        LONG
LastVisible           LONG
TabFEQ                LONG,DIM(20)
Active                BYTE
Init                  PROCEDURE(LONG SheetFEQ,*CSTRING Labels,LONG NotifyFEQ,LONG HeaderHeight=26),LONG
SetTabFEQ             PROCEDURE(LONG Tab,LONG TabFEQ),LONG
SetSelected           PROCEDURE(LONG Tab),LONG
GetSelected           PROCEDURE(),LONG
SetEnabled            PROCEDURE(LONG Tab,BYTE Enabled),LONG
SetLabels             PROCEDURE(*CSTRING Labels),LONG
SetBackgroundColors   PROCEDURE(LONG LightBackground,LONG DarkBackground),LONG
SyncPosition          PROCEDURE(),LONG
SyncVisibility        PROCEDURE(),LONG
SyncTabVisibility     PROCEDURE(),LONG
SyncBackground        PROCEDURE(),LONG
SyncTabContents       PROCEDURE(LONG Tab),LONG
TakeEvent             PROCEDURE()
Kill                  PROCEDURE()
                     END
