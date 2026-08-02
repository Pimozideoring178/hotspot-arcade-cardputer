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

// Same list, same order as the Flipper's game_select scene. `duel` marks the 1v1
// challenge games (they pair players off into matches); `desc` is a one-line blurb
// shown under the selection.
struct HaGameItem {
    uint8_t id;
    const char* label;
    const char* desc;
    bool duel;
};
static const HaGameItem HA_UI_GAMES[] = {
    {HA_GAME_TRIVIA, "Trivia", "Quiz, fastest right wins", false},
    {HA_GAME_WYR, "Would You Rather", "Group vote, A or B", false},
    {HA_GAME_SCRAMBLE, "Word Scramble", "Unscramble the word", false},
    {HA_GAME_SPECTRUM, "Spectrum", "Give a clue, dial to guess", false},
    {HA_GAME_KMK, "Kiss Marry Kill", "Predict a player's picks", false},
    {HA_GAME_REACT, "Reaction Duel", "Tap on green, fastest wins", false},
    {HA_GAME_CONNECT4, "Connect Four", "Four in a row", true},
    {HA_GAME_TICTACTOE, "Tic-Tac-Toe", "Three in a row", true},
    {HA_GAME_DOTS, "Dots & Boxes", "Close the most boxes", true},
    {HA_GAME_REVERSI, "Reversi", "Flip discs, most wins", true},
    {HA_GAME_DRAW, "Drawing", "Draw it, others guess", false},
    {HA_GAME_PONG, "Pong", "Classic paddle rally", true},
    {HA_GAME_GUESSCOLOR, "Guess the Color", "Match the RGB colour", false},
    {HA_GAME_BATTLESHIP, "Battleship", "Hide a fleet, sink theirs", true},
    {HA_GAME_NONE, "None (lobby)", "Just the join lobby", false},
};
static const int HA_UI_GAME_COUNT = sizeof(HA_UI_GAMES) / sizeof(HA_UI_GAMES[0]);

static const char* haUiGameLabel(uint8_t id) {
    for(int i = 0; i < HA_UI_GAME_COUNT; i++)
        if(HA_UI_GAMES[i].id == id) return HA_UI_GAMES[i].label;
    return "None";
}

enum HaUiView {
    HA_VIEW_DASH,
    HA_VIEW_GAMES,
    HA_VIEW_BOARD,
    HA_VIEW_CONSOLE,
    HA_VIEW_SSID,
    HA_VIEW_SETTINGS
};

// Audio level (0 off / 1 low / 2 high) lives in the .ino (the speaker jingles are
// there); the settings screen reads and cycles it.
extern uint8_t haAudioLevel;
#define HA_SET_COUNT 4 // settings rows: SSID, Audio, AP, Event log

static M5Canvas haUiCanvas(&M5Cardputer.Display);
static bool haUiSprite = false;
static HaUiView haUiView = HA_VIEW_DASH;
static int haUiCursor = 0;
static int haUiScroll = 0;
static int haUiDashScroll = 0; // dashboard player-list scroll offset
static char haUiEdit[33] = "";
static uint8_t haGameSort = 0;       // game picker order: 0 alphabetical, 1 most played
static uint16_t haGamePlays[16] = {}; // rough per-game play count (indexed by game id)
static int haGamesOrder[HA_UI_GAME_COUNT]; // display order, filled per sort mode
static int haHistIdx = 0;            // leaderboard/history: 0 = newest loaded session
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

