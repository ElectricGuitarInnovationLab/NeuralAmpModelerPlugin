#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef ArtifactSuffix
  #define ArtifactSuffix ""
#endif

#define PluginName "Puke Amp"
#define PluginBundle PluginName + ".vst3"
#define ArtifactBaseName "Puke-Amp-" + AppVersion + "-Windows-Setup" + ArtifactSuffix

[Setup]
AppId=org.theRATLAB.PukeAmp.VST3
AppName={#PluginName} VST3
AppVersion={#AppVersion}
VersionInfoVersion={#AppVersion}
AppPublisher=the RATLab
AppPublisherURL=https://github.com/ElectricGuitarInnovationLab/NeuralAmpModelerPlugin
AppSupportURL=https://github.com/ElectricGuitarInnovationLab/NeuralAmpModelerPlugin/issues
DefaultDirName={commonappdata}\{#PluginName} VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir=..\build-vst3\windows
OutputBaseFilename={#ArtifactBaseName}
LicenseFile=..\..\LicenseText.rtf
SetupLogging=yes
UninstallDisplayName={#PluginName} VST3

[Files]
Source: "..\build-vst3\windows\{#PluginBundle}\*"; DestDir: "{commoncf64}\VST3\{#PluginBundle}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#PluginBundle}"
