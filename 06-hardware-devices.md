# Lab 6: Accessing hardware devices

**Goal:** explore the devices Linux sees through `/dev` and `/sys`, drive GPIOs and
LEDs, enable an I2C bus and declare a device in a custom device tree, and build kernel
modules, including an out-of-tree one.

**Result:** I enabled the Pi's I2C3 bus in a custom device tree and declared my BNO085 IMU
on it. My own out-of-tree driver binds to it (`UU` in `i2cdetect`). I also read a GPIO,
drove the `ACT` LED, and built and loaded an in-tree module (USB audio). The custom board
`model` name doesn't reach the kernel on the Pi: U-Boot overwrites it.

## Pi 4 differences

| Lab (BeagleBone) | Pi 4 |
|---|---|
| P9 header | Pi 40-pin header |
| GPIO P9 pin 12 (GPIO1_28), sysfs number 540 | Pin 11 (GPIO17), sysfs number 529 (chip base 512 + 17) |
| GND P9 pin 1, 3.3 V P9 pin 3 | GND pin 6 (or 9), 3.3 V pin 1 or 17 |
| LED `beaglebone:green:heartbeat` | `ACT`, the green activity LED |
| I2C1 on P9 pins 17 and 18 | An extra BCM2711 I2C controller, e.g. I2C3 on pins 7 (SDA) and 29 (SCL) |
| `arch/arm/boot/dts/ti/omap/am335x-boneblack-custom.dts` | `arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b-custom.dts` |
| Nunchuk at `0x52`, with the lab's out-of-tree driver | BNO085 IMU at `0x4b`, with my own minimal out-of-tree driver (`ceva,bno085`) |
| `make zImage` | `make Image` |
| USB device path `1-1` (musb) | `1-1.x`, behind the VL805 hub (xhci) |
| `CONFIG_SND_USB_AUDIO` already `=m` | Not in the arm64 defconfig: set it to `m` |

## Steps

### Setup

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/hardware
```

The board keeps booting over NFS, with the root filesystem from Lab 5.

### Exploring /dev

On the board:

```text
ls /dev
cat /proc/consoles
```

- Terminals: `console` (the `console=` device), `ttyS1` (mini UART, my console).
- MMC: `mmcblk0` and its partitions (`mmcblk0p1`, ...) are the SD card.

When I connect a USB storage device, I see the device `sda` and its partitions `sda1`, `sda2`, `sda3`.

### Exploring /sys

```text
ls /sys/class/net
cat /sys/class/net/eth0/speed
cat /sys/class/net/eth0/address
cat /sys/class/net/eth0/statistics/rx_bytes
ls /sys/class/thermal
ls /sys/bus/mmc/devices
```

- `speed` shows 1000.
- `/sys/class/thermal` needs `CONFIG_BCM2711_THERMAL` (`=m` in defconfig).
- In the SD card's directory in `/sys/bus/mmc/devices/`: `type` is `SD`, `name` is `EB1QT`,
  and `date` is the manufacturing date.

### Driving GPIOs

Kernel options: `CONFIG_GPIO_SYSFS=y`, `CONFIG_DEBUG_FS=y`, `CONFIG_DEBUG_FS_ALLOW_ALL=y`.
`CONFIG_GPIO_SYSFS=y` was missing: I needed to enable `CONFIG_EXPERT=y` first, then rebuild
and copy the kernel again.

```text
mount -t debugfs debugfs /sys/kernel/debug/
cat /sys/kernel/debug/gpio
```

`gpiochip0` is `pinctrl-bcm2711` (58 lines, GPIO0–57). Its base is 512, so GPIO17 is
512 + 17 = 529; check the base in the output above, since it can change between kernels.

GPIO chip base: 512

Sysfs number for GPIO17: 529

Wiring: header pin 11 (GPIO17) to pin 6 (GND). GPIO17 is free and already an input,
so no pin muxing is needed.

```text
cd /sys/class/gpio
echo 529 > export
echo in > gpio529/direction
cat gpio529/value          # 0: connected to GND
```

Move the wire from pin 6 to pin 1 or 17 (3.3 V; never pins 2 or 4, which are 5 V):

```text
cat gpio529/value          # 1: connected to 3.3 V
cat /sys/kernel/debug/gpio      # now shows the line in use, as an input
echo 529 > unexport
```

### Driving LEDs

Kernel options: `CONFIG_LEDS_CLASS=y`, `CONFIG_LEDS_GPIO=y`, `CONFIG_LEDS_TRIGGER_TIMER=y`.

The Pi's green activity LED is `ACT` (heartbeat trigger by default in mainline). The red
`PWR` LED needs `CONFIG_GPIO_RASPBERRYPI_EXP`.

```text
ls /sys/class/leds
cd /sys/class/leds/ACT
cat trigger
echo none > trigger
echo 1 > brightness
echo 0 > brightness
echo timer > trigger
echo 10 > delay_on
echo 200 > delay_off
```

### Managing the I2C buses and devices

#### Enabling an I2C bus

The Pi's I2C1 (pins 3 and 5) is already enabled by the mainline device tree. To
practise enabling a bus as the lab does, I use I2C3 (GPIO4 = SDA3 on pin 7, GPIO5 =
SCL3 on pin 29), which `bcm2711.dtsi` defines but leaves disabled.

```text
i2cdetect -l
ls -l /sys/bus/i2c/devices/i2c-*
```

Linux bus numbers don't match the datasheet, so buses are identified by register
address. BCM2711 I2C ("BSC") controllers, as the ARM sees them: I2C0 `fe205000`,
I2C1 `fe804000`, I2C3 `fe205600`, I2C4 `fe205800`, I2C5 `fe205a00`, I2C6 `fe205c00`
(I2C2 and I2C7 are for HDMI).

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/kernel/linux
grep -n 'i2c@' arch/arm/boot/dts/broadcom/bcm2711.dtsi
grep -n 'i2c3_gpio' arch/arm/boot/dts/broadcom/bcm2711.dtsi
```

