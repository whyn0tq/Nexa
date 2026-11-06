#ifndef AppVersion
  #error AppVersion must be supplied by scripts/package.ps1
#endif
#ifndef SourceDir
  #error SourceDir must be supplied by scripts/package.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be supplied by scripts/package.ps1
#endif
#ifndef SettingsKey
  #define SettingsKey "Software\Nexa"
#endif
#ifndef PackageId
  #define PackageId "{{2ADBE1E5-9C48-4D93-B150-C3FB9254E39B}"
#endif
#ifndef ShortcutName
  #define ShortcutName "Nexa"
#endif
#ifndef LocalData
  #define LocalData "{localappdata}\Nexa"
#endif
#ifndef RoamingData
  #define RoamingData "{userappdata}\Nexa"
#endif
#ifndef EditData
  #define EditData "{%TEMP}\Nexa-edit"
#endif
#ifndef DropData
  #define DropData "{%TEMP}\Nexa-drop"
#endif

[Setup]
AppId={#PackageId}
AppName=Nexa
AppVersion={#AppVersion}
AppPublisher=teesa (whyn0tq)
AppPublisherURL=https://github.com/whyn0tq/Nexa
DefaultDirName={autopf}\Nexa
DefaultGroupName={#ShortcutName}
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Nexa-{#AppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
#if Ver >= EncodeVer(7, 0, 0)
WizardStyle=modern dark
#else
WizardStyle=modern
#endif
SetupIconFile=..\assets\nexa.ico
WizardImageFile=assets\wizard.bmp
WizardSmallImageFile=assets\header.bmp
WizardImageStretch=yes
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Nexa.exe
LicenseFile=..\LICENSE
CloseApplications=yes
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany=teesa (whyn0tq)
VersionInfoCopyright=Copyright (C) 2026 teesa (whyn0tq)
VersionInfoProductName=Nexa
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.DesktopTask=Create a desktop shortcut
russian.DesktopTask=Создать ярлык на рабочем столе
german.DesktopTask=Desktop-Verknüpfung erstellen
french.DesktopTask=Créer un raccourci sur le bureau
english.ShortcutGroup=Shortcuts:
russian.ShortcutGroup=Ярлыки:
german.ShortcutGroup=Verknüpfungen:
french.ShortcutGroup=Raccourcis :
english.LaunchApp=Open Nexa
russian.LaunchApp=Открыть Nexa
german.LaunchApp=Nexa öffnen
french.LaunchApp=Ouvrir Nexa

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopTask}"; GroupDescription: "{cm:ShortcutGroup}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKCU; Subkey: "{#SettingsKey}"; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{#LocalData}"
Type: filesandordirs; Name: "{#RoamingData}"
Type: filesandordirs; Name: "{#EditData}"
Type: filesandordirs; Name: "{#DropData}"
Type: filesandordirs; Name: "{app}"

[Icons]
Name: "{group}\{#ShortcutName}"; Filename: "{app}\Nexa.exe"
Name: "{autodesktop}\{#ShortcutName}"; Filename: "{app}\Nexa.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Nexa.exe"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent
