# yabi

Yet another bootsplash implementation: a boot splash for embedded Linux on the framebuffer, small
enough for any system and driven by plain commands from the init scripts.

yabi shows its animations with [fbv-ng](https://github.com/angeloc/fbv-ng), through its control
socket. `yabi start` launches its `fbv` binary and returns; every later `yabi` call sends one
command over the socket and exits. There is no daemon, so yabi works the same with SysV init,
busybox init or systemd.

```
yabi start              # show the default theme
yabi progress 30        # move the progress animation
yabi quit               # close the splash, the framebuffer is free on return
```

## Commands

| Command            | Effect                                                             |
|--------------------|--------------------------------------------------------------------|
| `start [theme]`    | start fbv with the theme (default `default`) and wait until ready  |
| `progress <0-100>` | step the animation to the frame matching the percentage            |
| `frame <n>`        | show frame n                                                       |
| `pause`, `resume`  | stop and restart the animation                                     |
| `show <file>`      | switch to another image or animation                               |
| `theme <theme>`    | switch to another theme                                            |
| `status`           | print the frame, the frame count and the pause state               |
| `ping`             | exit 0 when the splash is running                                  |
| `quit`             | close the splash; `yabi --retain quit` keeps it for the next program |
| `watch [dir...]`   | follow the boot in the background, moving the progress             |

Exit status: 0 done, 1 failed, 2 usage error, 3 splash not running, so scripts can call
`yabi progress` whether the splash is enabled or not. `yabi --help` and `man yabi` list the
options: socket, fbv path, theme directory, timeout, text mode.

## Themes

A theme is a directory with a `theme.conf` and its animation:

```
animation = progress.gif   # relative to the theme directory
mode = progress            # progress: hold the frame set by "yabi progress"; loop: play freely
scale = none               # none, shrink or fit to the screen
```

A progress animation with 101 frames gets one frame per percent. Forward progress steps through
the frames at the animation's own pace, so the bar moves smoothly. `themes/make_progress_gif.py`
draws such an animation, with an optional logo above the bar; it made the bundled themes:

* `default`: a progress bar on black
* `theme1`: the Buildroot logo above the bar, from Buildroot's own repository (GPL-2.0-or-later)
* `theme2`: "Linux" in the Lato font (SIL Open Font License) above the bar

## Boot integration

SysV or busybox init: yabi installs two scripts in `/etc/init.d` and changes nothing else in
the boot. `S01yabi` shows the splash and starts `yabi watch`, which follows the boot as the
distribution's own rc runs the other scripts: it sleeps until rc opens the next `S??*` script
(inotify, no polling) and moves the progress to its place. Under OpenRC it follows the started
services instead. At the end `S98yabi` moves the progress to 100%, pauses it
once the animation got there and closes the splash. `/etc/default/yabi` sets the theme, the init directories to follow and how
the splash ends:

```
YABI_THEME=default        # a theme name or a path
YABI_WATCH=/etc/init.d    # sysvinit with runlevels: "/etc/rcS.d /etc/rc2.d"
YABI_QUIT=yes             # yes: close it, retain: leave its picture on screen, no: keep it
```

For a seamless hand-over to a graphical application, set `YABI_QUIT=retain` and start the
application after `S98yabi`, e.g. from `S99app`: the splash's last picture stays on the
framebuffer until the application draws over it. Keep the text console off the display, as
the demo does by unbinding it in `/sys/class/vtconsole`, so nothing repaints the screen in
between.

Scripts can also set the progress themselves with `yabi progress <n>`.

systemd: build with `-DYABI_SYSTEMD=ON`, which needs libsystemd. `yabi-start.service` shows the
splash and starts `yabi watch`, which subscribes to systemd and, after every finished job,
reads the boot progress systemd keeps itself; it talks to PID 1 directly, so it works before
D-Bus is up. `yabi-quit.service` closes the splash as `YABI_QUIT` says.

## Building

yabi needs only a C++20 compiler and CMake 3.16 or later; no libraries beyond the C and C++
runtime. The `fbv` binary of fbv-ng, with `--control`, must be on the target.

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
cmake --install build
```

| Option                | Default                       | Description                  |
|-----------------------|-------------------------------|------------------------------|
| `YABI_DEFAULT_SOCKET` | `/run/yabi.sock`              | control socket               |
| `YABI_THEME_DIR`      | `<prefix>/share/yabi/themes`  | where themes are looked up   |
| `YABI_SYSVINIT`       | `ON`                          | install the SysV init scripts |
| `YABI_SYSTEMD`        | `OFF`                         | follow systemd, install its units |
| `YABI_HARDENING`      | `ON`                          | compiler and linker hardening |
| `YABI_SANITIZE`       | `OFF`                         | AddressSanitizer and UBSan   |
| `YABI_TEST_FBV`       | empty                         | fbv for the end-to-end test  |

### Size

A complete boot splash is yabi plus fbv built for GIF only, stripped MinSizeRel builds with GCC
11.4 on x86_64:

| Boot splash        | yabi    | fbv     | Total    | Libraries beyond the C and C++ runtime |
|--------------------|---------|---------|----------|----------------------------------------|
| SysV, busybox      | 62 KiB  | 78 KiB  | 140 KiB  | giflib (38 KiB)                        |
| with systemd       | 66 KiB  | 78 KiB  | 144 KiB  | giflib, libsystemd (already there)     |

A theme adds its animation; the bundled ones are 82 to 112 KiB.

### Cross compiling

Cross builds need nothing but a toolchain file; every compiler and linker flag is probed, so any
GCC or Clang toolchain works, e.g. a Bootlin or Buildroot one:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<toolchain>/share/buildroot/toolchainfile.cmake \
	-DCMAKE_CROSSCOMPILING_EMULATOR="qemu-aarch64;-L;<sysroot>"
cmake --build build && ctest --test-dir build
```

With an emulator set, `ctest` runs the tests on the build machine.

## Demo

`demo/` boots yabi in a small QEMU virtual machine: the prebuilt Alpine Linux kernel and
userspace, with static fbv and yabi in one initramfs and busybox init running SysV-style boot
scripts. The splash shows a logo above a progress bar that `yabi watch` moves as each
demo service starts; the text console is detached from the display, so only the splash and
the application ever draw on it. At the end `S98yabi` closes the splash leaving its last
picture (`YABI_QUIT=retain`) and `S99app` starts a dashboard application that draws over it,
so the hand-over from splash to application shows nothing in between. On the console you can
then stop the application and try the yabi commands.

```
demo/build.sh    # downloads Alpine and giflib, builds fbv and yabi (about a minute)
demo/run.sh      # boots it: the splash in the QEMU window, the console in the terminal
```

To try a theme of your own, pass its directory: `demo/run.sh --theme ~/mytheme` boots with it
instead of the built-in one, without rebuilding. `themes/make_progress_gif.py` is a quick start
for the animation.

`build.sh` needs curl, git, a C++20 compiler, cmake, unsquashfs and cpio;
`run.sh` needs `qemu-system-x86_64` and uses KVM when available. fbv is built from the
`../fbv-ng` checkout when present, else cloned. `run.sh --check` boots headless and saves
screenshots of the splash.

## Testing

* unit tests of the theme parser and the init script runner
* the `yabi` binary against a stand-in fbv, checking every command, error and exit status
* with `-DYABI_TEST_FBV=<fbv>`, yabi driving a real fbv on a fake framebuffer

## License

GPL-2.0-or-later, see `LICENSE`.
