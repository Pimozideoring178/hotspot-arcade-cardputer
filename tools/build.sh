#!/usr/bin/env bash
# Build the Cardputer firmware.
#
#   tools/build.sh          # regenerate baked assets + compile
#   tools/build.sh --deps   # install the esp32 core and M5Cardputer first
#
# Output: build/ (see README for the two install routes and their addresses).
set -euo pipefail

cd "$(dirname "$0")/.."

# The Cardputer v1 is an 8MB ESP32-S3 with no PSRAM. The board's DEFAULT partition
# scheme is the 4MB one with a 1.2MB app slot, which this firmware does not fit in.
FQBN="esp32:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB"

if [[ "${1:-}" == "--deps" ]]; then
  arduino-cli config set board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  arduino-cli core update-index
  arduino-cli core install esp32:esp32@3.3.11   # 3.x; vendor/libs are the 3.x-compatible copies
  arduino-cli lib install M5Cardputer           # pulls M5Unified + M5GFX
fi

node tools/gen-assets.mjs

arduino-cli compile \
  --fqbn "$FQBN" \
  --libraries vendor/libs \
  --output-dir build \
  hotspot-arcade-cardputer

# Strip the debug artefacts; they are 60MB+ and nobody flashes them.
rm -f build/*.elf build/*.map

node tools/trim-merged.mjs \
  build/hotspot-arcade-cardputer.ino.merged.bin \
  build/hotspot-arcade-cardputer.full.bin

echo
echo "app image  (keeps M5Launcher, flash to 0x170000) : build/hotspot-arcade-cardputer.ino.bin"
echo "full image (replaces everything, flash to 0x0)   : build/hotspot-arcade-cardputer.full.bin"
