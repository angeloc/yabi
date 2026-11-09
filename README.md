# yabi

yabi is a boot splash for embedded Linux. The name means "yet another bootsplash
implementation".

- It draws on the framebuffer.
- It is small enough for any system.
- The init scripts drive it with plain commands.

yabi shows its animations with [fbv-ng](https://github.com/angeloc/fbv-ng), through the
control socket of fbv-ng:

- `yabi start` launches the `fbv` program of fbv-ng and returns.
- Every later `yabi` call sends one command over the socket and exits.

There is no daemon. So yabi works the same way with SysV init, busybox init and systemd.

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
| `freeze`           | finish: move to 100%, wait for the animation, then pause it        |
| `show <file>`      | switch to another image or animation                               |
| `theme <theme>`    | switch to another theme                                            |
| `status`           | print the frame, the frame count and the pause state               |
| `ping`             | exit 0 when the splash is running                                  |
| `quit`             | close the splash; `yabi --retain quit` keeps it for the next program |
| `watch [dir...]`   | follow the boot in the background, moving the progress             |

The exit status tells what happened:

| Status | Meaning                    |
|--------|----------------------------|
| 0      | done                       |
| 1      | failed                     |
| 2      | usage error                |
| 3      | the splash is not running  |

So a script can call `yabi progress` without checking whether the splash is enabled.

`yabi --help` and `man yabi` list the options: the socket, the path of fbv, the theme
directory, the timeout and text mode.

## Themes

A theme is a directory with a `theme.conf` file and an animation:

```
animation = progress.gif   # relative to the theme directory
mode = progress            # progress: hold the frame set by "yabi progress"; loop: play freely
                           # and ignore the progress, e.g. a spinner
scale = none               # none, shrink or fit to the screen
```

In a progress animation with 101 frames, each percent has its own frame. When the progress
moves forward, yabi steps through the frames at the pace of the animation, so the bar moves
smoothly.

`themes/make_progress_gif.py` draws this kind of animation, with an optional logo above the
bar. The bundled themes were made with it:

* `default`: a progress bar on black
* `theme1`: the Buildroot logo above the bar, from Buildroot's own repository (GPL-2.0-or-later)
* `theme2`: "Linux" in the Lato font (SIL Open Font License) above the bar
* `theme3`: a spinner, without a progress bar (loop mode)

These recordings show each theme while the [demo](#demo) virtual machine boots, from the
first frame of the splash until the application takes over the screen:

| `default` | `theme1` |
|---|---|
| ![default theme booting](demo/gifs/default.gif) | ![theme1 booting](demo/gifs/theme1.gif) |
| **`theme2`** | **`theme3`** |
| ![theme2 booting](demo/gifs/theme2.gif) | ![theme3 booting](demo/gifs/theme3.gif) |

## Boot integration

### SysV init and busybox init

yabi installs two scripts in `/etc/init.d`. It changes nothing else in the boot.

1. `S00yabi` shows the splash and starts `yabi watch`.
2. `yabi watch` follows the boot while the rc of the distribution runs the other scripts.
   It sleeps until rc opens the next `S??*` script, then moves the progress to the place of
   that script. It uses inotify, so it does not poll. Under OpenRC, it follows the started
   services instead.
3. At the end, `S98yabi` runs `yabi freeze`. This moves the progress to 100% and pauses the
   animation when it gets there. A loop theme, such as a spinner, just stops. Then
   `S98yabi` closes the splash.

`/etc/default/yabi` sets the theme, the init directories to follow, and how the splash ends:

```
YABI_THEME=default        # a theme name or a path
YABI_WATCH=/etc/init.d    # sysvinit with runlevels: "/etc/rcS.d /etc/rc2.d"
YABI_QUIT=yes             # yes: close it, retain: leave its picture on screen, no: keep it
```

To hand the screen over to a graphical application without a visible break:

- set `YABI_QUIT=retain`;
- start the application after `S98yabi`, for example from `S99app`.

The last picture of the splash then stays on the framebuffer until the application draws
over it. Also keep the text console off the display, so that nothing repaints the screen in
between. The demo does this by unbinding the console in `/sys/class/vtconsole`.

Your scripts can also set the progress themselves, with `yabi progress <n>`.

### systemd

Build with `-DYABI_SYSTEMD=ON`. This needs libsystemd.

- `yabi-start.service` shows the splash and starts `yabi watch`.
- `yabi watch` subscribes to systemd. After every finished job, it reads the boot progress
  that systemd itself keeps. It talks to PID 1 directly, so it works before D-Bus is up.
- `yabi-quit.service` closes the splash as `YABI_QUIT` says.

## Building

yabi only needs a C++20 compiler and CMake 3.16 or later. It uses no library other than the
C and C++ runtime.

The target must also have the `fbv` program of fbv-ng, because yabi uses its `--control`
option.

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

A complete boot splash is yabi plus an fbv built for GIF only. These are the sizes of
stripped MinSizeRel builds, with GCC 11.4 on x86_64:

| Boot splash        | yabi    | fbv     | Total    | Libraries beyond the C and C++ runtime |
|--------------------|---------|---------|----------|----------------------------------------|
| SysV, busybox      | 62 KiB  | 78 KiB  | 140 KiB  | giflib (38 KiB)                        |
| with systemd       | 66 KiB  | 78 KiB  | 144 KiB  | giflib, libsystemd (already there)     |

A theme adds the size of its animation. The bundled ones are between 82 and 112 KiB.

### Cross compiling

A cross build needs nothing but a toolchain file. Every compiler and linker flag is tested
first, so any GCC or Clang toolchain works, for example one from Bootlin or Buildroot:

```
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<toolchain>/share/buildroot/toolchainfile.cmake \
	-DCMAKE_CROSSCOMPILING_EMULATOR="qemu-aarch64;-L;<sysroot>"
cmake --build build && ctest --test-dir build
```

When an emulator is set, `ctest` runs the tests on the build machine.

## Demo

`demo/` boots yabi in a small QEMU virtual machine. It is made of:

- the prebuilt Alpine Linux kernel and userspace;
- a static fbv and a static yabi, all in one initramfs;
- busybox init, which runs SysV-style boot scripts.

This is what happens:

1. The splash shows a logo above a progress bar. `yabi watch` moves the bar as each demo
   service starts.
2. The text console is detached from the display, so only the splash and the application
   ever draw on it.
3. At the end, `S98yabi` closes the splash but leaves its last picture on the screen
   (`YABI_QUIT=retain`). Then `S99app` starts a dashboard application, which draws over it.
   So nothing else shows between the splash and the application.

On the console, you can then stop the application and try the yabi commands.

```
demo/build.sh    # downloads Alpine and giflib, builds fbv and yabi (about a minute)
demo/run.sh      # boots it: the splash in the QEMU window, the console in the terminal
```

To try a theme of your own, pass its directory: `demo/run.sh --theme ~/mytheme` boots with
it in place of the built-in one, and you do not need to rebuild.
`themes/make_progress_gif.py` is a quick way to make the animation.

What the scripts need:

- `build.sh` needs curl, git, a C++20 compiler, cmake, unsquashfs and cpio. It builds fbv
  from the `../fbv-ng` checkout when there is one, and clones fbv-ng otherwise.
- `run.sh` needs `qemu-system-x86_64`, and uses KVM when it is available.

`run.sh` has two more modes, both without a window:

- `run.sh --check` boots and saves screenshots of the splash.
- `run.sh --theme themes/theme1 --record theme1.gif` boots and records the splash as a GIF,
  until the application takes over. This needs ffmpeg and Pillow. The GIFs of the
  [themes](#themes), in `demo/gifs/`, were made this way.

## Testing

The tests are:

- unit tests of the theme parser and of the init script runner;
- the `yabi` program against a stand-in fbv, to check every command, error and exit status;
- with `-DYABI_TEST_FBV=<fbv>`, yabi driving a real fbv on a fake framebuffer.

## License

GPL-2.0-or-later, see `LICENSE`.
