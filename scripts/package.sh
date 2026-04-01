#!/bin/bash
set -euo pipefail

# Usage: ./scripts/package.sh <platform> <version>
# platform: mac or win
# version: e.g. 0.1.0

PLATFORM="${1:?Usage: package.sh <mac|win> <version>}"
VERSION="${2:?Usage: package.sh <mac|win> <version>}"
ARTIFACTS="build/AudeoboxLink_artefacts/Release"
DIST="dist"

mkdir -p "$DIST"

if [ "$PLATFORM" = "mac" ]; then
    ARCHIVE="Audeobox-Link-${VERSION}-mac.zip"
    MANIFEST="latest-mac.yml"

    # Create zip with VST3 + AU + Standalone
    cd "$ARTIFACTS"
    zip -r -y "../../../${DIST}/${ARCHIVE}" \
        "VST3/Audeobox Link.vst3" \
        "AU/Audeobox Link.component" \
        "Standalone/Audeobox Link.app"
    cd ../../../

elif [ "$PLATFORM" = "win" ]; then
    ARCHIVE="Audeobox-Link-${VERSION}-win.zip"
    MANIFEST="latest.yml"

    # Create zip with VST3 + Standalone
    cd "$ARTIFACTS"
    # Use PowerShell on Windows for zip (this script runs in Git Bash on Windows CI)
    if command -v powershell &>/dev/null; then
        powershell -Command "Compress-Archive -Path 'VST3','Standalone' -DestinationPath '../../../${DIST}/${ARCHIVE}' -Force"
    else
        zip -r "../../../${DIST}/${ARCHIVE}" \
            "VST3/Audeobox Link.vst3" \
            "Standalone/Audeobox Link.exe" 2>/dev/null || \
        zip -r "../../../${DIST}/${ARCHIVE}" "VST3" "Standalone"
    fi
    cd ../../../
else
    echo "Unknown platform: $PLATFORM"
    exit 1
fi

# Compute SHA512 and size
FILE_PATH="${DIST}/${ARCHIVE}"
if [ "$PLATFORM" = "mac" ]; then
    SHA512=$(shasum -a 512 "$FILE_PATH" | awk '{print $1}')
else
    SHA512=$(sha512sum "$FILE_PATH" 2>/dev/null | awk '{print $1}' || shasum -a 512 "$FILE_PATH" | awk '{print $1}')
fi
FILE_SIZE=$(wc -c < "$FILE_PATH" | tr -d ' ')
RELEASE_DATE=$(date -u +"%Y-%m-%dT%H:%M:%S.000Z")

# Generate manifest (electron-builder compatible format)
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

echo "Packaged: ${DIST}/${ARCHIVE} (${FILE_SIZE} bytes)"
echo "Manifest: ${DIST}/${MANIFEST}"
