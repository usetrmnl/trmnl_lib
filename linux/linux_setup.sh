#!/bin/bash
set -e
# This script installs the required libraries to build trmnl_lib on Linux
echo "Linux supports I2C sensors, so we'll install two to start"
# save the current directory
  pushd .
# Install the required components
  sudo apt install libi2c-dev libcurl4-openssl-dev -y

# clone and build the bb_scd41 and bb_temperature libraries
  mkdir -p $HOME/Projects
  cd $HOME/Projects
  if [ -d $HOME/Projects/bb_scd41 ]; then
      echo "bb_scd41 already cloned, updating to latest..."
      cd bb_scd41
      git pull
      cd ..
  else
      git clone https://github.com/bitbank2/bb_scd41
  fi

  if [ -d $HOME/Projects/bb_temperature ]; then
      echo "bb_temperature already cloned, updating to latest..."
      cd bb_temperature
      git pull
      cd ..
  else
      git clone https://github.com/bitbank2/bb_temperature
  fi

  cd bb_scd41/Linux
  make
  cd ../../bb_temperature/Linux
  make
# restore the original directory
  popd
  echo "Compiling TRMNL library..."
  make
  echo "Build complete."

