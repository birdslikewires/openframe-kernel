# fh
Intel Firmware Hub Kernel Module

Tested on Openpeak OpenFrame 1 and 2 devices.

Build
------
Compiles happily against kernel 3.16 on Ubuntu Bionic, so you shouldn't have too many issues.

	make install
	modprobe fh
	
Then check for `/dev/fh` and see if you can read from it.

And you should also be able to write to it, but make sure you know what you're doing or you could end up with a brick. Particularly if you're playing with an OpenFrame 2 where the EFI IC is soldered, not socketed.
