#!/bin/sh
# Builds and runs the UsbMassStorage host test against fake Arduino USB classes.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/freeink-usb-msc-host-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Istubs -I../../include \
  test_usb_mass_storage.cpp ../../src/UsbMassStorage.cpp -o "$BUILD_DIR/test_usb_mass_storage"
"$BUILD_DIR/test_usb_mass_storage"
