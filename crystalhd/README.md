# crystalhd

The driver for Broadcom's Crystal HD video decoders, the BCM70012 and the BCM70015 ("Flea"), on mini
PCIe cards such as the AzureWave AW-VD920H. An OpenFrame 2 can take one in its mini PCIe slot in
place of the Wi-Fi card. The kernel workflow copies this folder to `drivers/staging/crystalhd` and
builds it as a module (`CONFIG_CRYSTALHD=m`), which udev loads by the card's PCI ID.

Userspace drives the card through `/dev/crystalhd` with libcrystalhd, which also loads the decoder's
firmware. Both, and a player, are in the `openframe-crystalhd` package from
[openframe-linux](https://github.com/birdslikewires/openframe-linux) (its `crystalhd/` folder).

## Where it comes from

The files are from [birdslikewires/crystalhd](https://github.com/birdslikewires/crystalhd) at commit
`67bac2dac7a9056bf9d596b6e0fb5f011781b843`: `driver/linux/*.c` and `*.h`, and the headers they
include from `include/` (`bc_dts_*.h`), `include/link/` (`bcm_70012_regs.h`, `bc_defines.h`) and
`include/flea/` (`bcm_70015_regs.h`, `DriverFwShare.h`). That repository is
[twopoint71/crystalhd](https://github.com/twopoint71/crystalhd) (commit `ec67f08`) with our fixes
as separate commits; it carries on the Broadcom, Jarod Wilson and dbason driver, which left mainline
staging in 2013. `Makefile` and `Kconfig` are ours.

Our changes to those files, each a commit there:

- `crystalhd_dmabuf.c` imports the `DMA_BUF` symbol namespace, which modpost requires since 6.13.
- `crystalhd_misc.c` pins user pages with `pin_user_pages_fast()`. `pin_user_pages()` was called
  without the mmap lock, which warned on every capture buffer.
- `crystalhd_fleafuncs.c`, `crystalhd_lnx.[ch]`: the BCM70015's registers are reached through one
  indirect address/data pair, and its memory through one window register. The interrupt handler and
  the ioctl threads used both without a lock, and on the hyperthreaded Atom they interleave. A
  spinlock (`hwio_lock`) now serialises them, and memory goes in 256-dword pieces.
- `crystalhd_fleafuncs.c`: the BCM70015's clock gating (`flea_pwm`), which Broadcom had switched off,
  is on by default. With the library fix (in openframe-crystalhd) it decodes normally and keeps the
  card about 20 °C cooler at 24 frames a second. `flea_pwm=0` turns it off. `tx_trace=1` (writable
  at run time, in `/sys/module/crystalhd/parameters/`) logs the firmware's input buffer handshake.

The random stall the BCM70015 was known for was in the library, not here: see openframe-crystalhd.

To take a change, commit it to birdslikewires/crystalhd first, then copy the same files from there.
