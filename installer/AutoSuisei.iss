; AutoSuisei — Inno Setup installer script
; Build with:  iscc installer\AutoSuisei.iss
;
; Sources: the release bundle assembled by scripts/make_bundle.ps1.
; Run that PS1 first so the bundle folder exists at the expected path.

#define MyAppName       "AutoSuisei"
#define MyAppVersion    "1.0.2"
#define MyAppPublisher  "hello"
#define MyAppExeName    "AutoSuisei.exe"
; เปลี่ยน path นี้ถ้า bundle อยู่ที่อื่น — make_bundle.ps1 ตั้งชื่อตาม timestamp
; ใช้ตัวที่ใหม่สุดเสมอ (resolved at compile time by ISCC)
#define BundleRoot      "C:\Users\hello\Backups\AutoSuisei"

; BundleDir = absolute path to the assembled portable folder
; ผู้เรียก ISCC ต้องส่งผ่าน /DBundleDir=<path>  (เช่น make_installer.ps1)
; ถ้าไม่ส่งมา จะ error ที่บรรทัด Source:
#ifndef BundleDir
  #define BundleDir BundleRoot + "\AutoSuisei-portable-MISSING"
#endif

[Setup]
AppId={{9F4C6E1A-AC2E-4E5A-A8F1-AUTOSUISEI2026}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#BundleRoot}
OutputBaseFilename=AutoSuisei-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupIconFile=AutoSuisei.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "*.log,*.tmp"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent

; (no InitializeSetup needed — Python + rapidocr-onnxruntime bundled
;  inside python/ subfolder, ready to use immediately after install)
