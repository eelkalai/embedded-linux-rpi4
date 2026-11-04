# Lab 3: Fetching Linux kernel sources

**Goal:** get the kernel sources with git from the official mainline tree, then add the
stable tree as a second remote to get the stable release branches.

**Result:** a clone of the mainline Linux repository, with the stable repository added
as the remote `stable`.

No Pi 4 differences in this lab.

## Steps

### Setup

```bash
mkdir -p ~/embedded-linux/embedded-linux-bbb-labs/kernel
cd ~/embedded-linux/embedded-linux-bbb-labs/kernel
```

### Cloning the mainline tree

```bash
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux
cd linux
```

### Adding the stable tree

```bash
git remote add stable https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux
git fetch stable
```

## Concepts

- **Mainline tree:** Linus Torvalds's repository; it contains the main Linux releases.
- **Stable tree:** maintained by a separate team and hosted in a separate repository;
  it takes released versions and adds bug and security fixes.
- **git remote, fetch and remote branches:**
  - `git remote add stable <url>` saves a second repository address under the name
    `stable`. Nothing is downloaded yet.
  - `git fetch stable` downloads what my repository is missing from it, without
    changing my files or my own branches.
  - The remote's branches appear as `stable/<branch>` (for example
    `stable/linux-6.6.y`), a read-only view of the remote. Tags, such as `v6.4.16`,
    keep their plain names.
  - The fetch skipped everything shared with the mainline. Git recognises shared
    commits by their IDs, which are hashes of their contents, so only the stable fixes
    were downloaded.

[Back to the project overview](README.md)