// Two-column live scoreboard at the small font, so all 10 (the softAP max) fit on
// one screen. Columns fill in rank order down the left, then down the right; each
// cell reads "rank.nick:score".
static void haUiDrawScoreCols(lgfx::LovyanGFX* g, uint8_t* order, int n, int top, int rowsPerCol) {
    g->setTextSize(1);
    const int rowH = 13;
    for(int i = 0; i < n && i < rowsPerCol * 2; i++) {
        int col = i / rowsPerCol, row = i % rowsPerCol;
        int x = col ? HA_UI_W / 2 + 4 : 3;
        int y = top + row * rowH;
        const HaHostPlayer& p = haUiSnap.p[order[i]];
        g->setTextColor(i == 0 ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        char nk[10], cell[24];
        snprintf(nk, sizeof(nk), "%s", p.nick); // clip nick to ~9 chars per column
        snprintf(cell, sizeof(cell), "%d.%s:%ld", i + 1, nk, (long)p.score);
        g->drawString(cell, x, y);
    }
}

static void haUiDrawDash(lgfx::LovyanGFX* g) {
    haUiHeader(g, "HOTSPOT ARCADE");
    g->setTextFont(1);
    g->setTextSize(1);

    // Line 1: SSID + the join URL, on one line.
    char line[80];
    g->setTextColor(haUiSnap.portalRunning ? TFT_GREEN : TFT_RED, TFT_BLACK);
    snprintf(line, sizeof(line), "%s  http://%s", haHostSsid(), haHostIp().c_str());
    g->drawString(line, 3, 15);

    // Line 2: active game + player count.
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);
    g->setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(line, sizeof(line), "Game: %s   Players: %d", haUiGameLabel(haUiSnap.activeGame), n);
    g->drawString(line, 3, 27);

    g->drawFastHLine(0, 38, HA_UI_W, TFT_DARKGREY);

    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString("waiting for phones to join...", 3, 46);
    } else {
        haUiDrawScoreCols(g, order, n, 44, 5); // 2 columns x 5 = up to 10
        if(n > 10) {
            g->setTextColor(TFT_DARKGREY, TFT_BLACK);
            snprintf(line, sizeof(line), "+%d more", n - 10);
            g->drawString(line, 3, HA_UI_H - 22);
        }
    }

    if(haUiSnap.lastEvent[0]) {
        g->setTextFont(1);
        g->setTextSize(1);
        g->setTextColor(TFT_YELLOW, TFT_BLACK);
        g->drawString(haUiSnap.lastEvent, 3, HA_UI_H - 22);
    }
    haUiFooter(g, "G game   L board   S settings   E end");
}

#define HA_GAMES_ROW 16 // px per game row at text size 2

// Fill haGamesOrder for the current sort mode. "None (lobby)" is always kept last.
static void haUiComputeGamesOrder() {
    int m = 0;
    for(int i = 0; i < HA_UI_GAME_COUNT; i++)
        if(HA_UI_GAMES[i].id != HA_GAME_NONE) haGamesOrder[m++] = i;
    // insertion sort: alphabetical by label, or by play count desc
    for(int a = 1; a < m; a++) {
        int k = haGamesOrder[a], j = a - 1;
        while(j >= 0) {
            bool swap;
            if(haGameSort == 1)
                swap = haGamePlays[HA_UI_GAMES[haGamesOrder[j]].id] <
                       haGamePlays[HA_UI_GAMES[k].id];
            else
                swap = strcmp(HA_UI_GAMES[haGamesOrder[j]].label, HA_UI_GAMES[k].label) > 0;
            if(!swap) break;
            haGamesOrder[j + 1] = haGamesOrder[j];
            j--;
        }
        haGamesOrder[j + 1] = k;
    }
    for(int i = 0; i < HA_UI_GAME_COUNT; i++) // append None last
        if(HA_UI_GAMES[i].id == HA_GAME_NONE) haGamesOrder[m++] = i;
}

