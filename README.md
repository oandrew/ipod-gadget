# ipod-gadget

[![Join the chat at https://gitter.im/ipod-gadget/Lobby](https://badges.gitter.im/ipod-gadget/Lobby.svg)](https://gitter.im/ipod-gadget/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
iPod usb audio gadget

ipod-gadget simulates an iPod USB device to stream digital audio to iPod compatible devices/docks.
It speaks iAP(iPod Accessory Protocol) and starts an audio streaming session.

Tested on Raspberry Pi Zero, Beaglebone Black and Nexus 5(mainline linux kernel) with Onkyo HT-R391 receiver as the host device (more host devices need to be tested).
Should work on any device that runs Linux 4.x (compiled with usb gadget configfs) and has a USB port that supports peripheral mode.


# implementation
It consists of two parts - linux kernel module and  client app (Golang).
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


```
git clone https://github.com/oandrew/ipod-gadget.git
cd ipod-gadget
```

kernel module (linux kernel need to be compiled with usb gadget configfs support)
```
cd gadget
make
# or cross compiling with  custom linux kernel source path
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- KERNEL_PATH=/home/andrew/pi-linux 

#load the module
modprobe libcomposite
insmod g_ipod.ko
```

client (golang)
```
cd cmd/ipod
go get -d

go build
# or cross compiling
GOARCH=arm GOARM=6 go build

./ipod
```
After launching `./ipod` plug in the device and verify it has been enumerated by the host.
You will see all the received/send messages on the output.

Now you can open a different terminal and test the playback!

```
speaker-test -D plughw:CARD=iPodUSB,DEV=0 -c 2 -r 44100
```

Let me know if you have any issues.

NOTE: currently it works only if the host device doesn't authenticate the iPod (typically only iPod authenticates the host device which is fine).






