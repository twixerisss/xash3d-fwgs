#!/bin/sh
# usage: run.sh <label> <cfg-file> [seconds] [dol]
# Boots the engine under Dolphin with the given userconfig and captures the
# USB Gecko log over TCP.
#
# HARD RULE: an empty log is a harness failure, never evidence about the game.
# Reading a zero-byte log as "the game did nothing" has produced false
# conclusions on this port more than once.
LABEL="$1"; CFG="$2"; DWELL="${3:-90}"; DOL="${4:-$HOME/wii-hl/xash3d-fwgs/build-gl/xash.dol}"
SD="$HOME/Library/Application Support/Dolphin/Load/WiiSD.raw"
LOG=/tmp/run_last.log

# Dolphin writes the emulated SD lazily. Killing it with SIGKILL mid-write
# corrupts the FAT, which then fails every later config write with EINVAL and
# silently invalidates whatever test comes next.
kill_dolphin() {
  pkill -TERM -f Dolphin 2>/dev/null
  for i in 1 2 3 4 5 6; do pgrep -f Dolphin >/dev/null 2>&1 || return 0; sleep 1; done
  pkill -9 -f Dolphin 2>/dev/null
}
kill_dolphin
while lsof -nP -iTCP:55020 >/dev/null 2>&1; do sleep 1; done

DEV=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$SD" | head -1 | awk '{print $1}')
diskutil mount "$DEV" >/dev/null 2>&1
if ! cp "$CFG" "/Volumes/NO NAME/xash3d/valve/userconfig.cfg" 2>/dev/null; then
  echo "SD write failed, repairing image" >&2
  diskutil unmount "$DEV" >/dev/null 2>&1
  diskutil repairVolume "$DEV" >/dev/null 2>&1
  diskutil mount "$DEV" >/dev/null 2>&1
  if ! cp "$CFG" "/Volumes/NO NAME/xash3d/valve/userconfig.cfg" 2>/dev/null; then
    echo "HARNESS FAILURE: cannot write userconfig.cfg to the SD image" >&2
    diskutil unmount "$DEV" >/dev/null 2>&1; hdiutil detach "$DEV" >/dev/null 2>&1
    exit 66
  fi
fi
sync; diskutil unmount "$DEV" >/dev/null 2>&1; hdiutil detach "$DEV" >/dev/null 2>&1

rm -rf /tmp/dwRun; mkdir -p /tmp/dwRun/Load; ln -s "$SD" /tmp/dwRun/Load/WiiSD.raw
: > "$LOG"
/Applications/Dolphin.app/Contents/MacOS/Dolphin -b -e "$DOL" -u /tmp/dwRun \
  -C Dolphin.Core.SlotB=7 -C Dolphin.Core.WiiSDCard=True -C Dolphin.Core.WiiSDCardAllowWrites=True \
  -v Software >/dev/null 2>&1 &

python3 -c "
import socket,time
end=time.time()+$DWELL
out=open('$LOG','ab',buffering=0)
s=None
while s is None and time.time()<end:
    try: s=socket.create_connection(('127.0.0.1',55020),timeout=1)
    except OSError: time.sleep(0.03)
if s:
    s.settimeout(2)
    while time.time()<end:
        try: d=s.recv(4096)
        except socket.timeout: continue
        except OSError: break
        if not d: break
        out.write(d)
"
# Did the emulator outlive the capture window, or did it die first? A log that
# just stops means nothing until you know which. If Dolphin is gone before the
# dwell expires, the guest crashed hard, and that is a result rather than an
# artifact.
if pgrep -f Dolphin >/dev/null 2>&1; then
  echo "$LABEL :: emulator ALIVE at cutoff (capture window ended normally)"
else
  echo "$LABEL :: emulator DIED before cutoff (guest crashed)"
fi
kill_dolphin

BYTES=$(wc -c < "$LOG" | tr -d ' ')
if [ "$BYTES" -lt 200 ]; then
	echo "$LABEL :: HARNESS FAILURE - log only $BYTES bytes, result means nothing"
	exit 2
fi
echo "$LABEL :: log ${BYTES}b"
