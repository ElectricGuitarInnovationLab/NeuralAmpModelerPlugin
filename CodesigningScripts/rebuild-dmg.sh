#!/usr/bin/env bash

# Install appdmg globally with: npm install -g appdmg
# Make executable with: chmod +x rebuild-dmg.sh
# Run with: ./rebuild-dmg.sh

set -Eeuo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
project_dir="${repo_root}/NeuralAmpModeler"
PRODUCT_NAME="${PRODUCT_NAME:-Puke Amp}"
OUTPUT_DIR="${OUTPUT_DIR:-${project_dir}/build-vst3/mac}"
PLUGIN_BUNDLE="${PLUGIN_BUNDLE:-${OUTPUT_DIR}/products/${PRODUCT_NAME}.vst3}"
CONFIG="${CONFIG:-${script_dir}/appdmg-config.json}"

TEAM_ID="${TEAM_ID:-HSAYDGFEVC}"
IDENTITY="${IDENTITY:-Developer ID Application: Clear Blue Media LLC (${TEAM_ID})}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"

if [[ -f "${script_dir}/signing.env" ]]; then
  # shellcheck disable=SC1091
  source "${script_dir}/signing.env"
fi

for command_name in appdmg codesign hdiutil python3 xcrun; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "error: required command not found: ${command_name}" >&2
    exit 1
  fi
done

if [[ -z "${NOTARY_PROFILE:-}" ]]; then
  echo "error: NOTARY_PROFILE is not set in ${script_dir}/signing.env or the environment." >&2
  exit 1
fi

if [[ ! -d "${PLUGIN_BUNDLE}" ]]; then
  echo "error: VST3 bundle not found at ${PLUGIN_BUNDLE}" >&2
  exit 1
fi

if [[ ! -f "${CONFIG}" ]]; then
  echo "error: DMG layout config not found: ${CONFIG}" >&2
  exit 1
fi

VERSION="unknown"
if [[ -f "${PLUGIN_BUNDLE}/Contents/Info.plist" ]]; then
  VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${PLUGIN_BUNDLE}/Contents/Info.plist" 2>/dev/null || echo "unknown")
fi

mkdir -p "${OUTPUT_DIR}"

DMG_NAME="${PRODUCT_NAME}-VST3-macOS-${VERSION}.dmg"
DMG_PATH="${OUTPUT_DIR}/${DMG_NAME}"

echo "🧼 Removing any previous DMG..."
rm -f "${DMG_PATH}"

echo "📀 Rebuilding DMG using appdmg..."
TMP_BASE="$(mktemp -t appdmg-config.XXXXXX)"
TMP_CONFIG="${TMP_BASE}.json"
mv "${TMP_BASE}" "${TMP_CONFIG}"
python3 - <<PY
import json
import os

config_path = "${CONFIG}"
plugin_path = os.path.abspath("${PLUGIN_BUNDLE}")

with open(config_path, "r", encoding="utf-8") as f:
    data = json.load(f)

for key in ("icon", "background"):
    if key in data and isinstance(data[key], str):
        value = data[key]
        if not os.path.isabs(value):
            data[key] = os.path.join(os.path.dirname(config_path), value)

if data.get("contents") and isinstance(data["contents"], list):
    for entry in data["contents"]:
        if entry.get("type") == "file":
            entry["path"] = plugin_path

with open("${TMP_CONFIG}", "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2)
PY

appdmg "${TMP_CONFIG}" "${DMG_PATH}"

echo "🧾 Verifying DMG was created..."
if [[ ! -f "${DMG_PATH}" ]]; then
  echo "error: DMG not found: ${DMG_PATH}" >&2
  exit 1
fi

echo "🔏 Signing DMG..."
codesign --force --timestamp --sign "${IDENTITY}" "${DMG_PATH}"
codesign --verify --verbose=2 "${DMG_PATH}"
hdiutil verify "${DMG_PATH}"

echo "☁️ Submitting to Apple for notarization..."
xcrun notarytool submit "${DMG_PATH}" \
  --keychain-profile "${NOTARY_PROFILE}" \
  --wait

echo "📎 Stapling ticket to DMG..."
xcrun stapler staple -v "${DMG_PATH}"
xcrun stapler validate -v "${DMG_PATH}"

rm -f "${TMP_CONFIG}"

echo "✅ DMG rebuilt, signed, notarized, and stapled: ${DMG_PATH}"
