#!/bin/sh
echo "Updating APT sources and updating currently installed software to latest version...."
echo ""
# Lets get the pi's software up to date and upgraded before we start
sudo apt update && sudo apt full-upgrade -y
# Install Pi Linux Headers if they're not allready install
echo "Installing kernel header sources for compilation..."
echo ""
sudo apt-get install raspberrypi-kernel-headers -y
# Now we install build dependancies so we can get the modules compiled
echo "Installing compilation dependancies..."
echo ""
sudo apt install git bc bison flex libssl-dev make -y
# Next we install DKMS without any recommended modules as some DKMS packages have a habit of install the wrong linux kernel headers as well
echo "Installing DKMS to allow for simpler kernel module install/upgrading..."
echo ""
sudo apt install --no-install-recommends dkms
# Switch into the downloaded directory and setup a softlink for DKMS
echo "Setting up DKMS local repo..."
echo ""
echo "Removing existing symbolic link if exists..."
sudo rm -r -f /usr/src/ipod-gadget-0.1
echo "Creating symbolic link..."
echo ""
sudo ln -s $PWD/gadget/ /usr/src/ipod-gadget-0.1
echo "Reove existing DKMS ipod-gadget tree if exists..."
sudo dkms remove ipod-gadget/0.1
echo "Installing ipod-gadget into DKMS tree..."
echo ""
sudo dkms add -m ipod-gadget/0.1
echo "Compiling and installing module via DKMS..."
echo ""
sudo dkms install ipod-gadget/0.1
