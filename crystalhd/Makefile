# SPDX-License-Identifier: GPL-2.0
# Broadcom Crystal HD (BCM70012, BCM70015) video decoder driver: see README.md. The kernel
# workflow copies this folder to drivers/staging/crystalhd.
obj-$(CONFIG_CRYSTALHD) += crystalhd.o
crystalhd-y := crystalhd_lnx.o crystalhd_misc.o crystalhd_cmds.o crystalhd_dmabuf.o \
	crystalhd_hw.o crystalhd_linkfuncs.o crystalhd_fleafuncs.o crystalhd_flea_ddr.o
ccflags-y := -I$(src)
