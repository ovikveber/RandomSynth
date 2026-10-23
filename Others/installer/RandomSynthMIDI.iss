; RandomSynth MIDI installer
; Build with: Others\build-installer.ps1

#define MyAppName "RandomSynth MIDI"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "OV"
#define MyAppURL "https://github.com/ovikveber/RandomSynth"
#define DistDir "..\dist"
#define VST3Source "..\dist\RandomSynth MIDI.vst3"

[Setup]
AppId={{1C424C8D-0E27-4D8D-A510-0C538379808B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\OV\RandomSynth MIDI
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#DistDir}
OutputBaseFilename=RandomSynth-MIDI-{#MyAppVersion}-win-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=..\..\LICENSE
UninstallDisplayName={#MyAppName}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Files]
; The single VST3 plugin. MIDI channel selection happens inside the plugin.
Source: "{#VST3Source}\*"; DestDir: "{commoncf}\VST3\RandomSynth MIDI.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\VST3_MIDI_README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\RandomSynth MIDI.vst3"
