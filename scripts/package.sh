#!/bin/bash
set -euo pipefail

# Usage: ./scripts/package.sh <platform> <version>
PLATFORM="${1:?Usage: package.sh <mac|win> <version>}"
VERSION="${2:?Usage: package.sh <mac|win> <version>}"
ARTIFACTS="build/AudeoboxLink_artefacts/Release"
DIST="dist"

mkdir -p "$DIST"

if [ "$PLATFORM" = "mac" ]; then
    PKG_NAME="Audeobox-Link-${VERSION}-mac.pkg"
    MANIFEST="latest-mac.yml"

    # Build .pkg installer that copies plugins to system folders
    # VST3 → /Library/Audio/Plug-Ins/VST3/
    # AU   → /Library/Audio/Plug-Ins/Components/

    # Pre-install scripts remove old bundles so reinstalls always overwrite
    mkdir -p "$DIST/vst3-scripts"
    cat > "$DIST/vst3-scripts/preinstall" << 'SCRIPT'
#!/bin/bash
rm -rf "/Library/Audio/Plug-Ins/VST3/Audeobox Link.vst3"
exit 0
SCRIPT
    chmod +x "$DIST/vst3-scripts/preinstall"

    mkdir -p "$DIST/au-scripts"
    cat > "$DIST/au-scripts/preinstall" << 'SCRIPT'
#!/bin/bash
rm -rf "/Library/Audio/Plug-Ins/Components/Audeobox Link.component"
exit 0
SCRIPT
    chmod +x "$DIST/au-scripts/preinstall"

    pkgbuild \
        --root "$ARTIFACTS/VST3/Audeobox Link.vst3" \
        --identifier "com.audeobox.link.vst3" \
        --version "$VERSION" \
        --install-location "/Library/Audio/Plug-Ins/VST3/Audeobox Link.vst3" \
        --scripts "$DIST/vst3-scripts" \
        "$DIST/vst3.pkg"

    pkgbuild \
        --root "$ARTIFACTS/AU/Audeobox Link.component" \
        --identifier "com.audeobox.link.au" \
        --version "$VERSION" \
        --install-location "/Library/Audio/Plug-Ins/Components/Audeobox Link.component" \
        --scripts "$DIST/au-scripts" \
        "$DIST/au.pkg"

    # Combine into a single product installer
    cat > "$DIST/distribution.xml" << DISTXML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Audeobox Link ${VERSION}</title>
    <welcome file="welcome.txt" />
    <options customize="allow" require-scripts="false" />
    <choices-outline>
        <line choice="vst3" />
        <line choice="au" />
    </choices-outline>
    <choice id="vst3" title="VST3 Plugin" description="Install Audeobox Link VST3 for all DAWs">
        <pkg-ref id="com.audeobox.link.vst3" />
    </choice>
    <choice id="au" title="Audio Unit Plugin" description="Install Audeobox Link AU for Logic, GarageBand, etc.">
        <pkg-ref id="com.audeobox.link.au" />
    </choice>
    <pkg-ref id="com.audeobox.link.vst3" version="${VERSION}">vst3.pkg</pkg-ref>
    <pkg-ref id="com.audeobox.link.au" version="${VERSION}">au.pkg</pkg-ref>
</installer-gui-script>
DISTXML

    cat > "$DIST/welcome.txt" << WELCOME
Audeobox Link v${VERSION}

This installer will add the Audeobox Link plugin to your system:

  • VST3 → /Library/Audio/Plug-Ins/VST3/
  • Audio Unit → /Library/Audio/Plug-Ins/Components/

After installation, restart your DAW and scan for new plugins.

Requires Audeobox Desktop to be running.
WELCOME

    productbuild \
        --distribution "$DIST/distribution.xml" \
        --resources "$DIST" \
        --package-path "$DIST" \
        "$DIST/$PKG_NAME"

    # Clean up intermediate files
    rm -f "$DIST/vst3.pkg" "$DIST/au.pkg" "$DIST/distribution.xml" "$DIST/welcome.txt"
    rm -rf "$DIST/vst3-scripts" "$DIST/au-scripts"

    ARCHIVE="$PKG_NAME"

elif [ "$PLATFORM" = "win" ]; then
    # Inno Setup creates the .exe installer in CI
    # This script just generates the .iss config
    ARCHIVE="Audeobox-Link-${VERSION}-win-setup.exe"
    MANIFEST="latest.yml"

    cat > "$DIST/installer.iss" << ISSEOF
[Setup]
AppName=Audeobox Link
AppVersion=${VERSION}
AppPublisher=Audeobox
AppId={{E7A2F1B3-4C5D-6E7F-8A9B-0C1D2E3F4A5B}
DefaultDirName={commoncf}\VST3\Audeobox Link.vst3
OutputDir=.
OutputBaseFilename=Audeobox-Link-${VERSION}-win-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
DisableDirPage=yes
UninstallDisplayName=Audeobox Link VST3

[InstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Audeobox Link.vst3"

[Files]
Source: "..\build\AudeoboxLink_artefacts\Release\VST3\Audeobox Link.vst3\*"; DestDir: "{commoncf}\VST3\Audeobox Link.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
WelcomeLabel2=This will install Audeobox Link v${VERSION} VST3 plugin.%n%nThe plugin will be installed to:%n  C:\Program Files\Common Files\VST3\%n%nRequires Audeobox Desktop to be running.
ISSEOF

    echo "Inno Setup script generated at $DIST/installer.iss"
    echo "Run 'iscc $DIST/installer.iss' to build the installer"
else
    echo "Unknown platform: $PLATFORM"
    exit 1
fi

# Compute SHA512 and size
FILE_PATH="${DIST}/${ARCHIVE}"
if [ -f "$FILE_PATH" ]; then
    if command -v shasum &>/dev/null; then
        SHA512=$(shasum -a 512 "$FILE_PATH" | awk '{print $1}')
    else
        SHA512=$(sha512sum "$FILE_PATH" | awk '{print $1}')
    fi
    FILE_SIZE=$(wc -c < "$FILE_PATH" | tr -d ' ')
else
    SHA512="pending"
    FILE_SIZE="0"
fi
RELEASE_DATE=$(date -u +"%Y-%m-%dT%H:%M:%S.000Z")

cat > "${DIST}/${MANIFEST}" << EOF
version: ${VERSION}
files:
  - url: ${ARCHIVE}
    sha512: ${SHA512}
    size: ${FILE_SIZE}
path: ${ARCHIVE}
sha512: ${SHA512}
releaseDate: '${RELEASE_DATE}'
EOF

echo "Packaged: ${DIST}/${ARCHIVE}"
echo "Manifest: ${DIST}/${MANIFEST}"
