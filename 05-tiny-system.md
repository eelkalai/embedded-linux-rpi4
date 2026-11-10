# Lab 5: Tiny embedded system with BusyBox

**Goal:** build a minimal root filesystem by hand, with BusyBox, and boot the Pi on it
over NFS from my PC.

**Result:** after setting up an NFS server on the host, I set `bootargs` so the Pi mounts
its root filesystem from it. The Pi now runs from a folder on my PC, so I can change its
files from the host and the Pi sees the changes right away.

In that folder I built a minimal system by hand: BusyBox for the commands and `init`, an
`inittab` and an `rcS` script to mount `/proc` and `/sys`, and a shell on the serial port.
I then switched BusyBox to shared libraries by adding the musl dynamic loader, and served
a web page from the Pi with BusyBox's `httpd`. As the optional last step, I built the
same root filesystem into the kernel as an initramfs.

## Pi 4 differences

| Lab (BeagleBone) | Pi 4 |
|---|---|
| Network over USB (`usb0`, `g_ether` parameters) | Ethernet (`eth0`), no `g_ether` parameters |
| Board `192.168.0.100`, PC `192.168.0.1` | Board `192.168.1.200`, PC `192.168.1.195` |
| NFS server: `nfs-kernel-server` with `apt` | `nfs-utils` on the Bazzite host (already installed) |
| NFS server starts automatically, no firewall | Start it (`systemctl enable --now nfs-server`), open the firewall, check SELinux (`nfs_export_all_rw`) |
| `/home/<user>/...` in `/etc/exports` and `nfsroot=` | `/var/home/eelkalai/embedded-linux/embedded-linux-bbb-labs/...` |
| `ttyS0::askfirst:/bin/sh` | `ttyS1::askfirst:/bin/sh` |
| `arm-linux-gcc`, `ld-musl-armhf.so.1` | `aarch64-linux-gcc`, `ld-musl-aarch64.so.1` |

## Steps

### Setup

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/tinysystem
```

### Kernel configuration

Check that the kernel from Lab 4 has everything needed to mount the root filesystem
over NFS, with the Ethernet drivers built in (`=y`, not `=m`):

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/kernel/linux
grep -E 'CONFIG_(ROOT_NFS|IP_PNP|NFS_FS|NFS_V3|DEVTMPFS|DEVTMPFS_MOUNT|BCMGENET|BROADCOM_PHY)=' .config
```

### Setting up the NFS server

On the Bazzite host:

```bash
mkdir -p ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot
rpm -q nfs-utils || sudo rpm-ostree install nfs-utils
sudo systemctl enable --now nfs-server
sudo firewall-cmd --permanent --add-service={nfs,nfs3,mountd,rpc-bind}
sudo firewall-cmd --reload
getsebool nfs_export_all_rw
```

Line added to `/etc/exports` (path and options on one line, no space before the
parenthesis):

```text
/var/home/eelkalai/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot 192.168.1.200(rw,no_root_squash,no_subtree_check)
```

```bash
sudo exportfs -r
sudo exportfs -v
```

### Booting the system

`bootargs` already holds `console=ttyS1,115200 earlycon` from Lab 4, so I append to it.
No `g_ether` parameters: the Pi uses its real Ethernet port (`eth0`).

```text
setenv bootargs ${bootargs} root=/dev/nfs ip=192.168.1.200:::::eth0 nfsroot=192.168.1.195:/var/home/eelkalai/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot,nfsvers=3,tcp rw
saveenv
boot
```

Expected: `VFS: Mounted root (nfs filesystem)`, then `devtmpfs: error mounting -2`,
because the kernel mounts devtmpfs on `/dev` and the directory doesn't exist:

```bash
mkdir ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot/dev
```

After a reboot the kernel stops with `No working init found`: the root filesystem is
still empty.

### Root filesystem with BusyBox

```bash
cd ~/embedded-linux/embedded-linux-bbb-labs/tinysystem
git clone git://busybox.net/busybox.git
cd busybox
git checkout 1_37_stable
ls ../data/
cp ../data/<busybox configuration file> .config
export CROSS_COMPILE=aarch64-linux-
make menuconfig
make -j$(nproc)
make install
```

- With GCC 14 or newer on the host, `make menuconfig` fails until the lab's patch is
  applied: `git am ../data/0001-menuconfig-GCC-failing-saying-ncurses-is-not-found.patch`
- In menuconfig: keep the static build, and set Settings → Installation Options →
  Destination path for 'make install' to the `nfsroot` directory.
- Also check Settings → Cross compiler prefix: the provided configuration may point to
  the 32-bit ARM toolchain. Clear it or set it to `aarch64-linux-`.

Booting again gives a shell prompt.

### Virtual filesystems

On the board:

```text
ps                      # fails: /proc does not exist
mkdir /proc /sys /etc
mount -t proc nodev /proc
ps
```

With `/proc` mounted, `halt` can unmount everything cleanly.

### System configuration and startup

The kernel starts `/sbin/init`, which reads `/etc/inittab` (BusyBox example:
`examples/inittab`).

