### What's new in v0.2.0

Three new games — **13 total** now:

- **Spectrum** — one player sees a hidden point on a dial and gives a clue; everyone else grabs the dial to guess where it lands. Closest wins.
- **Guess the Color** — a color flashes; guess its RGB. Closest guess wins, fastest gets a speed bonus.
- **Battleship** — 1v1: hide your fleet on a 10×10 grid, then take turns firing to sink the other's ships.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
