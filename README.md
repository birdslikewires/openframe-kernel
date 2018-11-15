# Kernel Patches for OpenFrame

These patches should be sufficient to get you up and running with a recent Linux kernel on your lovely **OpenFrame** device from OpenPeak. The long term **3.16** branch is tested and recommended, for ALSA reasons.*

The headliners are; inclusion of Andrew de Quincey's OpenFrame backlight driver; a means of telling the GMA drivers to use that backlight driver; a tweak that enables line output switching on the OpenFrame 1's STAC9202 chip; a twiddle to get I2C (and therefore the ambient light sensor) playing nicely too.

You don't need to use the kernel version patch. That's more for my benefit, to be honest.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started.


## Precompiled Versions?

Compiled debian packages should be [available from my website](https://birdslikewires.net/download/openframe/kernel/).


## *ALSA Reasons?

Absolutely everything in the more recent 4.* branch kernels works perfectly, 4.14 being the latest tested.

**Except!** Audio switching on the line output doesn't work. Either there is no switching whatsoever when a 3.5mm jack is connected, or after switching an interference crackle is present on the right speaker. I've tried [engaging with the alsa-devel mailing list](https://www.spinics.net/lists/alsa-devel/msg82909.html), but with no success so far.

If anybody has ALSA experience and could look into this, it would be much appreciated.