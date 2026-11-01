#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Boots the yabi demo VM built by build.sh. The splash shows in the QEMU
# window, the console runs in this terminal; quit with Ctrl-a x.
#
#   run.sh [--theme DIR]           boot with a window
#   run.sh [--theme DIR] --check   boot headless, save screenshots of the
#                                  splash and check that the boot completed
#
# --theme boots with the theme in DIR (a directory with theme.conf and its
# animation) instead of the built-in one, without rebuilding.
set -eu
umask 022

here=$(cd "$(dirname "$0")" && pwd)
out=${OUT:-$here/out}
check=
theme=
while [ $# -gt 0 ]; do
	case "$1" in
	--check) check=1 ;;
	--theme)
		[ $# -ge 2 ] || { echo "--theme needs a directory" >&2; exit 2; }
		theme=$2
		shift
		;;
	*) echo "usage: $0 [--theme DIR] [--check]" >&2; exit 2 ;;
	esac
	shift
done
for f in vmlinuz-virt initramfs.cpio.gz; do
	[ -f "$out/$f" ] || { echo "$out/$f missing, run demo/build.sh first" >&2; exit 1; }
done

initrd=$out/initramfs.cpio.gz
if [ -n "$theme" ]; then
	[ -f "$theme/theme.conf" ] || { echo "$theme/theme.conf not found" >&2; exit 1; }
	name=$(basename "$(cd "$theme" && pwd)")
	# The kernel unpacks concatenated archives in order, so a second one
	# adds the theme and selects it.
	overlay=$out/theme-overlay
	rm -rf "$overlay"
	mkdir -p "$overlay/usr/share/yabi/themes/$name" "$overlay/etc/default"
	cp -R "$theme/." "$overlay/usr/share/yabi/themes/$name/"
	echo "YABI_THEME=$name" > "$overlay/etc/default/yabi"
	initrd=$out/initramfs-theme.cpio.gz
	{
		cat "$out/initramfs.cpio.gz"
		(cd "$overlay" && find . | cpio -o -H newc -R 0:0 --quiet | gzip -9)
	} > "$initrd"
	echo "Booting with theme $name from $theme"
fi

accel=
[ -w /dev/kvm ] && accel="-enable-kvm -cpu host"

qemu() {
	# shellcheck disable=SC2086
	qemu-system-x86_64 $accel -m 512 -vga std \
		-kernel "$out/vmlinuz-virt" -initrd "$initrd" \
		-append "console=ttyS0 quiet loglevel=3" "$@"
}

if [ -z "$check" ]; then
	qemu -serial mon:stdio
	exit
fi

# Headless check: the console log shows the boot, screenshots taken
# through the QEMU monitor show the splash.
shots=$out/check
rm -rf "$shots"
mkdir -p "$shots"
mon=$shots/monitor.sock
qemu -display none -serial "file:$shots/console.log" -monitor "unix:$mon,server,nowait" &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT

monitor() {
	python3 - "$mon" "$1" <<'EOF'
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
s.sendall((sys.argv[2] + "\n").encode())
time.sleep(0.3)
s.close()
EOF
}

i=0
until grep -q "Boot complete" "$shots/console.log" 2>/dev/null; do
	i=$((i + 1))
	[ $i -le 120 ] || { echo "boot did not complete, see $shots/console.log" >&2; exit 1; }
	[ -S "$mon" ] && monitor "screendump $shots/boot-$(printf %03d $i).ppm"
	sleep 0.5
done
sleep 1
monitor "screendump $shots/done.ppm"
echo "Console log and screenshots in $shots"
