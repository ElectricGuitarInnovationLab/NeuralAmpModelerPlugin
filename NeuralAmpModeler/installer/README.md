# Installer Identity

Both installer build paths use the repository-root `LicenseText.rtf` for their click-through license agreement. Before publishing a pre-built installer from a private release fork, replace that file with the product's real license and override the installer identity if needed.

The Windows distribution script calls `scripts/update_installer-win.py`, which accepts these environment variable overrides:

- `INSTALLER_DISPLAY_NAME`
- `INSTALLER_APP_CONTACT`
- `INSTALLER_APP_COPYRIGHT`
- `INSTALLER_APP_PUBLISHER`
- `INSTALLER_APP_PUBLISHER_URL`
- `INSTALLER_APP_SUPPORT_URL`
- `INSTALLER_OUTPUT_BASE_FILENAME`
- `INSTALLER_WELCOME_LABEL`
- `INSTALLER_SETUP_WINDOW_TITLE`

The macOS installer package identifiers default to `com.StevenAtkinson.*`. Set `INSTALLER_PKG_ID_PREFIX` to use your own reverse-DNS prefix, for example `com.example.myproduct`.

`ThirdPartyNotices.txt` is installed with the standalone application and inside the VST3/AU bundle resources. Keep it current when adding, removing, or replacing dependencies. Private/product forks should update any source-availability language that points at this public repository.

For notarized macOS release builds, `scripts/makedist-mac.sh` also accepts:

- `NOTARIZE_BUNDLE_ID`
- `NOTARIZE_BUNDLE_ID_DEMO`
- `APP_SPECIFIC_ID`
- `APP_SPECIFIC_PWD`

These settings only affect installer/package identity. Plug-in identity, DAW compatibility, and saved-session compatibility are controlled elsewhere, including `config.h`, Xcode bundle identifiers, and plug-in format metadata.

## Tag-driven GitHub releases

Pushing a tag such as `v0.2.0` runs `.github/workflows/release-native.yml`. The tag must exactly match `PLUG_VERSION_STR` in `config.h`. After both platform builds pass, the workflow creates a draft GitHub release containing the installers and `SHA256SUMS.txt`. Review and publish the draft in GitHub to make it visible to update checks.

To test macOS signing without creating a tag or release, open **Actions → Release Puke Amp → Run workflow**. Leave **Also build the Windows installer** unchecked. The workflow builds, signs, notarizes, and staples the macOS package, then makes it available as the `release-macos` workflow artifact.

Configure these GitHub Actions secrets before pushing a release tag:

- `MACOS_CERTIFICATE_P12_BASE64`: Base64-encoded PKCS#12 containing the Developer ID Application and Developer ID Installer certificates and private keys.
- `MACOS_CERTIFICATE_P12_PASSWORD`: Password for that PKCS#12 file.
- `MACOS_KEYCHAIN_PASSWORD`: Ephemeral CI keychain password.
- `MACOS_APPLICATION_SIGNING_IDENTITY`: Full Developer ID Application identity.
- `MACOS_INSTALLER_SIGNING_IDENTITY`: Full Developer ID Installer identity.
- `APPLE_NOTARY_KEY_BASE64`: Base64-encoded App Store Connect API `.p8` key.
- `APPLE_NOTARY_KEY_ID`: App Store Connect API key ID.
- `APPLE_NOTARY_ISSUER_ID`: App Store Connect API issuer ID.
- `WINDOWS_CERTIFICATE_PFX_BASE64` (optional): Base64-encoded Windows code-signing PFX.
- `WINDOWS_CERTIFICATE_PASSWORD` (optional): Password for the Windows PFX.

The optional Actions variable `MACOS_PACKAGE_ID` overrides the default package identifier `org.theRATLAB.PukeAmp.VST3`.

If both Windows secrets are absent, the workflow still builds Windows artifacts but names them with an `-Unsigned` suffix. The draft release notes warn that Windows will show an unknown publisher and may display a SmartScreen warning.
