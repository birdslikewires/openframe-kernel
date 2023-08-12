# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel which works with your lovely **OpenFrame** device from OpenPeak.

## Compiled Versions

Packages in .deb format should be [available from my build system](http://openbeak.net/openframe/kernel/). Newly released kernels are compiled in the early hours of a UK morning. Kernels from branch 5.10 are built with Debian Bullseye in mind, while 6.1 is for Bookworm.

## Patches

The patches include Andrew de Quincey's OpenFrame backlight driver, a pin tweak which enables line output switching on the STAC9202 audio IC inside the OpenFrame 1, plus a twiddle to get the I2C bus to play nicely, which gets the ambient light sensor working.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started. If the version has changed it's best practice to do:

	make olddefconfig
	
Which will bring that .config file up to date with the latest defaults. Then you can:

	make menuconfig
	
To apply any changes you would like to make in a graphical interface.

You'll also need a 32-bit x86 build environment, which can be achieved with a chroot on 64-bit systems.

Once you have everything together, compile with:

	make -j`nproc` deb-pkg

This will compile the kernel and generate the .deb packages.
