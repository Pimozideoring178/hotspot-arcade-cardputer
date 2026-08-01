### v0.3.0 — host UI overhaul + audio

- **Live 2-column scoreboard** on the main screen — all 10 players at once, ranked, leader in gold
- **Settings screen** (`S`): rename the AP, audio off/low/high, AP on/off, event log
- 🔊 **speaker jingles** when the AP comes up and when players join or leave
- **Game picker**: sort A–Z or by most-played (`S`), `1v1` tags, one-line blurbs, page with `,`/`/`
- Reset scores moved to the **Leaderboard** (`L`)
- **Up to 10 phones** (the ESP32-S3 softAP hardware maximum)

Same 13 games as v0.2.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
