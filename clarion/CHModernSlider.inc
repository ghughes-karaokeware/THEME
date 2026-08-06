                     INCLUDE('EQUATES.CLW'),ONCE

CHSLIDER_HORIZONTAL EQUATE(0)
CHSLIDER_VERTICAL   EQUATE(1)

CHModernSlider      CLASS,TYPE,MODULE('CHModernSlider.clw')
RegionFEQ             SIGNED
NotifyFEQ             SIGNED
SliderHwnd            LONG
Active                BYTE
OwnNotify             BYTE
LastValue             LONG
LastVisible           LONG
Init                  PROCEDURE(LONG RegionFEQ,LONG Orientation,LONG Minimum,LONG Maximum,LONG Value,LONG NotifyFEQ=0,LONG Step=1),LONG
SyncPosition          PROCEDURE(),LONG
SyncVisibility        PROCEDURE(),LONG
SetRange              PROCEDURE(LONG Minimum,LONG Maximum),LONG
SetValue              PROCEDURE(LONG Value),LONG
GetValue              PROCEDURE(),LONG
SetStep               PROCEDURE(LONG Step),LONG
SetEnabled            PROCEDURE(BYTE Enabled),LONG
SetColors             PROCEDURE(LONG LightTrack,LONG LightFill,LONG DarkTrack,LONG DarkFill),LONG
TakeEvent             PROCEDURE()
Kill                  PROCEDURE()
                     END
