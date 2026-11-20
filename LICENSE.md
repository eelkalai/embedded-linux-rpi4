# License

This repository uses two licenses.

## Notes and configuration files

The lab notes (`*.md`), the files in `configs/` and `tests/` are licensed under the
[Creative Commons Attribution-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-sa/4.0/)
license (CC BY-SA 4.0). The full text is in [LICENSE-CC-BY-SA-4.0.txt](LICENSE-CC-BY-SA-4.0.txt).

These notes follow and adapt the lab instructions of Bootlin's
[Embedded Linux system development](https://bootlin.com/training/embedded-linux/) training,
© Bootlin, licensed under [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
CC BY-SA 3.0 allows adaptations to be shared under a later version of the same license.
Bootlin does not endorse this repository.

## Code

The ESP32-C3 UART bridge in `tools/esp32-uart-bridge/` is licensed under the MIT
License; see [tools/esp32-uart-bridge/LICENSE](tools/esp32-uart-bridge/LICENSE).

The BNO085 kernel driver in `tools/bno085-driver/` is licensed under the GNU General
Public License version 2 (`SPDX-License-Identifier: GPL-2.0`), like the Linux kernel it
builds against. The full text is in the kernel source tree, in `LICENSES/preferred/GPL-2.0`.

The device tree `configs/bcm2711-rpi-4-b-custom.dts` is also licensed under GPL-2.0,
like the kernel device trees it includes, unlike the rest of `configs/`.
