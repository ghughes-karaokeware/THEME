#TEMPLATE (CHModernTheme, 'CompuHost V4 Modern Theme')
#! --------------------------------------------------------------------------
#! CompuHost V4 native Win32 modernization templates for Clarion 10.
#! Global declarations, project support, Windows build gate, and window theming.
#! --------------------------------------------------------------------------
#EXTENSION (CHModernThemeGlobal, 'CompuHost V4 Modern Theme - Global'), APPLICATION
#SHEET
  #TAB('General')
    #PROMPT('Enforce Windows 10 version 1607 or newer', CHECK),%CHTEnforceMinimum,DEFAULT(1)
    #PROMPT('Minimum Windows build', SPIN(@N5,10240,99999)),%CHTMinimumBuild,DEFAULT(14393)
    #PROMPT('Default application theme', DROP('Dark|Light')),%CHTDefaultTheme,DEFAULT('Dark')
    #PROMPT('Light window background', COLOR),%CHTLightWindowBackground,DEFAULT(00FAF6F2h)
    #PROMPT('Dark window background', COLOR),%CHTDarkWindowBackground,DEFAULT(00140D07h)
    #PROMPT('Show compatibility error message', CHECK),%CHTShowCompatibilityMessage,DEFAULT(1)
  #ENDTAB
#ENDSHEET

#AT(%CustomGlobalDeclarations)
  #! Link through the import libraries and always deploy the runtime DLLs
  #! beside the generated application executable.
  #! Clarion 10 rejects absolute paths inside None(...). Refresh the local
  #! generated-project copies from the registered repository first, then use
  #! the standard relative project items that Clarion emits correctly.
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\bin\CHTheme.dll" "CHTheme.dll"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\bin\ButtonSubclass.dll" "ButtonSubclass.dll"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\lib\CHTheme.lib" "CHTheme.lib"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\lib\ButtonSubclass.lib" "ButtonSubclass.lib"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernTheme.inc" "CHModernTheme.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernList.clw" "CHModernList.clw"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernList.inc" "CHModernList.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernSlider.clw" "CHModernSlider.clw"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernSlider.inc" "CHModernSlider.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernSheet.clw" "CHModernSheet.clw"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernSheet.inc" "CHModernSheet.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernOption.clw" "CHModernOption.clw"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHModernOption.inc" "CHModernOption.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHStructuredDialogTypes.inc" "CHStructuredDialogTypes.inc"'),WAIT
  #RUN('cmd.exe /c copy /Y "' & %cwroot & '\accessory\libsrc\win\CHPromoDesignerTypes.inc" "CHPromoDesignerTypes.inc"'),WAIT
  #PROJECT('CHTheme.lib')
  #PROJECT('ButtonSubclass.lib')
  #PROJECT('None(CHTheme.dll),CopyToOutputDirectory=Always')
  #PROJECT('None(ButtonSubclass.dll),CopyToOutputDirectory=Always')
  #PROJECT('CHModernList.clw')
  #PROJECT('CHModernSlider.clw')
  #PROJECT('CHModernSheet.clw')
  #PROJECT('CHModernOption.clw')
#ENDAT

#AT(%BeforeGlobalIncludes),PRIORITY(3500)
  INCLUDE('CHModernTheme.inc'),ONCE
  INCLUDE('CHModernList.inc'),ONCE
  INCLUDE('CHModernSlider.inc'),ONCE
  INCLUDE('CHModernSheet.inc'),ONCE
  INCLUDE('CHModernOption.inc'),ONCE
  INCLUDE('CHStructuredDialogTypes.inc'),ONCE
  INCLUDE('CHPromoDesignerTypes.inc'),ONCE
#ENDAT

