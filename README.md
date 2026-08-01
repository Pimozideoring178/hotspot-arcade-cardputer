<p align="center">
  <img src="docs/img/logo.png" alt="Hotspot Arcade — M5 Cardputer Edition" width="600">
</p>

# Hotspot Arcade — M5Stack Cardputer

[![build](https://github.com/genkigenki/hotspot-arcade-cardputer/actions/workflows/build.yml/badge.svg)](https://github.com/genkigenki/hotspot-arcade-cardputer/actions/workflows/build.yml)
[![latest release](https://img.shields.io/github/v/release/genkigenki/hotspot-arcade-cardputer?sort=semver)](https://github.com/genkigenki/hotspot-arcade-cardputer/releases/latest)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Several offline party games your guests play from their phones. The Cardputer opens a WiFi
access point and a captive portal, everyone joins and plays in the browser — no app
to install — and the Cardputer's own screen is the host: lobby, game picker,
scoreboard, event log.

<p align="center">
  <img src="docs/img/photo-2.jpg" alt="Cardputer hosting a round while a phone votes on a Would You Rather pack" width="820">
</p>

This is a **host port** of [tarikbc/hotspot-arcade](https://github.com/tarikbc/hotspot-arcade),
which runs the same games as a Flipper Zero app.

If you have a Flipper, use upstream — it is the original and it is excellent.

## Install

**Easiest install** (Recommended): The app is in the "M5Burner" catalog and in "Launcher's" catalog. Search for "Hotspot Arcade".

**Manual install**: Flash by hand to 0x170000: 
```bash
esptool --chip esp32s3 --port COM7 --baud 921600 write_flash 0x170000 hotspot-arcade-cardputer.ino.bin
```
or you can drop the `.bin` on the microSD and launch it from the launcher.

The Cardputer's stock M5Launcher layout puts
loaded apps in `ota_0` at `0x170000` and keeps the launcher itself in the test
partition. Writing only the app image leaves the launcher untouched — you still get
back into it the usual way, by holding the button at boot.

**Full install** (replaces everything, launcher included):

```bash
esptool --chip esp32s3 --port COM7 --baud 921600 write_flash 0x0 hotspot-arcade-cardputer.full.bin
```

Replace `COM7` with your port (`/dev/ttyACM0`, `/dev/cu.usbmodem*`). `esptool` comes
with the esp32 core, or `pip install esptool`. If the board does not enter download
mode by itself, hold **G0** on the StampS3 while plugging in USB-C.

Both images are on the [releases page](../../releases). 

## Hardware

Cardputer **v1** (StampS3: ESP32-S3FN8, 8MB flash, no PSRAM). Firmware is ~1.2MB of
a 3.3MB app slot and ~78KB of static RAM, leaving ~250KB free at runtime.

Not tested on the Cardputer ADV. It should build, but the ADV has a different
keyboard controller (TCA8418) and antenna, so treat it as unverified.

Limit: the v1's antenna is weak — eight phones in a room is fine, range is worse than a dev board with an
external antenna. 

## Using it

The AP comes up at boot; there is no start step. Phones join **Hotspot Arcade** (open)
and land on `http://192.168.4.1` (if not automatically getting there via captive portal)

<p align="center">
  <img src="docs/img/photo-1.jpg" alt="The Cardputer dashboard: SSID, AP state, active game, player count and key hints" width="700">
</p>

The dashboard is the host view: SSID and IP, whether the AP is up, the active game,
the live scoreboard, and the last event. Everything else is one key away.

| key | |
| --- | --- |
| `G` | select game (arrow keys move, `Enter` picks, `Esc` backs out) |
| `L` | full leaderboard |
| `C` | event log |
| `R` | reset scores |
| `E` | end the current round |
| `N` | rename the AP (restarts it, which drops every phone) |
| `P` | stop / start the AP |
| `Esc` | back to the dashboard |

Serial at 115200 prints the AP address, asset counts and free heap at boot.

Games: trivia, would-you-rather, word scramble, spectrum, reaction duel, connect four,
tic-tac-toe, dots & boxes, reversi, drawing, pong, guess the color, battleship. All of
them are phone-driven; the host picks which one is live and watches.

<p align="center">
  <img src="docs/img/photo-3.jpg" alt="The game picker with all the games; the active one is marked" width="700">
</p>

## Build

Needs `node` and `arduino-cli`.

```bash
tools/build.sh --deps
```

`--deps` installs esp32 core 3.3.11 and the M5Cardputer library (which pulls
M5Unified and M5GFX); drop it after the first run. The FQBN matters:

```
esp32:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB
```

The board's *default* partition scheme is the 4MB one with a 1.2MB app slot, which
this firmware does not fit in.

## How it relates to upstream

| | upstream (Flipper + ESP board) | here |
| --- | --- | --- |
| game engine | `esp32/hotspot-arcade-fw/ha_games.h` | the same file, unmodified |
| phone client | streamed over UART at session start | baked into flash |
| content packs | read off the Flipper's SD, streamed | baked into flash |
| host reports | UART v2 frames (`docs/PROTOCOL.md`) | same, but simply called in-process |
| host UI | Flipper scenes, 128×64 mono | `ha_ui.h`, 240×135 colour + keyboard |

The engine reaches its host through six sink functions. This port implements them as direct calls into a local mirror.

New code lives in four files under `hotspot-arcade-cardputer/`:

| | |
| --- | --- |
| `hotspot-arcade-cardputer.ino` | AP, captive portal, WebSocket, the six sinks |
| `ha_ui.h` | five screens + keyboard |
| `ha_host.h` | roster / score / event mirror |
| `ha_content.h` | baked-pack parser (a port of upstream's `content_stream_pack()`) |

## Staying in sync

**The rule: nothing under `vendor/` is edited here.** Everything in it is
upstream's, copied verbatim, with the exact commit pinned in [UPSTREAM.md](UPSTREAM.md).
Want a game changed? Change it upstream — then both projects get it.

```bash
node tools/sync-upstream.mjs ../hotspot-arcade   # refresh vendor/ + re-pin the commit
node tools/gen-assets.mjs                        # re-bake into the sketch
```

`git diff vendor/` after a sync is exactly the upstream change. `gen-assets.mjs`
copies the three engine headers into the sketch folder because arduino-cli builds
from a copy of the sketch directory, so an include reaching outside it would not
resolve — those copies are generated and carry a banner saying so.

## Distribution

- **Releases** — the app image and the trimmed full image, built and published on every `v*` tag.
- **M5Burner** — listed in the catalog. Tagged releases auto-publish a new version when the
  `M5BURNER_USER` / `M5BURNER_PWD` repo secrets are set (via `tools/m5burner_post.py`).
- **M5Launcher / LauncherHub** — in the catalog; mirrored automatically from M5Burner.

## Status

Unverified: the Cardputer ADV, and long sessions with many many phones.

## License

MIT — see [LICENSE](LICENSE), same as upstream. `vendor/libs/` holds third-party
libraries under their own licenses.
