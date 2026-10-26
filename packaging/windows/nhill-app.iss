; Inno Setup 6 script: one installer per Nhill application.
; Called by scripts/build-windows.ps1:
;   ISCC /DAppId=nhill-pos /DSourceDir=<dist\nhill-pos> /DOutputDir=<dist> packaging\windows\nhill-app.iss
; NOT EXECUTED during development (no Windows machine available): build and test it on
; Windows 10/11 before distribution, then sign the installer (signtool).
; The installer contains only the client: no server address secret, no database
; credential, no API key. The server address is chosen at first launch.

#ifndef AppId
  #error Define AppId (nhill-business, nhill-console or nhill-pos)
#endif
#ifndef SourceDir
  #error Define SourceDir (folder prepared by windeployqt)
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#define AppVersion "0.1.0"

#if AppId == "nhill-business"
  #define AppName "Nhill Business"
  #define AppGuid "{{6C7B1E0A-3F5D-4B8E-9C2A-1D4E5F6A7B01}"
#elif AppId == "nhill-console"
  #define AppName "Nhill Console"
  #define AppGuid "{{6C7B1E0A-3F5D-4B8E-9C2A-1D4E5F6A7B02}"
#elif AppId == "nhill-pos"
  #define AppName "Nhill POS"
  #define AppGuid "{{6C7B1E0A-3F5D-4B8E-9C2A-1D4E5F6A7B03}"
#else
  #error Unknown AppId
#endif

[Setup]
AppId={#AppGuid}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Nhill
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#AppId}-{#AppVersion}-setup
SetupIconFile=..\..\assets\icons\{#AppId}.ico
UninstallDisplayIcon={app}\{#AppId}.exe
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppId}.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppId}.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppId}.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
