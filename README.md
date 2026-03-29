# Kernel Patches for OpenFrame

These patches should be sufficient to get you compiling a new Linux kernel which works with your lovely **OpenFrame** device from OpenPeak.

## Installing via apt

Kernels are now built automatically for Debian Trixie (i386) and published to an apt repository at [kernel.openbeak.net](https://kernel.openbeak.net). New upstream 6.1.x releases should be picked up within a few hours.

To add the repository:

```bash
sudo curl -fsSL https://kernel.openbeak.net/key.gpg -o /etc/apt/keyrings/openframe-kernel.gpg
echo "deb [signed-by=/etc/apt/keyrings/openframe-kernel.gpg] https://kernel.openbeak.net/ trixie main" | sudo tee /etc/apt/sources.list.d/openframe-kernel.list
sudo apt update
sudo apt install linux-image-*-openframe
```

Adding the repository to earlier Trixie images is completely possible, just make sure to put [this file](https://raw.githubusercontent.com/birdslikewires/openframe-linux/refs/heads/main/overlay-debian-trixie/etc/kernel/postinst.d/openframe-grub-update) in your `/etc/kernel/postinst.d` directory otherwise your `grub.cfg` file won't be updated to use it.

There's only enough space on the `/boot` partition to support two simultaneously installed kernel versions, so please remember to do a `sudo apt autoremove` once you've confirmed the new kernel is installed and working.

You might get away with using these packages with earlier Debian or Ubuntu releases too, though we build within a Trixie container with that toolchain, so your mileage may vary.


## Patches

The patches include a modified verison of Andrew de Quincey's OpenFrame backlight driver, a pin tweak which enables line output switching on the STAC9202 audio IC inside the OpenFrame 1, plus a twiddle to get the I2C bus to play nicely, which gets the ambient light sensor working.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a `.config` file to get you started. If the version has changed it's best practice to do:

	make olddefconfig
	
Which will bring that .config file up to date with the latest defaults. Then you can:

	make menuconfig
	
To apply any changes you would like to make in a graphical interface.

## Building

### The GitHub Way

Just fork this repository and you should inherit your own build system. The files live in `.github/workflows` and the magic happens behind the Actions tab. We build in a Debian Trixie container.

### The Self-Hosted Way

You'll need a 32-bit i686 build environment, which can be achieved with a chroot on 64-bit systems. The [Build Environment](https://github.com/birdslikewires/openframe-linux?tab=readme-ov-file#build-environment) setup instructions from my [openframe-linux](https://github.com/birdslikewires/openframe-linux) repo will help you there.

Once you have everything together, compile kernel versions 6.2 and below with:

	make -j`nproc` deb-pkg

For reasons undiscovered, kernel versions 6.3 and above require:

	make -j`nproc` bindeb-pkg

This will compile the kernel and generate the .deb packages.


