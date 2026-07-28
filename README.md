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

For local builds, first download the iPlug2 dependencies from a shell (use Git Bash on Windows):

```sh
(cd iPlug2/Dependencies/IPlug && ./download-iplug-sdks.sh)
(cd iPlug2/Dependencies && ./download-prebuilt-libs.sh)
```

On macOS with Xcode installed:

```sh
NeuralAmpModeler/scripts/make-vst3-mac.sh
```

The development build is ad-hoc signed and written to `NeuralAmpModeler/build-vst3/mac/products/Puke Amp.vst3`, with a distributable ZIP beside the `products` directory.

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
