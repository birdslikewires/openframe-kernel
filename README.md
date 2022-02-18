# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel for your lovely **OpenFrame** device from OpenPeak. I recommend the long term **5.10** branch, which matches nicely with Debian Bullseye.

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