#AT(%GlobalMap),PRIORITY(3500)
  MODULE('CHTheme.dll')
    CHTheme_GetWindowsBuild(),LONG,PASCAL,RAW,NAME('CHTheme_GetWindowsBuild')
    CHTheme_AttachWindow(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_AttachWindow')
    CHTheme_DetachWindow(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_DetachWindow')
    CHTheme_AttachAllControls(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_AttachAllControls')
    CHTheme_AttachDropList(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_AttachDropList')
    CHTheme_SetMode(LONG WindowHwnd,LONG Mode),LONG,PASCAL,RAW,NAME('CHTheme_SetMode')
    CHTheme_SetBackgroundColors(LONG WindowHwnd,LONG LightBackground,LONG DarkBackground),LONG,PASCAL,RAW,NAME('CHTheme_SetBackgroundColors')
    CHTheme_SetFont(LONG WindowHwnd,*CSTRING FaceName,LONG PointSize,LONG Bold,LONG Italic),LONG,PASCAL,RAW,NAME('CHTheme_SetFont')
    CHTheme_SetNotifyButton(LONG WindowHwnd,LONG NotifyButtonHwnd),LONG,PASCAL,RAW,NAME('CHTheme_SetNotifyButton')
    CHTheme_ConsumeFlatOptionNotify(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_ConsumeFlatOptionNotify')
    CHTheme_SetApplicationMode(LONG WindowHwnd,LONG Mode),LONG,PASCAL,RAW,NAME('CHTheme_SetApplicationMode')
    CHTheme_GetMode(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetMode')
    CHTheme_ForceRedraw(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_ForceRedraw')
    CHTheme_BeginUpdate(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_BeginUpdate')
    CHTheme_EndUpdate(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_EndUpdate')
    CHTheme_SetAccent(LONG WindowHwnd,LONG AccentColor),LONG,PASCAL,RAW,NAME('CHTheme_SetAccent')
    CHTheme_GetListHeader(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetListHeader')
    CHTheme_AttachListHeader(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_AttachListHeader')
    CHTheme_DetachListHeader(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_DetachListHeader')
    CHTheme_AttachClarionListHeader(LONG ListHwnd,LONG HeaderHeight),LONG,PASCAL,RAW,NAME('CHTheme_AttachClarionListHeader')
    CHTheme_SetClarionListColumns(LONG ListHwnd,*CSTRING Definition),LONG,PASCAL,RAW,NAME('CHTheme_SetClarionListColumns')
    CHTheme_SetClarionListSort(LONG ListHwnd,LONG Column,LONG Direction),LONG,PASCAL,RAW,NAME('CHTheme_SetClarionListSort')
    CHTheme_GetClarionListHeaderClick(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetClarionListHeaderClick')
    CHTheme_DetachClarionListHeader(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_DetachClarionListHeader')
    CHTheme_GetClarionListResizedColumn(LONG ListHwnd),LONG,PASCAL,RAW,NAME('CHTheme_GetClarionListResizedColumn')
    CHTheme_GetClarionListColumnWidth(LONG ListHwnd,LONG Column),LONG,PASCAL,RAW,NAME('CHTheme_GetClarionListColumnWidth')
    CHTheme_SetClarionListColumnBasis(LONG ListHwnd,LONG Column,LONG ClarionWidth),LONG,PASCAL,RAW,NAME('CHTheme_SetClarionListColumnBasis')
    CHTheme_SetClarionListControlBasis(LONG ListHwnd,LONG ClarionControlWidth),LONG,PASCAL,RAW,NAME('CHTheme_SetClarionListControlBasis')
    CHTheme_AttachMenu(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_AttachMenu')
    CHTheme_DetachMenu(LONG WindowHwnd),LONG,PASCAL,RAW,NAME('CHTheme_DetachMenu')
    CHTheme_AttachClarionMenu(LONG WindowHwnd,*CSTRING Definition),LONG,PASCAL,RAW,NAME('CHTheme_AttachClarionMenu')
    CHSlider_Create(LONG ParentHwnd,LONG x,LONG y,LONG Width,LONG Height,LONG Orientation,LONG Minimum,LONG Maximum,LONG Value,LONG NotifyButtonHwnd),LONG,PASCAL,RAW,NAME('CHSlider_Create')
    CHSlider_Destroy(LONG SliderHwnd),LONG,PASCAL,RAW,NAME('CHSlider_Destroy')
    CHSlider_SetRange(LONG SliderHwnd,LONG Minimum,LONG Maximum),LONG,PASCAL,RAW,NAME('CHSlider_SetRange')
    CHSlider_SetValue(LONG SliderHwnd,LONG Value),LONG,PASCAL,RAW,NAME('CHSlider_SetValue')
    CHSlider_GetValue(LONG SliderHwnd),LONG,PASCAL,RAW,NAME('CHSlider_GetValue')
    CHSlider_SetStep(LONG SliderHwnd,LONG Step),LONG,PASCAL,RAW,NAME('CHSlider_SetStep')
    CHSlider_SetEnabled(LONG SliderHwnd,LONG Enabled),LONG,PASCAL,RAW,NAME('CHSlider_SetEnabled')
    CHSlider_SetColors(LONG SliderHwnd,LONG LightTrack,LONG LightFill,LONG DarkTrack,LONG DarkFill),LONG,PASCAL,RAW,NAME('CHSlider_SetColors')
    CHUI_GetAbiVersion(),ULONG,PASCAL,RAW,NAME('CHUI_GetAbiVersion')
    CHUI_GetHeaderSize(),ULONG,PASCAL,RAW,NAME('CHUI_GetHeaderSize')
    CHUI_GetEntrySize(),ULONG,PASCAL,RAW,NAME('CHUI_GetEntrySize')
    CHUI_ValidateDialog(LONG HeaderAddr,LONG EntriesAddr),LONG,PASCAL,RAW,NAME('CHUI_ValidateDialog')
    CHUI_OpenDialog(LONG OwnerHwnd,LONG HeaderAddr,LONG EntriesAddr,LONG CallbackHwnd),LONG,PASCAL,RAW,NAME('CHUI_OpenDialog')
    CHUI_ConsumeCompletion(LONG CallbackHwnd,*ULONG InstanceID,*LONG Result),LONG,PASCAL,RAW,NAME('CHUI_ConsumeCompletion')
    CHUI_ConsumeChange(LONG CallbackHwnd,*ULONG InstanceID,*ULONG EntryID),LONG,PASCAL,RAW,NAME('CHUI_ConsumeChange')
    CHUI_ConsumeAction(LONG CallbackHwnd,*ULONG InstanceID,*ULONG EntryID),LONG,PASCAL,RAW,NAME('CHUI_ConsumeAction')
    CHUI_SetEntryValue(ULONG InstanceID,ULONG EntryID,*CSTRING Value),LONG,PASCAL,RAW,NAME('CHUI_SetEntryValue')
    CHPT_GetAbiVersion(),ULONG,PASCAL,RAW,NAME('CHPT_GetAbiVersion')
    CHPT_GetDataSize(),ULONG,PASCAL,RAW,NAME('CHPT_GetDataSize')
    CHPT_ValidateData(LONG DataAddress),LONG,PASCAL,RAW,NAME('CHPT_ValidateData')
    CHPT_OpenDesigner(LONG OwnerHwnd,LONG DataAddress,LONG CompletionButtonHwnd),LONG,PASCAL,RAW,NAME('CHPT_OpenDesigner')
    CHPT_ConsumeCompletion(LONG CompletionButtonHwnd,*ULONG InstanceID,*LONG DesignerResult),LONG,PASCAL,RAW,NAME('CHPT_ConsumeCompletion')
  END
  MODULE('ButtonSubclass.dll')
    CHButton_SetTheme(LONG Theme),LONG,PASCAL,RAW,NAME('CHButton_SetTheme')
    CHButton_Attach(LONG ButtonHwnd),LONG,PASCAL,RAW,NAME('CHButton_Attach')
    CHButton_Detach(LONG ButtonHwnd),LONG,PASCAL,RAW,NAME('CHButton_Detach')
    CHButton_SetColors(LONG ButtonHwnd,LONG NormalTop,LONG NormalBottom,LONG HoverTop,LONG HoverBottom,LONG PressedTop,LONG PressedBottom,LONG TextColor,LONG BorderColor),LONG,PASCAL,RAW,NAME('CHButton_SetColors')
    CHButton_SetMetrics(LONG ButtonHwnd,LONG CornerRadius,LONG BorderWidth,LONG UseGradient),LONG,PASCAL,RAW,NAME('CHButton_SetMetrics')
    CHButton_SetIcon(LONG ButtonHwnd,LONG IconHandle,LONG IconWidth,LONG Gap),LONG,PASCAL,RAW,NAME('CHButton_SetIcon')
    CHButton_SetIconFile(LONG ButtonHwnd,*CSTRING IconName,LONG IconWidth,LONG Gap),LONG,PASCAL,RAW,NAME('CHButton_SetIconFile')
    CHButton_SetContentAlignment(LONG ButtonHwnd,LONG Horizontal,LONG Vertical),LONG,PASCAL,RAW,NAME('CHButton_SetContentAlignment')
    CHButton_AttachAll(LONG ParentHwnd),LONG,PASCAL,RAW,NAME('CHButton_AttachAll')
    CHButton_DetachAll(),PASCAL,RAW,NAME('CHButton_DetachAll')
    CHButton_SetDisabledColors(LONG ButtonHwnd,LONG TopColor,LONG BottomColor,LONG TextColor,LONG BorderColor),LONG,PASCAL,RAW,NAME('CHButton_SetDisabledColors')
    CHButton_SetRole(LONG ButtonHwnd,LONG Role),LONG,PASCAL,RAW,NAME('CHButton_SetRole')
  END
  CHModernTheme_CheckWindows(LONG MinimumBuild,*LONG DetectedBuild),BYTE
  CHModernTheme_SetApplicationTheme(LONG WindowHwnd,LONG Mode),LONG
  CHModernTheme_Refresh(LONG WindowHwnd),LONG
#ENDAT

#GLOBALDATA
CHModernTheme:DetectedBuild LONG
CHModernTheme:DefaultMode   LONG(0)
CHModernTheme:SetupResult   LONG
#ENDGLOBALDATA

#AT(%ProgramSetup),PRIORITY(100)
#IF(%CHTDefaultTheme = 'Light')
  CHModernTheme:DefaultMode = CHTHEME_LIGHT
#ELSE
  CHModernTheme:DefaultMode = CHTHEME_DARK
#ENDIF
  CHModernTheme:SetupResult = CHModernTheme_SetApplicationTheme(0,CHModernTheme:DefaultMode)
#IF(%CHTEnforceMinimum)
  IF NOT CHModernTheme_CheckWindows(%CHTMinimumBuild,CHModernTheme:DetectedBuild)
#IF(%CHTShowCompatibilityMessage)
    MESSAGE('CompuHost V4 requires Windows 10 version 1607 or newer.' & |
            '<13,10>Detected Windows build: ' & CLIP(CHModernTheme:DetectedBuild) & |
            '<13,10>Required Windows build: %CHTMinimumBuild', |
            'CompuHost V4 - Unsupported Windows',ICON:Exclamation)
#ENDIF
    HALT
  END
#ENDIF
#ENDAT

#AT(%ProgramProcedures),PRIORITY(3500)
CHModernTheme_CheckWindows PROCEDURE(LONG MinimumBuild,*LONG DetectedBuild)
  CODE
  DetectedBuild = CHTheme_GetWindowsBuild()
  IF DetectedBuild = 0 OR DetectedBuild < MinimumBuild
    RETURN FALSE
  END
  RETURN TRUE

CHModernTheme_SetApplicationTheme PROCEDURE(LONG WindowHwnd,LONG Mode)
Result LONG
  CODE
  Result = CHTheme_SetApplicationMode(WindowHwnd,Mode)
  IF Mode = CHTHEME_LIGHT
    Result = CHButton_SetTheme(CHBUTTON_THEME_LIGHT)
  ELSE
    Result = CHButton_SetTheme(CHBUTTON_THEME_DARK)
  END
  Result = CHTheme_ForceRedraw(WindowHwnd)
  RETURN Result

CHModernTheme_Refresh PROCEDURE(LONG WindowHwnd)
  CODE
  RETURN CHTheme_ForceRedraw(WindowHwnd)
#ENDAT

#! Recursively emit every ITEM and child MENU using the Win32 positional path
#! expected by CHTheme_AttachClarionMenu (for example 1/2/0).
#GROUP(%CHTEmitMenuChildren,%CHTParentControl,%CHTParentPath)
#DECLARE(%CHTChildIndex)
#DECLARE(%CHTChildControl)
#DECLARE(%CHTChildCaption)
#DECLARE(%CHTChildPath)
#SET(%CHTChildIndex,0)
#FOR(%Control),WHERE(%ControlParent=%CHTParentControl AND (%ControlType='ITEM' OR %ControlType='MENU'))
#SET(%CHTChildControl,%Control)
#SET(%CHTChildPath,%CHTParentPath & '/' & %CHTChildIndex)
#IF(%ControlType='MENU')
#SET(%CHTChildCaption,EXTRACT(%ControlStatement,'MENU',0))
#ELSE
#SET(%CHTChildCaption,EXTRACT(%ControlStatement,'ITEM',0))
#ENDIF
#IF(%CHTChildCaption)
  IF %Control{PROP:Hide}
    CHModernTheme:MenuDefinition = CLIP(CHModernTheme:MenuDefinition) & '%CHTChildPath@' & %Control & '=~H~|;'
  ELSE
    CHModernTheme:MenuDefinition = CLIP(CHModernTheme:MenuDefinition) & '%CHTChildPath@' & %Control & '=' & %CHTChildCaption & '|' & %Control{PROP:Icon} & ';'
  END
#ELSE
    CHModernTheme:MenuDefinition = CLIP(CHModernTheme:MenuDefinition) & '%CHTChildPath@' & %Control & '=-|;'
#ENDIF
#IF(%ControlType='MENU')
#CALL(%CHTEmitMenuChildren,%CHTChildControl,%CHTChildPath)
#ENDIF
#SET(%CHTChildIndex,%CHTChildIndex+1)
#ENDFOR
#!

#GROUP(%CHTEmitMenuDefinition)
    CHModernTheme:MenuDefinition = ''
#DECLARE(%CHTMenuControl)
#DECLARE(%CHTMenuIndex)
#DECLARE(%CHTMenuCaption)
#SET(%CHTMenuIndex,0)
#FOR(%Control),WHERE(%ControlType='MENU' AND %ControlParentType='MENUBAR')
#SET(%CHTMenuControl,%Control)
#SET(%CHTMenuCaption,EXTRACT(%ControlStatement,'MENU',0))
  IF %Control{PROP:Hide}
    CHModernTheme:MenuDefinition = CLIP(CHModernTheme:MenuDefinition) & '%CHTMenuIndex@' & %Control & '=~H~|;'
  ELSE
    CHModernTheme:MenuDefinition = CLIP(CHModernTheme:MenuDefinition) & '%CHTMenuIndex@' & %Control & '=' & %CHTMenuCaption & '|' & %Control{PROP:Icon} & ';'
  END
#CALL(%CHTEmitMenuChildren,%CHTMenuControl,%CHTMenuIndex)
#SET(%CHTMenuIndex,%CHTMenuIndex+1)
#ENDFOR
#!

#! --------------------------------------------------------------------------
#! Procedure extension. Add once to each Legacy or ABC window procedure.
#! --------------------------------------------------------------------------
#EXTENSION (CHModernThemeWindow, 'CompuHost V4 Modern Theme - Window'), PROCEDURE, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTWControlChoices),UNIQUE
  #DECLARE(%CHTWButtonChoices),UNIQUE
  #FOR(%Control)
    #ADD(%CHTWControlChoices,%Control)
    #IF(%ControlType = 'BUTTON')
      #ADD(%CHTWButtonChoices,%Control)
    #ENDIF
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('Window')
    #PROMPT('Apply global default theme', CHECK),%CHTWApplyTheme,DEFAULT(1)
    #PROMPT('Apply modern font family', CHECK),%CHTWApplyFont,DEFAULT(1)
    #ENABLE(%CHTWApplyFont)
      #PROMPT('Font family', @S63),%CHTWFontFamily,DEFAULT('Segoe UI')
      #PROMPT('Default font size', SPIN(@N2,6,36)),%CHTWFontSize,DEFAULT(9)
      #PROMPT('Default bold', CHECK),%CHTWFontBold,DEFAULT(0)
      #PROMPT('Default italic', CHECK),%CHTWFontItalic,DEFAULT(0)
    #ENDENABLE
    #PROMPT('Override global window backgrounds', CHECK),%CHTWOverrideBackground,DEFAULT(0)
    #ENABLE(%CHTWOverrideBackground)
      #PROMPT('Light window background', COLOR),%CHTWLightWindowBackground,DEFAULT(00FAF6F2h)
      #PROMPT('Dark window background', COLOR),%CHTWDarkWindowBackground,DEFAULT(00140D07h)
    #ENDENABLE
    #PROMPT('Modernize standard controls', CHECK),%CHTWAttachControls,DEFAULT(1)
    #PROMPT('Apply automatic Clarion control colors', CHECK),%CHTWApplyControlColors,DEFAULT(1)
    #BUTTON('Controls preserving application colors'),MULTI(%CHTWColorExclusions,%CHTWColorExclusion),AT(,,174,)
      #PROMPT('Control:',FROM(%CHTWControlChoices)),%CHTWColorExclusion
    #ENDBUTTON
    #PROMPT('Modernize all BUTTON controls', CHECK),%CHTWAttachButtons,DEFAULT(1)
    #BUTTON('BUTTON controls not modernized'),MULTI(%CHTWButtonExclusions,%CHTWButtonExclusion),AT(,,174,)
      #PROMPT('Button:',FROM(%CHTWButtonChoices)),%CHTWButtonExclusion
    #ENDBUTTON
    #PROMPT('Derive and modernize the Clarion menu', CHECK),%CHTWAttachMenu,DEFAULT(1)
    #PROMPT('Discover and modernize LIST controls', CHECK),%CHTWAutoLists,DEFAULT(1)
    #PROMPT('LIST header height', SPIN(@N3,12,80)),%CHTWListHeaderHeight,DEFAULT(18)
    #PROMPT('Discover and modernize SHEET controls', CHECK),%CHTWAutoSheets,DEFAULT(1)
    #PROMPT('SHEET header height', SPIN(@N3,18,80)),%CHTWSheetHeaderHeight,DEFAULT(26)
    #PROMPT('Light SHEET content background', COLOR),%CHTWLightSheetBackground,DEFAULT(00FFFFFFh)
    #PROMPT('Dark SHEET content background', COLOR),%CHTWDarkSheetBackground,DEFAULT(00181008h)
    #PROMPT('Discover and flatten OPTION controls', CHECK),%CHTWAutoOptions,DEFAULT(1)
  #ENDTAB
#ENDSHEET

#AT(%DataSection),PRIORITY(3500)
CHModernTheme:Result LONG
CHModernTheme:StartupUpdate BYTE
CHModernTheme:StartupPass BYTE
CHModernTheme:AppliedMode LONG(-1)
CHModernTheme:ControlFEQ LONG
CHModernTheme:ControlType LONG
CHModernTheme:ControlExcluded BYTE
CHModernTheme:LastField LONG(-1)
CHModernTheme:ListColumn LONG
#IF(%CHTWApplyFont)
CHModernTheme:FontFamily CSTRING(64)
#ENDIF
#IF(%CHTWAttachButtons)
#DECLARE(%CHTButtonRuntimeIcon)
#FOR(%Control),WHERE(%ControlType='BUTTON' AND %Control <> '')
#SET(%CHTButtonRuntimeIcon,EXTRACT(%ControlStatement,'ICON',0))
#IF(%CHTButtonRuntimeIcon)
CHModernTheme:ButtonIcon:%(SUB(%Control,2,LEN(%Control)-1)) CSTRING(260)
#ENDIF
#ENDFOR
#ENDIF
#IF(%CHTWAttachMenu)
CHModernTheme:MenuDefinition CSTRING(32768)
CHModernTheme:AppliedMenuDefinition CSTRING(32768)
#ENDIF
#IF(%CHTWAutoLists)
#FOR(%Control),WHERE(%ControlType='LIST' AND %Control <> '' AND EXTRACT(%ControlStatement,'DROP',0) = '')
CHModernTheme:List:%(SUB(%Control,2,LEN(%Control)-1)) CHModernList
CHModernTheme:ListFormat:%(SUB(%Control,2,LEN(%Control)-1)) CSTRING(2048)
#ENDFOR
#ENDIF
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
CHModernTheme:Sheet:%(SUB(%Control,2,LEN(%Control)-1)) CHModernSheet
CHModernTheme:SheetLabels:%(SUB(%Control,2,LEN(%Control)-1)) CSTRING(2048)
#ENDFOR
#ENDIF
#IF(%CHTWAutoOptions)
#FOR(%Control),WHERE(%ControlType='OPTION' AND %Control <> '')
CHModernTheme:Option:%(SUB(%Control,2,LEN(%Control)-1)) CHModernOption
CHModernTheme:OptionLabels:%(SUB(%Control,2,LEN(%Control)-1)) CSTRING(2048)
#ENDFOR
#ENDIF
#ENDAT

#AT(%DataSectionEndWindow),PRIORITY(3400)
  BUTTON(''),AT(0,0,1,1),USE(?CHThemeNotify),HIDE
  BUTTON(''),AT(0,0,1,1),USE(?CHThemeStartupComplete),HIDE
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
  BUTTON(''),AT(0,0,1,1),USE(?CHSheetNotify:%(SUB(%Control,2,LEN(%Control)-1))),HIDE
#ENDFOR
#ENDIF
#ENDAT

#AT(%AfterWindowOpening),FIRST
#IF(EXTRACT(%WindowStatement,'APPLICATION') = '')
  IF 0{PROP:Handle}
    0{PROP:Hide} = TRUE
    CHModernTheme:Result = CHTheme_BeginUpdate(0{PROP:Handle})
    CHModernTheme:StartupUpdate = 1
    CHModernTheme:StartupPass = 0
  END
#ENDIF
#ENDAT

#! WindowResizeClass captures its baseline in %AfterWindowOpening. Theme and
#! custom-control initialization must run afterward but before INI restore.
#AT(%BeforeWindowIniResize),PRIORITY(9000)
  IF 0{PROP:Handle}
#IF(%CHTWApplyFont)
    CHModernTheme:FontFamily = '%CHTWFontFamily'
    0{PROP:FontName} = CHModernTheme:FontFamily
#IF(EXTRACT(%WindowStatement,'FONT',0) = '')
    0{PROP:FontSize} = %CHTWFontSize
#IF(%CHTWFontBold AND %CHTWFontItalic)
    0{PROP:FontStyle} = FONT:Bold + FONT:Italic
#ELSIF(%CHTWFontBold)
    0{PROP:FontStyle} = FONT:Bold
#ELSIF(%CHTWFontItalic)
    0{PROP:FontStyle} = FONT:Italic
#ELSE
    0{PROP:FontStyle} = FONT:Regular
#ENDIF
#ENDIF
#FOR(%Control),WHERE(%Control <> '')
    %Control{PROP:FontName} = CHModernTheme:FontFamily
#IF(EXTRACT(%ControlStatement,'FONT',0) = '')
    %Control{PROP:FontSize} = %CHTWFontSize
#IF(%CHTWFontBold AND %CHTWFontItalic)
    %Control{PROP:FontStyle} = FONT:Bold + FONT:Italic
#ELSIF(%CHTWFontBold)
    %Control{PROP:FontStyle} = FONT:Bold
#ELSIF(%CHTWFontItalic)
    %Control{PROP:FontStyle} = FONT:Italic
#ELSE
    %Control{PROP:FontStyle} = FONT:Regular
#ENDIF
#ENDIF
#ENDFOR
#ENDIF
    CHModernTheme:Result = CHTheme_AttachWindow(0{PROP:Handle})
#IF(%CHTWApplyFont)
    CHModernTheme:Result = CHTheme_SetFont(0{PROP:Handle},CHModernTheme:FontFamily,%CHTWFontSize,%CHTWFontBold,%CHTWFontItalic)
#ENDIF
    CHModernTheme:Result = CHTheme_SetNotifyButton(0{PROP:Handle},?CHThemeNotify{PROP:Handle})
#IF(%CHTWOverrideBackground)
    CHModernTheme:Result = CHTheme_SetBackgroundColors(0{PROP:Handle},%CHTWLightWindowBackground,%CHTWDarkWindowBackground)
#ELSE
    CHModernTheme:Result = CHTheme_SetBackgroundColors(0{PROP:Handle},%CHTLightWindowBackground,%CHTDarkWindowBackground)
#ENDIF
    DO CHModernThemeRefreshControls
#IF(%CHTWAttachMenu)
    DO CHModernThemeRefreshMenu
#ENDIF
#IF(%CHTWAutoLists)
#FOR(%Control),WHERE(%ControlType='LIST' AND %Control <> '' AND EXTRACT(%ControlStatement,'DROP',0) <> '')
    CHModernTheme:Result = CHTheme_AttachDropList(%Control{PROP:Handle})
#ENDFOR
#DECLARE(%CHTListFormat)
#FOR(%Control),WHERE(%ControlType='LIST' AND %Control <> '' AND EXTRACT(%ControlStatement,'DROP',0) = '')
#SET(%CHTListFormat,EXTRACT(%ControlStatement,'FORMAT',0))
#IF(%CHTListFormat)
    CHModernTheme:ListFormat:%(SUB(%Control,2,LEN(%Control)-1)) = %CHTListFormat
#ELSE
    CHModernTheme:ListFormat:%(SUB(%Control,2,LEN(%Control)-1)) = ''
#ENDIF
    CHModernTheme:Result = CHModernTheme:List:%(SUB(%Control,2,LEN(%Control)-1)).Init(%Control,CHModernTheme:ListFormat:%(SUB(%Control,2,LEN(%Control)-1)),%CHTWListHeaderHeight)
#ENDFOR
#ENDIF
#IF(%CHTWAutoSheets)
#DECLARE(%CHTSheetControl)
#DECLARE(%CHTTabNumber)
#DECLARE(%CHTTabCaption)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
#SET(%CHTSheetControl,%Control)
    CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)) = ''
#SET(%CHTTabNumber,1)
#FOR(%Control),WHERE(%ControlType='TAB' AND %ControlParent=%CHTSheetControl)
#SET(%CHTTabCaption,EXTRACT(%ControlStatement,'TAB',0))
#IF(%CHTTabNumber=1)
#IF(%CHTTabCaption)
    CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)) = %CHTTabCaption
#ELSE
    CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)) = ''
#ENDIF
#ELSE
#IF(%CHTTabCaption)
    CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)) = CLIP(CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1))) & '|' & %CHTTabCaption
#ELSE
    CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)) = CLIP(CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1))) & '|'
#ENDIF
#ENDIF
#SET(%CHTTabNumber,%CHTTabNumber+1)
#ENDFOR
    CHModernTheme:Result = CHModernTheme:Sheet:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)).Init(%CHTSheetControl,CHModernTheme:SheetLabels:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)),?CHSheetNotify:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)),%CHTWSheetHeaderHeight)
