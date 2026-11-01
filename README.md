# yabi

Yet another bootsplash implementation: a boot splash for embedded Linux on the framebuffer, small
enough for any system and driven by plain commands from the init scripts.

yabi shows its animations with [fbv](https://github.com/angeloc/fbv-ng), which has a control
socket. `yabi start` launches fbv and returns; every later `yabi` call sends one command over
the socket and exits. There is no daemon, so yabi works the same with SysV init, busybox init or
systemd.

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
| `quit`             | close the splash; with `--retain` keep it for the next program     |
| `rc [dir]`         | run the `S??*` init scripts of dir, moving the progress after each |

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
the frames at the animation's own pace, so the bar moves smoothly. `tools/make_progress_gif.py`
draws such an animation, with an optional logo above the bar; the `default` theme is made with
it.

## Boot integration

SysV or busybox init: start the splash first and let `yabi rc` run the other scripts, moving the
progress after each one, e.g. in `/etc/inittab`:

```
::sysinit:/etc/init.d/S01yabi start
::sysinit:/usr/bin/yabi rc /etc/init.d
::once:/usr/bin/yabi quit
```

Scripts can also report their own progress with `yabi progress <n>`. `init/S01yabi` reads the
theme from `/etc/default/yabi` (`YABI_THEME=<name>`).

systemd: build with `-DYABI_SYSTEMD=ON` to install `yabi-start.service` and
`yabi-quit.service`.

## Building

yabi needs only a C++20 compiler and CMake 3.16 or later; no libraries beyond the C and C++
runtime. fbv must be on the target.

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
| `YABI_SYSVINIT`       | `ON`                          | install `/etc/init.d/S01yabi` |
| `YABI_SYSTEMD`        | `OFF`                         | install the systemd units    |
| `YABI_HARDENING`      | `ON`                          | compiler and linker hardening |
| `YABI_SANITIZE`       | `OFF`                         | AddressSanitizer and UBSan   |
| `YABI_TEST_FBV`       | empty                         | fbv for the end-to-end test  |

The stripped binary is about 58 KiB on x86_64 and 70 KiB on aarch64 with musl.

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
scripts. The splash shows the Buildroot logo above a progress bar that `yabi rc` moves as each
demo service starts; once booted, try the yabi commands on the console.

```
demo/build.sh    # downloads Alpine, giflib and the logo, builds fbv and yabi (about a minute)
demo/run.sh      # boots it: the splash in the QEMU window, the console in the terminal
```

`build.sh` needs curl, git, a C++20 compiler, cmake, python3 with Pillow, unsquashfs and cpio;
`run.sh` needs `qemu-system-x86_64` and uses KVM when available. fbv is built from the
`../fbv-ng` checkout when present, else cloned. `run.sh --check` boots headless and saves
screenshots of the splash.

## Testing

* unit tests of the theme parser and the init script runner
* the `yabi` binary against a stand-in fbv, checking every command, error and exit status
* with `-DYABI_TEST_FBV=<fbv>`, yabi driving a real fbv on a fake framebuffer

## License

GPL-2.0-or-later, see `LICENSE`.
