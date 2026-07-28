#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
build_root="${project_dir}/build-vst3/mac"
products_dir="${build_root}/products"
plugin_path="${products_dir}/Puke Amp.vst3"
archive_path="${build_root}/Puke Amp-VST3-macOS.zip"

if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "error: xcodebuild was not found. Install Xcode and its command-line tools." >&2
  exit 1
fi

if [[ ! -d "${project_dir}/../iPlug2/Dependencies/IPlug/VST3_SDK" ]]; then
  echo "error: the VST3 SDK is missing." >&2
  echo "Run iPlug2/Dependencies/IPlug/download-iplug-sdks.sh first." >&2
  exit 1
fi

mkdir -p "${products_dir}"

xcodebuild \
  -project "${project_dir}/projects/NeuralAmpModeler-macOS.xcodeproj" \
  -xcconfig "${project_dir}/config/NeuralAmpModeler-mac.xcconfig" \
  -target VST3 \
  -configuration Release \
  "SYMROOT=${build_root}/intermediates" \
  "CONFIGURATION_BUILD_DIR=${products_dir}" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  clean build

if [[ ! -d "${plugin_path}" ]]; then
  echo "error: build succeeded but ${plugin_path} was not created." >&2
  exit 1
fi

# Ad-hoc signing makes local development builds loadable without a paid certificate.
codesign --force --deep --sign - "${plugin_path}"
ditto -c -k --keepParent "${plugin_path}" "${archive_path}"

echo "Built ${plugin_path}"
echo "Packaged ${archive_path}"
