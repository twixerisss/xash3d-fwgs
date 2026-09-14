#!/bin/sh
# usage: chain.sh <label> <dwell> <dol> <start-map> <map:landmark> ...
# Walks a real trigger_changelevel chain the way a player would, rather than
# loading each map cold, and reports which hop it actually reached. The map
# sweep does not cover this: transitions run the save/restore path, which is
# where the hardware crash lives.
LABEL="$1"; DWELL="$2"; DOL="$3"; START="$4"; shift 4
CFG=/tmp/chain.cfg
printf 'map %s\nwait 350\n' "$START" > $CFG
EXPECT="$START"
for hop in "$@"; do
  m=${hop%%:*}; lm=${hop#*:}
  printf 'changelevel2 %s %s\nwait 350\n' "$m" "$lm" >> $CFG
  EXPECT="$EXPECT $m"
done
OUT=$( $HOME/wii-hl/tools/run.sh "$LABEL" $CFG "$DWELL" "$DOL" 2>&1 )
case "$OUT" in *"HARNESS FAILURE"*) echo "$LABEL :: $OUT"; exit 66;; esac
# a short chain with the emulator still alive means the window ended, not the
# game: only a dead emulator turns "did not finish" into a real result
LIVE=$(echo "$OUT" | grep -oE "emulator (ALIVE|DIED)[^\n]*")
C=$(strings /tmp/run_last.log | sed 's/\x1b\[[0-9;]*m//g')
GOT=$(echo "$C" | grep -oE "Spawn Server: [a-z0-9_]+" | sed 's/Spawn Server: //' | tr '\n' ' ')
ERR=$(echo "$C" | grep -ciE "Host_Error|Sys_Error|out of memory|trashed|Can.t write save")
NEXP=$(echo $EXPECT | wc -w | tr -d ' '); NGOT=$(echo $GOT | wc -w | tr -d ' ')
echo "$LABEL  hops $NGOT/$NEXP  errors $ERR  [$LIVE]"
echo "  expected: $EXPECT"
echo "  reached : $GOT"
[ "$ERR" -gt 0 ] && echo "$C" | grep -iE "Host_Error|Sys_Error|out of memory|trashed" | head -3
exit 0