#SET(%CHTTabNumber,1)
#FOR(%Control),WHERE(%ControlType='TAB' AND %ControlParent=%CHTSheetControl)
    CHModernTheme:Result = CHModernTheme:Sheet:%(SUB(%CHTSheetControl,2,LEN(%CHTSheetControl)-1)).SetTabFEQ(%CHTTabNumber,%Control)
#SET(%CHTTabNumber,%CHTTabNumber+1)
#ENDFOR
#ENDFOR
#ENDIF
#IF(%CHTWAutoOptions)
#DECLARE(%CHTOptionControl)
#DECLARE(%CHTOptionOwner)
#DECLARE(%CHTOptionSheet)
#DECLARE(%CHTRadioNumber)
#DECLARE(%CHTRadioCaption)
#FOR(%Control),WHERE(%ControlType='OPTION' AND %Control <> '')
#SET(%CHTOptionControl,%Control)
#SET(%CHTOptionOwner,%ControlParent)
#SET(%CHTOptionSheet,'0')
#FOR(%Control),WHERE(%Control=%CHTOptionOwner)
#SET(%CHTOptionSheet,%ControlParent)
#ENDFOR
    CHModernTheme:OptionLabels:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)) = ''
    CHModernTheme:Result = CHModernTheme:Option:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)).Init(%CHTOptionControl,%ControlUse,%CHTOptionOwner,%CHTOptionSheet)
