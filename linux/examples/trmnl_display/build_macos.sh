#!/bin/bash
set -e
# This script adds the required libraries for the trmnl-lib display example
  echo "Installing required libraries."

# Install the required components
  brew install sdl2
# save the current directory
  pushd .
  mkdir -p $HOME/Projects
  mkdir -p $HOME/.config/trmnl
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
  echo "  Enter your API (device) key"
  read key
  JKEY=$(printf "{\n        \"api_key\": \"")
  JURL=$(printf "\",\n        \"base_url\": \"https://trmnl.app\"\n}\n")
  printf '%s%s%s' "$JKEY" "$key" "$JURL" > $HOME/.config/trmnl/config.json

  echo "Build complete. Run trmnl_display to start."
