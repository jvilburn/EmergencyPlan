; Ward Planning - Inno Setup Installer Script
; Requires a release build: build.bat release

#define MyAppName "Ward Planning"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Ward Planning Contributors"
#define MyAppExeName "WardPlanning.exe"

[Setup]
AppId={{B7E3F2A1-5C8D-4F6E-9A2B-1D3E5F7A9C0B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
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
PrivilegesRequiredOverridesAllowed=dialog
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "fileassoc"; Description: "Associate .emergencyplan files"; GroupDescription: "File associations:"

[Files]
; License
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

; Main executable
Source: "build\windows-release\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt DLLs (release builds have no 'd' suffix)
Source: "build\windows-release\bin\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6Pdf.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6PrintSupport.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\windows-release\bin\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion

; System DLLs deployed by windeployqt
Source: "build\windows-release\bin\d3dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\windows-release\bin\dxcompiler.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\windows-release\bin\dxil.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "build\windows-release\bin\opengl32sw.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt plugins
Source: "build\windows-release\bin\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "build\windows-release\bin\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; ICU (if present in release deploy)
Source: "build\windows-release\bin\icu*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Registry]
Root: HKA; Subkey: "Software\Classes\.emergencyplan"; ValueType: string; ValueName: ""; ValueData: "WardPlanning.Document"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardPlanning.Document"; ValueType: string; ValueName: ""; ValueData: "Ward Planning Document"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardPlanning.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardPlanning.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