static void haUiDrawGames(lgfx::LovyanGFX* g) {
    haUiComputeGamesOrder();
    char title[32];
    snprintf(title, sizeof(title), "GAMES - %s", haGameSort == 1 ? "MOST PLAYED" : "A-Z");
    haUiHeader(g, title);

    int descY = HA_UI_H - 22;
    int rows = (descY - 14) / HA_GAMES_ROW;
    if(rows < 1) rows = 1;
    if(haUiCursor < haUiScroll) haUiScroll = haUiCursor;
    if(haUiCursor >= haUiScroll + rows) haUiScroll = haUiCursor - rows + 1;

    g->setTextSize(2);
    int y = 15;
    for(int i = haUiScroll; i < HA_UI_GAME_COUNT && i < haUiScroll + rows; i++) {
        const HaGameItem& it = HA_UI_GAMES[haGamesOrder[i]];
        bool sel = (i == haUiCursor);
        bool live = (it.id == haUiSnap.activeGame);
        if(sel) g->fillRect(0, y - 1, HA_UI_W, HA_GAMES_ROW, TFT_BLUE);
        g->setTextColor(live ? TFT_GREEN : TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
        char nm[18];
        snprintf(nm, sizeof(nm), "%s%s", live ? "*" : "", it.label);
        g->drawString(nm, 3, y);
        if(it.duel) {
            g->setTextSize(1);
            g->setTextColor(sel ? TFT_WHITE : TFT_ORANGE, sel ? TFT_BLUE : TFT_BLACK);
            g->drawString("1v1", HA_UI_W - 22, y + 4);
            g->setTextSize(2);
        }
        y += HA_GAMES_ROW;
    }
    g->setTextSize(1);

    // Selected game's one-line description.
    g->fillRect(0, descY - 2, HA_UI_W, 12, TFT_BLACK);
    g->setTextColor(TFT_CYAN, TFT_BLACK);
    g->drawString(HA_UI_GAMES[haGamesOrder[haUiCursor]].desc, 3, descY);

    haUiFooter(g, ";/. move  S sort  ENTER pick  ESC back");
}

// The Leaderboard always shows the current session's live standings (they're
// auto-saved to the SD card when this screen is opened, so they survive a restart).
// R clears the scores to start a new session.
static void haUiDrawBoard(lgfx::LovyanGFX* g) {
    haUiHeader(g, "LEADERBOARD");
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haUiSorted(order);
    if(n == 0) {
        g->setTextColor(TFT_DARKGREY, TFT_BLACK);
        g->drawString("no players yet", 3, 18);
    } else {
        haUiDrawScoreCols(g, order, n, 16, 5); // same 2 columns x 5 as the dashboard
    }
    haUiFooter(g, haSdOk ? "R reset scores   ESC back" : "no SD   R reset   ESC back");
}

static const char* haUiAudioName() {
    return haAudioLevel == 0 ? "off" : haAudioLevel == 1 ? "low" : "high";
}

static void haUiDrawSettings(lgfx::LovyanGFX* g) {
    haUiHeader(g, "SETTINGS");
    char l0[48], l1[24], l2[20];
    snprintf(l0, sizeof(l0), "SSID: %s", haHostSsid());
    snprintf(l1, sizeof(l1), "Audio: %s", haUiAudioName());
    snprintf(l2, sizeof(l2), "AP: %s", haUiSnap.portalRunning ? "on" : "off");
    const char* items[HA_SET_COUNT] = {l0, l1, l2, "Show event log"};

    g->setTextSize(1);
    int y = 22;
    for(int i = 0; i < HA_SET_COUNT; i++) {
        bool sel = (i == haUiCursor);
        if(sel) g->fillRect(0, y - 2, HA_UI_W, 13, TFT_BLUE);
        g->setTextColor(TFT_WHITE, sel ? TFT_BLUE : TFT_BLACK);
        char buf[40];
        snprintf(buf, sizeof(buf), "%.38s", items[i]);
        g->drawString(buf, 4, y);
        y += 16;
    }
    haUiFooter(g, ";/. move   ENTER change   ESC back");
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
    case HA_VIEW_SETTINGS:
        haUiDrawSettings(g);
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
        haUiComputeGamesOrder(); // cursor is a position in the sorted display order
        for(int i = 0; i < HA_UI_GAME_COUNT; i++)
            if(HA_UI_GAMES[haGamesOrder[i]].id == haUiSnap.activeGame) haUiCursor = i;
    } else if(v == HA_VIEW_BOARD) {
        haHistSaveCurrent(); // persist the current standings whenever the board is opened
    }
    haUiForce = true;
}

