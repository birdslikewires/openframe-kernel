# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel for your lovely **OpenFrame** device from OpenPeak. I would recommend the long term **3.16** branch, which is tested and recommended for ALSA reasons (see below). However, 3.16 is no longer under development, so let's recommend 5.4 instead.

The patches include Andrew de Quincey's OpenFrame backlight driver, a pin tweak which enables line output switching on the STAC9202 audio IC inside the OpenFrame 1, plus a twiddle to get the I2C bus to play nicely, which gets the ambient light sensor working.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started. If the version has changed it's best practice to do:

	make olddefconfig
	
Which will bring that .config file up to date with the latest defaults. Then you can:

	make menuconfig
	
To apply any changes you would like to make in a graphical interface.

You'll also need a 32-bit x86 build environment; a chroot on an Ubuntu 18.04 system has been working fine for me.

Once you have everything together, compile with:

	make -j`nproc` deb-pkg

You can use the resulting linux-image and linux-headers .deb packages with the scripts in the openframe-ubuntu repo to create an image which, hopefully, works.


## Precompiled Versions?

Compiled debian packages should be [available from my website](https://birdslikewires.net/download/openframe/kernel/).


## ALSA Reasons?

Everything works _almost_ perfectly in the more recent 5.* branch kernels.

**Except!**

Audio switching on the line output doesn't work properly, possibly because of the way the STAC9202 chip is connected on the board, probably in order to support DECT telephone audio. If you have an Orby variant, you likely have DECT hardware installed. Nice.

When a 3.5mm jack is connected to the rear socket, the internal speaker is not properly muted, resulting in audible interference crackling, usually only on the right speaker. I have little experience with ALSA and though I've tried [engaging with the alsa-devel mailing list](https://www.spinics.net/lists/alsa-devel/msg82909.html), I've had no success resolving the problem so far.

If anybody has ALSA experience and could look into this, it would be really, really appreciated.

In the meantime you can just mute the speaker from the ALSA control panel, but that's not cool.
