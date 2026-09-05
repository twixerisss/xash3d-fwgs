#!/bin/sh
# Install the built engine onto a real SD card for the Homebrew Channel.
#
#   ./install_wii.sh /Volumes/WIISD [path/to/valve]
#
# Produces:
#   <sd>/apps/xash3d/boot.dol   the engine
#   <sd>/apps/xash3d/meta.xml   so the Homebrew Channel lists it properly
#   <sd>/xash3d/valve/          game data (only if a source is given)
set -e

SD="$1"
VALVE="$2"
DOL=${DOL:-build/xash.dol}

if [ -z "$SD" ]; then
	echo "usage: $0 <sd-card-mount-point> [path/to/valve]" >&2
	exit 1
fi
[ -d "$SD" ] || { echo "no such directory: $SD" >&2; exit 1; }
[ -f "$DOL" ] || { echo "no $DOL - build first" >&2; exit 1; }

APP="$SD/apps/xash3d"
mkdir -p "$APP"
cp "$DOL" "$APP/boot.dol"

cat > "$APP/meta.xml" <<XML
<?xml version="1.0" encoding="UTF-8"?>
<app version="1">
	<name>Xash3D FWGS</name>
	<coder>FWGS, MintFerret, twixerisss</coder>
	<version>$(git describe --always --dirty 2>/dev/null || echo unknown)</version>
	<release_date>$(date +%Y%m%d)</release_date>
	<short_description>Half-Life engine</short_description>
	<long_description>Wii port of the Xash3D FWGS engine. Put your Half-Life "valve" directory in xash3d/valve on the root of this card.</long_description>
</app>
XML

echo "installed engine to $APP"

if [ -n "$VALVE" ]; then
	[ -d "$VALVE" ] || { echo "no such directory: $VALVE" >&2; exit 1; }
	mkdir -p "$SD/xash3d"
	echo "copying game data (this takes a while)..."
	rsync -a --info=progress2 "$VALVE/" "$SD/xash3d/valve/"
	echo "installed game data to $SD/xash3d/valve"
else
	echo "no valve directory given - copy yours to $SD/xash3d/valve yourself"
fi

echo
echo "Eject the card, put it in the Wii, and launch from the Homebrew Channel."
