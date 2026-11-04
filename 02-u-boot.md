# Lab 2: Bootloader (U-Boot)

**Goal:** get a serial console on the board, then build and boot my own U-Boot,
and use it to load files from my PC over the network.

**Result:** my own 64-bit U-Boot v2024.04 boots from an SD card I partitioned and
formatted myself, prints to a serial console, saves its environment, and
downloads files from my PC over TFTP.

## How the Pi 4 boots (vs. the BeagleBone)

| Step | BeagleBone Black (lab) | Raspberry Pi 4 |
|---|---|---|
| First code | Boot ROM in the AM335x | Boot ROM in the BCM2711 (run by the VideoCore GPU) |
| Sets up RAM | U-Boot SPL (`MLO`), in on-chip SRAM | EEPROM bootloader on the board, then GPU firmware `start4.elf` |
| Boot source chosen by | SYSBOOT pins / S2 button | `BOOT_ORDER` in the EEPROM (SD first by default) |
| Bootloader | `u-boot.img` | `u-boot.bin`, started via `kernel=` in `config.txt` |
| Device tree for U-Boot | Built into U-Boot (`DEVICE_TREE=`) | Passed by the firmware (`devicetree: board`) |

So on the Pi there is no SPL and no `MLO`: the closed-source firmware does the
first stages, and U-Boot is a single file.

## Serial console

No USB-serial cable, so I used an **ESP32-C3 Super Mini** as a USB-to-UART bridge.
It has no separate USB-serial chip (USB goes straight into the C3), so the bridge is
a small sketch that copies bytes between the USB serial port (USB CDC) and a hardware
UART at 115200 baud, using 4 KB receive buffers so fast boot output isn't dropped.

The PlatformIO project is in [tools/esp32-uart-bridge](tools/esp32-uart-bridge/).
It needs USB CDC enabled at boot (`-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`
in `platformio.ini`).

| ESP32-C3 | Pi 4 header |
|---|---|
| GPIO3 (TX) | Pin 10 (GPIO15, RX) |
| GPIO4 (RX) | Pin 8 (GPIO14, TX) |
| GND | Pin 6 (GND) |

No power wire between the boards; both 3.3 V logic.

On the PC the bridge is `/dev/ttyACM0`:

```bash
picocom -b 115200 /dev/ttyACM0     # exit: Ctrl-A then Ctrl-X
```

- Permission: my user had to be in the `dialout` group. On Bazzite:
  `grep -E '^dialout:' /usr/lib/group | sudo tee -a /etc/group`, then
  `sudo usermod -aG dialout $USER` and a reboot.
- Loopback test before wiring the Pi: jumper GPIO4 to GPIO3 and type; characters echo back.
- Only the ESP32-to-Pi wire has real bit timing (115200 baud, set in the sketch). The
  PC-to-ESP32 side is USB (CDC), where picocom's `-b` value has no effect. I still pass
  `-b 115200` so the same command works with a normal USB-serial adapter.
- Finding the device: `lsusb` (Espressif is `303a`), `ls /dev/ttyACM*`,
  `ls -l /dev/serial/by-id/`, `sudo dmesg -w`. A charge-only USB cable shows nothing at all.

### Checking the serial path with Raspberry Pi OS first

Before building anything, I booted Raspberry Pi OS Lite (64-bit, written with
Raspberry Pi Imager) with this added to `config.txt`:

```ini
enable_uart=1
dtoverlay=disable-bt
```

It gave a login prompt on `ttyAMA0` over the bridge, proving the wiring and sketch.
The kernel log also showed `hwmon hwmon1: Undervoltage detected!`: check the power
supply (5 V, 3 A, short cable). `vcgencmd get_throttled` should print `0x0`.

## Compiling U-Boot

```bash
git clone https://git.u-boot-project.org/u-boot/u-boot.git
cd u-boot
git checkout v2024.04
sudo apt install libssl-dev device-tree-compiler swig python3-dev python3-setuptools

export CROSS_COMPILE=aarch64-linux-
make rpi_4_defconfig               # 64-bit Pi 4 (rpi_4_32b_defconfig is the 32-bit one)
make -j$(nproc)                    # result: u-boot.bin
```

No `DEVICE_TREE=` step: the firmware hands U-Boot the device tree.

### Problem: apt 404 when installing packages

```text
E: Failed to fetch .../zlib1g-dev_1.3.dfsg-3.1ubuntu2.2_amd64.deb  404  Not Found
```

