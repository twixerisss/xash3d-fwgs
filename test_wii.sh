#!/bin/sh
# Headless boot + live log capture for the Wii build, via Dolphin's USB Gecko.
#
# Needs the engine built with -DXASH_OGC_GECKO=1 (see CMakeLists.txt), which
# routes stdout to the USB Gecko in EXI slot B. Dolphin exposes that as a TCP
# socket on 55020, so we can read the engine's trace without a screen.
#
# Usage: ./test_wii.sh [seconds]   ->  prints xash's runtime trace to stdout.
set -e

DOL=${DOL:-build/xash.dol}
SDIMG=${SDIMG:-"$HOME/Library/Application Support/Dolphin/Load/WiiSD.raw"}
DOLPHIN=${DOLPHIN:-/Applications/Dolphin.app/Contents/MacOS/Dolphin}
SECONDS_TO_CAPTURE=${1:-60}
UD=/tmp/dolphin_wii_user
CAP=/tmp/gecko.log

[ -f "$DOL" ] || { echo "no $DOL - build first"; exit 1; }
[ -f "$SDIMG" ] || { echo "no SD image at $SDIMG"; exit 1; }

pkill -f Dolphin 2>/dev/null || true
perl -e 'select(undef,undef,undef,1.5)'

# Dolphin ignores WiiSDCardPath and always reads <userdir>/Load/WiiSD.raw,
# so hand it the image that way.
rm -rf "$UD"; mkdir -p "$UD/Load"
ln -s "$SDIMG" "$UD/Load/WiiSD.raw"
: > "$CAP"

# -v Software: the Null backend stalls libogc's VI wait; Software is slow but
# completes frames, which is all a headless log run needs.
"$DOLPHIN" -b -e "$DOL" -u "$UD" \
	-C Dolphin.Core.SlotB=7 \
	-C Dolphin.Core.WiiSDCard=True \
	-C Dolphin.Core.WiiSDCardEnableFolderSync=False \
	-C Dolphin.Core.WiiSDCardAllowWrites=True \
	-v Software > /tmp/dolphin_stdout.log 2>&1 &
DPID=$!

# grab the gecko stream the moment Dolphin opens it
# nc is no good here: it half-closes as soon as its stdin hits EOF, and the
# gecko's safe mode waits for a *live* peer, so the engine would block forever
# on CON_EnableGecko. Hold the socket fully open instead.
python3 - "$SECONDS_TO_CAPTURE" "$CAP" <<'PYCAP' || true
import socket, sys, time
deadline = time.time() + float( sys.argv[1] )
out = open( sys.argv[2], "ab", buffering=0 )

# The engine waits a few seconds for a live peer (XASH_OGC_GECKO_WAIT), so
# just hammer the port until Dolphin has it open.
s = None
while s is None and time.time() < deadline:
	try:
		s = socket.create_connection(( "127.0.0.1", 55020 ), timeout=1 )
	except OSError:
		time.sleep( 0.05 )
if s is None:
	sys.exit( "never reached the gecko socket" )
s.settimeout( 1.0 )
while time.time() < deadline:
	try:
		data = s.recv( 4096 )
	except socket.timeout:
		continue
	except OSError:
		break
	if not data:
		break
	out.write( data )
s.close()
PYCAP

kill "$DPID" 2>/dev/null || true; pkill -f Dolphin 2>/dev/null || true

echo "===== xash-wii runtime trace ====="
strings "$CAP" | sed 's/\x1b\[[0-9;]*m//g'
