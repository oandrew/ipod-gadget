KERNEL_PATH ?= /lib/modules/`uname -r`/build


g_ipod-y := ipod.o
g_ipod_hid-y := ipod_hid.o
g_ipod_audio-y := ipod_audio.o
g_ipod_gadget-y := ipod_gadget.o

#old
#obj-m += g_ipod.o 

obj-m += g_ipod_hid.o g_ipod_audio.o g_ipod_gadget.o

ccflags-y += -DDEBUG
ccflags-y += -DVERBOSE_DEBUG
ccflags-y += -Wno-unused-variable -Wno-unused-function
ccflags-y += -I$(src)

all:
	make -C $(KERNEL_PATH) M=$(CURDIR) modules

clean:
	make -C $(KERNEL_PATH) M=$(CURDIR) clean

