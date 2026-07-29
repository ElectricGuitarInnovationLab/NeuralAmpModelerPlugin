# Neural Amp Modeler Plug-in

[![Build](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml/badge.svg)](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml)

A VST3/AudioUnit plug-in\* for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler), built with [iPlug2](https://iplug2.github.io).

- https://www.youtube.com/user/RunawayThumbtack
- https://github.com/sdatkinson/neural-amp-modeler

## Building and Installation

To build the app or plugin, there are build scripts in [NeuralAmpModeler/scripts/](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/NeuralAmpModeler/scripts).
The [workflows](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/.github/workflows) can show you how to do this.

### Building Puke Amp VST3

The easiest option is **Actions → Build Puke Amp VST3 → Run workflow** on GitHub. When both jobs finish, download the `Puke-Amp-VST3-macOS` or `Puke-Amp-VST3-Windows` artifact. Each artifact contains a ZIP with the complete `Puke Amp.vst3` bundle, including `Models/NAM` and `Models/IR`.

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

On Windows, run this from a Visual Studio Developer Command Prompt:

```bat
NeuralAmpModeler\scripts\make-vst3-win.bat
```

The 64-bit build and its distributable ZIP are written to `NeuralAmpModeler\build-vst3\windows`.

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
