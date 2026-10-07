#!/bin/sh
# Builds and runs the JsonSax host tests.
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/freeink-jsonsax-host-tests"
mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../include \
  test_json_sax.cpp ../../src/StreamingJsonParser.cpp -o "$BUILD_DIR/test_json_sax"
"$BUILD_DIR/test_json_sax"
