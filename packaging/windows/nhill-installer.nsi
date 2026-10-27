; Installateur Windows d'une application Nhill (NSIS 3, Unicode).
; Compilable sous Windows (NSIS 3.08+) ou sous Linux (paquet « nsis » : makensis) :
;   makensis -DAPP_ID=nhill-pos -DSOURCE_DIR=C:\build\dist\nhill-pos -DOUTPUT_DIR=C:\build\dist nhill-installer.nsi
; SOURCE_DIR = dossier préparé par windeployqt (exécutable + DLL Qt + greffons + modules QML).
; Aucune donnée secrète n'est embarquée : l'adresse du serveur est choisie au premier lancement.

Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!ifndef APP_ID
  !error "Définir APP_ID (nhill-business, nhill-console ou nhill-pos)"
!endif
!ifndef SOURCE_DIR
  !error "Définir SOURCE_DIR (dossier préparé par windeployqt)"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif
!ifndef APP_VERSION
  !define APP_VERSION "2.1.0"   ; normally passed by the build scripts (/DAPP_VERSION=x.y.z from CMakeLists.txt)
!endif

!if "${APP_ID}" == "nhill-business"
  !define APP_NAME "Nhill Business"
!else if "${APP_ID}" == "nhill-console"
  !define APP_NAME "Nhill Console"
!else if "${APP_ID}" == "nhill-pos"
  !define APP_NAME "Nhill POS"
!else
  !error "APP_ID inconnu : ${APP_ID}"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_ID}"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${OUTPUT_DIR}\${APP_ID}-${APP_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
BrandingText "${APP_NAME}"

!ifndef ICON_FILE
  !define ICON_FILE "${SOURCE_DIR}\${APP_ID}.ico"
!endif
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_ID}.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=${LANG_FRENCH} "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=${LANG_FRENCH} "FileDescription" "Installateur ${APP_NAME}"
VIAddVersionKey /LANG=${LANG_FRENCH} "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_FRENCH} "LegalCopyright" "Nhill"

; ---------------------------------------------------------------- prérequis
Function .onInit
  ; Windows 10 (build 10240) ou plus récent, 64 bits : exigences de Qt 6 et de l'UCRT.
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "${APP_NAME} exige Windows 10 ou Windows 11. Installation annulée."
    Abort
  ${EndIf}
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "${APP_NAME} exige une version 64 bits de Windows. Installation annulée."
    Abort
  ${EndIf}
  SetRegView 64
  ; Direct3D : d3dcompiler_47.dll fait partie de Windows 10/11 ; son absence indique un
  ; système incomplet (Windows N sans Media Feature Pack, image réduite…).
  ${IfNot} ${FileExists} "$SYSDIR\d3dcompiler_47.dll"
    MessageBox MB_YESNO|MB_ICONEXCLAMATION "d3dcompiler_47.dll est absent de Windows ($SYSDIR). L'affichage pourrait échouer : installez les mises à jour de Windows. Continuer quand même ?" IDYES +2
    Abort
  ${EndIf}
  ; Instance en cours d'exécution
  FindWindow $0 "" "${APP_NAME}"
  ${If} $0 <> 0
    MessageBox MB_OK|MB_ICONEXCLAMATION "Fermez ${APP_NAME} avant de continuer l'installation."
    Abort
  ${EndIf}
  ; Mise à jour d'une version déjà installée : désinstallation silencieuse préalable
  ReadRegStr $1 HKLM "${UNINST_KEY}" "UninstallString"
  ${If} $1 != ""
    ReadRegStr $2 HKLM "${UNINST_KEY}" "InstallLocation"
    ExecWait '"$1" /S _?=$2' $3
    ${If} $3 <> 0
      MessageBox MB_OK|MB_ICONSTOP "La version déjà installée n'a pas pu être désinstallée (code $3). Désinstallez-la depuis Paramètres > Applications puis relancez l'installateur."
      Abort
    ${EndIf}
  ${EndIf}
FunctionEnd

; ---------------------------------------------------------------- installation
Section "Installation" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  ClearErrors
  File /r "${SOURCE_DIR}\*.*"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "Copie des fichiers impossible dans $INSTDIR (espace disque ou droits insuffisants)."
    Abort
  ${EndIf}
  ${IfNot} ${FileExists} "$INSTDIR\${APP_ID}.exe"
    MessageBox MB_OK|MB_ICONSTOP "Installation incomplète : ${APP_ID}.exe manquant."
    Abort
  ${EndIf}
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_ID}.exe"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Désinstaller ${APP_NAME}.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_ID}.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "Nhill"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_ID}.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" $0
SectionEnd

; ---------------------------------------------------------------- désinstallation
Section "Uninstall"
  SetRegView 64
  Delete "$DESKTOP\${APP_NAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APP_NAME}"
  ; Seul le dossier d'installation est supprimé ; les préférences de l'utilisateur
  ; (serveur, langue) et la session du Gestionnaire d'identification sont conservées.
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