#SET(%CHTRadioNumber,1)
#FOR(%Control),WHERE(%ControlType='RADIO' AND %ControlParent=%CHTOptionControl)
#SET(%CHTRadioCaption,EXTRACT(%ControlStatement,'RADIO',0))
#IF(%CHTRadioNumber=1)
    CHModernTheme:OptionLabels:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)) = %CHTRadioCaption
#ELSE
    CHModernTheme:OptionLabels:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)) = CLIP(CHModernTheme:OptionLabels:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1))) & '|' & %CHTRadioCaption
#ENDIF
    CHModernTheme:Result = CHModernTheme:Option:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)).SetRadioFEQ(%CHTRadioNumber,%Control)
#SET(%CHTRadioNumber,%CHTRadioNumber+1)
#ENDFOR
    CHModernTheme:Result = CHModernTheme:Option:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)).SetChoices(CHModernTheme:OptionLabels:%(SUB(%CHTOptionControl,2,LEN(%CHTOptionControl)-1)))
#ENDFOR
#ENDIF
  END
#ENDAT

#AT(%AcceptLoopAfterEventHandling),PRIORITY(9900)
  IF CHModernTheme:StartupUpdate AND EVENT() = EVENT:OpenWindow
    POST(EVENT:Accepted,?CHThemeStartupComplete)
  END