`/etc/inittab`:

```text
::sysinit:/etc/init.d/rcS
::askfirst:/bin/sh
```

`/etc/init.d/rcS` (made executable with `chmod +x`):

```sh
#!/bin/sh
mount -t proc nodev /proc
mount -t sysfs nodev /sys
```

### Starting the shell in a proper terminal

The shell prints `/bin/sh: can't access tty; job control turned off`, because with
nothing before `::` it runs on `/dev/console`. Running it on the real serial port
(`ttyS1` on the Pi, matching `console=`) enables job control (Ctrl-C, Ctrl-Z):

```text
::sysinit:/etc/init.d/rcS
ttyS1::askfirst:/bin/sh
```

### Switching to shared libraries

```bash
aarch64-linux-gcc -o hello hello.c
file hello
```

On the board, `./hello` gives a misleading `not found`: what is missing is the dynamic
loader, `/lib/ld-musl-aarch64.so.1` (with musl, the loader also contains the C library).

```bash
find ~/x-tools/aarch64-training-linux-musl -name ld-musl-aarch64.so.1
mkdir -p ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot/lib
cp <path from find> ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot/lib/
```

Then rebuild BusyBox without the static option, install it again and compare sizes:

| Build | Size |
|---|---|
| BusyBox, static | 386 KB |
| BusyBox, dynamic | 260 KB |
| Dynamic loader + C library (`ld-musl-aarch64.so.1`) | 1000 KB |
| Dynamic total | 1260 KB |

The dynamic BusyBox alone is smaller, but with the library it needs it takes over three
times the space of the static one. The static build only copies in the parts of the
library BusyBox uses; the shared library is complete, for any program. Shared libraries
only save space once several programs use them.

### Implement a web interface for your device

```bash
cp -r ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/data/www ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot/
```

On the board:

```text
/usr/sbin/httpd -h /www/
```

On the PC: `http://192.168.1.200/index.html`

Then add the `httpd` line to `/etc/init.d/rcS` so the server starts at every boot.

### Going further: initramfs booting

Optional in the lab. First prepare `nfsroot`, then build it into the kernel:

1. Add an `init` link at the top of `nfsroot`. From an initramfs, the kernel runs `/init`,
   not `/sbin/init`. The target is relative, because the link is read on the Pi:

   ```bash
   cd ~/embedded-linux/embedded-linux-bbb-labs/tinysystem/nfsroot
   ln -s sbin/init init
   ```

2. Mount devtmpfs from `rcS` (`mount -t devtmpfs nodev /dev`): the kernel doesn't do it
   automatically for an initramfs.
3. Point the kernel's initramfs source (`CONFIG_INITRAMFS_SOURCE`, under General setup)
   at the absolute path of `nfsroot`, rebuild the `Image` (in the distrobox) and copy it
   to `/var/lib/tftpboot` (in a host terminal). The files are copied into the `Image` at
   build time, so every change in `nfsroot` needs a rebuild.
4. `root=` is ignored when an initramfs is present, so `bootargs` stays the same.
   Go back to NFS booting afterwards, and don't save this configuration to
   `configs/linux-rpi4_defconfig`.
5. Reboot the Pi so U-Boot loads the new `Image`, then check with `mount` (`/` should be
   `rootfs`, not `nfs`) and `dmesg | grep 'as init'` (`Run /init as init process`).

`Image` size, without initramfs:

`Image` size, with initramfs:

`mount` output:

User/group ID mapped to root (`INITRAMFS_ROOT_UID`/`GID`):

## Problems and fixes

### Unable to mount the root filesystem

```text
[    6.406056]      bootserver=255.255.255.255, rootserver=192.168.1.195, rootpath=
...
[  107.664480] VFS: Unable to mount root fs via NFS.
[  107.696113] Kernel panic - not syncing: No working init found.
```

The Pi's network was fine (`eth0: Link is Up`, `IP-Config: Complete` at 6.4 s), but the
kernel then waited about 100 seconds with no messages before giving up: its NFS requests
got no answer at all.

`systemctl status rpcbind.socket nfs-mountd` showed `rpcbind` working but `mountd` stopped
since the moment the server was started:

```text
● rpcbind.socket - RPCbind Server Activation Socket
     Active: active (running) since Mon 2026-11-09 05:10:48 IST; 44min ago
             0.0.0.0:111 (Stream)

○ nfs-mountd.service - NFS Mount Daemon
     Active: inactive (dead) since Mon 2026-11-09 05:10:48 IST; 44min ago
   Duration: 500us
Nov 09 05:10:48 bazzite rpc.mountd[176546]: couldn't open /var/lib/nfs/etab
```

**What is `etab`?** From `man rpc.mountd`:

> Mountd uses a table of information stored in /var/lib/nfs/etab and maintained by
> exportfs(8), possibly based on the contents of exports(5), to respond to each request.

```text
/etc/exports  →  sudo exportfs -r  →  /var/lib/nfs/etab  →  mountd answers the Pi
```

