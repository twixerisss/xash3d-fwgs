# Xash3D FWGS Engine (Wii port) <img align="right" width="128" height="128" src="https://github.com/FWGS/xash3d-fwgs/raw/master/game_launch/icon-xash-material.png" alt="Xash3D FWGS icon" />

Xash3D ([pronounced](https://ipa-reader.com/?text=ks%C9%91%CA%82) `[ksɑʂ]`) FWGS is a game engine, aimed to provide compatibility with Half-Life Engine and extend it, as well as to give game developers well known workflow. This is the Wii/Gamecube port of the engine

Xash3D FWGS is a heavily modified fork of an original [Xash3D Engine](https://www.moddb.com/engines/xash3d-engine) by Unkle Mike.

## Installation & Running

0) Rename `xash.dol` to `boot.dol` if it was compiled
1) Move the `boot.dol` file to some directory inside the `apps` folder
2) Copy `valve` directory to a folder named `xash3d` in the root of the sd card
3) Run it via the Homebrew Channel

## Build instructions
The Wii/GC port currently uses cmake to build its binaries. Will integrate it into waf at some point

**NOTE: NEVER USE GitHub's ZIP ARCHIVES. GitHub doesn't include external dependencies we're using!**

### Prerequisites

*  Install CMake
*  Install [devkitPro](https://devkitpro.org/wiki/Getting_Started)
*  Install devkitPPC and the needed libraries
 `sudo (dkp-)pacman -S wii-dev wii-sdl2 wii-opengx ppc-bzip2 ppc-freetype ppc-zlib`
*  Create a development directory
*  Clone the following repositories in the same directory
```
git clone --recursive https://github.com/twixerisss/xash3d-fwgs
git clone --recursive https://github.com/twixerisss/mainui_cpp
git clone --recursive https://github.com/twixerisss/hlsdk-portable
```

### Building
1) Configure build `cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="/opt/devkitpro/cmake/Wii.cmake"`
2) Compile `make -C build`

Or just `./build_wii.sh`, which wraps both steps and picks up devkitPro from
`$DEVKITPRO`. On CMake 4 the configure step additionally needs
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, because the vendored opus still declares
a pre-3.5 minimum; the helper script passes it for you.

This will build:
- the filesystem
-  hlsdk (game libraries)
-  mainui
-  the engine itself

### Debugging

The Wii has nowhere to print to, so the engine can route stdout to a USB Gecko
in memory card slot B (`-DXASH_OGC_GECKO=1`, on by default in this tree). That
covers everything from the first line of `main()` onwards, which is where the
interesting crashes still are.

Dolphin emulates the gecko as a TCP socket, so a headless boot can be captured
end to end:

```
./test_wii.sh [seconds]
```

It launches Dolphin in batch mode with a USB Gecko in slot B, attaches to the
socket and prints the engine's trace. It reads the SD image from
`~/Library/Application Support/Dolphin/Load/WiiSD.raw` (Dolphin ignores
`WiiSDCardPath`), which needs `xash3d/valve` in its root - the same layout as a
real card. `SDIMG=`, `DOL=` and `DOLPHIN=` override the paths.

Note that Dolphin's SD emulation is roughly a thousand times slower than real
hardware - every read is a full emulated IOS round trip - so booting under the
emulator takes minutes where a real Wii takes seconds. Don't optimise for it.

### Note
- This is a work in progress
- Expect crashes and instability

### Credits
- Uncle Mike for the original Xash3D Engine
- FWGS team for Xash3D FWGS fork
- mardy for the SDL2 port and OpenGX
- devkitPro team

