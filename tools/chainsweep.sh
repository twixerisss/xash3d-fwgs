#!/bin/sh
# Walks every chapter's transition chain end to end on the given build.
DOL="${1:-$HOME/wii-hl/xash3d-fwgs/build-gl/xash.dol}"
PASS=0; SHORT=0; ERRS=0; BAD=""
while read -r ch start hops; do
  [ -z "$ch" ] && continue
  R=$( $HOME/wii-hl/tools/chain.sh "$ch" 900 "$DOL" "$start" $hops )
  echo "$R"
  H=$(echo "$R" | head -1 | sed -n 's/.*hops \([0-9]*\)\/\([0-9]*\).*/\1 \2/p')
  G=$(echo $H | cut -d' ' -f1); E=$(echo $H | cut -d' ' -f2)
  ERR=$(echo "$R" | head -1 | sed -n 's/.*errors \([0-9]*\).*/\1/p')
  if [ "$ERR" != "0" ]; then ERRS=$((ERRS+1)); BAD="$BAD $ch(err)"
  elif [ "$G" = "$E" ]; then PASS=$((PASS+1))
  else SHORT=$((SHORT+1)); BAD="$BAD $ch($G/$E)"; fi
done < /tmp/chapter_plans.txt
echo "=== CHAIN SWEEP: $PASS complete, $SHORT short, $ERRS with errors ==="
[ -n "$BAD" ] && echo "not clean:$BAD"