#ENDAT

#! INI restore can resize or maximize the window after themed overlays were
#! created. Queue the normal resize event and repaint without changing the
#! resize manager's already-captured baseline geometry.
#AT(%AfterWindowIniResize),PRIORITY(9000)
  IF 0{PROP:Handle}
    POST(EVENT:DoResize,0,THREAD())
    CHModernTheme:Result = CHTheme_ForceRedraw(0{PROP:Handle})
  END
#ENDAT

#AT(%AcceptLoopAfterFieldHandling),PRIORITY(9000)
  IF CHModernTheme:StartupUpdate AND FIELD() = ?CHThemeStartupComplete AND EVENT() = EVENT:Accepted
    IF CHModernTheme:StartupPass < 8
      CHModernTheme:StartupPass += 1
      POST(EVENT:Accepted,?CHThemeStartupComplete)
    ELSE
      CHModernTheme:Result = CHTheme_EndUpdate(0{PROP:Handle})
      0{PROP:Hide} = FALSE
      DISPLAY
      CHModernTheme:StartupUpdate = 0
    END
  END
#IF(%CHTWAttachMenu)
  DO CHModernThemeRefreshMenu
#ENDIF
#IF(%CHTWAttachControls OR %CHTWAttachButtons)
  CHModernTheme:Result = CHTheme_GetMode(0{PROP:Handle})
  IF CHModernTheme:Result <> CHModernTheme:AppliedMode OR LASTFIELD() <> CHModernTheme:LastField
    DO CHModernThemeRefreshControls
  END
#ENDIF
#ENDAT

#AT(%AcceptLoopAfterFieldHandling),PRIORITY(9050)
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
  CHModernTheme:Sheet:%(SUB(%Control,2,LEN(%Control)-1)).TakeEvent()
#ENDFOR
#ENDIF
#ENDAT

#AT(%ProcedureRoutines),PRIORITY(9000)
#IF(%CHTWAttachMenu)
CHModernThemeRefreshMenu ROUTINE
  IF 0{PROP:Handle}
#CALL(%CHTEmitMenuDefinition)
    IF CHModernTheme:MenuDefinition <> CHModernTheme:AppliedMenuDefinition
      IF CHModernTheme:MenuDefinition
        CHModernTheme:Result = CHTheme_AttachClarionMenu(0{PROP:Handle},CHModernTheme:MenuDefinition)
      ELSE
        CHModernTheme:Result = CHTheme_AttachMenu(0{PROP:Handle})
      END
      CHModernTheme:AppliedMenuDefinition = CHModernTheme:MenuDefinition
    END
  END
#ENDIF

