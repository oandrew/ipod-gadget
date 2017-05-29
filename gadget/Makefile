#KERNEL_PATH = ~/beagle/linux


g_ipod-y := ipod.o
obj-m += g_ipod.o

ccflags-y += -DDEBUG
ccflags-y += -DVERBOSE_DEBUG

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean

