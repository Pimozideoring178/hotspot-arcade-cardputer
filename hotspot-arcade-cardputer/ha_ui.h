// Cardputer host UI: the screens the Flipper build draws with ViewDispatcher +
// SceneManager, redone for a 240x135 colour panel and a 56-key keyboard.
//
// Drawing happens only from loop(), never from an async callback: the UI takes a
// snapshot of the host mirror under the engine lock, releases it, and draws from
// the copy. That keeps the ~10ms sprite push off the lock, so the WebSocket task
// is never blocked by the screen.
#pragma once
#include <M5Cardputer.h>
#include "ha_host.h"

// ---- implemented in the .ino (they touch the engine / WiFi under the lock) ----
void haHostSelectGame(uint8_t game);
void haHostResetScores();
void haHostRoundEnd();
void haHostApplySsid(const char* ssid);
void haHostTogglePortal();
const char* haHostSsid();
String haHostIp();
void haHostSnapshot(HaHost& dst);

// Same list, same order as the Flipper's game_select scene.
struct HaGameItem {
    uint8_t id;
    const char* label;
};
static const HaGameItem HA_UI_GAMES[] = {
    {HA_GAME_TRIVIA, "Trivia"},
    {HA_GAME_WYR, "Would You Rather"},
    {HA_GAME_SCRAMBLE, "Word Scramble"},
    {HA_GAME_SPECTRUM, "Spectrum"},
    {HA_GAME_REACT, "Reaction Duel"},
    {HA_GAME_CONNECT4, "Connect Four"},
    {HA_GAME_TICTACTOE, "Tic-Tac-Toe"},
    {HA_GAME_DOTS, "Dots & Boxes"},
    {HA_GAME_REVERSI, "Reversi"},
    {HA_GAME_DRAW, "Drawing"},
    {HA_GAME_PONG, "Pong"},
    {HA_GAME_GUESSCOLOR, "Guess the Color"},
    {HA_GAME_BATTLESHIP, "Battleship"},
    {HA_GAME_NONE, "None (lobby)"},
};
static const int HA_UI_GAME_COUNT = sizeof(HA_UI_GAMES) / sizeof(HA_UI_GAMES[0]);

static const char* haUiGameLabel(uint8_t id) {
    for(int i = 0; i < HA_UI_GAME_COUNT; i++)
        if(HA_UI_GAMES[i].id == id) return HA_UI_GAMES[i].label;
    return "None";
}

enum HaUiView { HA_VIEW_DASH, HA_VIEW_GAMES, HA_VIEW_BOARD, HA_VIEW_CONSOLE, HA_VIEW_SSID };

static M5Canvas haUiCanvas(&M5Cardputer.Display);
static bool haUiSprite = false;
static HaUiView haUiView = HA_VIEW_DASH;
static int haUiCursor = 0;
static int haUiScroll = 0;
static char haUiEdit[33] = "";
static HaHost haUiSnap; // draw source; never touched by the async task
static uint32_t haUiDrawnRev = 0xFFFFFFFF;
static uint32_t haUiLastDraw = 0;
static bool haUiForce = true;

#define HA_UI_W 240
#define HA_UI_H 135
#define HA_UI_ROW 10 // px per list row at the 6x8 font

static lgfx::LovyanGFX* haUiG() {
    return haUiSprite ? (lgfx::LovyanGFX*)&haUiCanvas : (lgfx::LovyanGFX*)&M5Cardputer.Display;
}

static void haUiBegin() {
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(90);
    // 8bpp keeps the off-screen buffer at ~32KB. 16bpp would be 65KB, which is a
    // lot to hold alongside the WiFi stack and eight WebSocket clients on a board
    // with no PSRAM. If it still can't be had, fall back to drawing direct (which
    // flickers, but works).
    haUiCanvas.setPsram(false);
    haUiCanvas.setColorDepth(8);
    haUiSprite = haUiCanvas.createSprite(HA_UI_W, HA_UI_H) != nullptr;
    haUiG()->setTextFont(1);
    haUiG()->setTextSize(1);
}

