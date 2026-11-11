Arietta G25 + XTERM-01: yabi theme1 demo
========================================

A Buildroot external tree that boots the Acme Systems Arietta G25 (128 MB)
from its microSD card and shows the yabi theme1 boot splash on the XTERM-01
2.8" display (ILI9341, 320x240, on SPI1). The serial console stays on the
debug port (ttyS0, 115200 8N1).

Build
-----

  br-yabi/build.sh

downloads the Buildroot release and builds the image in
build/arietta_xterm01_yabi, next to the CMake build of yabi
(BUILDROOT=<tree> uses an existing Buildroot tree). By hand, from a
Buildroot tree:

  make BR2_EXTERNAL=/path/to/yabi/br-yabi arietta_xterm01_yabi_defconfig
  make

Flash
-----

  sudo dd if=<output>/images/sdcard.img of=/dev/sdX bs=1M conv=fsync

The first partition (FAT) holds boot.bin (at91bootstrap), zImage and
acme-arietta.dtb; at91bootstrap loads the kernel and the device tree from it.

What the demo adds
------------------

- at91-ariettag25-xterm01.dts: the Arietta DTS with the display on SPI1
  (CS PA8, D/C PC4, RESET PC3), driven by the DRM ili9341 driver;
- linux.fragment: the ili9341 driver and /dev/fb0 through the DRM fbdev
  emulation, and no framebuffer console: the kernel and the login stay
  on the serial console, and only the splash and the application draw on
  the display;
- patches/at91bootstrap3:
  - the memory node of the device tree found also when named
    memory@20000000, as in Linux since 2022 (backport from v4);
  - the display cleared to black at power on (out of reset the panel is
    white, and a panel that stayed powered, e.g. through the serial
    cable, keeps its last picture), then logo.bmp of the boot partition
    drawn on it before loading Linux;
- patches/linux: spi-atmel fixes for the 8 and 16 bit transfers of the
  ili9341 driver (changing the word size between transfers, and frames
  longer than the 64 KiB DMA bounce buffer), and the fbdev emulation
  sending the splash's first frame before turning the display on, so
  that the logo goes over to the splash without a black flash;
- logo.bmp: the first frame of theme1 at the display size, which
  at91bootstrap draws within a second from power on: the splash then
  starts on the same picture;
- /etc/default/yabi: theme1, kept on the screen at the end of the boot;
- theme1/theme.conf: the stock theme1 (640x480) with scale = fit, so
  that yabi scales it down to the display;
- BR2_PACKAGE_YABI_DEMO: the demo application, which takes the screen
  over from the splash; S99app replaces the one of the demo to fit its
  1024x768 animation to the display.

The XTERM-01 touch panel (on the AT91 ADC) is not used.
