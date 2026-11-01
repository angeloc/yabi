#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# yabi driving a real fbv on a fake framebuffer.
# Usage: fbv_test.sh <yabi> <fbv> <fakefb.so> <themes dir>
set -eu
yabi=$1 fbv=$2 shim=$3 themes=$4
dir=$(mktemp -d /tmp/yabi-fbv-XXXXXX)
trap 'rm -rf "$dir"' EXIT

# 64x48 at 32 bpp, lines padded by 16 bytes, see fakefb.cpp
dd if=/dev/zero of="$dir/fb" bs=272 count=48 2>/dev/null
export FRAMEBUFFER="$dir/fb" FAKEFB_GEOMETRY=64,48,32 LD_PRELOAD="$shim"
export ASAN_OPTIONS=verify_asan_link_order=0
y() { "$yabi" -s "$dir/sock" -f "$fbv" "$@"; }

# The default animation shrunk to the small screen, so the bar shows
mkdir "$dir/theme"
printf 'animation = %s\nscale = shrink\n' "$themes/default/progress.gif" > "$dir/theme/theme.conf"

y --no-graphics start "$dir/theme"
[ "$(y status)" = "frame=0 frames=101 paused=0" ]
y progress 100
i=0
until [ "$(y status)" = "frame=100 frames=101 paused=0" ]; do
	i=$((i + 1))
	[ $i -lt 100 ] || { echo "progress 100 not reached: $(y status)"; exit 1; }
	sleep 0.05
done
y progress 25
[ "$(y status)" = "frame=25 frames=101 paused=0" ]
y quit
! y ping 2>/dev/null
[ ! -e "$dir/sock" ]
# fbv left the bar on the screen
[ -n "$(tr -d '\0' < "$dir/fb" | head -c 1)" ]
echo ok
