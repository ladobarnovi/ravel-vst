; ============================================================================
; Ravel - Windows installer (Inno Setup 6.3+)
;
; Compiled by package.ps1, which builds Release first and then passes the
; version and paths in as preprocessor defines:
;
;     ISCC.exe /DRavelVersion=0.1.0 /DSourceRoot=<repo> /DBuildConfig=Release Ravel.iss
;
; The #ifndef fallbacks below are only so the script still opens in the Inno Setup
; IDE; package.ps1 is the supported way to build it, because it is what keeps the
; installer's version in step with CMakeLists.txt.
; ============================================================================

#ifndef RavelVersion
  #define RavelVersion "0.0.0"
#endif
#ifndef SourceRoot
  #define SourceRoot ".."
#endif
#ifndef BuildConfig
  #define BuildConfig "Release"
#endif

#define AppName      "Ravel"
#define Publisher    "Ladob"
#define ArtefactDir  SourceRoot + "\build\Ravel_artefacts\" + BuildConfig
#define Vst3Bundle   ArtefactDir + "\VST3\Ravel.vst3"
#define StandaloneEx ArtefactDir + "\Standalone\Ravel.exe"

; Fail at compile time rather than shipping an installer with nothing in it.
#if !FileExists(Vst3Bundle + "\Contents\x86_64-win\Ravel.vst3")
  #error Built VST3 not found. Run .\build.ps1 (or package.ps1, which does both) first.
#endif
#if !FileExists(StandaloneEx)
  #error Built standalone not found. Run .\build.ps1 first.
#endif

[Setup]
; AppId is what makes a second run an *upgrade* of the first rather than a
; second entry in Apps & features. It never changes, even across renames.
AppId={{98724527-9AD3-4108-8C0F-3841EF785B5B}
AppName={#AppName}
AppVersion={#RavelVersion}
AppVerName={#AppName} {#RavelVersion}
AppPublisher={#Publisher}
VersionInfoVersion={#RavelVersion}

; The plugin's destination is fixed by the VST3 spec, so {app} only ever holds
; the standalone, the setup notes and the uninstaller - not worth a wizard page.
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes

; Writing to Common Files needs elevation; asking for it up front means the UAC
; prompt lands before the wizard instead of failing halfway through the copy.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

OutputDir={#SourceRoot}\dist
OutputBaseFilename=Ravel-{#RavelVersion}-Windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName} {#RavelVersion}

[Types]
Name: "full";   Description: "VST3 plugin and standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin (Ableton Live and any other VST3 host)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app (runs Ravel without a DAW)";          Types: full

[InstallDelete]
; A VST3 on Windows is a folder, and [Files] only overwrites the files it is
; about to copy. Clearing the bundle first means a version that drops a file
; doesn't leave the old one behind inside the new install.
Type: filesandordirs; Name: "{commoncf64}\VST3\Ravel.vst3"; Components: vst3

[Files]
; recursesubdirs + createallsubdirs because the .vst3 is a bundle directory
; (Contents\x86_64-win\Ravel.vst3 plus Contents\Resources\moduleinfo.json), and
; a host only loads it if that whole structure arrives intact.
Source: "{#Vst3Bundle}\*";  DestDir: "{commoncf64}\VST3\Ravel.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#StandaloneEx}";  DestDir: "{app}";                        Flags: ignoreversion; Components: standalone
Source: "{#SourceRoot}\Ravel-Setup.txt"; DestDir: "{app}";           Flags: ignoreversion isreadme

[Icons]
Name: "{group}\Ravel";                        Filename: "{app}\Ravel.exe";       Components: standalone
Name: "{group}\Setup and MPE routing notes";  Filename: "{app}\Ravel-Setup.txt"
Name: "{group}\Uninstall Ravel";              Filename: "{uninstallexe}"

[UninstallDelete]
; Belt and braces: the bundle folder goes even if something wrote into it after
; install, which would otherwise leave an empty Ravel.vst3 that hosts still scan.
Type: filesandordirs; Name: "{commoncf64}\VST3\Ravel.vst3"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nClose your DAW first. A host that already has Ravel loaded holds the plugin file open, and Setup would have to ask you to close it anyway.

[Code]
// A machine that has been used to *develop* Ravel already has a copy in
// Documents\VST3 (see RAVEL_VST3_DIR in CMakeLists.txt). If Live is scanning
// both that folder and the system one it finds the plugin twice and lists it
// twice, so offer to clear the old one out.
//
// Setup is elevated, so {userdocs} is the profile that answered the UAC prompt.
// That is the same account in the normal case and a different one if you
// elevated as another admin - which is why the prompt prints the full path.
procedure CurStepChanged(CurStep: TSetupStep);
var
  DevCopy: String;
begin
  if CurStep <> ssPostInstall then
    Exit;

  DevCopy := ExpandConstant('{userdocs}\VST3\Ravel.vst3');

  if not DirExists(DevCopy) then
    Exit;

  if MsgBox('An older copy of Ravel is still here:' + #13#10#13#10 + DevCopy +
            #13#10#13#10 + 'A host scanning that folder as well as the system one will list Ravel twice. Remove it?',
            mbConfirmation, MB_YESNO) = IDYES then
  begin
    if not DelTree(DevCopy, True, True, True) then
      MsgBox('Could not remove it - it is probably open in a host. Close the host and delete it by hand:' +
             #13#10#13#10 + DevCopy, mbInformation, MB_OK);
  end;
end;
