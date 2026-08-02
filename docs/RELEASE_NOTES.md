### v0.4.0 — new game: Kiss Marry Kill + always-on leaderboard

- 🎭 **New party game: Kiss Marry Kill** — one player secretly tags three (in)famous people **Kiss / Marry / Kill**; everyone else predicts how they tagged them. Read someone right and you score; being *readable* pays off too. Lobby pack-vote, four name packs (historical, fiction, famous, mix). **14 games now.**
- 🏆 **Leaderboard reworked** — it now **always** shows the current live standings (no more "no session"), and **auto-saves to the SD card** the moment you open it. The manual "save session" is gone — your results are simply always there.
- 💾 Scores are also written to SD **when a player drops out**, so a mid-game roster change never loses the standings.

### Install

Search for **"Hotspot Arcade"** in the **M5Burner** app or the **M5Launcher** catalog — one tap, no cables.

**Or flash by hand** (keeps M5Launcher):
```
esptool --chip esp32s3 --port <PORT> --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```

Cardputer v1 (StampS3, 8MB). Full image and recovery are in the README.
