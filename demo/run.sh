#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Boots the yabi demo VM built by build.sh. The splash shows in the QEMU
# window, the console runs in this terminal; quit with Ctrl-a x.
#
#   run.sh [--theme DIR]           boot with a window
#   run.sh [--theme DIR] --check   boot headless, save screenshots of the
#                                  splash and check that the boot completed
#   run.sh [--theme DIR] --record FILE.gif
#                                  boot headless and record the splash, from
#                                  its first frame to the application, as a GIF
#
# --theme boots with the theme in DIR (a directory with theme.conf and its
# animation) instead of the built-in one, without rebuilding.
set -eu
umask 022

here=$(cd "$(dirname "$0")" && pwd)
out=${OUT:-$here/out}
check=
theme=
record=
while [ $# -gt 0 ]; do
	case "$1" in
	--check) check=1 ;;
	--record)
		[ $# -ge 2 ] || { echo "--record needs a file name" >&2; exit 2; }
		record=$2
		shift
		;;
	--theme)
		[ $# -ge 2 ] || { echo "--theme needs a directory" >&2; exit 2; }
		theme=$2
		shift
		;;
	*) echo "usage: $0 [--theme DIR] [--check | --record FILE.gif]" >&2; exit 2 ;;
	esac
	shift
done
command -v qemu-system-x86_64 >/dev/null || { echo "qemu-system-x86_64 not found, install QEMU (qemu-system-x86)" >&2; exit 1; }
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
	# exec: run in the background, "$!" is QEMU itself, so the EXIT trap stops it
	# shellcheck disable=SC2086
	exec qemu-system-x86_64 $accel -m 512 -vga none -device VGA,edid=on,xres=1024,yres=768 \
		-kernel "$out/vmlinuz-virt" -initrd "$initrd" \
		-append "console=ttyS0 quiet loglevel=3" "$@"
}

if [ -z "$check" ] && [ -z "$record" ]; then
	qemu -serial mon:stdio
	exit
fi

if [ -n "$record" ]; then
	# Screenshots through one QEMU monitor connection, about ten per second,
	# until a few seconds after the boot completed, then a 512 pixel wide GIF.
	command -v ffmpeg >/dev/null || { echo "--record needs ffmpeg" >&2; exit 1; }
	frames=$out/record
	rm -rf "$frames"
	mkdir -p "$frames"
	mon=$frames/monitor.sock
	qemu -display none -serial "file:$frames/console.log" -monitor "unix:$mon,server,nowait" &
	pid=$!
	trap 'kill $pid 2>/dev/null || true' EXIT
	fps=$(python3 - "$frames" <<'EOF'
import os, socket, sys, time
d = sys.argv[1]
mon, log = os.path.join(d, "monitor.sock"), os.path.join(d, "console.log")
for _ in range(100):
    if os.path.exists(mon):
        break
    time.sleep(0.1)
s = socket.socket(socket.AF_UNIX)
s.connect(mon)
s.settimeout(0.05)
period, after, limit = 0.1, 3.0, 120.0
start, done, n = time.monotonic(), None, 0
while True:
    t = time.monotonic()
    s.sendall(("screendump %s/f%05d.ppm\n" % (d, n)).encode())
    n += 1
    try:
        while s.recv(4096):
            pass
    except OSError:
        pass
    if done is None and os.path.exists(log) and b"Boot complete" in open(log, "rb").read():
        done = t
    if (done is not None and t - done > after) or t - start > limit:
        break
    time.sleep(max(0.0, period - (time.monotonic() - t)))
print("%.2f" % (n / (time.monotonic() - start)))
EOF
)
	# Keep the frames from the splash on: drop the firmware and early kernel
	# text screens (other resolutions) and the black screen before the splash.
	python3 - "$frames" <<'EOF'
import glob, os, sys
from PIL import Image
d = sys.argv[1]
def size(f):
    try:
        return Image.open(f).size
    except OSError:
        return None
files = sorted(glob.glob(os.path.join(d, "f*.ppm")))
final = size(files[-1])
for f in files:
    if size(f) != final:
        os.remove(f)
# Leading frames that are black or only show text from the top-left corner
# (firmware, sometimes at the final resolution) come before the splash.
def splash(f):
    im = Image.open(f).convert("L")
    box = im.point(lambda v: 255 if v > 16 else 0).getbbox()
    return box is not None and box[1] > im.height // 20
files = sorted(glob.glob(os.path.join(d, "f*.ppm")))
first = next((i for i, f in enumerate(files) if splash(f)), 0)
for f in files[:first]:
    os.remove(f)
for j, f in enumerate(sorted(glob.glob(os.path.join(d, "f*.ppm")))):
    os.rename(f, os.path.join(d, "g%05d.ppm" % j))
EOF
	ffmpeg -loglevel error -y -framerate "$fps" -i "$frames/g%05d.ppm" \
		-vf "scale=512:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" \
		-loop 0 "$record"
	echo "Recorded $(ls "$frames"/g*.ppm | wc -l) frames at $fps fps to $record"
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
