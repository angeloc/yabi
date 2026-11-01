#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the yabi demo VM: the Alpine Linux kernel and userspace, prebuilt,
# with static fbv and yabi and a splash theme, packed in one initramfs.
#
# Needs: curl, git, a C++20 compiler, cmake, make, unsquashfs, cpio, gzip;
# pigz is used when present. Nothing is compiled but giflib, fbv and yabi.
#
# Environment:
#   OUT      output directory (default demo/out)
#   FBV_SRC  fbv-ng checkout with the control socket (default ../fbv-ng next
#            to this repository, else cloned from FBV_REPO at FBV_REF)
set -eu
umask 022

here=$(cd "$(dirname "$0")" && pwd)
top=$(dirname "$here")
out=${OUT:-$here/out}
dl=$out/dl
work=$out/work
rootfs=$work/rootfs

ALPINE=3.24.3
ALPINE_URL=https://dl-cdn.alpinelinux.org/alpine/v${ALPINE%.*}/releases/x86_64
MINIROOTFS=alpine-minirootfs-$ALPINE-x86_64.tar.gz
MINIROOTFS_SHA=f8e1cafba9b1b283b5d30d06f9885c8d2bc161ffe9083ade8e2e157813a53407
KERNEL_SHA=17fbd3efd16f2e7cb1b945f06423b37399ba7d5ac8cdd3665e7f13f363500e85
MODLOOP_SHA=16f3a2470f538f529c475134103e83321619e6d8e2b0367beb4a758304449ffe
GIFLIB=giflib-6.1.3
GIFLIB_SHA=b65b66b99f0424b93525f987386f22fc5efb9da2bfc92ad4a532249aaffbab0e
FBV_REPO=${FBV_REPO:-https://github.com/angeloc/fbv-ng.git}
FBV_REF=${FBV_REF:-control}

# fetch <url> <file> [sha256]
fetch() {
	if [ ! -f "$dl/$2" ]; then
		echo "Downloading $1"
		curl -fL --retry 3 -o "$dl/$2.part" "$1"
		mv "$dl/$2.part" "$dl/$2"
	fi
	if [ -n "${3:-}" ] && ! echo "$3  $dl/$2" | sha256sum -c --quiet; then
		rm -f "$dl/$2"
		echo "$2: checksum mismatch" >&2
		exit 1
	fi
}

mkdir -p "$dl"
rm -rf "$work"
mkdir -p "$work"

# Build and compress with every CPU of this machine.
jobs=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
compress="gzip -9"
command -v pigz >/dev/null && compress="pigz -9 -p $jobs"

fetch "$ALPINE_URL/$MINIROOTFS" "$MINIROOTFS" "$MINIROOTFS_SHA"
fetch "$ALPINE_URL/netboot-$ALPINE/vmlinuz-virt" vmlinuz-virt "$KERNEL_SHA"
fetch "$ALPINE_URL/netboot-$ALPINE/modloop-virt" modloop-virt "$MODLOOP_SHA"
fetch "https://sources.buildroot.net/giflib/$GIFLIB.tar.gz" "$GIFLIB.tar.gz" "$GIFLIB_SHA"

# Static binaries run on Alpine's musl userspace as they are.
echo "Building giflib"
tar -C "$work" -xzf "$dl/$GIFLIB.tar.gz"
make -s -j"$jobs" -C "$work/$GIFLIB" libgif.a CFLAGS=-O2 >/dev/null

# Hardening would make them position independent, which -static is not.
static="-DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_TESTING=OFF -DCMAKE_EXE_LINKER_FLAGS=-static"

echo "Building fbv"
fbv_src=${FBV_SRC:-}
if [ -z "$fbv_src" ] && [ -d "$top/../fbv-ng" ]; then
	fbv_src=$top/../fbv-ng
fi
if [ -z "$fbv_src" ]; then
	git clone -q --depth 1 -b "$FBV_REF" "$FBV_REPO" "$work/fbv-ng"
	fbv_src=$work/fbv-ng
fi
cmake -S "$fbv_src" -B "$work/fbv" $static -DFBVNG_HARDENING=OFF -DFBVNG_WITH_PNG=OFF -DFBVNG_WITH_JPEG=OFF \
	-DGIF_INCLUDE_DIR="$work/$GIFLIB" -DGIF_LIBRARY="$work/$GIFLIB/libgif.a" >/dev/null
cmake --build "$work/fbv" -j "$jobs" >/dev/null

echo "Building yabi"
cmake -S "$top" -B "$work/yabi" $static -DYABI_HARDENING=OFF -DCMAKE_INSTALL_PREFIX=/usr \
	-DYABI_THEME_DIR=/usr/share/yabi/themes >/dev/null
cmake --build "$work/yabi" -j "$jobs" >/dev/null

echo "Assembling the root filesystem"
mkdir -p "$rootfs"
tar -C "$rootfs" -xzf "$dl/$MINIROOTFS"
DESTDIR=$rootfs cmake --install "$work/yabi" --strip >/dev/null
install -m 755 -s "$work/fbv/fbv" "$rootfs/usr/bin/fbv"
cp -R "$here/rootfs/." "$rootfs/"

# The bochs DRM driver and what it needs, from the kernel's module image
unsquashfs -q -d "$work/modloop" "$dl/modloop-virt" >/dev/null
kver=$(basename "$(dirname "$(ls "$work"/modloop/modules/*/modules.dep)")")
mods=$work/modloop/modules/$kver
dest=$rootfs/lib/modules/$kver
mkdir -p "$dest"
cp "$mods/modules.dep" "$mods/modules.order" "$mods/modules.builtin" "$dest/"
for m in $(grep '/bochs\.ko:' "$mods/modules.dep" | tr -d ':'); do
	mkdir -p "$dest/$(dirname "$m")"
	cp "$mods/$m" "$dest/$m"
done

cp "$dl/vmlinuz-virt" "$out/vmlinuz-virt"
(cd "$rootfs" && find . | cpio -o -H newc -R 0:0 --quiet | $compress) > "$out/initramfs.cpio.gz"
echo "Done: $out/vmlinuz-virt and $out/initramfs.cpio.gz, boot them with demo/run.sh"
