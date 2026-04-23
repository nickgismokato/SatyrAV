!include "MUI2.nsh"

Name "SatyrAV"
OutFile "SatyrAV-Installer.exe"
InstallDir "$PROGRAMFILES\SatyrAV"
RequestExecutionLevel admin

!define MUI_ICON "..\assets\satyr-av-icon.ico"
!define MUI_UNICON "..\assets\satyr-av-icon.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath $INSTDIR

    ; Main executable
    File "..\build-win\satyr-av.exe"

    ; Assets
    SetOutPath $INSTDIR\assets
    File /r "..\assets\*.*"
    SetOutPath $INSTDIR

    ; All DLLs from build directory (SDL2, FFmpeg, MinGW runtime)
    File "..\build-win\*.dll"

    ; Create shortcuts with icon
    CreateShortcut "$DESKTOP\SatyrAV.lnk" "$INSTDIR\satyr-av.exe" "" "$INSTDIR\assets\satyr-av-icon.ico"
    CreateDirectory "$SMPROGRAMS\SatyrAV"
    CreateShortcut "$SMPROGRAMS\SatyrAV\SatyrAV.lnk" "$INSTDIR\satyr-av.exe" "" "$INSTDIR\assets\satyr-av-icon.ico"
    CreateShortcut "$SMPROGRAMS\SatyrAV\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry for Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SatyrAV" \
        "DisplayName" "SatyrAV"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SatyrAV" \
        "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SatyrAV" \
        "DisplayIcon" "$INSTDIR\satyr-av.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SatyrAV" \
        "Publisher" "SaTyR"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\satyr-av.exe"
    Delete "$INSTDIR\*.dll"
    RMDir /r "$INSTDIR\assets"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    Delete "$DESKTOP\SatyrAV.lnk"
    RMDir /r "$SMPROGRAMS\SatyrAV"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SatyrAV"
SectionEnd
