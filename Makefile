.PHONY: all clean test

ARCH ?= arm64

TEST_INCLUDE ?=.

# 2. Specify the Compiler Prefix
# Ensure this is in your $PATH, or provide the absolute path like /usr/bin/aarch64-linux-gnu-
CROSS_COMPILE ?=aarch64-linux-gnu-

# 3. Specify the Path to the Kernel Source
# This must point to your prepared Linux 6.1.31 folder
KDIR?=$(realpath ../linux-6.1.31)

# SRCS:=pci_debugfs.c
SRCS:=main.c example.c

TEST_FILES?=$(realpath ./pci_test.c)


# Module Name
obj-m+=pci_debugfs.o

TEST_OBJS?=
pci_debugfs-y+=example.o main.o $(TEST_OBJS)
# pci_debugfs-y+=$(TEST_OBJS)

ccflags-y+=-I$(TEST_INCLUDE)

all:
# 	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules_prepares
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	
test:
	make -C wdtesting TEST_FILES="$(TEST_FILES)" KDIR=$(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) SRCS="$(SRCS)"  all

clean:
	make -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	