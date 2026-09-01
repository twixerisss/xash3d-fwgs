# Playing Half-Life on the Wii

A step-by-step guide to getting this port running on a real console.

---

## Read this first

**This has never been run on real hardware.** Everything below has been
developed and tested under the Dolphin emulator. It gets through the opening
chapters cleanly there, but the first person to put it on an actual Wii is
doing something nobody has done yet, and it may simply not boot.

It also does not run the whole game yet. See [Known
issues](#known-issues) before you spend an evening on it.

Nothing here can damage your console — homebrew launched from the Homebrew
Channel runs and exits like any other app, and this port only ever reads from
your SD card, apart from writing its own config files. But go in expecting a
work in progress, not a finished game.

---

## What you need

| | |
| --- | --- |
| A Wii | Any model, with the [Homebrew Channel](https://wiibrew.org/wiki/Homebrew_Channel) already installed |
| An SD card | FAT32 formatted. The game data is about 600MB, so 2GB or larger |
| Half-Life | **Your own copy.** No game data is distributed here — you supply the `valve` folder from an installation you own |
| A Nunchuk | Required. The stick is how you move; there is no alternative control scheme yet |

Both the Steam and the original WON releases of Half-Life work. If you have it
on Steam, the folder you want is inside your Steam library:

```
steamapps/common/Half-Life/valve
```

---

## Step 1 — Get the engine

There are no published builds yet, so build it yourself:

```sh
git clone --recursive https://github.com/twixerisss/xash3d-fwgs
git clone --recursive https://github.com/twixerisss/mainui_cpp
git clone --recursive https://github.com/twixerisss/hlsdk-portable
cd xash3d-fwgs
./build_wii.sh
```

All three repositories must sit **next to each other** in the same directory —
the build reads the game code and menu from the other two. You will also need
devkitPro with these packages:

```sh
sudo dkp-pacman -S wii-dev wii-sdl2 wii-opengx ppc-bzip2 ppc-freetype ppc-zlib
```

The result is `build/xash.dol`.

---

## Step 2 — Set up the SD card

There is a script that does the whole thing:

```sh
./install_wii.sh /Volumes/YOUR_SD ~/path/to/valve
```

Or lay it out by hand. The card needs to end up looking like this:

```
SD:/
├── apps/
│   └── xash3d/
│       ├── boot.dol      ← the engine, renamed from xash.dol
│       └── meta.xml      ← so the Homebrew Channel lists it properly
└── xash3d/
    └── valve/            ← your Half-Life game data
        ├── maps/
        ├── models/
        ├── sound/
        └── ...
```

Two details that are easy to miss:

- The game folder is `xash3d/valve` at the **root of the card**, not inside
  `apps`.
- `boot.dol` is the name the Homebrew Channel looks for. If you built it
  yourself, rename `xash.dol` to `boot.dol`.

---

## Step 3 — Two things your Half-Life copy will get wrong

Both of these come from the PC version and will bite you otherwise.

### The menu will be silent

Half-Life ships its soundtrack as `media/Half-Life01.mp3` and up, but the
engine looks for `media/gamestartup.mp3`, which no retail copy contains. Copy
any track to that name:

```sh
cp valve/media/Half-Life01.mp3 valve/media/gamestartup.mp3
```

### The frame limit will be ignored

If you copied `valve` from a PC install, `config.cfg` almost certainly contains
a line like `fps_max "9999"`. That is an archived setting and it overrides the
port's own frame cap, which is what keeps the frame rate steady. Delete the
line:

```sh
sed -i '' '/fps_max/d' valve/config.cfg
```

---

## Step 4 — Run it

Put the card in the Wii, open the Homebrew Channel, and launch Xash3D.

First boot takes a few seconds while it mounts the card and loads the WADs.
You should land on the Half-Life menu with its proper background.

---

## Controls

Aiming works the way console shooters do, rather than as a mouse pointer.

- **Point near the middle of the screen** and the view stays still. Only the
  weapon leans towards where you are pointing. This is what makes fine aiming
  steady — small hand movements do not drag the whole world with them.
- **Point towards an edge** and the view turns, faster the further out you
  point. You swing the camera by pushing outwards and stop by bringing the
  pointer back to the middle.
- **The nunchuk stick moves you.**

Buttons use the engine's normal defaults and can be rebound from
Options → Controls in the menu.

There is a diagram of the aiming scheme in
[`wii-aiming.html`](wii-aiming.html) if the description above is hard to
picture.

---

## Tuning it

Everything here is a console command. Open the console from the main menu, or
put the lines in `valve/userconfig.cfg` to apply them at every launch.

### Aiming feel

| command | default | what it does |
| --- | --- | --- |
| `wii_ir_deadzone` | `0.35` | How much of the screen is the no-turn zone, as a fraction. Raise for a calmer centre, lower to start turning sooner |
| `wii_ir_yawspeed` | `220` | Degrees per second of turn at the screen edge |
| `wii_ir_pitchspeed` | `160` | The same for looking up and down |
| `wii_ir_gunsway` | `7` | Degrees the weapon leans towards the pointer |
| `wii_ir` | `1` | Set to `0` to turn pointer aiming off entirely |

If it feels twitchy while you are lining up a shot, raise `wii_ir_deadzone`.
If it feels sluggish to turn around, raise `wii_ir_yawspeed`.

### Resolution and frame rate

The port renders at 320×240 and the Wii's video hardware scales that to your
TV. That is not an arbitrary choice — the software renderer draws every pixel
on the CPU, and it is the resolution that lets the game hold a steady 30fps.

To change it, edit `valve/video.cfg`:

```
width  "320"
height "240"
```

What actually works, measured on the opening chapter:

| mode | result |
| --- | --- |
| **320×240** | ~48fps uncapped, holds 30 comfortably. The default |
| 640×240 | ~28fps capped. Twice the horizontal detail, but drops frames more often |
| 640×480 | ~24fps. **Cannot hold 30**, and no amount of tuning changes that — the renderer alone needs more than the whole frame budget |
| 512×384 | Not a real mode on this hardware. Silently falls back to 640×480 |

`fps_max` sets the cap and defaults to `30`. Raising it does not make the game
faster; it makes the frame time less consistent, which feels worse.

---

## Known issues

Being straight with you about where this actually is:

- **Never tested on real hardware.** All of the above is from Dolphin.
- **Only the opening chapters are verified.** Every map through "We've Got
  Hostiles" has been checked to load and reach gameplay. Everything from Blast
  Pit onwards is untested — roughly three quarters of the game.
- **Level transitions and saves are untested.** Loading a map directly works;
  walking through a `trigger_changelevel` or using a save has not been
  verified.
- **The software renderer is the only one that works.** There is a hardware
  renderer (`ref_gl`, using the Wii's GPU through opengx) that would look far
  better and allow a higher resolution. It reaches gameplay but hangs while
  loading, on a bug that is narrowed to the texture upload path and not yet
  fixed.
- **The weapon lean is cosmetic.** The gun tilts towards your pointer but
  shots still travel along the view axis.
- **`c1a3c` fails to load** for a reason that has not been diagnosed.

---

## Credits

- Valve, for Half-Life
- Uncle Mike, for the original Xash3D engine
- The FWGS team, for Xash3D FWGS
- MintFerret, whose Wii port this builds on
- mardy, for the SDL2 port and opengx
- The devkitPro team