**Fix:** create `etab`, then restart the server so `mountd` starts with it:

```bash
sudo exportfs -r
cat /var/lib/nfs/etab
sudo systemctl restart nfs-server
systemctl status nfs-mountd
rpcinfo -p | grep mountd
```

Lesson: on a first setup, run `sudo exportfs -r` before starting the NFS server, or restart
the server after it.

### Kernel configuration turned into x86

When I came back to the kernel, I ran a configuration command before setting
`ARCH=arm64`. The kernel build then assumed my PC's architecture and saved `.config` as
an x86 configuration. The next `make`, with `ARCH=arm64`, asked about every arm64
option as `(NEW)`:

```text
Actions Semi Platforms (ARCH_ACTIONS) [N/y/?] (NEW)
```

The round trip through x86 dropped `ARCH_BCM2835`, and with it every Pi driver that
depends on it (mini UART, PCIe, clocks, SD card, I2C). `.config.old` didn't help: it held
the x86 version.

**Fix:** a fresh `make defconfig` with `ARCH` set, the options from the
[Lab 4](04-kernel-cross-compile.md) table set to `y` again, and a backup:

```bash
make savedefconfig
cp defconfig ~/embedded-linux/notes/configs/linux-rpi4_defconfig
```

Lesson: `export ARCH=arm64` and `export CROSS_COMPILE=aarch64-linux-` first, in every new
terminal. I don't put them in `~/.bashrc`, because the distrobox shares my home folder with
the host. To restore the backup:

```bash
cp ~/embedded-linux/notes/configs/linux-rpi4_defconfig arch/arm64/configs/rpi4_defconfig
make rpi4_defconfig
```

### Copying to the TFTP folder from the distrobox

```text
cp: cannot create regular file '/var/lib/tftpboot/': Not a directory
```

`/var/lib/tftpboot` only exists on the Bazzite host, where the TFTP server runs. Inside the
distrobox the folder is missing, so `cp` tried to create a file with that name. Build in
the distrobox, copy in a host terminal. My home folder is shared, so the paths are the same.

### New kernel not running

After copying a new `Image`, `mount` on the board still showed the root on NFS: the Pi was
still running the old kernel. U-Boot only loads the `Image` at boot, so `reboot` the Pi
after every copy.

## Concepts

- **NFS root filesystem:**
  - NFS (Network File System): the root filesystem stays in a folder on my host and the
    Pi mounts it over the network, which makes development easier.
- **`root=`, `ip=` and `nfsroot=` kernel arguments:**
  - Set in `bootargs`.
  - `root=` tells the kernel where its root filesystem is. `/dev/nfs` is a special value
    meaning "mount it over NFS, as described by `nfsroot=`".
  - `ip=` sets the Pi's own IP address and network device (`eth0`). The IP must match the
    client allowed in the host's `/etc/exports`.
  - `nfsroot=` gives the NFS server's IP and the root filesystem's folder on the server,
    plus mount options (`nfsvers=3,tcp`).
- **devtmpfs:**
  - A virtual filesystem in RAM where the kernel creates the device files by itself, as
    drivers find devices. Mounted on `/dev`.
- **BusyBox:**
  - A toolbox that contains the basic programs a Linux system needs, such as `sh` (`ash`),
    `ls`, `cat`, `mount`, `halt`, `init` and `httpd`.
  - It is a single binary: the commands are links to it, and it runs the command whose
    name it was started under.
- **init and `/etc/inittab`:**
  - The kernel starts `/sbin/init`, which reads `/etc/inittab`. `inittab` lists the
    programs to start and when (`sysinit`, `askfirst`, `respawn`, ...).
  - BusyBox's format is `<tty>::<action>:<process>`: the first field is the terminal to
    run on, and the second field (runlevels in other init systems) is ignored.
- **proc and sysfs:**
  - `/proc` holds information about processes and the kernel. Each running process has a folder named after its PID, such as `/proc/1` for init, and there are system-wide files such as `/proc/cpuinfo`, `/proc/meminfo`, `/proc/mounts` and `/proc/cmdline` (the kernel arguments from U-Boot). Tools like `ps`, `top` and `mount` read from it, so they fail or show nothing without it.
  - `/sys` (`mount -t sysfs nodev /sys`) shows the kernel's view of devices and drivers as a tree: buses, devices, their drivers and their settings, one value per file. Some files can be written to change a setting, for example `echo 1 > /sys/class/leds/<led>/brightness`.
- **Static vs dynamic linking, and the dynamic loader:**
  - A static program holds its libraries in its binary. It's useful for an independent
    program, but because the libraries are part of the binary, it is larger. We used it
    for BusyBox at first because the root filesystem had no dynamic loader yet.
  - Dynamic linking is when a program uses shared libraries on the system, which makes
    it lighter. The libraries are shared by every program that uses them.
  - The dynamic loader is a program that runs before a program's `main`: it loads the
    needed libraries and resolves the symbols. Its path is written inside each dynamic
    program, which is why a missing loader gives a misleading `not found`.

[Back to the project overview](README.md)
