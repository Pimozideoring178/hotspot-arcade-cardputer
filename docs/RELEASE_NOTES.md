### v0.2.1 — boot-loop fix

Fixes an **endless launcher → app → reset loop** when installed via M5Launcher:
the firmware now confirms its OTA image on boot, so the launcher's rollback no
longer bounces it back. If v0.2.0 kept rebooting for you, install this.

Same **13 games** as v0.2.0 (Spectrum, Guess the Color, Battleship + the rest).

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
