#
# fh/Makefile
#
# Build the firmware hub driver
#

MOD	= fh

#include $(TOP)/Makefile.master

obj-m	+= $(MOD).o

all:
	make -C $(TOP)/$(LINUX) $(LINUX_MARGS) SUBDIRS=$(PWD) modules

clean:
	rm -f .fh.ko.cmd .fh.mod.o.cmd .fh.o.cmd Module.symvers fh.ko fh.mod.c fh.mod.o fh.o
	rm -rf .tmp_versions
