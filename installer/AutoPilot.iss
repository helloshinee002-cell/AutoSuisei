; AutoPilot — Inno Setup installer script
; Build with:  iscc installer\AutoPilot.iss
;
; Sources: the release bundle assembled by scripts/make_bundle.ps1.
; Run that PS1 first so the bundle folder exists at the expected path.

#define MyAppName       "AutoPilot"
#define MyAppVersion    "0.9.0"
#define MyAppPublisher  "hello"
#define MyAppExeName    "AutoPilot.exe"
; เปลี่ยน path นี้ถ้า bundle อยู่ที่อื่น — make_bundle.ps1 ตั้งชื่อตาม timestamp
; ใช้ตัวที่ใหม่สุดเสมอ (resolved at compile time by ISCC)
#define BundleRoot      "C:\Users\hello\Backups\AutoPilot"

; BundleDir = absolute path to the assembled portable folder
; ผู้เรียก ISCC ต้องส่งผ่าน /DBundleDir=<path>  (เช่น make_installer.ps1)
; ถ้าไม่ส่งมา จะ error ที่บรรทัด Source:
#ifndef BundleDir
  #define BundleDir BundleRoot + "\AutoPilot-portable-MISSING"
#endif

[Setup]
AppId={{9F4C6E1A-AC2E-4E5A-A8F1-AUTOPILOT2026}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#BundleRoot}
OutputBaseFilename=AutoPilot-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
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

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  // Soft warning: Python 3.10+ + rapidocr-onnxruntime ต้องลงเอง
  // ไม่ block install — แค่บอกผู้ใช้
  if MsgBox(
       'AutoPilot ต้องการ Python 3.10+ และ rapidocr-onnxruntime สำหรับการประมวลผล OCR' + #13#10 +
       'ถ้ายังไม่ได้ติดตั้ง ให้ทำหลังจาก install เสร็จ:' + #13#10 + #13#10 +
       '  1. ติดตั้ง Python จาก https://www.python.org/downloads/' + #13#10 +
       '  2. เปิด PowerShell แล้วรัน:' + #13#10 +
       '     pip install rapidocr-onnxruntime onnxruntime' + #13#10 + #13#10 +
       'ดำเนินการต่อ?',
       mbConfirmation, MB_YESNO) = IDNO then
    Result := False;
end;
