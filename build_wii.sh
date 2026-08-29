#!/bin/sh
# Wii build helper. Written for macOS + devkitPPC, works anywhere devkitPro does.
#
#   ./build_wii.sh [configure|build|clean]   (default: build)
#
# Expects hlsdk-portable and mainui_cpp checked out next to this directory,
# and these devkitPro packages:
#   dkp-pacman -S wii-dev wii-sdl2 wii-opengx ppc-bzip2 ppc-freetype ppc-zlib
set -e

DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITPRO
export PATH="$DEVKITPRO/tools/bin:$DEVKITPRO/devkitPPC/bin:$PATH"

CMAKE=${CMAKE:-cmake}
JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}

configure()
{
	# 3rdparty/opus still asks for cmake_minimum_required < 3.5, which CMake 4
	# refuses outright, hence the policy floor.
	"$CMAKE" -S. -Bbuild \
		-DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Wii.cmake" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5
}

case "${1:-build}" in
	configure) configure ;;
	clean)     rm -rf build ;;
	*)         [ -f build/Makefile ] || configure
	           make -C build -j"$JOBS"
	           ls -l build/xash.dol ;;
esac