CHModernThemeRefreshControls ROUTINE
  IF 0{PROP:Handle}
    CHModernTheme:AppliedMode = CHTheme_GetMode(0{PROP:Handle})
    IF CHModernTheme:AppliedMode = CHTHEME_LIGHT
#IF(%CHTWAttachButtons)
      CHModernTheme:Result = CHButton_SetTheme(CHBUTTON_THEME_LIGHT)
#ENDIF
#IF(%CHTWAttachControls)
#IF(%CHTWApplyControlColors)
#IF(%CHTWOverrideBackground)
      0{PROP:Color} = %CHTWLightWindowBackground
#ELSE
      0{PROP:Color} = %CHTLightWindowBackground
#ENDIF
      LOOP CHModernTheme:ControlFEQ = FIRSTFIELD() TO LASTFIELD()
        CHModernTheme:ControlExcluded = FALSE
#FOR(%CHTWColorExclusions)
        IF CHModernTheme:ControlFEQ = %CHTWColorExclusion
          CHModernTheme:ControlExcluded = TRUE
        END
#ENDFOR
#! Preserve deliberately colored STRING controls, including shadow/highlight
#! pairs. Ordinary uncolored STRING controls continue to follow the theme.
#FOR(%Control),WHERE(%ControlType='STRING' AND %Control <> '' AND INSTRING('COLOR:',UPPER(%ControlStatement),1,1) > 0)
        IF CHModernTheme:ControlFEQ = %Control
          CHModernTheme:ControlExcluded = TRUE
        END
#ENDFOR
        IF NOT CHModernTheme:ControlExcluded
        CHModernTheme:ControlType = CHModernTheme:ControlFEQ{PROP:Type}
        IF CHModernTheme:ControlType = CREATE:Entry OR |
           CHModernTheme:ControlType = CREATE:Text OR |
           CHModernTheme:ControlType = CREATE:Spin OR |
           CHModernTheme:ControlType = CREATE:Combo OR |
           CHModernTheme:ControlType = CREATE:DropCombo OR |
           CHModernTheme:ControlType = CREATE:DropList OR |
           CHModernTheme:ControlType = CREATE:List
          CHModernTheme:ControlFEQ{PROP:Color} = 00FFFFFFh
          CHModernTheme:ControlFEQ{PROP:FontColor} = 00342619h
          IF CHModernTheme:ControlType = CREATE:Combo OR |
             CHModernTheme:ControlType = CREATE:DropCombo OR |
             CHModernTheme:ControlType = CREATE:DropList OR |
             CHModernTheme:ControlType = CREATE:List
            CHModernTheme:ControlFEQ{PROP:Color,1} = 00FFFFFFh
            CHModernTheme:ControlFEQ{PROP:Color,2} = 00FFFFFFh
            CHModernTheme:ControlFEQ{PROP:Color,3} = 00D47800h
            LOOP CHModernTheme:ListColumn = 1 TO 255
              IF NOT CHModernTheme:ControlFEQ{PROPLIST:Exists,CHModernTheme:ListColumn}
                BREAK
              END
              CHModernTheme:ControlFEQ{PROPLIST:TextColor,CHModernTheme:ListColumn} = 00342619h
              CHModernTheme:ControlFEQ{PROPLIST:BackColor,CHModernTheme:ListColumn} = 00FFFFFFh
              CHModernTheme:ControlFEQ{PROPLIST:TextSelected,CHModernTheme:ListColumn} = 00FFFFFFh
              CHModernTheme:ControlFEQ{PROPLIST:BackSelected,CHModernTheme:ListColumn} = 00D47800h
            END
          END
        ELSIF CHModernTheme:ControlType = CREATE:String OR |
              CHModernTheme:ControlType = CREATE:Prompt OR |
              CHModernTheme:ControlType = CREATE:Option OR |
              CHModernTheme:ControlType = CREATE:Radio OR |
              CHModernTheme:ControlType = CREATE:Check OR |
              CHModernTheme:ControlType = CREATE:Group
          CHModernTheme:ControlFEQ{PROP:FontColor} = 00342619h
        END
        END
      END
#ENDIF
#ENDIF
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '')
      %Control{PROP:Color} = %CHTWLightSheetBackground
#ENDFOR
#ENDIF
    ELSE
#IF(%CHTWAttachButtons)
      CHModernTheme:Result = CHButton_SetTheme(CHBUTTON_THEME_DARK)
#ENDIF
#IF(%CHTWAttachControls)
#IF(%CHTWApplyControlColors)
#IF(%CHTWOverrideBackground)
      0{PROP:Color} = %CHTWDarkWindowBackground
#ELSE
      0{PROP:Color} = %CHTDarkWindowBackground
#ENDIF
      LOOP CHModernTheme:ControlFEQ = FIRSTFIELD() TO LASTFIELD()
        CHModernTheme:ControlExcluded = FALSE
#FOR(%CHTWColorExclusions)
        IF CHModernTheme:ControlFEQ = %CHTWColorExclusion
          CHModernTheme:ControlExcluded = TRUE
        END
#ENDFOR
#FOR(%Control),WHERE(%ControlType='STRING' AND %Control <> '' AND INSTRING('COLOR:',UPPER(%ControlStatement),1,1) > 0)
        IF CHModernTheme:ControlFEQ = %Control
          CHModernTheme:ControlExcluded = TRUE
        END
#ENDFOR
        IF NOT CHModernTheme:ControlExcluded
        CHModernTheme:ControlType = CHModernTheme:ControlFEQ{PROP:Type}
        IF CHModernTheme:ControlType = CREATE:Entry OR |
           CHModernTheme:ControlType = CREATE:Text OR |
           CHModernTheme:ControlType = CREATE:Spin OR |
           CHModernTheme:ControlType = CREATE:Combo OR |
           CHModernTheme:ControlType = CREATE:DropCombo OR |
           CHModernTheme:ControlType = CREATE:DropList OR |
           CHModernTheme:ControlType = CREATE:List
          CHModernTheme:ControlFEQ{PROP:Color} = 00181008h
          CHModernTheme:ControlFEQ{PROP:FontColor} = 00F8F1EBh
          IF CHModernTheme:ControlType = CREATE:Combo OR |
             CHModernTheme:ControlType = CREATE:DropCombo OR |
             CHModernTheme:ControlType = CREATE:DropList OR |
             CHModernTheme:ControlType = CREATE:List
            CHModernTheme:ControlFEQ{PROP:Color,1} = 00181008h
            CHModernTheme:ControlFEQ{PROP:Color,2} = 00FFFFFFh
            CHModernTheme:ControlFEQ{PROP:Color,3} = 00D47800h
            LOOP CHModernTheme:ListColumn = 1 TO 255
              IF NOT CHModernTheme:ControlFEQ{PROPLIST:Exists,CHModernTheme:ListColumn}
                BREAK
              END
              CHModernTheme:ControlFEQ{PROPLIST:TextColor,CHModernTheme:ListColumn} = 00F8F1EBh
              CHModernTheme:ControlFEQ{PROPLIST:BackColor,CHModernTheme:ListColumn} = 00181008h
              CHModernTheme:ControlFEQ{PROPLIST:TextSelected,CHModernTheme:ListColumn} = 00FFFFFFh
              CHModernTheme:ControlFEQ{PROPLIST:BackSelected,CHModernTheme:ListColumn} = 00D47800h
            END
          END
        ELSIF CHModernTheme:ControlType = CREATE:String OR |
              CHModernTheme:ControlType = CREATE:Prompt OR |
              CHModernTheme:ControlType = CREATE:Option OR |
              CHModernTheme:ControlType = CREATE:Radio OR |
              CHModernTheme:ControlType = CREATE:Check OR |
              CHModernTheme:ControlType = CREATE:Group
          CHModernTheme:ControlFEQ{PROP:FontColor} = 00F8F1EBh
        END
        END
      END
