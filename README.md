# Embedded Linux on Raspberry Pi 4

Based on Bootlin's [Embedded Linux system development](https://bootlin.com/training/embedded-linux/)
training labs (materials released under CC BY-SA 3.0), BeagleBone Black variant,
adapted to a **Raspberry Pi 4 Model B**.

## Setup

- Development machine: Bazzite, with an Ubuntu distrobox for building
- Serial console: an ESP32-C3 used as a USB-to-UART bridge ([tools/esp32-uart-bridge](tools/esp32-uart-bridge/))
- Versions: Crosstool-NG 1.28.0, U-Boot v2024.04, Linux 6.6

## Labs

1. [Toolchain](01-toolchain.md)
2. [U-Boot](02-u-boot.md)
3. [Fetching kernel sources](03-kernel-sources.md)
4. [Kernel cross-compiling](04-kernel-cross-compile.md)

Board and tool configurations are in [configs/](configs/).

## License

Notes under CC BY-SA 4.0, bridge code under MIT. See [LICENSE.md](LICENSE.md).
