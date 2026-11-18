; Ward Planning - Inno Setup Installer Script
; Requires a release build: build.bat release

; MyAppVersion, which the build writes from the project's version
#include "build\version.iss"

#define MyAppName "Ward Planning"
#define MyAppId "B7E3F2A1-5C8D-4F6E-9A2B-1D3E5F7A9C0B"
#define MyAppPublisher "Ward Planning Contributors"
#define MyAppExeName "WardPlanning.exe"

[Setup]
AppId={{{#MyAppId}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UsePreviousGroup=no
DisableProgramGroupPage=yes
OutputDir=installer_output
OutputBaseFilename=WardPlanning-{#MyAppVersion}-setup
SetupIconFile=src\resources\app_icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
LicenseFile=LICENSE.txt
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; License
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

; Main executable
Source: "build\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Every Qt DLL windeployqt put beside the executable. The build folder must hold a release
; build alone: a debug build's DLLs, left from before, would be packed too.
Source: "build\bin\Qt6*.dll"; DestDir: "{app}"; Flags: ignoreversion

; System DLLs deployed by windeployqt
Source: "build\bin\d3dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\bin\dxcompiler.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\bin\dxil.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\bin\opengl32sw.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt plugins
Source: "build\bin\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\bin\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; ICU (if present in release deploy)
Source: "build\bin\icu*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; An upgrade installs into the earlier release's folder, and takes out its executable, Start
; Menu folder and desktop shortcut, which carry its name, and its tile cache
[InstallDelete]
Type: files; Name: "{app}\EmergencyPlan.exe"
Type: filesandordirs; Name: "{autoprograms}\Ward Emergency Plan"
Type: files; Name: "{autodesktop}\Ward Emergency Plan.lnk"
Type: filesandordirs; Name: "{localappdata}\Emergency Plan\cache"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

; An update file opens in Ward Planning, which takes it into its ward; an invitation opens in
; Ward Planning, which joins its ward
[Registry]
Root: HKA; Subkey: "Software\Classes\.wardupdate"; ValueType: string; ValueName: ""; ValueData: "WardPlanning.Update"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\WardPlanning.Update"; ValueType: string; ValueName: ""; ValueData: "Ward Planning update"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\WardPlanning.Update\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKA; Subkey: "Software\Classes\WardPlanning.Update\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""
Root: HKA; Subkey: "Software\Classes\.wardinvite"; ValueType: string; ValueName: ""; ValueData: "WardPlanning.Invitation"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\WardPlanning.Invitation"; ValueType: string; ValueName: ""; ValueData: "Ward Planning invitation"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\WardPlanning.Invitation\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKA; Subkey: "Software\Classes\WardPlanning.Invitation\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
#include "packaging\windows\invitation.iss"

// Where this same version of Ward Planning is installed, read from the uninstall entry this
// installer writes; empty when it is not installed, or another version is.
function InstalledSameVersion(): String;
var
  Key, Version, Location: String;
begin
  Result := '';
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#MyAppId}}_is1';
  if RegQueryStringValue(HKCU, Key, 'DisplayVersion', Version)
     and RegQueryStringValue(HKCU, Key, 'InstallLocation', Location)
     and (Version = '{#MyAppVersion}') and FileExists(AddBackslash(Location) + '{#MyAppExeName}') then
    Result := AddBackslash(Location) + '{#MyAppExeName}';
end;

// The invitation the installer's file name carries is left in Ward Planning's app data, where
// the app finds it each time it starts until it has asked about it (AppData::invitationFilePath).
// So installing from an invitation link leads to joining whenever the app first starts. Where
// this version is already installed, the app is started and nothing is installed again.
function InitializeSetup(): Boolean;
var
  Folder, Installed: String;
  ResultCode: Integer;
begin
  Result := True;
  if InvitationCode() = '' then
    Exit;
  Folder := ExpandConstant('{localappdata}\{#MyAppName}');
  ForceDirectories(Folder);
  SaveStringToFile(Folder + '\invitation', InvitationCode(), False);
  Installed := InstalledSameVersion();
  if Installed <> '' then
  begin
    Exec(Installed, '', '', SW_SHOWNORMAL, ewNoWait, ResultCode);
    Result := False;
  end;
end;

// An upgrade removes the .emergencyplan association an older release registered;
// ChangesAssociations tells the shell it is gone.
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
  begin
    RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\.emergencyplan');
    RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\WardEmergencyPlan.Document');
  end;
end;
