# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel which works with your lovely **OpenFrame** device from OpenPeak.

## Compiled Versions

Packages in .deb format should be [available from my build system](https://openbeak.net/openframe/kernel/). Newly released kernels are compiled in the early hours of a UK morning. Kernels from branch 5.10 are built with Debian Bullseye in mind, 6.1 is for Bookworm, and 6.12 is intended for Trixie.

## Patches

The patches include Andrew de Quincey's OpenFrame backlight driver, a pin tweak which enables line output switching on the STAC9202 audio IC inside the OpenFrame 1, plus a twiddle to get the I2C bus to play nicely, which gets the ambient light sensor working.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started. If the version has changed it's best practice to do:

	make olddefconfig
	
Which will bring that .config file up to date with the latest defaults. Then you can:

	make menuconfig
	
To apply any changes you would like to make in a graphical interface.

## Building

You'll need a 32-bit i686 build environment, which can be achieved with a chroot on 64-bit systems. The [Build Environment](https://github.com/birdslikewires/openframe-linux?tab=readme-ov-file#build-environment) setup instructions from my [openframe-linux](https://github.com/birdslikewires/openframe-linux) repo will help you there.

Once you have everything together, compile kernel versions 6.2 and below with:

	make -j`nproc` deb-pkg

For reasons undiscovered, kernel versions 6.3 and above require:

	make -j`nproc` bindeb-pkg

This will compile the kernel and generate the .deb packages.
