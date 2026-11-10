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

`test/test_guards.c` runs the write path in user space against a fake chip, to check those
guards without writing a real one. From `fh/test`:

	cc -I. -include kstub.h -o /tmp/test_guards test_guards.c && /tmp/test_guards
