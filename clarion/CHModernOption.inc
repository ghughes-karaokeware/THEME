                     INCLUDE('EQUATES.CLW'),ONCE

CHModernOption      CLASS,TYPE,MODULE('CHModernOption.clw')
OptionFEQ             SIGNED
OwnerFEQ              SIGNED
OwnerSheetFEQ         SIGNED
MaskHwnd              LONG
Active                BYTE
ChoiceCount           BYTE
Selected              BYTE
LastVisible           LONG
RadioFEQ              LONG,DIM(20)
CoverFEQ              LONG,DIM(20)
CoverWasHidden        BYTE,DIM(20)
Init                  PROCEDURE(LONG OptionFEQ,LONG OwnerFEQ=0,LONG OwnerSheetFEQ=0),LONG
SetRadioFEQ           PROCEDURE(LONG Choice,LONG RadioFEQ),LONG
SetChoices            PROCEDURE(*CSTRING Labels),LONG
SyncPosition          PROCEDURE(),LONG
SyncVisibility        PROCEDURE(),LONG
TakeEvent             PROCEDURE()
Kill                  PROCEDURE()
                     END
