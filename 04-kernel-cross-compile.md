# Lab 4: Kernel cross-compiling

**Goal:** check out a stable version of the Linux kernel, cross-compile it for the
Raspberry Pi 4, load it with U-Boot over TFTP and check that it starts.

**Result:** the LTS branch `linux-6.6.y` (6.6.161 at the time) cross-compiled for the
Raspberry Pi 4 and served from my TFTP server. U-Boot loaded the kernel `Image` and the
device tree into RAM and started the kernel, which then panicked because there is no
root filesystem yet (expected at this stage).

## Pi 4 differences

| Lab (BeagleBone) | Pi 4 |
|---|---|
| `ARCH=arm`, `arm-linux-` | `ARCH=arm64`, `aarch64-linux-` |
| OMAP2 defconfig | `defconfig` (generic 64-bit ARM) |
| MUSB / AM335X / USB gadget options | Not needed: BeagleBone hardware |
| `zImage`, booted with `bootz` | `Image`, booted with `booti` |
| `am335x-boneblack.dtb` | `bcm2711-rpi-4-b.dtb` |
| Load addresses `0x81000000`, `0x82000000` | `${kernel_addr_r}`, `0x08000000` |
| `console=ttyS0` | `console=ttyS1` (the mini UART) |

## Steps

### Choosing the stable version

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/kernel/linux
git branch -a
make kernelversion                 # current version
git checkout linux-6.6.y
make kernelversion                 # now 6.6.161
```

### Cross-compiling environment

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-
sudo apt install bc
```

`bc` is a command-line calculator. The kernel build uses it to compute constants such
as the conversion between timer ticks and real time.

### Configuration

```bash
make defconfig
make menuconfig
```

Or with xconfig:

```bash
sudo apt install qtbase5-dev pkg-config
make xconfig
```

Options I changed or checked:

| Option | Purpose | defconfig | Mine |
|---|---|---|---|
| `BCMGENET` | Ethernet controller driver | m | y |
| `BROADCOM_PHY` | Driver for the Ethernet PHY chip | m | y |
| `PCIE_BRCMSTB` | PCIe controller (the USB chip sits behind it) | m | y |
| `USB_XHCI_HCD` | USB 3 host driver | y | y |
| `USB_XHCI_PCI` | USB 3 host support for PCIe-attached controllers (the VL805) | m | y |
| `RESET_RASPBERRYPI` | Firmware-controlled reset of the VL805 USB chip | m | y |
| `SERIAL_8250` | Serial driver family the mini UART belongs to | y | y |
| `SERIAL_8250_CONSOLE` | Lets that serial driver be the kernel console | y | y |
| `SERIAL_8250_BCM2835AUX` | The Pi's mini UART (console on pins 8 and 10) | y | y |
| `RASPBERRYPI_FIRMWARE` | Talks to the GPU firmware, used by the clock and reset drivers | y | y |
| `CLK_RASPBERRYPI` | Clocks controlled through the firmware | m | y |
| `MMC_SDHCI_IPROC` | SD card controller | y | y |
| `I2C_BCM2835` | I2C controllers (for the hardware lab) | m | y |

### Building

```bash
make -j$(nproc)
ls -lh arch/arm64/boot/Image
ls arch/arm64/boot/dts/broadcom/ | grep rpi-4
```

### Loading and booting with U-Boot

On the host, copy the files to the TFTP folder (copies, not moves; see the SELinux note
in [Lab 2](02-u-boot.md)):

```bash
K=~/embedded-linux/embedded-linux-bbb-labs/kernel/linux
sudo cp $K/arch/arm64/boot/Image /var/lib/tftpboot/
sudo cp $K/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb /var/lib/tftpboot/
```

In U-Boot, tell the kernel which device is its console:

```text
setenv bootargs console=ttyS1,115200 earlycon
saveenv
```

U-Boot's default device tree address, `${fdt_addr_r}`, is `0x02600000`. The generic
arm64 kernel is large: `booti` reported that it runs from `0x200000` to `0x2480000`.
That leaves under 2 MB between the end of the kernel and the device tree, so any
growth of the kernel could overwrite it. I load the device tree higher, at
`0x08000000`, instead:

```text
tftp ${kernel_addr_r} Image
tftp 0x08000000 bcm2711-rpi-4-b.dtb
booti ${kernel_addr_r} - 0x08000000
```

### Automating the boot

Set `bootcmd` so U-Boot loads and boots the kernel by itself:

```text
setenv bootcmd 'tftp ${kernel_addr_r} Image; tftp 0x08000000 bcm2711-rpi-4-b.dtb; booti ${kernel_addr_r} - 0x08000000'
saveenv
reset
```

## Concepts

- **LTS (Long Term Support):**
  - An LTS version gets no new features, but gets bug and security fixes for years.
  - Products can keep getting updates for years without moving to a new kernel and
    retesting everything.
- **Why the labs use 6.6:**
  - The labs were tested with this version, so results are consistent.
  - It is an LTS version.
  - It must not be older than the kernel headers in the toolchain.
- **ARCH and CROSS_COMPILE:**
  - `export ARCH=arm64` tells the kernel build which architecture to build for;
    without it, the build assumes the development machine's own architecture.
  - `export CROSS_COMPILE=aarch64-linux-` is the prefix put in front of every tool name.
- **defconfig and menuconfig; built in (`=y`) vs module (`=m`):**
  - `make defconfig` creates a default `.config` from the architecture's in-tree
    default configuration.
  - `make xconfig` and `make menuconfig` edit the `.config` file.
  - `make savedefconfig` saves `.config` as a minimal file named `defconfig`, leaving
    out options that have their default values.
  - `scripts/diffconfig <old.config> <new.config>` compares two configurations. Comparing
    a full `.config` with a `defconfig` file also lists the default values, since the
    `defconfig` file leaves them out.
  - `=y` builds an option into the kernel image; `=m` builds it as a separate module
    file stored on the root filesystem and loaded later. Anything needed before the
    root filesystem is mounted must be `=y`.
- **Kernel image (`Image`) and device tree (`.dtb`):**
  - `arch/arm64/boot/Image` is the uncompressed arm64 kernel, the file `booti` starts.
    The kernel is generic.
  - `arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dtb` is the compiled device tree:
    the description of the Pi 4's hardware, what it has and where.
- **bootargs and the `console=` parameter:**
  - `bootargs` is the kernel command line.
  - `console=ttyS1,115200` sends the kernel's console to `ttyS1`. The device tree
    describes the hardware behind it (the mini UART at `0xfe215040`, routed to
    GPIO14/15, pins 8 and 10).
  - `earlycon` prints from the very start of the boot, before the full serial driver
    loads.
- **booti and the load addresses:**
  - `booti <kernel> <initramfs> <dtb>` starts an arm64 kernel; `-` means no initramfs.
  - `tftp <address> <file>` downloads a file from the TFTP server into RAM.
  - `booti` moves the kernel from `0x80000` to `0x200000`, because the arm64 boot rules
    in Linux's documentation require the `Image` to be placed at a 2 MB-aligned address.
  - `bdinfo` shows the RAM layout, to find free memory for loading.
- **bootcmd:**
  - The command U-Boot runs automatically when the boot countdown ends without a key
    press.
  - The Pi's default searches the SD card, USB, then the network.
  - I replaced it with my own sequence, saved with `saveenv`, so the board loads and
    boots my kernel over TFTP by itself.

[Back to the project overview](README.md)
