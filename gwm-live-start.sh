#! /bin/sh
# gwm-live-start.sh
# Set up a GWM session on the live CD

# Firstly, create a fake home
mkdir /run/live-home
mount -t bind /run/live-home /root

# Set the default wallpaper
wallpaper --scale --image=/usr/share/images/wallpaper.png

# Set the theme
theme load /usr/share/themes/GlidixGreen.thm

# Set up the environment
cd /root
export HOME="/root"
export SHELL="/bin/sh"
export USERNAME="root"

# Create directories
mkdir "Desktop"
mkdir "Documents"
mkdir "Pictures"
mkdir "Music"
mkdir "Videos"
mkdir "Downloads"

# Make sure we can later start the file manager in desktop mode
mkdir /run/bin
export PATH="/run/bin:$PATH"
ln -s /usr/bin/filemgr /run/bin/-filemgr

# Start the file manager and task bar (sysbar)
fork -filemgr
fork sysbar

# Final message
msgbox 'Session ready' 'Welcome to the live session. You can try out the operating system without installing it, but not all features will be available. To install Glidix, use the icon on the desktop.' --success --ok
