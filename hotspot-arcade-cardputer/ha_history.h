// Session history on the microSD card.
//
// A "session" is a scoreboard snapshot: when the host resets scores, the current
// standings are archived here first, so the Leaderboard screen can look back at
// past games. Stored as a plain text file the user can read or delete:
//
//   /hotspot-arcade/history.txt
//   SESSION <num> <playerCount>
//   <score>\t<nick>
//   ...
//
// There is no real clock offline (the Cardputer has no RTC), so a session is
// identified by a running number kept in NVS. Real date/time will come once the
// phone-time feature lands upstream -- the format has room to grow.
#pragma once
#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include "ha_host.h"

#define HA_HIST_MAX 20 // most recent sessions kept in RAM for the viewer
static const char* HA_HIST_DIR = "/hotspot-arcade";
static const char* HA_HIST_PATH = "/hotspot-arcade/history.txt";

struct HaHistPlayer {
    char nick[HA_NICK_LEN];
    int32_t score;
};
struct HaHistSession {
    uint32_t num;
    int count;
    HaHistPlayer p[HA_MAX_PLAYERS];
};
struct HaHistBuf {
    HaHistSession s[HA_HIST_MAX]; // oldest..newest of the last HA_HIST_MAX
    int count;
};
static HaHistBuf haHist;

extern bool haSdOk; // set by the .ino's SD mount

// Sorted (score desc) list of currently-used pids into out[]; returns the count.
static int haHistSortLive(uint8_t* out) {
    int n = 0;
    for(uint8_t i = 1; i <= HA_MAX_PLAYERS; i++)
        if(haHost.p[i].used) out[n++] = i;
    for(int a = 1; a < n; a++) {
        uint8_t k = out[a];
        int j = a - 1;
        while(j >= 0 && haHost.p[out[j]].score < haHost.p[k].score) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = k;
    }
    return n;
}

// Save the CURRENT standings to SD, overwriting the single current-session file, so
// the leaderboard's live scores are always persisted (viewing the board triggers
// this). Not a per-view append -- one rolling snapshot.
static void haHistSaveCurrent() {
    if(!haSdOk) return;
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haHistSortLive(order);
    if(n == 0) return;
    SD.mkdir(HA_HIST_DIR);
    SD.remove("/hotspot-arcade/current.txt"); // truncate by removing first
    File f = SD.open("/hotspot-arcade/current.txt", FILE_WRITE);
    if(!f) return;
    f.printf("CURRENT %d\n", n);
    for(int i = 0; i < n; i++)
        f.printf("%ld\t%s\n", (long)haHost.p[order[i]].score, haHost.p[order[i]].nick);
    f.close();
}

// Archive the current standings as a new session. No-op if no SD or no players.
static void haHistSave() {
    if(!haSdOk) return;
    uint8_t order[HA_MAX_PLAYERS + 1];
    int n = haHistSortLive(order);
    if(n == 0) return;

    Preferences prefs;
    uint32_t num = 1;
    if(prefs.begin("ha_hist", false)) {
        num = prefs.getUInt("n", 0) + 1;
        prefs.putUInt("n", num);
        prefs.end();
    }
    SD.mkdir(HA_HIST_DIR);
    File f = SD.open(HA_HIST_PATH, FILE_APPEND);
    if(!f) return;
    f.printf("SESSION %u %d\n", num, n);
    for(int i = 0; i < n; i++)
        f.printf("%ld\t%s\n", (long)haHost.p[order[i]].score, haHost.p[order[i]].nick);
    f.close();
}

// Push a parsed session into the ring, keeping the newest HA_HIST_MAX.
static void haHistPush(const HaHistSession& s) {
    if(haHist.count < HA_HIST_MAX) {
        haHist.s[haHist.count++] = s;
    } else {
        for(int i = 1; i < HA_HIST_MAX; i++) haHist.s[i - 1] = haHist.s[i];
        haHist.s[HA_HIST_MAX - 1] = s;
    }
}

// Load the most recent sessions from SD into haHist (oldest..newest).
static void haHistLoad() {
    haHist.count = 0;
    if(!haSdOk) return;
    File f = SD.open(HA_HIST_PATH, FILE_READ);
    if(!f) return;
    HaHistSession cur = {};
    bool in = false;
    while(f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if(line.startsWith("SESSION")) {
            if(in) haHistPush(cur);
            cur = HaHistSession{};
            unsigned num = 0, cnt = 0;
            sscanf(line.c_str(), "SESSION %u %u", &num, &cnt);
            cur.num = num;
            cur.count = 0; // counted from the actual player lines
            in = true;
        } else if(in && line.length()) {
            int tab = line.indexOf('\t');
            if(tab > 0 && cur.count < HA_MAX_PLAYERS) {
                cur.p[cur.count].score = atol(line.substring(0, tab).c_str());
                strlcpy(cur.p[cur.count].nick, line.substring(tab + 1).c_str(), HA_NICK_LEN);
                cur.count++;
            }
        }
    }
    if(in) haHistPush(cur);
    f.close();
}
