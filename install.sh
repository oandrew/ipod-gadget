#!/bin/bash



if [[ $EUID > 0 ]]; then
    echo "$0 is not running as root. Try using sudo."
    exit 2
fi

BOOT_CONFIG_FILE="/boot/config.txt"
OVERLAY="dwc2"
CMDLINE_FILE="/boot/firmware/cmdline.txt"
MODULES_KEY="modules-load="
MODULES_VALUE="dwc2,g_ipod_audio,g_ipod_hid,g_ipod_gadget"

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

echo "Updating $BOOT_CONFIG_FILE ..."
# Check if the file exists
if [[ ! -f "$BOOT_CONFIG_FILE" ]]; then
  echo "Error: $BOOT_CONFIG_FILE does not exist."
  exit 1
fi

# Ensure the line with 'dtoverlay=' exists and is properly updated
if grep -q "^dtoverlay=" "$BOOT_CONFIG_FILE"; then
  # If the line starts with 'dtoverlay=', append ',dwc2' if not already present
  if ! grep -q "dtoverlay=.*,$OVERLAY" "$BOOT_CONFIG_FILE"; then
    sed -i "s/^dtoverlay=\(.*\)/dtoverlay=\1,$OVERLAY/" "$BOOT_CONFIG_FILE"
  fi
else
  # Add a new line for 'dtoverlay=dwc2' if not present
  echo "dtoverlay=$OVERLAY" >> "$BOOT_CONFIG_FILE"
fi

echo "Updated $BOOT_CONFIG_FILE successfully."
echo ""
echo "Updating $CMDLINE_FILE File..."
# Check if the file exists
if [[ ! -f "$CMDLINE_FILE" ]]; then
  echo "Error: $CMDLINE_FILE does not exist."
  exit 1
fi

# Read the current content of the cmdline file
CURRENT_CMDLINE=$(cat "$CMDLINE_FILE")

# Check if the file already contains the key
if echo "$CURRENT_CMDLINE" | grep -q "$MODULES_KEY"; then
  # Extract the current value of modules-load=
  CURRENT_MODULES=$(echo "$CURRENT_CMDLINE" | sed -n "s/.*$MODULES_KEY\([^ ]*\).*/\1/p")
  
  # Check if dwc2,g_serial is already in the modules-load=
  if [[ "$CURRENT_MODULES" != *"$MODULES_VALUE"* ]]; then
    # Append dwc2,g_serial to the existing value
    UPDATED_MODULES="$CURRENT_MODULES,$MODULES_VALUE"
    UPDATED_CMDLINE=$(echo "$CURRENT_CMDLINE" | sed "s/$MODULES_KEY[^ ]*/$MODULES_KEY$UPDATED_MODULES/")
    echo "$UPDATED_CMDLINE" > "$CMDLINE_FILE"
    echo "Updated modules-load= to include $MODULES_VALUE."
  else
    echo "modules-load= already contains $MODULES_VALUE. No changes needed."
  fi
else
  # Append modules-load=dwc2,g_serial to the end of the line if not present
  UPDATED_CMDLINE="$CURRENT_CMDLINE $MODULES_KEY$MODULES_VALUE"
  echo "$UPDATED_CMDLINE" > "$CMDLINE_FILE"
  echo "Added $MODULES_KEY$MODULES_VALUE to $CMDLINE_FILE."
fi
