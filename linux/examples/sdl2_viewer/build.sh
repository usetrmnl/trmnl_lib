#!/bin/bash
set -e
# This script builds the sdl2 example program for trmnl-lib
  echo "Installing required libraries."

# Install the required components
  sudo apt install curl libsdl2-dev -y
  make
  
  echo "Build complete. Run sdl2_viewer <API key> <optional backend URL> to start."