#### Customizing the Device Tree

A new device tree that includes the board's one, instead of editing a file the kernel
maintainers own. The final version is
[configs/bcm2711-rpi-4-b-custom.dts](configs/bcm2711-rpi-4-b-custom.dts); I built it
up step by step. The first version only has `status = "okay"` in `&i2c3`.

```bash
cp ~/embedded-linux/notes/configs/bcm2711-rpi-4-b-custom.dts arch/arm64/boot/dts/broadcom/
```

Line added to `arch/arm64/boot/dts/broadcom/Makefile`:

```make
dtb-$(CONFIG_ARCH_BCM2835) += bcm2711-rpi-4-b-custom.dtb
```

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux- dtbs
sudo cp arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b-custom.dtb /var/lib/tftpboot/bcm2711-rpi-4-b.dtb
```

I copy it under the stock name, because my `bootcmd` loads `bcm2711-rpi-4-b.dtb`.

After a reboot, `i2cdetect -l` lists one more bus. It shows the controller's bus address
(`7e205600`; the ARM sees it at `fe205600`):

```text
i2c-2   i2c   bcm2835 (i2c@7e205600)   I2C adapter
```

```text
i2cdetect -l
i2cdetect -r 2
```

#### Pin muxing

On the Pi, `pins` lists GPIO numbers and `function` selects the ALTn mode: ALT5 on
GPIO4/5 is SDA3/SCL3 (BCM2711 datasheet, "Alternative Function Assignments").
`bcm2711.dtsi` already has that group as `i2c3_gpio4`, so `&i2c3` references it with
`pinctrl-names` and `pinctrl-0`.

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux- dtbs
sudo cp arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b-custom.dtb /var/lib/tftpboot/bcm2711-rpi-4-b.dtb
```

```text
i2cdetect -r 2       # now answers, with an empty bus
```

#### Adding and enabling an I2C device

BNO085 (CEVA 9-axis IMU) breakout, in its default I2C mode:

| BNO085 pin | Pi pin |
|---|---|
| VIN | Pin 1 (3.3 V, never 2 or 4) |
| GND | Pin 6 |
| SDA | Pin 7 (GPIO4) |
| SCL | Pin 29 (GPIO5) |

```text
i2cdetect -r 2
```

Address detected: `0x4B`

### Plugging a USB audio headset

I use my USB DAC, a FiiO RETRO NANO:

```text
lsusb                           # before plugging in
lsusb                           # after: one more device
ls /sys/bus/usb/devices/
```

I detected my device:

```text
Bus 001 Device 006: ID 0a12:4007 FiiO RETRO NANO
```

On the Pi, USB goes through the VL805 hub (`2109:3431`). The kernel message is
`usb 1-1.1.1: new full-speed USB device ... using xhci_hcd`, so the device is in
`/sys/bus/usb/devices/1-1.1.1`:

```text
cd /sys/bus/usb/devices/1-1.1.1
cat idVendor idProduct manufacturer product
```

There is no sound card yet, because no driver is bound to the device:

```text
cat /proc/asound/cards
```

It prints `--- no soundcards ---`.

### Enabling, installing and using in-tree kernel modules

