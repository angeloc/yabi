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
| `show <file>`      | switch to another image or animation                               |
| `theme <theme>`    | switch to another theme                                            |
| `status`           | print the frame, the frame count and the pause state               |
| `ping`             | exit 0 when the splash is running                                  |
| `quit`             | close the splash; with `--retain` keep it for the next program     |
| `rc [dir]`         | run the `S??*` init scripts of dir, moving the progress after each |

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
scale = none               # none, shrink or fit to the screen
```

In a progress animation with 101 frames, each percent has its own frame. When the progress
moves forward, yabi steps through the frames at the pace of the animation, so the bar moves
smoothly.

`tools/make_progress_gif.py` draws this kind of animation, with an optional logo above the
bar. The bundled themes were made with it:

* `default`: a progress bar on black
* `theme1`: the Buildroot logo above the bar, from Buildroot's own repository (GPL-2.0-or-later)
* `theme2`: "Linux" in the Lato font (SIL Open Font License) above the bar

## Boot integration

### SysV init and busybox init

Start the splash first, then let `yabi rc` run the other scripts. It moves the progress
after each one. For example, in `/etc/inittab`:

```
::sysinit:/etc/init.d/S00yabi start
::sysinit:/usr/bin/yabi rc /etc/init.d
::once:/usr/bin/yabi quit
```

Scripts can also report their own progress with `yabi progress <n>`.

`init/S00yabi` reads the theme from `/etc/default/yabi` (`YABI_THEME=<name>`).

### systemd

Build with `-DYABI_SYSTEMD=ON` to install `yabi-start.service` and `yabi-quit.service`.

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
| `YABI_SYSVINIT`       | `ON`                          | install `/etc/init.d/S00yabi` |
| `YABI_SYSTEMD`        | `OFF`                         | install the systemd units    |
| `YABI_HARDENING`      | `ON`                          | compiler and linker hardening |
| `YABI_SANITIZE`       | `OFF`                         | AddressSanitizer and UBSan   |
| `YABI_TEST_FBV`       | empty                         | fbv for the end-to-end test  |

The stripped binary is about 58 KiB on x86_64, and about 70 KiB on aarch64 with musl.

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

The splash shows a logo above a progress bar. `yabi rc` moves the bar as each demo
service starts. Once it has booted, you can try the yabi commands on the console.

```
demo/build.sh    # downloads Alpine and giflib, builds fbv and yabi (about a minute)
demo/run.sh      # boots it: the splash in the QEMU window, the console in the terminal
```

To try a theme of your own, pass its directory: `demo/run.sh --theme ~/mytheme` boots with
it in place of the built-in one, and you do not need to rebuild.
`tools/make_progress_gif.py` is a quick way to make the animation.

What the scripts need:

- `build.sh` needs curl, git, a C++20 compiler, cmake, unsquashfs and cpio. It builds fbv
  from the `../fbv-ng` checkout when there is one, and clones fbv-ng otherwise.
- `run.sh` needs `qemu-system-x86_64`, and uses KVM when it is available.

`run.sh --check` boots without a window and saves screenshots of the splash.

## Testing

The tests are:

- unit tests of the theme parser and of the init script runner;
- the `yabi` program against a stand-in fbv, to check every command, error and exit status;
- with `-DYABI_TEST_FBV=<fbv>`, yabi driving a real fbv on a fake framebuffer.

## License

GPL-2.0-or-later, see `LICENSE`.
