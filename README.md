Kernel Patches for OpenFrame
============================

These patches should be sufficient to get you up and running with the latest Linux kernel on your lovely **OpenPeak OpenFrame** device.

The headliners are inclusion of Andrew de Quincey's OpenFrame backlight driver, a means of telling the GMA drivers to use that backlight driver, a tweak that enables line-out switching on the OpenFrame 1's STAC9202 chip and a twiddle to get I2C (and therefore the ambient light sensor) playing nicely too.

You don't need to use the kernel version patch. That's more for my benefit, to be honest.

Apply to the kernel source like this:

	patch -p1 -d linux-version < patchfile

There's also a .config file to get you started.


Working Versions?
-----------------

Precompiled debian packages should be [available from my website](http://birdslikewires.co.uk/download/openframe/kernel/).


kernel-package 13.003
---------------------

Included because there's a foul-up in the one distributed with Ubuntu Trusty that means things don't compile properly. Use this if you're building kernel packages.

