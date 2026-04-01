#!/bin/bash
set -euo pipefail

# Usage: ./scripts/package.sh <platform> <version>
PLATFORM="${1:?Usage: package.sh <mac|win> <version>}"
VERSION="${2:?Usage: package.sh <mac|win> <version>}"
ARTIFACTS="build/AudeoboxLink_artefacts/Release"
DIST="dist"
STAGING="dist/staging"

mkdir -p "$DIST"
rm -rf "$STAGING"
mkdir -p "$STAGING"

if [ "$PLATFORM" = "mac" ]; then
    ARCHIVE="Audeobox-Link-${VERSION}-mac.zip"
    MANIFEST="latest-mac.yml"

    # Stage only the plugin bundles
    cp -R "$ARTIFACTS/VST3/Audeobox Link.vst3" "$STAGING/"
    cp -R "$ARTIFACTS/AU/Audeobox Link.component" "$STAGING/"
    cp -R "$ARTIFACTS/Standalone/Audeobox Link.app" "$STAGING/"

    cd "$STAGING"
    zip -r -y "../../${DIST}/${ARCHIVE}" \
        "Audeobox Link.vst3" \
        "Audeobox Link.component" \
        "Audeobox Link.app"
    cd ../../

elif [ "$PLATFORM" = "win" ]; then
    ARCHIVE="Audeobox-Link-${VERSION}-win.zip"
    MANIFEST="latest.yml"

    # Stage only the actual plugin files (no .exp/.lib build artifacts)
    mkdir -p "$STAGING/Audeobox Link.vst3/Contents/x86_64-win"
    cp -R "$ARTIFACTS/VST3/Audeobox Link.vst3/Contents/" "$STAGING/Audeobox Link.vst3/Contents/"
    cp "$ARTIFACTS/Standalone/Audeobox Link.exe" "$STAGING/"

    cd "$STAGING"
    if command -v powershell &>/dev/null; then
        powershell -Command "Compress-Archive -Path '*' -DestinationPath '../../${DIST}/${ARCHIVE}' -Force"
    else
        zip -r "../../${DIST}/${ARCHIVE}" .
    fi
    cd ../../
else
    echo "Unknown platform: $PLATFORM"
    exit 1
fi

rm -rf "$STAGING"

# Compute SHA512 and size
FILE_PATH="${DIST}/${ARCHIVE}"
if command -v shasum &>/dev/null; then
    SHA512=$(shasum -a 512 "$FILE_PATH" | awk '{print $1}')
else
    SHA512=$(sha512sum "$FILE_PATH" | awk '{print $1}')
fi
FILE_SIZE=$(wc -c < "$FILE_PATH" | tr -d ' ')
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

echo "Packaged: ${DIST}/${ARCHIVE} (${FILE_SIZE} bytes)"
echo "Manifest: ${DIST}/${MANIFEST}"