Cause: stale package index. Fix: `sudo apt update` (and `sudo apt upgrade` if a `-dev`
package must match an installed library), then install again.

### Problem: the build used the PC's own compiler

```text
cc1: warning: unknown register name: x18
cc1: error: bad value 'armv8-a+crc' for '-march=' switch
cc1: note: valid arguments to '-march=' switch are: nocona core2 ... znver4 ...
```

The list of valid values is x86 processors, so `make` was running the native `gcc`.
`CROSS_COMPILE` has to be **exported** (visible to `make`), not just set in the shell,
and the toolchain must be on `PATH`:

```bash
env | grep CROSS_COMPILE           # only shows exported variables
which aarch64-linux-gcc
```

Then start the configuration over, because `.config` records the compiler it tested:

```bash
make distclean
make rpi_4_defconfig
make -j$(nproc)
```

## Preparing the SD card (on the Bazzite host)

The card is in a USB reader, so it appears as `/dev/sdX`, not `/dev/mmcblk0`.
In my case `/dev/sdd` (29.8G). Always check with `lsblk` immediately before
`dd`, `cfdisk` or `mkfs`; my USB drive is also an `sdX` device.

```bash
lsblk -o NAME,SIZE,TYPE,TRAN,RM,MODEL
sudo umount /dev/sdd1 /dev/sdd2            # unmount each partition, not /dev/sdd
sudo dd if=/dev/zero of=/dev/sdd bs=1M count=16   # wipe the partition table
sudo blockdev --rereadpt /dev/sdd          # or remove and reinsert the card
sudo cfdisk /dev/sdd
```

In cfdisk: label `dos` (MBR), one **primary** partition, type `c` (W95 FAT32 (LBA)),
bootable flag. I made it larger than the lab's 64 MB so the 64-bit kernel fits later.
Note: typing `256MB` means decimal megabytes (shows as 244M); `256M` means MiB.

```bash
sudo mkfs.vfat -a -F 32 -n boot /dev/sdd1  # warning about lowercase labels is harmless
udisksctl mount -b /dev/sdd1               # mounts at /run/media/$USER/boot on Bazzite
```

### Files on the boot partition

