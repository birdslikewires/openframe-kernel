# fh
Intel Firmware Hub Kernel Module

Tested on Openpeak OpenFrame 1 and 2 devices.

Build
------
Compiles happily against kernel 3.16 on Ubuntu Bionic, so you shouldn't have too many issues.

	make install
	modprobe fh

Then check for `/dev/fh` and see if you can read from it. OpenFrame kernels built after
2026-11-10 include the module already.

It only loads on the two chips it knows (Intel 82802AC, SST 49LF008A), and it's read only
unless loaded with `writable=1`. Even then it refuses a write that would change NVRAM (block 0)
or the boot block (blocks 14 and 15) unless `unprotect=1` is given as well, since a bad write
there leaves a brick, particularly on an OpenFrame 2, where the EFI IC is soldered, not socketed.
A refused write changes nothing. Both options can only be given when the module is loaded:

	modprobe -r fh; modprobe fh writable=1

Writes go in whole 64 KB blocks; a block that already matches is skipped.

`/dev/fhp` takes the same writes but never erases. Flash can only be programmed by clearing bits
(1 to 0), so a write is refused, with nothing written, if any byte would need a bit set; otherwise
only the bytes that differ are programmed, then the block is verified. The same `writable` and
`unprotect` rules apply. It exists so that an update can write a file's body with its header left
blank and then the header last: if the update is interrupted, the flash holds either erased space
(which the firmware reads as no file) or the whole file, never part of one.

`test/test_guards.c` runs the write path in user space against a fake chip, to check those
guards without writing a real one. From `fh/test`:

	cc -I. -include kstub.h -o /tmp/test_guards test_guards.c && /tmp/test_guards