// ---- drawing ---------------------------------------------------------------

static void haUiHeader(lgfx::LovyanGFX* g, const char* title) {
    g->fillRect(0, 0, HA_UI_W, 12, TFT_NAVY);
    g->setTextColor(TFT_WHITE, TFT_NAVY);
    g->drawString(title, 3, 2);
    char bat[8];
    snprintf(bat, sizeof(bat), "%d%%", (int)M5Cardputer.Power.getBatteryLevel());
    g->drawString(bat, HA_UI_W - 6 * (int)strlen(bat) - 3, 2);
}

static void haUiFooter(lgfx::LovyanGFX* g, const char* hint) {
    g->fillRect(0, HA_UI_H - 11, HA_UI_W, 11, TFT_DARKGREY);
    g->setTextColor(TFT_WHITE, TFT_DARKGREY);
    g->drawString(hint, 3, HA_UI_H - 9);
}

// Player order for both the dashboard and the leaderboard: score desc, then pid,
// so the board doesn't reshuffle on every tie.
static int haUiSorted(uint8_t* out) {
    int n = 0;
    for(uint8_t pid = 1; pid <= HA_MAX_PLAYERS; pid++)
        if(haUiSnap.p[pid].used) out[n++] = pid;
    for(int i = 1; i < n; i++) {
        uint8_t k = out[i];
        int j = i - 1;
        while(j >= 0 && haUiSnap.p[out[j]].score < haUiSnap.p[k].score) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = k;
    }
    return n;
}

static void haUiDrawDash(lgfx::LovyanGFX* g) {
    haUiHeader(g, "HOTSPOT ARCADE");
    g->setTextColor(TFT_WHITE, TFT_BLACK);

    char line[64];
    snprintf(line, sizeof(line), "SSID %s", haHostSsid());
    g->drawString(line, 3, 15);

    g->setTextColor(haUiSnap.portalRunning ? TFT_GREEN : TFT_RED, TFT_BLACK);
    snprintf(
        line,
        sizeof(line),
        "%s  %s",
        haUiSnap.portalRunning ? "AP UP" : "AP OFF",
        haHostIp().c_str());
    g->drawString(line, 3, 25);

    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);

    // Everything on screen comes from the one snapshot, so the count can't
    // disagree with the list under it.
    g->setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(line, sizeof(line), "Game %s   Players %d", haUiGameLabel(haUiSnap.activeGame), n);
    g->drawString(line, 3, 35);

    g->drawFastHLine(0, 45, HA_UI_W, TFT_DARKGREY);

    int y = 48;
    int shown = n < 6 ? n : 6;
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    for(int i = 0; i < shown; i++) {
        const HaHostPlayer& p = haUiSnap.p[order[i]];
        snprintf(line, sizeof(line), "%d %-20s %5ld", i + 1, p.nick, (long)p.score);
        g->drawString(line, 3, y);
        y += HA_UI_ROW;
    }
    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString("waiting for phones to join...", 3, y);
    } else if(n > shown) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        snprintf(line, sizeof(line), "+%d more (L)", n - shown);
        g->drawString(line, 3, y);
    }

    if(haUiSnap.lastEvent[0]) {
        g->setTextColor(TFT_YELLOW, TFT_BLACK);
        g->drawString(haUiSnap.lastEvent, 3, HA_UI_H - 22);
    }
    haUiFooter(g, "G game  L board  C log  R reset  E end  N ssid");
}

