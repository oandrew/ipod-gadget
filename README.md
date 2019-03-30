# ipod-gadget
[![Join the chat at https://gitter.im/ipod-gadget/Lobby](https://badges.gitter.im/ipod-gadget/Lobby.svg)](https://gitter.im/ipod-gadget/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)


ipod-gadget simulates an iPod USB device to stream digital audio to iPod compatible devices/docks.
It speaks iAP(iPod Accessory Protocol) and starts an audio streaming session.

Tested on Raspberry Pi Zero, Beaglebone Black and Nexus 5(mainline linux kernel) with Onkyo HT-R391 receiver as the host device (more host devices need to be tested).
Should work on any device that runs Linux 4.x (compiled with usb gadget configfs) and has a USB port that supports peripheral mode.


# implementation
It consists of two parts - linux kernel module and  client app (golang).
## kernel module
 
The kernel module takes care of the USB device gadget side. 
An iPod, when plugged in a dock, presents a USB configuration with 2 interfaces:
1. UAC1(USB Audio Class 1) - standart usb audio streaming interface.
2. HID - bidirectional transport for iAP packets.

The kernel module creates a new ALSA audio card "iPodUSB" for audio playback and iap0 char device for iAP communications.

The gadget driver is activated when the character device iap0 is opened and deregistered when it's closed.

## client app

The client app speaks to the host device over iAP by reading/writing packets from/to /dev/iap0 character device.
It handles the authentication and activates the audio streaming so that ALSA device can be used for playback.

# build and run

## DKMS (Auto-build kernel modules)

On the Raspberry PI:
```
sudo apt update && sudo apt upgrade
sudo apt install dkms raspberrypi-kernel-headers

sudo git clone https://github.com/oandrew/ipod-gadget.git /usr/src/ipod-gadget-0.1
cd /usr/src/ipod-gadget-0.1
sudo make dkms
```

Now everytime the kernel gets updated the kernel modules will be automatically rebuilt.

## kernel modules

```
git clone https://github.com/oandrew/ipod-gadget.git
cd ipod-gadget/gadget

make
# or cross compiling
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KERNEL_PATH=/home/andrew/pi-linux 

#load the module
modprobe libcomposite
insmod g_ipod_audio.ko
insmod g_ipod_hid.ko
insmod g_ipod_gadget.ko [swap_configs=0] [product_id=0x1297]

#optional params
swap_config: swap USB configurations. 
Might be useful when the dock sees only the Mass Storage configuation.

product_id: override the usb product id.
See doc/apple-usb.ids for the list of ids

```

Check the messages from `dmesg` and verify that the device `/dev/iap0` is available.

## client app

Follow the instructions here: https://github.com/oandrew/ipod

```
./ipod -d serve -w /tmp/ipod.trace /dev/iap0
```

Now you can open a different terminal and test the playback!

```
speaker-test -D plughw:CARD=iPodUSB,DEV=0 -c 2 -r 44100
```

Let me know if you have any issues.

Attach the trace file (e.g. `/tmp/ipod.trace` above) to the issue.

NOTE: currently it works only if the host device doesn't authenticate the iPod (typically only iPod authenticates the host device which is fine).