#ENDIF
#ENDIF
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '')
          %Control{PROP:Color} = %CHTWDarkSheetBackground
#ENDFOR
#ENDIF
    END
#IF(%CHTWAttachControls)
    CHModernTheme:Result = CHTheme_AttachAllControls(0{PROP:Handle})
#ENDIF
#IF(%CHTWAttachButtons)
    CHModernTheme:Result = CHButton_AttachAll(0{PROP:Handle})
#DECLARE(%CHTButtonIcon)
#DECLARE(%CHTButtonStatement)
#DECLARE(%CHTButtonHorizontal)
#DECLARE(%CHTButtonVertical)
#FOR(%Control),WHERE(%ControlType='BUTTON' AND %Control <> '')
#SET(%CHTButtonIcon,EXTRACT(%ControlStatement,'ICON',0))
#SET(%CHTButtonStatement,UPPER(%ControlStatement))
#SET(%CHTButtonHorizontal,0)
#SET(%CHTButtonVertical,0)
#IF(INSTRING(',LEFT',%CHTButtonStatement,1,1))
#SET(%CHTButtonHorizontal,-1)
#ELSIF(INSTRING(',RIGHT',%CHTButtonStatement,1,1))
#SET(%CHTButtonHorizontal,1)
#ENDIF
#IF(INSTRING(',ABOVE',%CHTButtonStatement,1,1))
#SET(%CHTButtonVertical,-1)
#ELSIF(INSTRING(',BELOW',%CHTButtonStatement,1,1))
#SET(%CHTButtonVertical,1)
#ENDIF
    CHModernTheme:Result = CHButton_SetContentAlignment(%Control{PROP:Handle},%CHTButtonHorizontal,%CHTButtonVertical)
#IF(%CHTButtonIcon)
    CHModernTheme:ButtonIcon:%(SUB(%Control,2,LEN(%Control)-1)) = %Control{PROP:Icon}
    CHModernTheme:Result = CHButton_SetIconFile(%Control{PROP:Handle},CHModernTheme:ButtonIcon:%(SUB(%Control,2,LEN(%Control)-1)),0,6)
#ENDIF
#ENDFOR
#FOR(%CHTWButtonExclusions)
    CHModernTheme:Result = CHButton_Detach(%CHTWButtonExclusion{PROP:Handle})
#ENDFOR
#ENDIF
    CHModernTheme:LastField = LASTFIELD()
    DISPLAY
    CHModernTheme:Result = CHTheme_ForceRedraw(0{PROP:Handle})
  END
#ENDAT

#AT(%AcceptLoopBeforeEventHandling),PRIORITY(9000)
  IF EVENT() = EVENT:Accepted AND FIELD() = ?CHThemeNotify
    IF NOT CHTheme_ConsumeFlatOptionNotify(0{PROP:Handle})
      DO CHModernThemeRefreshControls
    END
  END
#IF(%CHTWAutoLists)
#FOR(%Control),WHERE(%ControlType='LIST' AND %Control <> '' AND EXTRACT(%ControlStatement,'DROP',0) = '')
  CHModernTheme:List:%(SUB(%Control,2,LEN(%Control)-1)).TakeEvent()
#ENDFOR
#ENDIF
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
  CHModernTheme:Sheet:%(SUB(%Control,2,LEN(%Control)-1)).TakeEvent()
#ENDFOR
#ENDIF
#IF(%CHTWAutoOptions)
#FOR(%Control),WHERE(%ControlType='OPTION' AND %Control <> '')
  CHModernTheme:Option:%(SUB(%Control,2,LEN(%Control)-1)).TakeEvent()
#ENDFOR
#ENDIF
#ENDAT

#AT(%BeforeWindowClosing),PRIORITY(9000)
#IF(%CHTWAutoLists)
#FOR(%Control),WHERE(%ControlType='LIST' AND %Control <> '' AND EXTRACT(%ControlStatement,'DROP',0) = '')
  CHModernTheme:List:%(SUB(%Control,2,LEN(%Control)-1)).Kill()
#ENDFOR
#ENDIF
#IF(%CHTWAutoSheets)
#FOR(%Control),WHERE(%ControlType='SHEET' AND %Control <> '' AND INSTRING('WIZARD',UPPER(%ControlStatement),1,1) = 0)
  CHModernTheme:Sheet:%(SUB(%Control,2,LEN(%Control)-1)).Kill()
#ENDFOR
#ENDIF
#IF(%CHTWAutoOptions)
#FOR(%Control),WHERE(%ControlType='OPTION' AND %Control <> '')
  CHModernTheme:Option:%(SUB(%Control,2,LEN(%Control)-1)).Kill()
#ENDFOR
#ENDIF
  IF 0{PROP:Handle}
#IF(%CHTWAttachMenu)
    CHModernTheme:Result = CHTheme_DetachMenu(0{PROP:Handle})
#ENDIF
    CHModernTheme:Result = CHTheme_DetachWindow(0{PROP:Handle})
  END
#ENDAT

#! --------------------------------------------------------------------------
#! Repeatable control extensions. Instance labels must be unique application-
#! wide because Legacy applications place these helper objects in global data.
#! --------------------------------------------------------------------------
#! Optional advanced overrides. Normal procedures use discovery-driven
#! generation in CHModernThemeWindow and do not require these extensions.
#EXTENSION (CHModernThemeList, 'CompuHost V4 Modern Theme - LIST (advanced override)'), PROCEDURE, MULTI, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTLControlChoices),UNIQUE
  #FOR(%Control),WHERE(%ControlType = 'LIST')
    #ADD(%CHTLControlChoices,%Control)
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('LIST')
    #PROMPT('Helper instance label', @S64),%CHTLInstance,REQ
    #PROMPT('LIST control', FROM(%CHTLControlChoices)),%CHTLControl,REQ
    #PROMPT('Columns CSTRING variable/expression', EDIT(256)),%CHTLColumns,REQ
    #PROMPT('Columns value/expression (optional)', TEXT),%CHTLSetupCode,AT(10,,250,90),PROP(PROP:FontName,'Courier New'),PROP(PROP:FontSize,9)
    #PROMPT('Header height', SPIN(@N3,12,80)),%CHTLHeaderHeight,DEFAULT(18)
  #ENDTAB
#ENDSHEET

#AT(%AfterGlobalDataUser),PRIORITY(3500)
%CHTLInstance CHModernList
#ENDAT

#AT(%BeforeWindowIniResize),PRIORITY(9100)
#IF(%CHTLSetupCode)
  %CHTLColumns = %CHTLSetupCode
#ENDIF
  %CHTLInstance.Init(%CHTLControl,%CHTLColumns,%CHTLHeaderHeight)
#ENDAT

#AT(%AcceptLoopBeforeEventHandling),PRIORITY(9000)
  %CHTLInstance.TakeEvent()
#ENDAT

