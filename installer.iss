; Ward Emergency Plan - Inno Setup Installer Script
; Requires a release build: build.bat release

#define MyAppName "Ward Emergency Plan"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Ward Emergency Plan Contributors"
#define MyAppExeName "EmergencyPlan.exe"

[Setup]
AppId={{B7E3F2A1-5C8D-4F6E-9A2B-1D3E5F7A9C0B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=installer_output
OutputBaseFilename=WardEmergencyPlan-{#MyAppVersion}-setup
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
Source: "build\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt DLLs (release builds have no 'd' suffix)
Source: "build\bin\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6Pdf.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6PrintSupport.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion

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

[Registry]
Root: HKA; Subkey: "Software\Classes\.emergencyplan"; ValueType: string; ValueName: ""; ValueData: "WardEmergencyPlan.Document"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardEmergencyPlan.Document"; ValueType: string; ValueName: ""; ValueData: "Ward Emergency Plan Document"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardEmergencyPlan.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\WardEmergencyPlan.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
