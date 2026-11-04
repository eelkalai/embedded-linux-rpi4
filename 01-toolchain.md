# Lab 1: Building a cross-compiling toolchain

**Goal:** build a compiler that runs on my PC and produces 64-bit ARM code for the
Pi 4, using the musl C library.

**Result:** a 64-bit ARM cross-compiler using the musl C library, built with
Crosstool-NG 1.28.0, installed in `~/x-tools/aarch64-training-linux-musl`,
with the short alias `aarch64-linux-` (for example `aarch64-linux-gcc`).

## Environment

- Host: Bazzite (immutable Fedora-based; no `apt`).
- Lab work runs in an Ubuntu container created with distrobox, named `bootlin`:

  ```bash
  distrobox create --name bootlin --image ubuntu:24.04
  distrobox enter bootlin
  ```

- The container shares my home folder with the host, so lab files are visible
  from both. Packages installed with `apt` live only in the container.

## Building Crosstool-NG

Dependencies (inside the container):

```bash
sudo apt install build-essential git autoconf bison flex texinfo help2man gawk libtool-bin \
    libncurses-dev unzip gettext python3 rsync
```

(The lab says `libncurses5-dev`; `libncurses-dev` is the current Ubuntu name.)

Get a tested release and build it locally, without installing system-wide:

```bash
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
git checkout crosstool-ng-1.28.0   # "detached HEAD" warning is expected: a fixed release tag
./bootstrap                        # generates ./configure (Autotools; not in git)
./configure --enable-local
make
./ct-ng help
```

Where the dependency list comes from: Crosstool-NG's `configure.ac` checks for each
tool, and its `testing/docker` Dockerfiles list the packages per distribution.
`apt-file search <file>` maps a missing program or header to its package.

## Configuring the toolchain for the Pi 4

The lab loads the Cortex-A8 (32-bit BeagleBone) sample. For the Pi 4:

```bash
./ct-ng aarch64-rpi4-linux-gnu
./ct-ng menuconfig
```

| Lab setting | Pi 4 value |
|---|---|
| Try features marked as EXPERIMENTAL | Enabled |
| Use specific FPU = vfpv3, Floating point = hardware | Not present for 64-bit ARM (always has an FPU) |
| Tuple's vendor string | `training` |
| Tuple's alias | `aarch64-linux` (lab: `arm-linux`) |
| Version of linux (kernel headers) | Closest version at or below 6.6 |
| C library | musl |
| gcc | 15.2.0, with C++ enabled |
| Debug facilities | All disabled |

Target options shown by the sample: architecture `arm`, bitness `64-bit`,
CPU `cortex-a72`. Crosstool-NG treats 32- and 64-bit ARM as one architecture and
distinguishes them by bitness.

```bash
./ct-ng build
```

## Testing the toolchain

`PATH` set with `export` lasts only for that terminal. To have the toolchain in every
container shell, I added it to the container's own startup files (the container's
`/etc` is its own, so the host never reads them):

```bash
echo 'export PATH="$HOME/x-tools/aarch64-training-linux-musl/bin:$PATH"' | sudo tee /etc/profile.d/bootlin-toolchain.sh
```

Then, in a new container shell:

```bash
aarch64-linux-gcc -o hello hello.c
file hello
```

Running it on the PC through user-mode emulation, pointing QEMU at the
toolchain's copy of the musl loader:

```bash
sudo apt install qemu-user
qemu-aarch64 -L ~/x-tools/aarch64-training-linux-musl/aarch64-training-linux-musl/sysroot hello
```

Things to remember:

- The program is dynamically linked against **musl** and needs
  `/lib/ld-musl-aarch64.so.1` on the target. Raspberry Pi OS uses glibc, so this
  binary won't start there ("No such file or directory"), unless built with
  `-static`.

## Concepts

- **Toolchain:** compiler, assembler/linker (binutils), C library and kernel headers,
  plus debugger. A *cross* toolchain runs on x86 and produces ARM code.
- **Tuple:** `arch-vendor-os-libc`, here `aarch64-training-linux-musl`. It becomes the
  prefix of every tool name.
- **Kernel headers:** the kernel's user-space interface (system call numbers,
  constants, structures). Must not be newer than the kernel that will run.
- **C library choice:** glibc (largest, most compatible), musl and uClibc-ng (small).
  The run-time copy on the target must match the one in the toolchain's sysroot.

[Back to the project overview](README.md)
