#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds a Buildroot demo of this external tree, by default the Arietta G25
# with the XTERM-01 display (configs/arietta_xterm01_yabi_defconfig).
#
# Usage: br-yabi/build.sh [DEMO]   DEMO is a configs/DEMO_defconfig name
#
# Needs curl and what Buildroot needs on the build machine; the cross
# toolchain is downloaded by Buildroot.
#
# Environment:
#   OUT                output directory (default build, the CMake build
#                      directory of yabi); DEMO builds in OUT/DEMO, its SD card
#                      image is OUT/DEMO/images/sdcard.img
#   BUILDROOT          Buildroot tree to use (default: the release tarball
#                      from buildroot.org, unpacked in OUT/buildroot-VERSION)
#   BR2_DL_DIR         download directory, as in Buildroot
set -eu

here=$(cd "$(dirname "$0")" && pwd)
top=$(dirname "$here")
demo=${1:-arietta_xterm01_yabi}

# The Buildroot release, with the SHA256 of its signed .sign file
BUILDROOT_VERSION=2026.08.1
BUILDROOT_SHA=38a6c152490096018720a9ade379ee1e571c9bf5f93a3657a166566945a98b0d
BUILDROOT_URL=https://buildroot.org/downloads/buildroot-$BUILDROOT_VERSION.tar.gz

if [ ! -f "$here/configs/${demo}_defconfig" ]; then
	echo "$demo: no such demo; the demos are:" >&2
	ls "$here/configs" | sed -n 's/_defconfig$//p' >&2
	exit 2
fi

mkdir -p "${OUT:-$top/build}"
out=$(cd "${OUT:-$top/build}" && pwd)

if [ -n "${BUILDROOT:-}" ]; then
	if [ ! -f "$BUILDROOT/Makefile" ] || [ ! -f "$BUILDROOT/Config.in" ]; then
		echo "$BUILDROOT: not a Buildroot tree" >&2
		exit 1
	fi
	br=$(cd "$BUILDROOT" && pwd)
else
	br=$out/buildroot-$BUILDROOT_VERSION
	tarball=$out/buildroot-$BUILDROOT_VERSION.tar.gz
	if [ ! -f "$br/Makefile" ]; then
		if [ ! -f "$tarball" ]; then
			echo "Downloading $BUILDROOT_URL"
			curl -fL --retry 3 -o "$tarball.part" "$BUILDROOT_URL"
			mv "$tarball.part" "$tarball"
		fi
		if ! echo "$BUILDROOT_SHA  $tarball" | sha256sum -c --quiet; then
			rm -f "$tarball"
			echo "$tarball: checksum mismatch" >&2
			exit 1
		fi
		rm -rf "$br"
		tar -xzf "$tarball" -C "$out"
	fi
fi

# Configure only once: later runs keep the changes made with
# make -C "$br" O="$out/$demo" menuconfig
if [ ! -f "$out/$demo/.config" ]; then
	make -C "$br" O="$out/$demo" BR2_EXTERNAL="$here" "${demo}_defconfig"
fi
make -C "$br" O="$out/$demo"

echo "SD card image: $out/$demo/images/sdcard.img"