#AT(%BeforeWindowClosing),PRIORITY(8000)
  %CHTLInstance.Kill()
#ENDAT

#EXTENSION (CHModernThemeSlider, 'CompuHost V4 Modern Theme - Slider REGION (manual configuration)'), PROCEDURE, MULTI, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTSControlChoices),UNIQUE
  #FOR(%Control),WHERE(%ControlType = 'REGION')
    #ADD(%CHTSControlChoices,%Control)
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('Slider')
    #PROMPT('Helper instance label', @S64),%CHTSInstance,REQ
    #PROMPT('Slider REGION control', FROM(%CHTSControlChoices)),%CHTSRegion,REQ
    #PROMPT('Orientation', DROP('Horizontal|Vertical')),%CHTSOrientation,DEFAULT('Horizontal')
    #PROMPT('Minimum value/expression', EDIT(64)),%CHTSMinimum,DEFAULT('0'),REQ
    #PROMPT('Maximum value/expression', EDIT(64)),%CHTSMaximum,DEFAULT('100'),REQ
    #PROMPT('Initial value/expression', EDIT(128)),%CHTSValue,DEFAULT('0'),REQ
    #PROMPT('Step value/expression', EDIT(64)),%CHTSStep,DEFAULT('1'),REQ
    #PROMPT('Light track color', COLOR),%CHTSLightTrack,DEFAULT(00D6CABEh)
    #PROMPT('Light fill color', COLOR),%CHTSLightFill,DEFAULT(00FF8423h)
    #PROMPT('Dark track color', COLOR),%CHTSDarkTrack,DEFAULT(00352719h)
    #PROMPT('Dark fill color', COLOR),%CHTSDarkFill,DEFAULT(00FF8423h)
  #ENDTAB
#ENDSHEET

#AT(%AfterGlobalDataUser),PRIORITY(3500)
%CHTSInstance CHModernSlider
#ENDAT

#AT(%DataSectionEndWindow),PRIORITY(3500)
  BUTTON(''),AT(0,0,1,1),USE(?CHSliderNotify:%CHTSInstance),HIDE
#ENDAT

#AT(%BeforeWindowIniResize),PRIORITY(9100)
#IF(%CHTSOrientation = 'Vertical')
  %CHTSInstance.Init(%CHTSRegion,CHSLIDER_VERTICAL,%CHTSMinimum,%CHTSMaximum,%CHTSValue,?CHSliderNotify:%CHTSInstance,%CHTSStep)
#ELSE
  %CHTSInstance.Init(%CHTSRegion,CHSLIDER_HORIZONTAL,%CHTSMinimum,%CHTSMaximum,%CHTSValue,?CHSliderNotify:%CHTSInstance,%CHTSStep)
#ENDIF
  %CHTSInstance.SetColors(%CHTSLightTrack,%CHTSLightFill,%CHTSDarkTrack,%CHTSDarkFill)
#ENDAT

#AT(%AcceptLoopBeforeEventHandling),PRIORITY(9200)
  %CHTSInstance.TakeEvent()
#ENDAT

#AT(%BeforeWindowClosing),PRIORITY(8000)
  %CHTSInstance.Kill()
#ENDAT

#EXTENSION (CHModernThemeSheet, 'CompuHost V4 Modern Theme - SHEET (advanced override)'), PROCEDURE, MULTI, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTHControlChoices),UNIQUE
  #FOR(%Control),WHERE(%ControlType = 'SHEET')
    #ADD(%CHTHControlChoices,%Control)
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('SHEET')
    #PROMPT('SHEET control', FROM(%CHTHControlChoices)),%CHTHControl,REQ
    #PROMPT('Light content background', COLOR),%CHTHLightBackground,DEFAULT(00FFFFFFh)
    #PROMPT('Dark content background', COLOR),%CHTHDarkBackground,DEFAULT(00181008h)
  #ENDTAB
#ENDSHEET

#AT(%BeforeWindowIniResize),PRIORITY(9100)
  CHModernTheme:Result = CHModernTheme:Sheet:%(SUB(%CHTHControl,2,LEN(%CHTHControl)-1)).SetBackgroundColors(%CHTHLightBackground,%CHTHDarkBackground)
#ENDAT

#EXTENSION (CHModernThemeOption, 'CompuHost V4 Modern Theme - Flat OPTION (advanced override)'), PROCEDURE, MULTI, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTOControlChoices),UNIQUE
  #FOR(%Control),WHERE(%ControlType = 'OPTION')
    #ADD(%CHTOControlChoices,%Control)
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('OPTION')
    #PROMPT('Helper instance label', @S64),%CHTOInstance,REQ
    #PROMPT('OPTION control', FROM(%CHTOControlChoices)),%CHTOControl,REQ
  #ENDTAB
#ENDSHEET

#AT(%AfterGlobalDataUser),PRIORITY(3500)
%CHTOInstance CHModernOption
#ENDAT

#AT(%BeforeWindowIniResize),PRIORITY(9100)
  %CHTOInstance.Init(%CHTOControl)
#ENDAT

#AT(%AcceptLoopBeforeEventHandling),PRIORITY(9000)
  %CHTOInstance.TakeEvent()
#ENDAT

#AT(%BeforeWindowClosing),PRIORITY(8000)
  %CHTOInstance.Kill()
#ENDAT

#EXTENSION (CHModernThemeButton, 'CompuHost V4 Modern Theme - BUTTON settings'), PROCEDURE, MULTI, REQ(CHModernThemeGlobal)
#PREPARE
  #DECLARE(%CHTBControlChoices),UNIQUE
  #FOR(%Control),WHERE(%ControlType = 'BUTTON')
    #ADD(%CHTBControlChoices,%Control)
  #ENDFOR
#ENDPREPARE
#SHEET
  #TAB('BUTTON')
    #PROMPT('BUTTON control', FROM(%CHTBControlChoices)),%CHTBControl,REQ
    #PROMPT('Role', DROP('Standard|Primary|Destructive|Toolbar|Selected|Success')),%CHTBRole,DEFAULT('Standard')
    #PROMPT('Override metrics', CHECK),%CHTBMetrics,DEFAULT(0)
    #PROMPT('Corner radius', SPIN(@N3,0,100)),%CHTBCornerRadius,DEFAULT(8)
    #PROMPT('Border width', SPIN(@N2,0,20)),%CHTBBorderWidth,DEFAULT(1)
    #PROMPT('Use gradient', CHECK),%CHTBGradient,DEFAULT(1)
  #ENDTAB
#ENDSHEET

#AT(%BeforeWindowIniResize),PRIORITY(9200)
#IF(%CHTBRole = 'Primary')
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_PRIMARY)
#ELSIF(%CHTBRole = 'Destructive')
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_DESTRUCTIVE)
#ELSIF(%CHTBRole = 'Toolbar')
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_TOOLBAR)
#ELSIF(%CHTBRole = 'Selected')
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_SELECTED)
#ELSIF(%CHTBRole = 'Success')
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_SUCCESS)
#ELSE
  CHButton_SetRole(%CHTBControl{PROP:Handle},CHBUTTON_ROLE_STANDARD)
#ENDIF
#IF(%CHTBMetrics)
  CHButton_SetMetrics(%CHTBControl{PROP:Handle},%CHTBCornerRadius,%CHTBBorderWidth,%CHTBGradient)
#ENDIF
#ENDAT
