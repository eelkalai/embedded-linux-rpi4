# Lab 3: Fetching Linux kernel sources

Bootlin "Embedded Linux system development" labs, BeagleBone Black variant,
adapted to a **Raspberry Pi 4 Model B**.

**Result:** <!-- One sentence: what you have at the end of the lab. -->

## Pi 4 differences

None for this lab: it only downloads the kernel source with git.
The board-specific work starts in the next lab (cross-compiling the kernel).

## Steps

### Setup

```bash
mkdir -p ~/embedded-linux-bbb-labs/kernel
cd ~/embedded-linux-bbb-labs/kernel
```

<!-- Note: free disk space before starting (df -h ~), and where the clone lives. -->

### Cloning the mainline tree

```bash
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux
cd linux
```

<!-- Note: how long it took, how big it is (du -sh .git), anything that went wrong. -->

### Adding the stable tree

```bash
git remote add stable https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux
git fetch stable
git branch -a | grep 6.6
```

<!-- Note: what git remote -v shows, and that stable/linux-6.6.y is available. -->

## Problems and fixes

<!-- For each problem:

### Problem: <short title>

```
<the exact error message>
```

Cause: <why it happened>

Fix: <what solved it, with the command>
-->

## Concepts

<!-- In your own words, a line or two each: -->

- **Mainline tree:**
- **Stable tree and LTS releases:**
- **Why the labs use 6.6:**
- **git remote, fetch, branch:**

## Questions for later

<!-- Anything unclear to come back to. -->
