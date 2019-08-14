# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel on your lovely **OpenFrame** device from OpenPeak. The long term **3.16** branch is tested and recommended, for ALSA reasons, explained below.

The patches include Andrew de Quincey's OpenFrame backlight driver, a pin tweak which enables line output switching on the STAC9202 audio IC inside the OpenFrame 1, plus a twiddle to get the I2C bus to play nicely, which gets the ambient light sensor working.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started. You'll also need a 32-bit x86 build environment; a Xubuntu virtual machine works well for me. Once you have everything together, compile with:

	make -j`nproc` deb-pkg

You can use the resulting linux-image and linux-headers .deb packages with the scripts in my openframe-ubuntu repo to create an image which, hopefully, works.


## Precompiled Versions?

Compiled debian packages should be [available from my website](https://birdslikewires.net/download/openframe/kernel/).


## ALSA Reasons?

Everything works almost perfectly in the more recent 4.* and 5.* branch kernels works perfectly, 5.0.9 being the latest tested.

**Except!** Audio switching on the line output doesn't work properly. When a 3.5mm jack is connected to the rear socket, the internal speaker is not properly muted, resulting in audible interference crackling, usually only on the right speaker. I have little experience with ALSA and though I've tried [engaging with the alsa-devel mailing list](https://www.spinics.net/lists/alsa-devel/msg82909.html), I've had no success resolving the problem so far.

If anybody has ALSA experience and could look into this, it would be really, really appreciated.
