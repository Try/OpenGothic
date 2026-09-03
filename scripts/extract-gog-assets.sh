#!/bin/sh
# Extract Gothic 2 Classic (English, no addon) game data from the GOG.com
# installer, without needing Wine. Requires innoextract:
#   macOS: brew install innoextract
#   Debian/Ubuntu: sudo apt install innoextract
#   Arch: sudo pacman -S innoextract
#
# Usage:
#   scripts/extract-gog-assets.sh <path-to-setup_gothic_2_*.exe> [destination-dir]
#
# If the installer is split into a .exe and a .bin, point this at the .exe;
# innoextract picks up the matching .bin automatically.
#
# Default destination:
#   macOS:  ~/Library/Application Support/OpenGothic  (auto-detected by OpenGothic)
#   Other:  ~/.local/share/OpenGothic

set -eu

if [ "${1:-}" = "" ]; then
  echo "usage: $0 <path-to-setup_gothic_2_*.exe> [destination-dir]" >&2
  exit 1
fi
INSTALLER="$1"

if [ "${2:-}" != "" ]; then
  DEST="$2"
elif [ "$(uname)" = "Darwin" ]; then
  DEST="$HOME/Library/Application Support/OpenGothic"
else
  DEST="$HOME/.local/share/OpenGothic"
fi

if ! command -v innoextract >/dev/null 2>&1; then
  echo "innoextract not found. Install it first (e.g. 'brew install innoextract')." >&2
  exit 1
fi

mkdir -p "$DEST"

echo "Extracting Gothic game data from '$INSTALLER' into '$DEST'..."
innoextract -e -m -g \
  -I "Data" -I "System/Gothic.INI" \
  -d "$DEST" \
  "$INSTALLER"

# innoextract preserves the installer's original file casing (e.g.
# "System/Gothic.INI"). OpenGothic looks up "system/Gothic.ini" and on a
# case-sensitive filesystem that lookup can fail, so normalize it.
sysdir=$(find "$DEST" -mindepth 1 -maxdepth 1 -iname 'system' -type d | head -n1)
if [ -n "$sysdir" ] && [ "$(basename "$sysdir")" != "system" ]; then
  mv "$sysdir" "$DEST/system"
fi

if [ -d "$DEST/system" ]; then
  ini=$(find "$DEST/system" -mindepth 1 -maxdepth 1 -iname 'gothic.ini' -type f | head -n1)
  if [ -n "$ini" ] && [ "$(basename "$ini")" != "Gothic.ini" ]; then
    mv "$ini" "$DEST/system/Gothic.ini"
  fi
fi

# innoextract also creates an empty "app/" placeholder dir - not part of the
# actual game layout.
rm -rf "$DEST/app"

echo "Done. Game data is ready at: $DEST"
echo "Run OpenGothic with: -g \"$DEST\" -g2c"