// ESC/back: SSID and the event log are reached from Settings, so they return there;
// everything else returns to the dashboard.
static void haUiBack() {
    if(haUiView == HA_VIEW_SSID || haUiView == HA_VIEW_CONSOLE)
        haUiOpen(HA_VIEW_SETTINGS);
    else
        haUiOpen(HA_VIEW_DASH);
}

static void haUiChar(char c) {
    if(haUiView == HA_VIEW_SSID) {
        if(c == '`') { // esc -> back to settings
            haUiBack();
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
        haUiBack();
        return;
    case ';': // up
        if(haUiView == HA_VIEW_GAMES && haUiCursor > 0) haUiCursor--;
        else if(haUiView == HA_VIEW_SETTINGS && haUiCursor > 0) haUiCursor--;
        haUiForce = true;
        return;
    case '.': // down
        if(haUiView == HA_VIEW_GAMES && haUiCursor < HA_UI_GAME_COUNT - 1) haUiCursor++;
        else if(haUiView == HA_VIEW_SETTINGS && haUiCursor < HA_SET_COUNT - 1) haUiCursor++;
        haUiForce = true;
        return;
    case ',': // left = page up (whole screen)
        if(haUiView == HA_VIEW_GAMES) haUiCursor = haUiCursor > 6 ? haUiCursor - 6 : 0;
        haUiForce = true;
        return;
    case '/': // right = page down
        if(haUiView == HA_VIEW_GAMES)
            haUiCursor = haUiCursor + 6 < HA_UI_GAME_COUNT ? haUiCursor + 6 : HA_UI_GAME_COUNT - 1;
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
    case 's':
    case 'S':
        if(haUiView == HA_VIEW_GAMES) { // in the picker, S toggles the sort order
            haGameSort ^= 1;
            haUiCursor = 0;
            haUiScroll = 0;
        } else {
            haUiOpen(HA_VIEW_SETTINGS);
        }
        haUiForce = true;
        return;
    case 'r':
    case 'R':
        if(haUiView == HA_VIEW_BOARD) haHostResetScores(); // reset lives on the board
        haUiForce = true;
        return;
    case 'e':
    case 'E':
        haHostRoundEnd();
        haUiForce = true;
        return;
    default:
        return;
    }
}

static void haUiEnter() {
    if(haUiView == HA_VIEW_GAMES) {
        const HaGameItem& it = HA_UI_GAMES[haGamesOrder[haUiCursor]];
        if(it.id != HA_GAME_NONE) haGamePlays[it.id]++; // rough play tally for "most played"
        haHostSelectGame(it.id);
        haUiOpen(HA_VIEW_DASH);
    } else if(haUiView == HA_VIEW_SSID) {
        if(haUiEdit[0]) haHostApplySsid(haUiEdit);
        haUiOpen(HA_VIEW_SETTINGS); // back to settings, where SSID lives
    } else if(haUiView == HA_VIEW_SETTINGS) {
        switch(haUiCursor) {
        case 0: // SSID -> open the editor
            strlcpy(haUiEdit, haHostSsid(), sizeof(haUiEdit));
            haUiView = HA_VIEW_SSID;
            break;
        case 1: // Audio -> cycle off/low/high
            haAudioLevel = (uint8_t)((haAudioLevel + 1) % 3);
            break;
        case 2: // AP -> toggle
            haHostTogglePortal();
            break;
        case 3: // Event log
            haUiView = HA_VIEW_CONSOLE;
            break;
        }
    }
    haUiForce = true;
}

static void haUiDel() {
    if(haUiView == HA_VIEW_SSID) {
        size_t n = strlen(haUiEdit);
        if(n) haUiEdit[n - 1] = '\0';
    } else {
        haUiBack();
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
