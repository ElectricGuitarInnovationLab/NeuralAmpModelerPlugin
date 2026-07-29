# Neural Amp Modeler Plug-in

[![Build](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml/badge.svg)](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml)

A VST3/AudioUnit plug-in\* for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler), built with [iPlug2](https://iplug2.github.io).

- https://www.youtube.com/user/RunawayThumbtack
- https://github.com/sdatkinson/neural-amp-modeler

## Building and Installation

To build the app or plugin, there are build scripts in [NeuralAmpModeler/scripts/](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/NeuralAmpModeler/scripts).
The [workflows](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/.github/workflows) can show you how to do this.

### Building Puke Amp VST3

The easiest option is **Actions → Build Puke Amp VST3 → Run workflow** on GitHub. When both jobs finish, download the `Puke-Amp-VST3-macOS` or `Puke-Amp-VST3-Windows` artifact. Each artifact contains a ZIP with the complete `Puke Amp.vst3` bundle, including `Models/NAM` and `Models/IR`. The Windows artifact also contains `Puke Amp-VST3-Windows-Setup.exe`.

#### Compiling the VST3 locally on macOS

Requirements:

- A Mac running a supported version of macOS.
- Xcode and its command-line tools.
- Git, including submodule support.
- A clone of this repository. A source ZIP does not contain Git submodule metadata.

Run all build commands in the macOS Terminal application. A prompt that starts
with something like `root@container:/workspaces/...` is a Linux container and
cannot access Xcode on the Mac. Confirm that the shell is running directly on
macOS:

```sh
uname -s
```

The output must be:

```text
Darwin
```

If you have not cloned the repository yet, clone it with all submodules:

```sh
git clone --recursive https://github.com/ElectricGuitarInnovationLab/NeuralAmpModelerPlugin.git
cd NeuralAmpModelerPlugin
```

If you already cloned it with Git, do not clone it again. Change to the existing
repository root and initialize any missing submodules:

```sh
cd "/path/to/NeuralAmpModelerPlugin"
git submodule update --init --recursive
```

If you originally downloaded a source ZIP, make a recursive Git clone instead.

Verify that Xcode is available:

```sh
xcodebuild -version
xcode-select -p
```

If `xcodebuild` is not found and Xcode is installed at
`/Applications/Xcode.app`, select it and complete its first-launch setup:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
sudo xcodebuild -runFirstLaunch
```

From the repository root, download the VST3 SDK and iPlug2 prebuilt libraries:

```sh
./iPlug2/Dependencies/IPlug/download-iplug-sdks.sh
./iPlug2/Dependencies/download-prebuilt-libs.sh
```

These dependency commands normally need to be run only once. Then compile,
ad-hoc sign, and package the VST3:

```sh
./NeuralAmpModeler/scripts/make-vst3-mac.sh
```

Successful builds produce:

```text
NeuralAmpModeler/build-vst3/mac/products/Puke Amp.vst3
NeuralAmpModeler/build-vst3/mac/Puke Amp-VST3-macOS.zip
```

The `.vst3` path is a plugin bundle, even though Finder displays it like a
single file. To install it for the current user:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
ditto "NeuralAmpModeler/build-vst3/mac/products/Puke Amp.vst3" \
  "$HOME/Library/Audio/Plug-Ins/VST3/Puke Amp.vst3"
```

Restart the DAW or rescan its plugins after installation.

##### Publishing a signed and notarized macOS release

For a distributable release, the VST3 and its DMG must be signed with a
Developer ID Application certificate rather than the development ad-hoc signature.

Additional requirements:

- A Developer ID Application certificate installed in the login keychain.
- The `puke-amp-notary` notarytool profile stored in the macOS keychain.
- `appdmg`, used to create the drag-install disk image.

Install `appdmg` once if it is not already available:

```sh
npm install --global appdmg
```

The checked-in `CodesigningScripts/signing.env` selects the existing notary
profile without storing the Apple ID, password, or App Store Connect key:

```sh
NOTARY_PROFILE=puke-amp-notary
```

The default signing identity is `Developer ID Application: Clear Blue Media LLC
(HSAYDGFEVC)`. Override it for a different certificate when invoking the command:

```sh
IDENTITY="Developer ID Application: Company Name (TEAMID)" \
  ./CodesigningScripts/build-and-sign.sh
```

To build, sign, notarize, staple, and package everything with the default identity:

```sh
./CodesigningScripts/build-and-sign.sh
```

- Stages a real bundle under `build-vst3/mac/release` instead of Xcode's symlink.
The command:

- Builds the release VST3.
- Replaces its ad-hoc signature with a timestamped hardened-runtime signature.
- Submits the signed VST3 to Apple using `puke-amp-notary`.
- Staples and validates the VST3 notarization ticket.
- Creates a DMG with the plugin and a link to `/Library/Audio/Plug-Ins/VST3`.
- Signs, notarizes, staples, and validates the final DMG.

Successful releases produce:

```text
NeuralAmpModeler/build-vst3/mac/release/Puke Amp.vst3
NeuralAmpModeler/build-vst3/mac/Puke Amp-VST3-macOS-0.7.15.zip
NeuralAmpModeler/build-vst3/mac/Puke Amp-VST3-macOS-0.7.15.dmg
```

The plugin ZIP and DMG use separate notarization submissions so both artifacts
can carry their own stapled ticket. See Apple's [custom notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow).
To update and rebuild later:

```sh
git pull
git submodule update --init --recursive
./NeuralAmpModeler/scripts/make-vst3-mac.sh
```

If the build reports that the VST3 SDK is missing, rerun the two dependency
download commands above. If it reports `xcodebuild was not found`, make sure the
command is running in macOS Terminal rather than a container and repeat the
`xcode-select` setup. For other Xcode failures, read the first `error:` above
`BUILD FAILED`; exit code 65 is Xcode's general build-failure code.

Generated dependencies under `Build/`, local `build-vst3/` output, and macOS
`.DS_Store` metadata are ignored by Git.

For local Windows builds, first download the iPlug2 dependencies from Git Bash:

```sh
(cd iPlug2/Dependencies/IPlug && ./download-iplug-sdks.sh)
(cd iPlug2/Dependencies && ./download-prebuilt-libs.sh)
```

Install [Inno Setup 6](https://jrsoftware.org/isinfo.php), then run this from a
Visual Studio Developer Command Prompt:

```bat
NeuralAmpModeler\scripts\make-vst3-win.bat
```

The 64-bit build, distributable ZIP, and Windows installer are written to:

```text
NeuralAmpModeler\build-vst3\windows\Puke Amp.vst3
NeuralAmpModeler\build-vst3\windows\Puke Amp-VST3-Windows.zip
NeuralAmpModeler\build-vst3\windows\Puke Amp-VST3-Windows-Setup.exe
```

Run the installer as an administrator to install the complete VST3 bundle in
`C:\Program Files\Common Files\VST3\Puke Amp.vst3`, the standard system-wide
location for a 64-bit Windows VST3. The installer also adds a normal Windows
uninstall entry. Restart the DAW or rescan its plugins after installation.

The generated installer is not digitally signed. Windows may display an
Unknown Publisher or SmartScreen warning until the release workflow is
configured with a Windows code-signing certificate.

### Pre-built installers

If you want a pre-built installer from this repo without having to , I've made "Gateway", a fork of this repo, availble at https://neuralampmodeler.com/users!

## Supported Platforms

The Neural Amp Modeler plugin currently supports Windows 10 (64bit) or later, and macOS 10.15 (Catalina) or later.

For Linux support, there is an LV2 plugin available: https://github.com/mikeoliphant/neural-amp-modeler-lv2.

## About

This is a cleaned up version of [the original iPlug2-based NAM plugin](https://github.com/sdatkinson/iPlug2) with some refactoring to adopt better practices recommended by the developers of iPlug2.
(Thanks [Oli](https://github.com/olilarkin) for your generous suggestions!)

\*could also support AAX, CLAP, Linux, iOS soon.

## Rough edges

### Standalone I/O
The I/O for the standalone doesn't inherit the stability of most plugin hosts (DAWs), so it's a bit sparser on features. For complex routing, the plugin (VST3/AU) inside a plugin host is still the most reliable option.

### Graphics backend
If you're having trouble with NAM crashing before the GUI comes up, then you might have an unsupported graphics configuration. Usually, this is when you have a dedicated graphics card (like an nVIDIA GPU) and you're using the integrated (CPU) graphics on a Windows system. To fix this, Go to the control panel, pick NAM (or your DAW), and make sure that it uses your graphics card. (If you know more and can help fix this, please make an Issue and let me know more!)
