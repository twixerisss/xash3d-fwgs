# Test harness

Scripts for running the engine headlessly under Dolphin and reading back what
it did. They exist because the Wii has nowhere to print to: the engine routes
stdout to a USB Gecko in slot B, Dolphin exposes that as a TCP socket, and
these wrap the capture.

| script | what it does |
| --- | --- |
| `run.sh` | boots one config under Dolphin and captures the gecko log |
| `chain.sh` | walks a real `trigger_changelevel` chain and reports which hop it reached |
| `chainsweep.sh` | runs `chain.sh` for every chapter |

```sh
tools/run.sh "label" /path/to/userconfig.cfg 200 build-gl/xash.dol
tools/chain.sh "c1a1" 900 build-gl/xash.dol c1a1 c1a1a:c1a1 c1a1f:c1a1atof
tools/chainsweep.sh build-gl/xash.dol
```

`chain.sh` takes `map:landmark` pairs. Both parts have to be real: the engine
refuses a smooth transition whose landmark the target map does not contain.
Pull them out of the BSP entity lump rather than guessing, since the names are
not predictable (`a1a2`, `c1a1c/d`, `lm_c1a3_0d2`).

## Two traps worth knowing

**An empty log is a harness failure, not a result.** `run.sh` exits non-zero
below 200 bytes for this reason. Reading a zero-byte log as "the game did
nothing" has produced false conclusions on this port more than once.

**A chain that stops early means nothing on its own.** It can mean the game
died or it can mean the capture window ended, and those are opposite
conclusions. `run.sh` reports whether the emulator was still alive at the
cutoff, and `chain.sh` passes that through. Only a dead emulator makes a short
chain a real finding.

`run.sh` also shuts Dolphin down with SIGTERM before resorting to SIGKILL.
Killing it outright mid-write corrupts the FAT on the emulated SD card, which
then fails every later config write and silently invalidates whatever test
runs next; the script repairs the image and retries if it hits that.