`CONFIG_SND_USB_AUDIO` is not in the arm64 defconfig, so I set it to `m` myself (it
depends on `CONFIG_SND_USB`).

Export `ARCH` and `CROSS_COMPILE` in the terminal first, and check them:

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-
env | grep -E 'ARCH|CROSS_COMPILE'
```

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/kernel/linux
grep CONFIG_SND_USB_AUDIO .config # Edit if not set to module
make -j$(nproc) Image modules
make INSTALL_MOD_PATH=$HOME/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot INSTALL_MOD_STRIP=1 modules_install
sudo cp arch/arm64/boot/Image /var/lib/tftpboot/
```

The kernel version is now `6.6.161-dirty` (uncommitted changes in the tree), and the
running kernel must match the `/lib/modules/6.6.161-dirty` directory, hence the new `Image`.
The arm64 defconfig builds hundreds of modules: `INSTALL_MOD_STRIP=1` keeps them small.

On the board:

```text
modprobe snd-usb-audio
lsmod
ls /sys/bus/usb/drivers/snd-usb-audio
cat /proc/asound/cards
ls /dev/snd
```

Then add `modprobe snd-usb-audio` to `/etc/init.d/rcS` so it loads at every boot.

### Compiling and installing an out-of-tree kernel module

Mainline Linux has no BNO085 driver (the in-tree `bno055` driver is for a different
chip), so in place of the lab's Nunchuk driver I wrote a minimal one:
[tools/bno085-driver](tools/bno085-driver/). It binds to `compatible = "ceva,bno085"`
and, in `probe()`, reads one packet header to prove the chip answers.

```bash
cd ~/embedded-linux/notes/tools/bno085-driver
make -C $HOME/embedded-linux/embedded-linux-bbb-labs/kernel/linux M=$PWD
make -C $HOME/embedded-linux/embedded-linux-bbb-labs/kernel/linux M=$PWD INSTALL_MOD_PATH=$HOME/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot modules_install
```

`-C` uses the kernel's top-level Makefile; `M=$PWD` builds the module in the current
directory. It installs under `lib/modules/<version>/updates/`. The module must be
built against the exact kernel that runs on the board, or `modprobe` refuses it. (The
driver's Makefile wraps these: `make`, then
`make install INSTALL_MOD_PATH=<nfsroot>`.)

```text
modprobe bno085                 # "loading out-of-tree module taints kernel"
i2cdetect -r 2                  # 4b seen, but not UU: no driver bound
ls /sys/bus/i2c/drivers/bno085  # no device link yet
```

The driver is loaded, but the kernel doesn't know the device exists until the device
tree describes it.

### Declaring an I2C device

The device is the `imu@4b` node inside `&i2c3`, with `clock-frequency` added to the bus.

- `clock-frequency`: run the bus at 100 kHz.
- `compatible` must match one of the driver's strings, or it never probes the device.
  `ceva` isn't a registered vendor prefix; that only matters for upstreaming.