The Pi firmware files come from the [raspberrypi/firmware](https://github.com/raspberrypi/firmware) repository (`boot/`):

```bash
mkdir -p ~/embedded-linux/rpi-firmware && cd ~/embedded-linux/rpi-firmware
for f in start4.elf fixup4.dat bcm2711-rpi-4-b.dtb; do
  curl -LO https://github.com/raspberrypi/firmware/raw/master/boot/$f
done

B=/run/media/eelkalai/boot
ls "${B:?}"                                # check the mount before copying
cp ~/embedded-linux/rpi-firmware/* "$B"/
cp ~/embedded-linux/u-boot/u-boot.bin "$B"/
printf 'arm_64bit=1\nenable_uart=1\nkernel=u-boot.bin\n' > "$B"/config.txt
sudo umount "$B"
```

| File | Read by | Purpose |
|---|---|---|
| `start4.elf` | EEPROM bootloader | GPU firmware: sets up RAM, reads `config.txt`, starts the ARM cores |
| `fixup4.dat` | `start4.elf` | Memory split between GPU and ARM; must match `start4.elf` |
| `config.txt` | `start4.elf` | `arm_64bit=1` (64-bit mode), `enable_uart=1` (serial on pins 8/10), `kernel=u-boot.bin` |
| `bcm2711-rpi-4-b.dtb` | `start4.elf` | Device tree passed to U-Boot |
| `u-boot.bin` | `start4.elf` | My bootloader |

Mistake to avoid: with `B` unset, `cp ... $B/` targets `/` ("Read-only file system").
`"${B:?}"` makes the shell stop instead.

## First boot

```text
U-Boot 2024.04 (Oct 29 2026 - 18:14:11 +0200)

DRAM:  948 MiB (effective 7.9 GiB)
RPI 4 Model B (0xd03114)
Core:  213 devices, 16 uclasses, devicetree: board
MMC:   mmcnr@7e300000: 1, mmc@7e340000: 0
Loading Environment from FAT... Unable to read "uboot.env" from mmc0:1...
In:    serial,usbkbd
Out:   serial,vidconsole
Err:   serial,vidconsole
Net:   eth0: ethernet@7d580000
...
Hit any key to stop autoboot:  0
```

- `mmc 0` is the SD card; `mmc 1` is the Wi-Fi chip (SDIO).
- `Unable to read "uboot.env"` is normal until the first `saveenv`.
- `0xd03114` is the revision code: 8 GB, Sony UK, BCM2711, Pi 4B, rev 1.4.
- Without a key press, autoboot tries the Wi-Fi "card" and network boot and prints
  harmless errors (`Card did not respond to voltage select`, PHY timeout without a cable).
- No `CPU :` / `Model:` lines as on the BeagleBone: those come from TI's board code.

At the U-Boot prompt it is safe to just unplug the power (nothing writes in the background).
`reset` restarts.

## Adding a command

```bash
make menuconfig                    # enable the "config" command (CONFIG_CMD_CONFIG)
grep CMD_CONFIG .config
make -j$(nproc)
# copy the new u-boot.bin to the boot partition
```

On the board, `config` prints the full build configuration. To see only my changes:

```bash
make savedefconfig
diff configs/rpi_4_defconfig defconfig
```

## Environment

```text
U-Boot> printenv
U-Boot> setenv foo bar             # lost on reset
U-Boot> saveenv                    # writes uboot.env on the FAT partition (env info)
U-Boot> env default -a
U-Boot> saveenv
```

`setenv` = `env set`, `saveenv` = `env save`; `editenv` edits a value in place.

## Networking and TFTP

The lab uses Ethernet over USB (BeagleBone only). The Pi has a real Ethernet port;
I connected it to my home router, so nothing had to be configured on the PC side.

| | Address |
|---|---|
| PC (`enp7s0`, from `ip -4 addr`) | 192.168.1.195 |
| Pi in U-Boot | 192.168.1.200 |

```text
U-Boot> setenv ipaddr 192.168.1.200
U-Boot> setenv serverip 192.168.1.195
U-Boot> saveenv
U-Boot> ping ${serverip}
host 192.168.1.195 is alive
```

(`setenv autoload no; dhcp` would get an address from the router instead.)
Pinging the Pi from the PC doesn't work: U-Boot only uses the network during its commands.

### TFTP server on the host, not in the container

`tftpd-hpa` installed with `apt` in the container exits silently: a rootless container
can't run the service or bind port 69. The server runs on Bazzite:

```bash
rpm -q tftp-server || sudo rpm-ostree install tftp-server   # then reboot
sudo systemctl enable --now tftp.socket
sudo firewall-cmd --permanent --add-service=tftp
sudo firewall-cmd --reload
```

My test file is `FirstTFTP.txt`, kept in this repository at `tests/FirstTFTP.txt`.
Files must be in `/var/lib/tftpboot` (copies, not links):

```bash
sudo cp ~/embedded-linux/notes/tests/FirstTFTP.txt /var/lib/tftpboot/
```

Load it into RAM with the Pi's address variable, not the lab's BeagleBone address
`0x81000000`, then display that memory:

```text
U-Boot> printenv kernel_addr_r     # kernel_addr_r=0x00080000
U-Boot> tftp ${kernel_addr_r} FirstTFTP.txt
Using ethernet@7d580000 device
TFTP from server 192.168.1.195; our IP address is 192.168.1.200
Filename 'FirstTFTP.txt'.
Load address: 0x80000
U-Boot> md ${kernel_addr_r}        # the file's text appears in the right-hand column
```

### Problem: `TFTP error: 'Permission denied'`

The request reached the server, which refused the file. The two usual causes on Bazzite:

```bash
ls -lZ /var/lib/tftpboot/
sudo chmod a+r /var/lib/tftpboot/*          # must be readable by everyone
sudo restorecon -Rv /var/lib/tftpboot       # SELinux label must be tftpdir_t
```

A file moved in with `mv` keeps its home-folder SELinux label; use `cp`.

## Concepts

- **Boot chain:** boot ROM, then a first stage in on-chip SRAM that initialises DDR,
  then the full bootloader in DDR, then the kernel. Each stage only initialises what it
  needs to load the next; Linux reinitialises the hardware with its own drivers.
- **Partition table vs filesystem:** the MBR (cfdisk's `dos`) says where partitions start
  and end plus a type code; `mkfs` writes the actual filesystem inside a partition.
- **Serial console:** works from the first instructions of boot and when everything else
  fails; Ethernet in U-Boot is only for moving files.
- **U-Boot addresses:** `tftp`, `load`, `md` take RAM addresses; numbers are hexadecimal.

[Back to the project overview](README.md)
