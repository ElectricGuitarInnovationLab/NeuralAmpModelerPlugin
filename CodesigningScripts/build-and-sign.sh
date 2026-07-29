#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
project_dir="${repo_root}/NeuralAmpModeler"
built_plugin="${BUILT_PLUGIN:-${project_dir}/build-vst3/mac/products/Puke Amp.vst3}"
output_dir="${OUTPUT_DIR:-${project_dir}/build-vst3/mac}"
release_dir="${output_dir}/release"
staged_plugin="${release_dir}/Puke Amp.vst3"

export PLUGIN_BUNDLE="${staged_plugin}"
export OUTPUT_DIR="${output_dir}"

for command_name in xcodebuild codesign xcrun ditto appdmg python3; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "error: required command not found: ${command_name}" >&2
    exit 1
  fi
done

echo "Building Puke Amp VST3..."
"${project_dir}/scripts/make-vst3-mac.sh"

# The development builder creates an ad-hoc-signed ZIP. Avoid distributing it.
rm -f "${output_dir}/Puke Amp-VST3-macOS.zip"

if [[ ! -d "${built_plugin}" ]]; then
  echo "error: VST3 bundle was not created at ${built_plugin}" >&2
  exit 1
fi

source_plugin="$(cd "${built_plugin}" && pwd -P)"
if [[ ! -d "${source_plugin}" ]]; then
  echo "error: could not resolve the real VST3 bundle behind ${built_plugin}" >&2
  exit 1
fi

echo "Staging a real VST3 bundle from ${source_plugin}..."
mkdir -p "${release_dir}"
rm -rf "${staged_plugin}"
ditto "${source_plugin}" "${staged_plugin}"

echo "Signing, notarizing, and stapling the VST3..."
"${script_dir}/codesign-puke-amp.sh"

echo "Building, signing, notarizing, and stapling the DMG..."
"${script_dir}/rebuild-dmg.sh"

echo "Release completed successfully."
echo "Notarized VST3: ${staged_plugin}"
echo "Distribution files: ${output_dir}"