static void haUiDrawGames(lgfx::LovyanGFX* g) {
    haUiHeader(g, "SELECT GAME");
    int rows = 10; // 12..112
    if(haUiCursor < haUiScroll) haUiScroll = haUiCursor;
    if(haUiCursor >= haUiScroll + rows) haUiScroll = haUiCursor - rows + 1;
    int y = 14;
    for(int i = haUiScroll; i < HA_UI_GAME_COUNT && i < haUiScroll + rows; i++) {
        bool sel = (i == haUiCursor);
        bool live = (HA_UI_GAMES[i].id == haUiSnap.activeGame);
        if(sel) g->fillRect(0, y - 1, HA_UI_W, HA_UI_ROW, TFT_BLUE);
        g->setTextColor(live ? TFT_GREEN : TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
        char line[48];
        snprintf(line, sizeof(line), "%s%s", live ? "* " : "  ", HA_UI_GAMES[i].label);
        g->drawString(line, 3, y);
        y += HA_UI_ROW;
    }
    // Hints name the key by its printed legend, not the character it sends: the
    // top-left key is labelled ESC (it reports '`'), and ; / . are labelled with
    // arrows. Telling the host to press '`' would be telling them the wrong thing.
    haUiFooter(g, "UP/DOWN move   ENTER select   ESC back");
}

static void haUiDrawBoard(lgfx::LovyanGFX* g) {
    haUiHeader(g, "LEADERBOARD");
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString("no players", 3, 16);
    }
    int y = 14;
    for(int i = 0; i < n && i < 11; i++) {
        const HaHostPlayer& p = haUiSnap.p[order[i]];
        char line[64];
        snprintf(line, sizeof(line), "%2d %-20s %6ld", i + 1, p.nick, (long)p.score);
        g->setTextColor(i == 0 ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        g->drawString(line, 3, y);
        y += HA_UI_ROW;
    }
    haUiFooter(g, "R reset scores   ESC back");
}

static void haUiDrawConsole(lgfx::LovyanGFX* g) {
    haUiHeader(g, "EVENT LOG");
    int rows = 11;
    uint32_t total = haUiSnap.evTotal;
    uint32_t have = total < HA_EV_MAX ? total : HA_EV_MAX;
    int y = 14;
    g->setTextColor(TFT_GREEN, TFT_BLACK);
    for(uint32_t i = 0; i < have && i < (uint32_t)rows; i++) {
        // newest first
        uint32_t idx = (total - 1 - i) % HA_EV_MAX;
        g->drawString(haUiSnap.ev[idx], 3, y);
        y += HA_UI_ROW;
    }
    if(have == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString("nothing yet", 3, 16);
    }
    haUiFooter(g, "ESC back");
}

static void haUiDrawSsid(lgfx::LovyanGFX* g) {
    haUiHeader(g, "AP NAME");
    g->setTextColor(TFT_WHITE, TFT_BLACK);
    g->drawString("Type a new SSID:", 3, 20);
    g->fillRect(3, 34, HA_UI_W - 6, 14, TFT_DARKGREY);
    g->setTextColor(TFT_WHITE, TFT_DARKGREY);
    char shown[40];
    snprintf(shown, sizeof(shown), "%s_", haUiEdit);
    g->drawString(shown, 6, 37);
    g->setTextColor(TFT_DARKGREY, TFT_BLACK);
    g->drawString("Applying restarts the access point,", 3, 58);
    g->drawString("which drops every connected phone.", 3, 68);
    haUiFooter(g, "ENTER apply   DEL erase   ESC cancel");
}

static void haUiDraw() {
    haHostSnapshot(haUiSnap);
    lgfx::LovyanGFX* g = haUiG();
    if(haUiSprite) haUiCanvas.fillSprite(TFT_BLACK);
    else g->fillScreen(TFT_BLACK);
    g->setTextFont(1);
    g->setTextSize(1);
    switch(haUiView) {
    case HA_VIEW_GAMES:
        haUiDrawGames(g);
        break;
    case HA_VIEW_BOARD:
        haUiDrawBoard(g);
        break;
    case HA_VIEW_CONSOLE:
        haUiDrawConsole(g);
        break;
    case HA_VIEW_SSID:
        haUiDrawSsid(g);
        break;
    default:
        haUiDrawDash(g);
        break;
    }
    if(haUiSprite) haUiCanvas.pushSprite(0, 0);
    haUiDrawnRev = haUiSnap.rev;
    haUiLastDraw = millis();
    haUiForce = false;
}

// ---- input -----------------------------------------------------------------

static void haUiOpen(HaUiView v) {
    haUiView = v;
    haUiCursor = 0;
    haUiScroll = 0;
    if(v == HA_VIEW_GAMES) {
        for(int i = 0; i < HA_UI_GAME_COUNT; i++)
            if(HA_UI_GAMES[i].id == haUiSnap.activeGame) haUiCursor = i;
    }
    haUiForce = true;
}

static void haUiChar(char c) {
    if(haUiView == HA_VIEW_SSID) {
        if(c == '`') { // esc
            haUiOpen(HA_VIEW_DASH);
            return;
        }
        size_t n = strlen(haUiEdit);
        if(c >= 0x20 && c < 0x7F && n < sizeof(haUiEdit) - 1) {
            haUiEdit[n] = c;
            haUiEdit[n + 1] = '\0';
            haUiForce = true;
        }
        return;
    }

    switch(c) {
    case '`': // esc
        haUiOpen(HA_VIEW_DASH);
        return;
    case ';': // up
        if(haUiView == HA_VIEW_GAMES && haUiCursor > 0) haUiCursor--;
        haUiForce = true;
        return;
    case '.': // down
        if(haUiView == HA_VIEW_GAMES && haUiCursor < HA_UI_GAME_COUNT - 1) haUiCursor++;
        haUiForce = true;
        return;
    case 'g':
    case 'G':
        haUiOpen(HA_VIEW_GAMES);
        return;
    case 'l':
    case 'L':
        haUiOpen(HA_VIEW_BOARD);
        return;
    case 'c':
    case 'C':
        haUiOpen(HA_VIEW_CONSOLE);
        return;
    case 'r':
    case 'R':
        haHostResetScores();
        haUiForce = true;
        return;
    case 'e':
    case 'E':
        haHostRoundEnd();
        haUiForce = true;
        return;
    case 'n':
    case 'N':
        strlcpy(haUiEdit, haHostSsid(), sizeof(haUiEdit));
        haUiOpen(HA_VIEW_SSID);
        return;
    case 'p':
    case 'P':
        haHostTogglePortal();
        haUiForce = true;
        return;
    default:
        return;
    }
}

static void haUiEnter() {
    if(haUiView == HA_VIEW_GAMES) {
        haHostSelectGame(HA_UI_GAMES[haUiCursor].id);
        haUiOpen(HA_VIEW_DASH);
    } else if(haUiView == HA_VIEW_SSID) {
        if(haUiEdit[0]) haHostApplySsid(haUiEdit);
        haUiOpen(HA_VIEW_DASH);
    }
    haUiForce = true;
}

static void haUiDel() {
    if(haUiView == HA_VIEW_SSID) {
        size_t n = strlen(haUiEdit);
        if(n) haUiEdit[n - 1] = '\0';
    } else {
        haUiOpen(HA_VIEW_DASH);
    }
    haUiForce = true;
}

static void haUiPumpKeys() {
    if(!M5Cardputer.Keyboard.isChange()) return;
    if(!M5Cardputer.Keyboard.isPressed()) return;
    auto st = M5Cardputer.Keyboard.keysState();
    for(auto c : st.word) haUiChar(c);
    if(st.del) haUiDel();
    if(st.enter) haUiEnter();
}

// Redraw when the mirror moved or a key changed the view, rate-limited so a busy
// game (pong ticks at 30Hz) can't spend all its time pushing pixels. The 1Hz
// floor keeps the battery percentage honest.
static void haUiTick() {
    uint32_t now = millis();
    bool changed = haUiForce || haHost.rev != haUiDrawnRev;
    if(changed && now - haUiLastDraw < 100) return;
    if(!changed && now - haUiLastDraw < 1000) return;
    haUiDraw();
}