- `reg` is the device's I2C address; the node name's `@4b` must match it.
- The node name describes what the device is (`imu`), not which chip.

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux- dtbs
sudo cp arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b-custom.dtb /var/lib/tftpboot/bcm2711-rpi-4-b.dtb
```

After a reboot:

```text
modprobe bno085
ls -l /sys/bus/i2c/drivers/bno085       # now links to 2-004b
cat /sys/bus/i2c/devices/2-004b/modalias
i2cdetect -r 2                          # now UU
dmesg | grep bno085                     # "BNO085 found: ...-byte packet on channel 0"
rmmod bno085                            # "BNO085 removed"
```

### Setting the board's model name

The `model` property in the root node (`/`) overrides the board's name. The new name
appears in the first kernel messages and in:

```text
cat /sys/firmware/devicetree/base/model
```

**This doesn't work on the Pi.** The `.dtb` has my name: at the U-Boot prompt, after
`tftp`, `fdt print / model` shows `Raspberry Pi 4 media player`. But the kernel boots
with `Raspberry Pi 4 Model B Rev 1.4`, in `/sys/firmware/devicetree/base/model` and in
`dmesg`. U-Boot's `update_fdt_from_fw()` in `board/raspberrypi/rpi/rpi.c` copies the root
`model` from the firmware's device tree into the loaded one ("the firmware provides a
more precise model"), so a `.dts` can't change it. My `imu@4b` node does show up in the
running tree, so the custom `.dtb` is the one in use.

### Committing kernel tree changes

```bash
git checkout -b bootlin-labs
git add arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b-custom.dts arch/arm64/boot/dts/broadcom/Makefile
git commit -as -m "Custom DTS for Bootlin lab"
git format-patch stable/linux-6.6.y          # 0001-Custom-DTS-for-Bootlin-lab.patch
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-
make -j$(nproc) Image modules
make INSTALL_MOD_PATH=$HOME/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot INSTALL_MOD_STRIP=1 modules_install
sudo cp arch/arm64/boot/Image /var/lib/tftpboot/
```

The commit changes the kernel version, from `6.6.161-dirty` to
`6.6.161-00001-g450271f5a639`, so the modules go to a new directory. Check with
`uname -r` that the board runs the new kernel, then remove the old `-dirty` directory
from `nfsroot/lib/modules` to save space.

## Concepts

- **/dev and device files:**
  - An important UNIX design decision was to represent most system objects as files.
    Devices are represented by device files.
  - The kernel gives applications access to hardware devices through device files in `/dev`.
  - There are two main kinds of devices. Block: a device made of fixed-size blocks that
    can be read and written in any order, such as a hard disk or an SD card. Character:
    originally an unbounded stream of bytes, such as a serial port, a terminal or a sound
    card.
- **sysfs (/sys) and debugfs:**
  - sysfs is the kernel's view of the buses, devices and drivers in the system.
  - sysfs is mounted on `/sys`: `mount -t sysfs nodev /sys`.
  - All devices are visible in `/sys`.
  - debugfs is mounted on `/sys/kernel/debug` and contains debug information from the
    kernel.
- **GPIO numbering (chip base + line):**
  - A pin has three different numbers: the header pin (11), the SoC's GPIO number
    (GPIO17), and the kernel's global number (529). Wiring uses the first, the datasheet
    and device tree use the second, and the sysfs interface uses the third.
  - Each GPIO controller (`gpiochip`) numbers its own lines from 0. On the Pi, `gpiochip0`
    is the BCM2711's controller, so GPIO17 is line 17 of `gpiochip0`.
  - The kernel gives each chip a base in one global numbering:
    global number = chip base + line. Recent kernels start dynamic bases at 512, so
    GPIO17 is 512 + 17 = 529 (the BeagleBone's GPIO1_28 is 512 + 28 = 540 the same way).
  - The base depends on the kernel version and on the order the chips are registered, so
    don't hard-code it: read it from `/sys/kernel/debug/gpio` or
    `/sys/class/gpio/gpiochip*/base`.
  - `/sys/class/gpio` is the old, deprecated interface. Its replacement, the character
    device `/dev/gpiochip0` (used by the `gpiod` tools, such as `gpioget gpiochip0 17`),
    uses the chip and the line directly, with no global number.
- **LED class and triggers:**
  - The LED class (`CONFIG_LEDS_CLASS`) gives every LED the same interface,
    whatever drives it: a directory in `/sys/class/leds/<name>`. The name comes from the
    device tree (the `label` property, or `function` and `color`).
  - Drivers such as `leds-gpio` (`CONFIG_LEDS_GPIO`) register LEDs with the class. On the
    Pi, the `ACT` LED is a `gpio-leds` node in the device tree, so the GPIO is driven
    through the LED class rather than the GPIO interface.
  - `brightness` sets the LED by hand: `0` is off and any value up to `max_brightness` is
    on (a GPIO LED has `max_brightness` 1).
  - A trigger is a kernel event source that drives the LED automatically: `heartbeat`,
    `timer`, `mmc0` (SD card activity), `default-on`, and so on. `cat trigger` lists
    them, with the active one in brackets. `none` gives control back to `brightness`.
  - Each trigger is a separate kernel option (`CONFIG_LEDS_TRIGGER_*`), and some add
    their own files when selected: `timer` adds `delay_on` and `delay_off`, in
    milliseconds.
  - The default trigger comes from the device tree's `linux,default-trigger` property.
- **Device tree includes, overrides and `status`:**
  - A `.dts` can `#include` another one. The C preprocessor handles it before `dtc`
    compiles the result into a `.dtb`. My custom file includes the board's
    `bcm2711-rpi-4-b.dts`, so I don't edit files the kernel maintainers own. A `.dtsi` is
    a fragment meant to be included, such as `bcm2711.dtsi`.
  - `&i2c3 { ... }` refers to the node with the label `i2c3` and merges my properties into
    it: new properties are added, and existing ones (such as `status`) are replaced.
    Everything I don't mention stays as it was.
  - `status = "disabled"` (the default for I2C3 in `bcm2711.dtsi`) makes the kernel skip
    the node. `status = "okay"` enables it, which is why the new bus appeared.
  - A change to the `.dts` only takes effect after rebuilding the `.dtb`, copying it to the
    TFTP folder and rebooting. The `dtb-$(CONFIG_ARCH_BCM2835) += ...` line in the
    directory's Makefile adds the `.dtb` to the build when the option is `y`.
  - The loaded tree isn't always the final one: U-Boot patches it before booting the
    kernel (for example, it overwrites `model` on the Pi).
