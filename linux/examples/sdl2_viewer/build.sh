#!/bin/bash
set -e
# This script adds the required libraries for the trmnl-lib sdl2 example
  echo "Installing required libraries."

# Install the required components
  sudo apt install libsdl2-dev -y
# save the current directory
  pushd .
  mkdir -p $HOME/Projects
  cd $HOME/Projects
  if [ -d PNGdec ]; then
	echo "PNGdec already cloned, updating to latest..."
	cd PNGdec
	git pull
        cd ..
  else
	git clone https://github.com/bitbank2/PNGdec
  fi
  if [ -d JPEGDEC ]; then
        echo "JPEGDEC already cloned, updating to latest..."
        cd JPEGDEC
        git pull
        cd ..
  else
        git clone https://github.com/bitbank2/JPEGDEC
  fi
  cd PNGdec/linux
  make
  cd ../../JPEGDEC/linux
  make
  popd 
  make
  
  echo "Build complete. Run sdl2_viewer <API key> <optional backend URL> to start."

