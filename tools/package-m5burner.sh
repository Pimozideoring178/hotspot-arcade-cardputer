#!/usr/bin/env bash
# Lay the build out the way M5Burner expects, so this repo can be listed in
# m5stack/M5Stack-Firmware.
#
#   tools/build.sh && tools/package-m5burner.sh
#
# M5Burner's rule is `filename = name + "_" + flash address`, and it flashes every
# file in the category folder at the address in its name. Note the ESP32-S3 puts the
# bootloader at 0x0, not the 0x1000 you see in M5Stack's ESP32 examples.
#
# This produces the FULL install: bootloader + partition table + app. It replaces
# whatever is on the device, M5Launcher included. The launcher-friendly install is
# the plain app image at 0x170000 -- see README.
set -euo pipefail

cd "$(dirname "$0")/.."
SRC="build"
OUT="firmware/cardputer"

[[ -f "$SRC/hotspot-arcade-cardputer.ino.bin" ]] || { echo "run tools/build.sh first" >&2; exit 1; }

# boot_app0.bin is the initial otadata content and ships with the core, not with
# the sketch build -- ask arduino-cli where the core lives rather than guessing.
DATA_DIR="$(arduino-cli config get directories.data 2>/dev/null || true)"
[[ -n "$DATA_DIR" ]] || DATA_DIR="$HOME/.arduino15"
BOOT_APP0="$(find "$DATA_DIR/packages/esp32/hardware/esp32" -name boot_app0.bin 2>/dev/null | sort | tail -1)"
[[ -n "$BOOT_APP0" ]] || { echo "boot_app0.bin not found under $DATA_DIR -- is the esp32 core installed?" >&2; exit 1; }

mkdir -p "$OUT"
rm -f "$OUT"/*.bin

cp "$SRC/hotspot-arcade-cardputer.ino.bootloader.bin" "$OUT/bootloader_0x0.bin"
cp "$SRC/hotspot-arcade-cardputer.ino.partitions.bin" "$OUT/partitions_0x8000.bin"
cp "$BOOT_APP0" "$OUT/boot_app0_0xe000.bin"
cp "$SRC/hotspot-arcade-cardputer.ino.bin" "$OUT/hotspot-arcade_0x10000.bin"

echo "packaged for M5Burner:"
ls -l "$OUT"
echo
echo "m5burner.json version must match the release tag before opening the PR against"
echo "m5stack/M5Stack-Firmware (add this repo's URL to firmware-repo.list)."