- **Pin muxing (pinctrl):**
  - A SoC pin can serve several functions. Pin muxing selects which one. On the BCM2711
    each GPIO has alternate functions ALT0 to ALT5. GPIO4 and GPIO5 are plain GPIOs by
    default, and ALT5 makes them SDA3 and SCL3.
  - In the device tree, a pin group in the pin controller node gives the pins and the
    function (`i2c3_gpio4` in `bcm2711.dtsi`). A device uses it with `pinctrl-names` and
    `pinctrl-0`, and the kernel applies it when the device probes.
  - Without the muxing, the bus was enabled but SDA and SCL weren't connected to it, so
    nothing could answer.
  - GPIO4 and GPIO5 have no pull-up resistors on the board (GPIO2 and GPIO3 do). I2C needs
    pull-ups, so some breakouts have their own.
- **I2C buses and device addresses:**
  - I2C uses two wires, SDA (data) and SCL (clock). The controller drives the clock, and
    many devices share the same two wires. Each device has a 7-bit address, and the
    controller addresses one at a time.
  - Linux names the buses `i2c-N` in the order they are registered, which doesn't match
    the datasheet. I identify a bus by its controller's register address (`fe205600` for
    I2C3, which was `i2c-2` here).
  - `i2cdetect -r N` scans a bus. `--` means no answer, an address means a device answered
    and no driver uses it, and `UU` means a driver is bound to it.
  - Devices don't announce themselves, unlike USB ones, so they have to be declared in the
    device tree, with their address in `reg`. In sysfs they appear as
    `/sys/bus/i2c/devices/<bus>-<address>`, such as `2-004b`.
  - The bus speed (`clock-frequency`, 100 kHz) is a property of the bus node, not of the
    device.
- **`compatible` and driver binding:**
  - A device tree node has a `compatible` string (`vendor,device`). A driver lists the
    strings it supports in its `of_match_table`. When a node and a driver match, the kernel
    calls the driver's `probe()` function.
  - If no driver lists the string, the device is never probed. The match works whether the
    module is loaded before or after the node exists, since it happens on both events.
  - `MODULE_DEVICE_TABLE` puts the IDs in the module's aliases, so `modprobe` can find the
    module from a device's `modalias`.
  - A successful bind shows as a link in `/sys/bus/i2c/drivers/bno085/` and `UU` in
    `i2cdetect`.
- **In-tree and out-of-tree modules, `modules_install`:**
  - A module is kernel code loaded at run time (a `.ko` file, option `=m`). An in-tree
    module is in the kernel sources and is selected with Kconfig (`CONFIG_SND_USB_AUDIO=m`),
    and `make modules` builds it. An out-of-tree module lives in its own folder and is built
    against the kernel tree with `make -C <kernel tree> M=$PWD`.
  - Loading an out-of-tree module prints "taints kernel": the kernel is flagged as running
    code that isn't part of it. The module still works.
  - `make modules_install` copies the `.ko` files to `<INSTALL_MOD_PATH>/lib/modules/<version>/`
    (out-of-tree ones go to `updates/`) and builds the index files, such as `modules.dep`.
    `INSTALL_MOD_PATH` is the root of the target's root filesystem, and `INSTALL_MOD_STRIP=1`
    strips the debug symbols.
  - `modprobe` uses the index to load a module with its dependencies. `insmod` loads a
    single file by path and doesn't resolve dependencies.
  - A module must be built for the exact kernel that runs on the board, or it is refused.
- **Kernel version suffixes (`-dirty`, commit count and hash):**
  - The kernel's version string (`make kernelrelease`, `uname -r`) is the base version
    plus a suffix from git, added by `scripts/setlocalversion`.
  - `-dirty` means the tree has uncommitted changes to tracked files. After a commit, the
    suffix is `-<commits since the last tag>-g<hash>`, such as `6.6.161-00001-g450271f5a639`.
  - The modules directory name is the version string, and it must equal `uname -r`. Every
    commit changes the version, so I rebuild, reinstall the modules and copy the new
    `Image`. Then I remove the old directory from `nfsroot/lib/modules`.
  - Creating a branch doesn't change the version, only commits do.

[Back to the project overview](README.md)
