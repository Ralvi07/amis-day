// ============================================================================
//  AMI'S DAY  -  a little 8-bit style day-out game for native Windows
//  Pure Win32 + GDI + winmm (no external libraries). All art and sound effects
//  are generated in code; the background music is embedded in the executable.
//
//  Controls:  Arrow keys = walk    Z = interact / confirm    X = cancel
//             M = music on/off     F11 = fullscreen
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include "resource.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ------------------------------------------------------------------ config
static const int VW = 320, VH = 240, TS = 16;
static const int MAXW = 34, MAXH = 26;

constexpr uint32_t RGB32(int r, int g, int b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }

static uint32_t   g_pix[VW * VH];
static BITMAPINFO g_bmi;
static HWND       g_hwnd;
static bool       g_running = true, g_fullscreen = false;
static WINDOWPLACEMENT g_wp = { sizeof(WINDOWPLACEMENT) };
static int        g_tick = 0;

static const uint32_t C_GRASS_MID   = RGB32( 63, 143,  42);
static const uint32_t C_GRASS_DARK  = RGB32( 40, 104,  32);
static const uint32_t C_GRASS_LIGHT = RGB32( 98, 180,  58);
static const uint32_t C_SHADOW      = RGB32( 30,  82,  28);
static const uint32_t C_WHITE       = RGB32(250, 250, 252);
static const uint32_t C_BLACK       = RGB32(  0,   0,   0);
static const uint32_t C_GOLD        = RGB32(255, 214,  90);

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
};
static Rng g_rng(777);

// ------------------------------------------------------------------ pixel helpers
static inline void PutPx(int x, int y, uint32_t c) { if ((unsigned)x < (unsigned)VW && (unsigned)y < (unsigned)VH) g_pix[y * VW + x] = c; }

static void PxRect(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VW ? VW : x + w, y1 = y + h > VH ? VH : y + h;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx)
            g_pix[yy * VW + xx] = c;
}

static inline uint32_t Mix(uint32_t a, uint32_t b, int t) {   // t: 0..256
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB32(ar + (br - ar) * t / 256, ag + (bg - ag) * t / 256, ab + (bb - ab) * t / 256);
}

static void DimRect(int x, int y, int w, int h, int f) {      // f: 0..256 brightness
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VW ? VW : x + w, y1 = y + h > VH ? VH : y + h;
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) {
            uint32_t c = g_pix[yy * VW + xx];
            g_pix[yy * VW + xx] = RGB32((int)(((c >> 16) & 255) * f >> 8), (int)(((c >> 8) & 255) * f >> 8), (int)((c & 255) * f >> 8));
        }
}

static void FillCircle(int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r + r / 2) PutPx(cx + x, cy + y, c);
}

// ------------------------------------------------------------------ 5x7 pixel font (ASCII 32..126, column bytes, bit0 = top)
static const uint8_t FONT[95 * 5] = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x56,0x20,0x50, 0x00,0x08,0x07,0x03,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x2A,0x1C,0x7F,0x1C,0x2A, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x80,0x70,0x30,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x00,0x60,0x60,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x72,0x49,0x49,0x49,0x46, 0x21,0x41,0x49,0x4D,0x33,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x31, 0x41,0x21,0x11,0x09,0x07,
0x36,0x49,0x49,0x49,0x36, 0x46,0x49,0x49,0x29,0x1E, 0x00,0x00,0x14,0x00,0x00, 0x00,0x40,0x34,0x00,0x00,
0x00,0x08,0x14,0x22,0x41, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x59,0x09,0x06,
0x3E,0x41,0x5D,0x59,0x4E, 0x7C,0x12,0x11,0x12,0x7C, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x41,0x3E, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x41,0x51,0x73,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x1C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x26,0x49,0x49,0x49,0x32,
0x03,0x01,0x7F,0x01,0x03, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x03,0x04,0x78,0x04,0x03, 0x61,0x59,0x49,0x4D,0x43, 0x00,0x7F,0x41,0x41,0x41,
0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x41,0x7F, 0x04,0x02,0x01,0x02,0x04, 0x40,0x40,0x40,0x40,0x40,
0x00,0x03,0x07,0x08,0x00, 0x20,0x54,0x54,0x78,0x40, 0x7F,0x28,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x28,
0x38,0x44,0x44,0x28,0x7F, 0x38,0x54,0x54,0x54,0x18, 0x00,0x08,0x7E,0x09,0x02, 0x18,0xA4,0xA4,0x9C,0x78,
0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x40,0x3D,0x00, 0x7F,0x10,0x28,0x44,0x00,
0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x78,0x04,0x78, 0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38,
0xFC,0x18,0x24,0x24,0x18, 0x18,0x24,0x24,0x18,0xFC, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x24,
0x04,0x04,0x3F,0x44,0x24, 0x3C,0x40,0x40,0x20,0x7C, 0x1C,0x20,0x40,0x20,0x1C, 0x3C,0x40,0x30,0x40,0x3C,
0x44,0x28,0x10,0x28,0x44, 0x4C,0x90,0x90,0x90,0x7C, 0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00,
0x00,0x00,0x77,0x00,0x00, 0x00,0x41,0x36,0x08,0x00, 0x02,0x01,0x02,0x04,0x02
};

static void DrawChar(int x, int y, char ch, uint32_t col, int sc) {
    if (ch < 32 || ch > 126) return;
    const uint8_t* g = &FONT[(ch - 32) * 5];
    for (int c = 0; c < 5; ++c)
        for (int r = 0; r < 8; ++r)
            if ((g[c] >> r) & 1) PxRect(x + c * sc, y + r * sc, sc, sc, col);
}
static int TxtW(const std::string& s, int sc = 1) { return (int)s.size() * 6 * sc; }
static void Txt(int x, int y, const std::string& s, uint32_t col, int sc = 1, bool shadow = true) {
    if (shadow) for (size_t i = 0; i < s.size(); ++i) DrawChar(x + (int)i * 6 * sc + sc, y + sc, s[i], RGB32(16, 16, 28), sc);
    for (size_t i = 0; i < s.size(); ++i) DrawChar(x + (int)i * 6 * sc, y, s[i], col, sc);
}
static void TxtC(int cx, int y, const std::string& s, uint32_t col, int sc = 1, bool shadow = true) { Txt(cx - TxtW(s, sc) / 2, y, s, col, sc, shadow); }

static std::vector<std::string> Wrap(const std::string& s, int maxc) {
    std::vector<std::string> out; std::string line, word;
    for (size_t i = 0; i <= s.size(); ++i) {
        char c = i < s.size() ? s[i] : ' ';
        if (c == ' ' || c == '\n') {
            if (!word.empty()) {
                if (line.empty()) line = word;
                else if ((int)(line.size() + 1 + word.size()) <= maxc) line += " " + word;
                else { out.push_back(line); line = word; }
                word.clear();
            }
            if (c == '\n') { out.push_back(line); line.clear(); }
        } else word += c;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

// ------------------------------------------------------------------ keyboard
static bool g_kd[256], g_kp[256];
static void PollKeys() { for (int i = 0; i < 256; ++i) { g_kp[i] = g_kd[i]; g_kd[i] = (GetAsyncKeyState(i) & 0x8000) != 0; } }
static inline bool Down(int vk) { return g_kd[vk & 255]; }
static inline bool Pressed(int vk) { return g_kd[vk & 255] && !g_kp[vk & 255]; }

// ------------------------------------------------------------------ sound (synthesised) + music (MCI)
static const int SR = 22050;
enum { SND_CHIRP0, SND_CHIRP1, SND_CHIRP2, SND_BLIP, SND_OK, SND_BACK, SND_COUNT };
static std::vector<uint8_t> g_wav[SND_COUNT];

static void Tone(std::vector<int16_t>& s, float f0, float f1, float ms, float amp) {
    int n = (int)(SR * ms / 1000.f); float ph = 0;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)n, f = f0 + (f1 - f0) * t;
        ph += 2.f * 3.14159265f * f / SR;
        float env = sinf(3.14159265f * t);
        float v = sinf(ph) + 0.25f * sinf(2.f * ph);
        s.push_back((int16_t)(v * env * amp * 20000.f));
    }
}
static void Gap(std::vector<int16_t>& s, float ms) { s.insert(s.end(), (size_t)(SR * ms / 1000.f), (int16_t)0); }
static void MakeWav(std::vector<uint8_t>& out, const std::vector<int16_t>& s) {
    uint32_t bytes = (uint32_t)s.size() * 2;
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back((uint8_t)((v >> (8 * i)) & 255)); };
    auto w16 = [&](uint16_t v) { out.push_back((uint8_t)(v & 255)); out.push_back((uint8_t)(v >> 8)); };
    auto tag = [&](const char* t) { for (int i = 0; i < 4; ++i) out.push_back((uint8_t)t[i]); };
    tag("RIFF"); w32(36 + bytes); tag("WAVE"); tag("fmt "); w32(16); w16(1); w16(1); w32(SR); w32(SR * 2); w16(2); w16(16);
    tag("data"); w32(bytes);
    for (size_t i = 0; i < s.size(); ++i) w16((uint16_t)s[i]);
}
static void BuildSounds() {
    std::vector<int16_t> s;
    Tone(s, 2800, 3900, 70, 0.35f); Gap(s, 40); Tone(s, 2600, 3700, 70, 0.30f); Gap(s, 30); Tone(s, 3000, 4000, 60, 0.25f);
    MakeWav(g_wav[SND_CHIRP0], s); s.clear();
    for (int i = 0; i < 6; ++i) { Tone(s, 3300.f + (i % 2) * 500.f, 3500.f + (i % 2) * 400.f, 35, 0.30f); Gap(s, 14); }
    MakeWav(g_wav[SND_CHIRP1], s); s.clear();
    Tone(s, 4200, 3000, 140, 0.35f); Gap(s, 60); Tone(s, 3800, 2800, 110, 0.25f);
    MakeWav(g_wav[SND_CHIRP2], s); s.clear();
    Tone(s, 700, 700, 30, 0.22f);  MakeWav(g_wav[SND_BLIP], s); s.clear();
    Tone(s, 900, 1400, 80, 0.25f); MakeWav(g_wav[SND_OK], s);   s.clear();
    Tone(s, 500, 300, 70, 0.25f);  MakeWav(g_wav[SND_BACK], s);
}
static void Snd(int id) {
    if (id < 0 || id >= SND_COUNT || g_wav[id].empty()) return;
    PlaySoundA((LPCSTR)g_wav[id].data(), NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static bool g_musicOn = true, g_musicOpen = false;
static wchar_t g_musicFile[MAX_PATH] = {};
static void MusicStop() {
    if (g_musicOpen) {
        mciSendStringW(L"close bgm", NULL, 0, NULL);
        g_musicOpen = false;
    }
    if (g_musicFile[0] && DeleteFileW(g_musicFile)) g_musicFile[0] = L'\0';
}
static void MusicStart(HWND hwnd) {
    HMODULE module = GetModuleHandleW(NULL);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_MUSIC), MAKEINTRESOURCEW(10));
    if (!resource) return;
    DWORD size = SizeofResource(module, resource);
    HGLOBAL loaded = LoadResource(module, resource);
    const void* bytes = loaded ? LockResource(loaded) : nullptr;
    if (!bytes || !size) return;

    wchar_t tempDir[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, tempDir);
    if (!length || length >= MAX_PATH) return;
    if (!GetTempFileNameW(tempDir, L"ami", 0, g_musicFile)) return;
    HANDLE file = CreateFileW(g_musicFile, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING,
                              FILE_ATTRIBUTE_TEMPORARY, NULL);
    DWORD written = 0;
    bool ready = false;
    if (file != INVALID_HANDLE_VALUE) {
        ready = WriteFile(file, bytes, size, &written, NULL) && written == size;
        CloseHandle(file);
    }
    std::wstring cmd = std::wstring(L"open \"") + g_musicFile + L"\" type mpegvideo alias bgm";
    if (ready && mciSendStringW(cmd.c_str(), NULL, 0, NULL) == 0) {
        g_musicOpen = true;
        mciSendStringA("setaudio bgm volume to 650", NULL, 0, NULL);
        mciSendStringA("play bgm notify", NULL, 0, hwnd);
    } else {
        MusicStop();
    }
}
static void MusicToggle() {
    if (!g_musicOpen) return;
    g_musicOn = !g_musicOn;
    mciSendStringA(g_musicOn ? "resume bgm" : "pause bgm", NULL, 0, NULL);
}

// ------------------------------------------------------------------ game data
enum Period { P_MORNING, P_NOON, P_AFTERNOON, P_EVENING, P_NIGHT };
enum BId { B_OTTOS, B_CUSTOM, B_MOCHERI, B_MULLIRI, B_YAMATO, B_PASTARIA, B_GOFISH, B_HOME, B_CHARLES, B_LAKE, B_COUNT };
enum { ICON_NONE, ICON_CUP, ICON_FISH, ICON_PLATE, ICON_STAR, ICON_HEART };
enum { T_GRASS, T_PATH, T_WATER, T_SAND, T_DECK };
enum State { ST_TITLE, ST_WALK, ST_DIALOG, ST_MENU, ST_NAMING, ST_FADE_OUT, ST_FADE_IN, ST_CARD, ST_END };
enum Dir { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };

struct BInfo { const char* name; uint32_t wall, roof, a1, a2; int icon; };
static const BInfo BINFO[B_COUNT] = {
    {"Ottos",      RGB32(160,110, 80), RGB32(110, 60, 40), RGB32(200, 50, 50), RGB32(250,250,250), ICON_CUP},
    {"",           RGB32(200,180,230), RGB32(125, 95,175), RGB32(255,200, 60), RGB32(255,255,255), ICON_STAR},
    {"Mocheri",    RGB32(245,195,205), RGB32(165, 80,115), RGB32(115, 65, 45), RGB32(250,225,205), ICON_CUP},
    {"Mulliri",    RGB32(155,205,195), RGB32( 50,115,115), RGB32( 60,120,165), RGB32(240,240,240), ICON_CUP},
    {"Yamato",     RGB32(238,228,205), RGB32(190, 40, 45), RGB32(190, 40, 45), RGB32( 40, 40, 48), ICON_FISH},
    {"Pastaria",   RGB32(250,228,175), RGB32( 70,135, 65), RGB32(200, 50, 50), RGB32(250,250,250), ICON_PLATE},
    {"GoFish",     RGB32(155,195,238), RGB32( 40, 85,165), RGB32(250,150, 40), RGB32(250,250,250), ICON_FISH},
    {"Ami's Home", RGB32(248,238,218), RGB32(195,115, 95), RGB32(250,250,250), RGB32(240,200,210), ICON_HEART},
    {"Charles",    RGB32( 90, 55,120), RGB32( 45, 28, 75), RGB32(255,215, 85), RGB32(225, 65,165), ICON_STAR},
    {"Lake",       RGB32(0,0,0), RGB32(0,0,0), RGB32(0,0,0), RGB32(0,0,0), ICON_NONE}
};
static const char* PERIOD_NAME[5] = { "MORNING", "NOON", "AFTERNOON", "EVENING", "NIGHT" };
static const char* PERIOD_LOWER[5] = { "Morning", "Noon", "Afternoon", "Evening", "Night" };

static const char*    DRESS_NAME[4] = { "Ruby Red", "Midnight Blue", "Emerald Green", "Rose Pink" };
static const uint32_t DRESS_C1[4]   = { RGB32(190,40,70), RGB32(60,70,170), RGB32(30,140,95), RGB32(235,125,165) };
static const uint32_t DRESS_C2[4]   = { RGB32(130,20,50), RGB32(35,40,110), RGB32(15,95,65),  RGB32(185,85,125) };
static const char*    ACC_NAME[3]   = { "Pearl necklace", "Flower in her hair", "No accessory" };

struct Choices {
    std::string custom;
    int morning = -1, noon = -1, aftFirst = -1, gofish = -1, dress = -1, acc = -1;
    bool lake = false, dressed = false, charles = false;
    bool customAt[5] = { false, false, false, false, false };
};
static Choices g_ch;
static bool    g_visited[B_COUNT];
static bool    g_renameAvail = true;
static int     g_period = P_MORNING;

static std::string BName(int id) {
    if (id == B_CUSTOM) return g_ch.custom.empty() ? std::string("???") : g_ch.custom;
    return BINFO[id].name;
}

// ------------------------------------------------------------------ scene
struct Zone { int id, x0, y0, x1, y1; };
struct Bld  { int id, tx, ty, tw; };
struct Tree { int x, y, v; };
struct IPt  { int x, y; };
struct Scene {
    int w = 30, h = 22;
    uint8_t t[MAXH][MAXW];
    bool solid[MAXH][MAXW];
    std::vector<Tree> trees; std::vector<Bld> blds; std::vector<Zone> zones; std::vector<IPt> lamps;
    bool fountain = false; int fx = 0, fy = 0;
    bool bench = false; int bx = 0, by = 0;
    float sx = 0, sy = 0;
};
static Scene g_sc;

static uint32_t g_tiles[12][TS * TS];
static inline void TilePx(int v, int x, int y, uint32_t c) { if (x >= 0 && x < TS && y >= 0 && y < TS) g_tiles[v][y * TS + x] = c; }

static void BuildTiles() {
    const uint32_t petals[4] = { RGB32(250,250,250), RGB32(255,230,60), RGB32(250,120,190), RGB32(120,190,255) };
    for (int v = 0; v < 8; ++v) {
        for (int i = 0; i < TS * TS; ++i) g_tiles[v][i] = C_GRASS_MID;
        Rng r(v + 1);
        for (int i = 0; i < 16; ++i) TilePx(v, r.range(0, 15), r.range(0, 15), C_GRASS_LIGHT);
        for (int i = 0; i < 12; ++i) TilePx(v, r.range(0, 15), r.range(0, 15), C_GRASS_DARK);
        for (int i = 0; i < 3; ++i) {
            int x = r.range(2, 13), y = r.range(4, 14);
            TilePx(v, x, y, C_GRASS_DARK); TilePx(v, x, y - 1, C_GRASS_DARK); TilePx(v, x - 1, y - 1, C_GRASS_DARK);
            TilePx(v, x + 1, y - 2, C_GRASS_LIGHT); TilePx(v, x + 1, y - 1, C_GRASS_DARK);
        }
        if (v >= 4) {
            int x = r.range(3, 12), y = r.range(3, 10);
            uint32_t p = petals[v - 4], center = (v == 5) ? RGB32(240,130,30) : RGB32(255,220,50);
            TilePx(v, x, y + 2, C_GRASS_DARK); TilePx(v, x, y + 1, C_GRASS_DARK);
            TilePx(v, x - 1, y, p); TilePx(v, x + 1, y, p); TilePx(v, x, y - 1, p); TilePx(v, x, y, center);
        }
    }
    for (int v = 8; v < 10; ++v) {   // dirt path
        for (int i = 0; i < TS * TS; ++i) g_tiles[v][i] = RGB32(226,200,146);
        Rng r(100 + v);
        for (int i = 0; i < 14; ++i) TilePx(v, r.range(0, 15), r.range(0, 15), RGB32(240,222,172));
        for (int i = 0; i < 12; ++i) TilePx(v, r.range(0, 15), r.range(0, 15), RGB32(200,170,115));
    }
    for (int i = 0; i < TS * TS; ++i) g_tiles[10][i] = RGB32(240,226,172);   // sand
    { Rng r(200);
      for (int i = 0; i < 14; ++i) TilePx(10, r.range(0, 15), r.range(0, 15), RGB32(218,198,140));
      for (int i = 0; i < 8; ++i)  TilePx(10, r.range(0, 15), r.range(0, 15), RGB32(252,244,205)); }
    for (int y = 0; y < TS; ++y)                                              // wooden deck
        for (int x = 0; x < TS; ++x) {
            uint32_t c = RGB32(172,114,62);
            if (y % 8 == 7) c = RGB32(120,76,40); else if (y % 8 == 0) c = RGB32(190,134,80);
            g_tiles[11][y * TS + x] = c;
        }
    TilePx(11, 2, 3, RGB32(90,56,30)); TilePx(11, 13, 3, RGB32(90,56,30)); TilePx(11, 2, 11, RGB32(90,56,30)); TilePx(11, 13, 11, RGB32(90,56,30));
}

static int TileVariant(int tx, int ty) {
    uint32_t h = (uint32_t)tx * 73856093u ^ (uint32_t)ty * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    int r = (int)(h % 100), k = (int)((h >> 8) % 4);
    return r < 84 ? k : 4 + k;
}

static bool InMap(int x, int y) { return x >= 0 && y >= 0 && x < g_sc.w && y < g_sc.h; }
static void SetT(int x, int y, int t) { if (InMap(x, y)) g_sc.t[y][x] = (uint8_t)t; }
static void SetSolid(int x, int y) { if (InMap(x, y)) g_sc.solid[y][x] = true; }

static void Pond(int cx, int cy, int rx, int ry) {
    for (int y = 0; y < g_sc.h; ++y)
        for (int x = 0; x < g_sc.w; ++x) {
            float dx = (x - cx) / (rx + 0.5f), dy = (y - cy) / (ry + 0.5f);
            if (dx * dx + dy * dy <= 1.f && g_sc.t[y][x] == T_GRASS) g_sc.t[y][x] = T_WATER;
        }
}
static void SandRing() {
    static uint8_t tmp[MAXH][MAXW];
    memcpy(tmp, g_sc.t, sizeof(tmp));
    for (int y = 0; y < g_sc.h; ++y)
        for (int x = 0; x < g_sc.w; ++x)
            if (g_sc.t[y][x] == T_GRASS) {
                bool nearW = false;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (InMap(x + dx, y + dy) && g_sc.t[y + dy][x + dx] == T_WATER) nearW = true;
                if (nearW) tmp[y][x] = T_SAND;
            }
    memcpy(g_sc.t, tmp, sizeof(tmp));
}

static void BuildScene(int p) {
    Scene& s = g_sc;
    s.w = 30; s.h = 22;
    memset(s.t, 0, sizeof(s.t)); memset(s.solid, 0, sizeof(s.solid));
    s.trees.clear(); s.blds.clear(); s.zones.clear(); s.lamps.clear();
    s.fountain = false; s.bench = false;
    int roadY = (p == P_NOON) ? 11 : 10;
    for (int x = 0; x < s.w; ++x) { SetT(x, roadY, T_PATH); SetT(x, roadY + 1, T_PATH); }

    if (p != P_EVENING) {
        for (int y = roadY + 2; y <= 19; ++y) { SetT(14, y, T_PATH); SetT(15, y, T_PATH); }
        for (int y = 16; y <= 19; ++y) for (int x = 12; x <= 17; ++x) SetT(x, y, T_PATH);
        s.fountain = true; s.fx = 14; s.fy = 17;
        for (int y = 17; y <= 18; ++y) for (int x = 14; x <= 15; ++x) SetSolid(x, y);
        if (p == P_MORNING) Pond(5, 17, 3, 2);
        if (p == P_NOON)    Pond(24, 17, 3, 2);
    } else {
        for (int y = 14; y < s.h; ++y) for (int x = 0; x < s.w; ++x) {
            float dx = (x - 15) / 13.f, dy = (y - 21) / 7.f;
            if (dx * dx + dy * dy <= 1.f) SetT(x, y, T_WATER);
        }
        for (int y = roadY + 2; y <= 13; ++y) { SetT(14, y, T_PATH); SetT(15, y, T_PATH); }
        for (int y = 14; y <= 18; ++y) { SetT(14, y, T_DECK); SetT(15, y, T_DECK); }
        s.bench = true; s.bx = 14; s.by = 18; SetSolid(14, 18); SetSolid(15, 18);
        s.zones.push_back({ B_LAKE, 14, 16, 15, 17 });
    }
    SandRing();

    auto addB = [&](int id, int tx, int tw) {
        int ty = roadY - 5;
        s.blds.push_back({ id, tx, ty, tw });
        for (int y = ty; y <= ty + 2; ++y) for (int x = tx; x < tx + tw; ++x) SetSolid(x, y);
        int dx = tx + tw / 2;
        for (int x = dx - 1; x <= dx + 1; ++x) SetT(x, ty + 3, T_PATH);
        SetT(dx, ty + 4, T_PATH);
        s.zones.push_back({ id, dx - 1, ty + 3, dx + 1, ty + 4 });
    };
    switch (p) {
    case P_MORNING:   addB(B_OTTOS, 3, 5);  addB(B_CUSTOM, 21, 5); break;
    case P_NOON:      addB(B_MOCHERI, 2, 5); addB(B_MULLIRI, 12, 5); addB(B_CUSTOM, 22, 5); break;
    case P_AFTERNOON: addB(B_CUSTOM, 2, 5);  addB(B_YAMATO, 12, 5);  addB(B_PASTARIA, 22, 5); break;
    case P_EVENING:   addB(B_GOFISH, 3, 5);  addB(B_CUSTOM, 21, 5); break;
    default:          addB(B_HOME, 2, 5);    addB(B_CHARLES, 11, 7); addB(B_CUSTOM, 22, 5); break;
    }

    if (p >= P_EVENING) {
        int lx[4] = { 4, 10, 19, 25 };
        for (int i = 0; i < 4; ++i)
            if (s.t[roadY + 2][lx[i]] == T_GRASS) { s.lamps.push_back({ lx[i], roadY + 2 }); SetSolid(lx[i], roadY + 2); }
    }

    Rng r(p * 131 + 7);
    // forest border
    for (int y = 1; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x) {
            bool border = (y == 1) || (y == s.h - 1) || x == 0 || x == s.w - 1;
            if (border && s.t[y][x] == T_GRASS && !s.solid[y][x]) { s.trees.push_back({ x, y, r.range(0, 3) }); SetSolid(x, y); }
        }
    // scattered trees
    int want = 36;
    for (int a = 0; a < 900 && want > 0; ++a) {
        int x = r.range(2, s.w - 3), y = r.range(3, s.h - 2);
        if (s.t[y][x] != T_GRASS || s.solid[y][x]) continue;
        bool ok = true;
        for (int dy = -1; dy <= 1 && ok; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int xx = x + dx, yy = y + dy;
            if (!InMap(xx, yy)) continue;
            if (s.t[yy][xx] == T_PATH || s.t[yy][xx] == T_DECK || s.solid[yy][xx]) { ok = false; break; }
        }
        if (!ok) continue;
        s.trees.push_back({ x, y, r.range(0, 3) }); SetSolid(x, y); --want;
    }
    s.sx = 14 * 16 + 8; s.sy = (float)(roadY * 16);
}

// ------------------------------------------------------------------ player
struct Player { float x = 0, y = 0; int dir = DIR_DOWN; float anim = 0; bool moving = false; };
static Player g_pl;
static void ResetPlayer() { g_pl.x = g_sc.sx; g_pl.y = g_sc.sy; g_pl.dir = DIR_DOWN; g_pl.anim = 0; g_pl.moving = false; }

struct Look { uint32_t d1, d2; int acc; };   // acc: 0 none, 1 pearls, 2 flower
static Look CurLook() {
    if (g_ch.dressed && g_ch.dress >= 0) return { DRESS_C1[g_ch.dress], DRESS_C2[g_ch.dress], g_ch.acc == 0 ? 1 : (g_ch.acc == 1 ? 2 : 0) };
    return { RGB32(92,150,205), RGB32(62,108,160), 0 };
}

// Draws Ami. (px,py) = top-left of her 16x16 cell, sc = pixel scale.
static void DrawAmi(int px, int py, int dir, bool moving, float anim, int sc, const Look& lk) {
    auto R = [&](int a, int b, int w, int h, uint32_t c) { PxRect(px + a * sc, py + b * sc, w * sc, h * sc, c); };
    const uint32_t HAIR = RGB32(26,22,36), HI = RGB32(78,70,104), SKIN = RGB32(250,206,168), BLUSH = RGB32(244,160,150);
    const uint32_t EYE = RGB32(40,24,24), GL = RGB32(120,56,110), MOUTH = RGB32(206,84,96), WHITE = RGB32(248,248,252);
    const uint32_t SHOE = RGB32(70,40,36), PEARL = RGB32(252,240,236), FLOWER = RGB32(250,130,180), YEL = RGB32(255,230,90);
    int fr = moving ? ((int)(anim / 6.f) & 3) : 0;
    int b = (moving && (fr == 1 || fr == 3)) ? -1 : 0;           // upper-body bob
    int sway = (fr == 1) ? -1 : ((fr == 3) ? 1 : 0);
    bool liftL = (fr == 1), liftR = (fr == 3);
    uint32_t d1 = lk.d1, d2 = lk.d2;
    if (sc == 1) R(3, 15, 10, 2, C_SHADOW);

    if (dir == DIR_DOWN || dir == DIR_UP) {
        // skirt + shoes
        R(4, 11, 8, 1, d1); R(3 + sway, 12, 10, 1, d1); R(3 + sway, 13, 10, 1, d1); R(2 + sway, 14, 12, 1, d2);
        if (!liftL) R(5, 15, 2, 1, SHOE);
        if (!liftR) R(9, 15, 2, 1, SHOE);
        // torso, sleeves, belt, hands
        R(5, 8 + b, 6, 3, d1); R(4, 8 + b, 1, 3, d1); R(11, 8 + b, 1, 3, d1);
        R(4, 11, 1, 1, SKIN); R(11, 11, 1, 1, SKIN);
        R(5, 10 + b, 6, 1, d2);
        if (dir == DIR_DOWN) {
            R(6, 8 + b, 4, 1, WHITE);
            if (lk.acc == 1) R(6, 9 + b, 4, 1, PEARL);
            for (int y = 2; y <= 10; ++y) { int wv = (y >> 1) & 1; R(2 + wv, y + b, 2, 1, HAIR); R(12 - wv, y + b, 2, 1, HAIR); }
            R(4, 1 + b, 8, 7, SKIN);
            R(4, -2 + b, 8, 1, HAIR); R(3, -1 + b, 10, 2, HAIR); R(3, 1 + b, 10, 1, HAIR);
            R(4, 2 + b, 2, 1, HAIR); R(10, 2 + b, 2, 1, HAIR);
            R(5, -2 + b, 2, 1, HI); R(9, 0 + b, 2, 1, HI);
            R(4, 3 + b, 3, 3, GL); R(9, 3 + b, 3, 3, GL); R(7, 4 + b, 2, 1, GL);
            R(5, 4 + b, 1, 1, EYE); R(10, 4 + b, 1, 1, EYE);
            R(5, 6 + b, 1, 1, BLUSH); R(10, 6 + b, 1, 1, BLUSH); R(7, 6 + b, 2, 1, MOUTH);
            if (lk.acc == 2) { R(3, 0 + b, 2, 2, FLOWER); R(3, 0 + b, 1, 1, YEL); }
        } else {
            R(4, -2 + b, 8, 1, HAIR); R(3, -1 + b, 10, 3, HAIR); R(4, 2 + b, 8, 8, HAIR);
            for (int y = 2; y <= 10; ++y) { int wv = (y >> 1) & 1; R(2 + wv, y + b, 2, 1, HAIR); R(12 - wv, y + b, 2, 1, HAIR); }
            R(5, -2 + b, 2, 1, HI); R(8, 3 + b, 1, 3, HI); R(6, 7 + b, 1, 2, HI);
            if (lk.acc == 2) { R(10, 0 + b, 2, 2, FLOWER); R(11, 0 + b, 1, 1, YEL); }
        }
    } else {
        bool right = (dir == DIR_RIGHT);
        auto S = [&](int a, int bb, int w, int h, uint32_t c) { int xx = right ? a : 16 - a - w; PxRect(px + xx * sc, py + bb * sc, w * sc, h * sc, c); };
        for (int y = 1; y <= 10; ++y) { int wv = (y >> 1) & 1; S(3 + wv, y + b, 3, 1, HAIR); }
        S(4, 11, 8, 1, d1); S(3 + sway, 12, 10, 1, d1); S(3 + sway, 13, 10, 1, d1); S(2 + sway, 14, 12, 1, d2);
        if (!liftL) S(5, 15, 3, 1, SHOE);
        if (!liftR) S(8, 15, 3, 1, SHOE);
        S(5, 8 + b, 6, 3, d1); S(5, 10 + b, 6, 1, d2); S(7, 8 + b, 2, 3, d2); S(7, 11 + b, 2, 1, SKIN);
        S(8, 8 + b, 3, 1, WHITE);
        if (lk.acc == 1) S(8, 9 + b, 3, 1, PEARL);
        S(6, 1 + b, 6, 7, SKIN);
        S(4, -2 + b, 8, 1, HAIR); S(3, -1 + b, 10, 2, HAIR); S(3, 1 + b, 9, 1, HAIR); S(9, 2 + b, 3, 1, HAIR);
        S(5, 2 + b, 1, 3, HAIR); S(5, -2 + b, 2, 1, HI);
        S(8, 3 + b, 3, 3, GL); S(9, 4 + b, 1, 1, EYE); S(5, 4 + b, 3, 1, GL);
        S(12, 5 + b, 1, 1, SKIN); S(10, 6 + b, 1, 1, MOUTH); S(8, 6 + b, 1, 1, BLUSH);
        if (lk.acc == 2) { S(4, 0 + b, 2, 2, FLOWER); S(4, 0 + b, 1, 1, YEL); }
    }
}

// ------------------------------------------------------------------ world drawing
struct Glow  { int x, y, w, h; uint32_t c; };
struct Light { int x, y, r; uint32_t c; int k; };
static std::vector<Glow>  g_glows;
static std::vector<Light> g_lights;
static int g_camX = 0, g_camY = 0;

static void DrawTree(int x, int y, int v) {
    const uint32_t tr = RGB32(104,66,34), trd = RGB32(72,44,22);
    if (v & 1) {
        static const int rowX[14] = { 5,3,2,1,1,1,1,1,1,1,1,2,3,5 };
        static const int rowW[14] = { 6,10,12,14,14,14,14,14,14,14,14,12,10,6 };
        uint32_t base = (v & 2) ? RGB32(52,140,60) : RGB32(40,120,52);
        uint32_t dark = (v & 2) ? RGB32(34,100,46) : RGB32(28,86,42);
        uint32_t lite = (v & 2) ? RGB32(100,186,84) : RGB32(80,164,72);
        PxRect(x + 6, y + 14, 4, 10, tr); PxRect(x + 9, y + 14, 1, 10, trd);
        for (int r = 0; r < 14; ++r) PxRect(x + rowX[r], y + r, rowW[r], 1, base);
        for (int r = 2; r <= 6; ++r) PxRect(x + rowX[r] + 1, y + r, 2, 1, lite);
        for (int r = 7; r <= 13; ++r) PxRect(x + rowX[r] + rowW[r] - 3, y + r, 2, 1, dark);
        PutPx(x + 6, y + 8, dark); PutPx(x + 4, y + 10, dark); PutPx(x + 8, y + 4, lite); PutPx(x + 10, y + 6, lite);
    } else {
        uint32_t base = (v & 2) ? RGB32(30,100,60) : RGB32(26,88,50);
        uint32_t dark = RGB32(16,64,38), lite = RGB32(60,140,84);
        PxRect(x + 7, y + 15, 3, 9, tr);
        for (int r = 0; r < 15; ++r) {
            int tier = r / 5, loc = r % 5, w = 4 + tier * 2 + loc * 2; if (w > 14) w = 14;
            PxRect(x + 8 - w / 2, y + r, w, 1, base);
            PxRect(x + 8 + w / 2 - 2, y + r, 2, 1, dark);
            if (loc == 0) PutPx(x + 8 - w / 2 + 1, y + r, lite);
        }
    }
}

static void DrawIcon(int kind, int x, int y) {
    switch (kind) {
    case ICON_CUP:
        PxRect(x + 1, y + 3, 6, 5, C_WHITE); PxRect(x + 7, y + 4, 2, 3, C_WHITE); PxRect(x + 8, y + 5, 1, 1, RGB32(160,205,238));
        PxRect(x + 2, y + 3, 4, 1, RGB32(110,60,30)); PxRect(x + 3, y, 1, 2, RGB32(220,220,230)); PxRect(x + 5, y + 1, 1, 2, RGB32(220,220,230)); break;
    case ICON_FISH:
        PxRect(x + 1, y + 3, 6, 4, RGB32(250,150,60)); PxRect(x + 7, y + 2, 1, 6, RGB32(250,150,60)); PxRect(x + 8, y + 1, 1, 8, RGB32(250,150,60));
        PutPx(x + 2, y + 4, RGB32(30,30,40)); break;
    case ICON_PLATE:
        PxRect(x + 1, y + 3, 8, 5, C_WHITE); PxRect(x + 3, y + 4, 4, 3, RGB32(240,200,90)); PutPx(x + 4, y + 4, RGB32(200,50,40)); PutPx(x + 6, y + 5, RGB32(200,50,40)); break;
    case ICON_STAR:
        PxRect(x + 4, y, 2, 9, C_GOLD); PxRect(x, y + 4, 9, 2, C_GOLD); PxRect(x + 2, y + 2, 5, 5, C_GOLD); break;
    case ICON_HEART: {
        uint32_t c = RGB32(230,60,90);
        PxRect(x + 1, y + 2, 3, 3, c); PxRect(x + 5, y + 2, 3, 3, c); PxRect(x + 1, y + 4, 7, 2, c); PxRect(x + 2, y + 6, 5, 1, c); PxRect(x + 3, y + 7, 3, 1, c); break; }
    default: break;
    }
}

static void DrawBuilding(const Bld& b) {
    const BInfo& bi = BINFO[b.id];
    int x = b.tx * TS - g_camX, y = b.ty * TS - g_camY, w = b.tw * TS;
    uint32_t roofD = Mix(bi.roof, C_BLACK, 70), wallD = Mix(bi.wall, C_BLACK, 60);
    bool dusk = g_period >= P_EVENING;
    // walls
    PxRect(x, y + 18, w, 30, bi.wall); PxRect(x, y + 46, w, 2, wallD);
    // roof
    PxRect(x - 2, y, w + 4, 20, bi.roof);
    for (int r = 3; r < 20; r += 4) PxRect(x - 2, y + r, w + 4, 1, roofD);
    PxRect(x - 2, y + 19, w + 4, 2, roofD);
    // sign
    std::string nm = BName(b.id);
    int tw = (int)nm.size() * 6 + 6, sxp = x + w / 2 - tw / 2;
    bool fancy = (b.id == B_CHARLES);
    PxRect(sxp - 1, y + 4, tw + 2, 13, RGB32(50,34,22));
    PxRect(sxp, y + 5, tw, 11, fancy ? RGB32(30,16,50) : RGB32(245,232,200));
    Txt(sxp + 3, y + 7, nm, fancy ? C_GOLD : RGB32(60,36,24), 1, false);
    // door + awning
    int dx = x + w / 2 - 6;
    PxRect(dx, y + 28, 12, 20, RGB32(120,76,44)); PxRect(dx + 1, y + 29, 10, 1, RGB32(150,100,60));
    PxRect(dx + 9, y + 38, 2, 2, C_GOLD);
    for (int i = 0; i < 5; ++i) PxRect(x + w / 2 - 10 + i * 4, y + 22, 4, 6, (i & 1) ? bi.a2 : bi.a1);
    // windows
    int wx[2] = { x + 8, x + w - 22 };
    for (int i = 0; i < 2; ++i) {
        PxRect(wx[i] - 1, y + 25, 16, 14, RGB32(250,250,250));
        uint32_t glass = dusk ? RGB32(255,226,130) : RGB32(160,205,238);
        if (fancy && dusk) { uint32_t pc[3] = { RGB32(255,80,190), RGB32(80,220,255), RGB32(255,230,90) }; glass = pc[((g_tick / 12) + i) % 3]; }
        PxRect(wx[i], y + 26, 14, 12, glass);
        PxRect(wx[i] + 6, y + 26, 1, 12, RGB32(250,250,250));
        if (dusk) g_glows.push_back({ wx[i], y + 26, 14, 12, glass });
        if (i == 0 && !dusk) DrawIcon(bi.icon, wx[i] + 2, y + 28);
        if (i == 0 && dusk)  DrawIcon(bi.icon, wx[i] + 2, y + 28);
    }
    if (fancy && dusk) {
        for (int i = 0; i < w + 4; i += 8) {
            uint32_t pc[3] = { RGB32(255,80,190), RGB32(80,220,255), RGB32(255,230,90) };
            uint32_t c = pc[((g_tick / 10) + i / 8) % 3];
            PxRect(x - 2 + i, y + 20, 3, 3, c);
            g_glows.push_back({ x - 2 + i, y + 20, 3, 3, c });
            g_lights.push_back({ x - 1 + i, y + 21, 9, c, 90 });
        }
    }
    if (dusk) g_lights.push_back({ x + w / 2, y + 46, 26, RGB32(255,210,130), 110 });
}

static void DrawLamp(const IPt& l) {
    int x = l.x * TS - g_camX, y = l.y * TS - 8 - g_camY;
    PxRect(x + 7, y + 8, 2, 16, RGB32(50,50,62)); PxRect(x + 5, y + 22, 6, 2, RGB32(50,50,62));
    PxRect(x + 4, y + 2, 8, 2, RGB32(50,50,62)); PxRect(x + 5, y + 4, 6, 5, RGB32(255,240,170));
    g_lights.push_back({ x + 8, y + 6, 40, RGB32(255,220,140), 150 });
    g_glows.push_back({ x + 5, y + 4, 6, 5, RGB32(255,244,190) });
}

static void DrawFountain() {
    int x = g_sc.fx * TS - g_camX, y = g_sc.fy * TS - g_camY;
    PxRect(x + 1, y + 10, 30, 20, RGB32(140,140,152)); PxRect(x + 2, y + 9, 28, 20, RGB32(190,190,202));
    PxRect(x + 4, y + 12, 24, 14, RGB32(70,150,225));
    int ph = (g_tick / 8) & 3;
    for (int i = 0; i < 4; ++i) PutPx(x + 6 + ((i * 7 + ph * 3) % 20), y + 14 + ((i * 5 + ph) % 10), RGB32(170,220,250));
    PxRect(x + 14, y + 4, 4, 12, RGB32(170,170,184));
    for (int i = 0; i < 5; ++i) { int yy = y + 2 + ((g_tick / 5 + i * 2) % 9); PutPx(x + 12 + ((i * 3) % 7), yy, RGB32(200,235,255)); }
    PxRect(x + 13, y + 1, 6, 3, RGB32(210,210,222));
}

static void DrawBench() {
    int x = g_sc.bx * TS - g_camX, y = g_sc.by * TS - g_camY;
    PxRect(x + 1, y + 2, 30, 4, RGB32(150,100,58)); PxRect(x + 1, y + 6, 30, 5, RGB32(184,128,74));
    PxRect(x + 2, y + 11, 3, 4, RGB32(90,58,30)); PxRect(x + 27, y + 11, 3, 4, RGB32(90,58,30));
    // two coffee cups on the bench
    for (int i = 0; i < 2; ++i) {
        int cx = x + 8 + i * 12;
        PxRect(cx, y + 3, 5, 4, C_WHITE); PxRect(cx + 5, y + 4, 1, 2, C_WHITE); PxRect(cx, y + 3, 5, 1, RGB32(110,60,30));
        int st = (g_tick / 14 + i) & 1;
        PutPx(cx + 1 + st, y, RGB32(235,235,245)); PutPx(cx + 3 - st, y + 1 - 2 * 0, RGB32(235,235,245));
    }
}

// ------------------------------------------------------------------ birds
struct Bird { float x, y, vx, vy, ph; int turn, kind, sing; };
static std::vector<Bird> g_birds;
static int g_chirpT = 120;

static void InitBirds(int p) {
    g_birds.clear();
    int n = (p == P_MORNING || p == P_NOON) ? 8 : (p == P_AFTERNOON ? 7 : (p == P_EVENING ? 5 : 3));
    Rng r(900 + p);
    for (int i = 0; i < n; ++i) {
        Bird b; float a = r.range(0, 628) / 100.f, sp = 0.4f + r.range(0, 60) / 100.f;
        b.x = (float)r.range(20, g_sc.w * TS - 20); b.y = (float)r.range(20, g_sc.h * TS - 40);
        b.vx = cosf(a) * sp; b.vy = sinf(a) * sp * 0.6f; b.ph = r.range(0, 100) / 10.f;
        b.turn = r.range(60, 240); b.kind = i % 3; b.sing = 0;
        g_birds.push_back(b);
    }
}
static void UpdateBirds() {
    float ww = (float)(g_sc.w * TS), hh = (float)(g_sc.h * TS);
    for (size_t i = 0; i < g_birds.size(); ++i) {
        Bird& b = g_birds[i];
        b.x += b.vx; b.y += b.vy; b.ph += 0.35f; if (b.sing > 0) --b.sing;
        if (--b.turn <= 0) {
            float a = g_rng.range(0, 628) / 100.f, sp = 0.4f + g_rng.range(0, 60) / 100.f;
            b.vx = cosf(a) * sp; b.vy = sinf(a) * sp * 0.6f; b.turn = g_rng.range(80, 260);
        }
        if (b.x < 8) { b.x = 8; b.vx = fabsf(b.vx); }
        if (b.x > ww - 8) { b.x = ww - 8; b.vx = -fabsf(b.vx); }
        if (b.y < 12) { b.y = 12; b.vy = fabsf(b.vy); }
        if (b.y > hh - 20) { b.y = hh - 20; b.vy = -fabsf(b.vy); }
    }
}
static void UpdateChirp(bool audible) {
    if (--g_chirpT > 0) return;
    g_chirpT = 90 + g_rng.range(0, 260);
    if (!audible || g_birds.empty()) return;
    std::vector<int> vis;
    for (size_t i = 0; i < g_birds.size(); ++i) {
        int sx = (int)g_birds[i].x - g_camX, sy = (int)g_birds[i].y - g_camY;
        if (sx > -10 && sx < VW + 10 && sy > -10 && sy < VH + 10) vis.push_back((int)i);
    }
    if (vis.empty()) { g_chirpT = 40; return; }
    g_birds[vis[g_rng.range(0, (int)vis.size() - 1)]].sing = 36;
    Snd(g_rng.range(SND_CHIRP0, SND_CHIRP2));
}
static void DrawBirds() {
    const uint32_t body[3] = { RGB32(130,88,52), RGB32(70,110,200), RGB32(205,75,55) };
    const uint32_t wing[3] = { RGB32(92,60,36), RGB32(46,76,150), RGB32(110,80,60) };
    for (size_t i = 0; i < g_birds.size(); ++i) {
        const Bird& b = g_birds[i];
        int sx = (int)b.x - g_camX, sy = (int)b.y - g_camY + (int)(sinf(b.ph * 0.5f) * 1.5f);
        DimRect(sx, sy + 10, 5, 1, 170);
        uint32_t bc = body[b.kind], wc = wing[b.kind];
        if (((int)b.ph) & 1) { PutPx(sx, sy, wc); PutPx(sx + 1, sy + 1, wc); PutPx(sx + 3, sy + 1, wc); PutPx(sx + 4, sy, wc); PutPx(sx + 2, sy + 2, bc); PutPx(sx + 2, sy + 1, bc); }
        else            { PutPx(sx, sy + 2, wc); PutPx(sx + 1, sy + 1, wc); PutPx(sx + 3, sy + 1, wc); PutPx(sx + 4, sy + 2, wc); PutPx(sx + 2, sy + 1, bc); PutPx(sx + 2, sy + 2, bc); }
        PutPx(b.vx >= 0 ? sx + 3 : sx + 1, sy, bc);
        if (b.sing > 0) { PutPx(sx + 6, sy - 3 - (b.sing / 12), C_GOLD); PutPx(sx + 7, sy - 4 - (b.sing / 12), C_GOLD); }
    }
}

// ------------------------------------------------------------------ tint / light
struct Tint { int mr, mg, mb, ar, ag, ab; };
static const Tint TINTS[5] = {
    { 256, 250, 236, 4, 2, 0 }, { 262, 262, 262, 0, 0, 0 }, { 256, 232, 196, 12, 4, 0 },
    { 250, 176, 150, 22, 6, 12 }, { 86, 100, 168, 8, 10, 26 }
};
static void ApplyTint(int p) {
    const Tint& t = TINTS[p];
    for (int i = 0; i < VW * VH; ++i) {
        uint32_t c = g_pix[i];
        int r = (int)(((c >> 16) & 255) * t.mr >> 8) + t.ar, g = (int)(((c >> 8) & 255) * t.mg >> 8) + t.ag, b = (int)((c & 255) * t.mb >> 8) + t.ab;
        g_pix[i] = RGB32(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
    }
}
static void ApplyLight(const Light& L) {
    int r2 = L.r * L.r;
    for (int y = L.y - L.r; y <= L.y + L.r; ++y) {
        if (y < 0 || y >= VH) continue;
        for (int x = L.x - L.r; x <= L.x + L.r; ++x) {
            if (x < 0 || x >= VW) continue;
            int dx = x - L.x, dy = y - L.y, d2 = dx * dx + dy * dy;
            if (d2 >= r2) continue;
            int t = 256 - d2 * 256 / r2; t = (t * t) >> 8; t = (t * L.k) >> 8;
            g_pix[y * VW + x] = Mix(g_pix[y * VW + x], L.c, t);
        }
    }
}

// ------------------------------------------------------------------ world render
static int ZoneAtPlayer() {
    int fx = (int)(g_pl.x + 8) / TS, fy = (int)(g_pl.y + 13) / TS;
    for (size_t i = 0; i < g_sc.zones.size(); ++i) {
        const Zone& z = g_sc.zones[i];
        if (fx >= z.x0 && fx <= z.x1 && fy >= z.y0 && fy <= z.y1) return z.id;
    }
    return -1;
}

static void DrawWaterTile(int sx, int sy, int tx, int ty) {
    PxRect(sx, sy, TS, TS, RGB32(52,112,196));
    uint32_t h = (uint32_t)tx * 7349u ^ (uint32_t)ty * 9151u; h ^= h >> 11; h *= 0x9E3779B1u;
    for (int k = 0; k < 3; ++k) {
        int hx = (int)((h >> (k * 5)) & 15), hy = (int)((h >> (k * 3 + 2)) & 15);
        if ((((g_tick / 30) + k + tx + ty) & 3) == 0) PxRect(sx + hx % 12, sy + hy, 3, 1, RGB32(130,190,240));
        else PxRect(sx + (hx + 5) % 12, sy + (hy + 7) % 16, 2, 1, RGB32(40,92,176));
    }
}

static void DrawGround() {
    int tx0 = std::max(0, g_camX / TS), ty0 = std::max(0, g_camY / TS);
    int tx1 = std::min(g_sc.w - 1, (g_camX + VW - 1) / TS), ty1 = std::min(g_sc.h - 1, (g_camY + VH - 1) / TS);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int tx = tx0; tx <= tx1; ++tx) {
            int sx = tx * TS - g_camX, sy = ty * TS - g_camY, t = g_sc.t[ty][tx];
            if (t == T_WATER) { DrawWaterTile(sx, sy, tx, ty); continue; }
            int v = (t == T_GRASS) ? TileVariant(tx, ty) : (t == T_PATH ? 8 + ((tx * 3 + ty) & 1) : (t == T_SAND ? 10 : 11));
            const uint32_t* tile = g_tiles[v];
            for (int y = 0; y < TS; ++y) {
                int yy = sy + y; if (yy < 0 || yy >= VH) continue;
                for (int x = 0; x < TS; ++x) { int xx = sx + x; if (xx < 0 || xx >= VW) continue; g_pix[yy * VW + xx] = tile[y * TS + x]; }
            }
            if (t == T_PATH) {
                uint32_t e = RGB32(186,150,96);
                if (InMap(tx, ty - 1) && g_sc.t[ty - 1][tx] == T_GRASS) PxRect(sx, sy, TS, 1, e);
                if (InMap(tx, ty + 1) && g_sc.t[ty + 1][tx] == T_GRASS) PxRect(sx, sy + 15, TS, 1, e);
                if (InMap(tx - 1, ty) && g_sc.t[ty][tx - 1] == T_GRASS) PxRect(sx, sy, 1, TS, e);
                if (InMap(tx + 1, ty) && g_sc.t[ty][tx + 1] == T_GRASS) PxRect(sx + 15, sy, 1, TS, e);
            }
        }
}

static bool Blocked(float x, float y) {
    int l = (int)(x + 4), r = (int)(x + 11), t = (int)(y + 11), b = (int)(y + 15);
    if (l < 0 || t < 0) return true;
    for (int ty = t / TS; ty <= b / TS; ++ty)
        for (int tx = l / TS; tx <= r / TS; ++tx) {
            if (tx >= g_sc.w || ty >= g_sc.h) return true;
            if (g_sc.solid[ty][tx] || g_sc.t[ty][tx] == T_WATER) return true;
        }
    return false;
}

struct DItem { int base, type, idx; };

static void DrawHintBubble(int sx, int sy) {
    int bob = ((g_tick / 15) & 1);
    int x = sx + 3, y = sy - 16 - bob;
    PxRect(x, y, 11, 11, RGB32(30,30,50)); PxRect(x + 1, y + 1, 9, 9, C_WHITE);
    PxRect(x + 4, y + 11, 3, 2, RGB32(30,30,50)); PutPx(x + 5, y + 11, C_WHITE);
    DrawChar(x + 3, y + 1, 'Z', RGB32(40,40,90), 1);
}

static void RenderWorld(bool showHint) {
    int camX = (int)(g_pl.x + 8) - VW / 2, camY = (int)(g_pl.y + 8) - VH / 2;
    camX = std::max(0, std::min(camX, g_sc.w * TS - VW));
    camY = std::max(0, std::min(camY, g_sc.h * TS - VH));
    g_camX = camX; g_camY = camY;
    g_glows.clear(); g_lights.clear();
    DrawGround();

    std::vector<DItem> items;
    for (size_t i = 0; i < g_sc.trees.size(); ++i) items.push_back({ (g_sc.trees[i].y + 1) * TS, 0, (int)i });
    for (size_t i = 0; i < g_sc.blds.size(); ++i)  items.push_back({ (g_sc.blds[i].ty + 3) * TS, 1, (int)i });
    for (size_t i = 0; i < g_sc.lamps.size(); ++i) items.push_back({ (g_sc.lamps[i].y + 1) * TS, 2, (int)i });
    if (g_sc.fountain) items.push_back({ (g_sc.fy + 2) * TS, 3, 0 });
    if (g_sc.bench)    items.push_back({ (g_sc.by + 1) * TS, 4, 0 });
    items.push_back({ (int)g_pl.y + 16, 5, 0 });
    std::stable_sort(items.begin(), items.end(), [](const DItem& a, const DItem& b) { return a.base < b.base; });
    int psx = (int)g_pl.x - camX, psy = (int)g_pl.y - camY;
    for (size_t k = 0; k < items.size(); ++k) {
        const DItem& it = items[k];
        switch (it.type) {
        case 0: { const Tree& t = g_sc.trees[it.idx]; DrawTree(t.x * TS - camX, t.y * TS - 8 - camY, t.v); break; }
        case 1: DrawBuilding(g_sc.blds[it.idx]); break;
        case 2: DrawLamp(g_sc.lamps[it.idx]); break;
        case 3: DrawFountain(); break;
        case 4: DrawBench(); break;
        case 5: DrawAmi(psx, psy, g_pl.dir, g_pl.moving, g_pl.anim, 1, CurLook()); break;
        }
    }
    DrawBirds();

    ApplyTint(g_period);
    for (size_t i = 0; i < g_glows.size(); ++i) PxRect(g_glows[i].x, g_glows[i].y, g_glows[i].w, g_glows[i].h, g_glows[i].c);
    if (g_period == P_NIGHT) g_lights.push_back({ psx + 8, psy + 8, 34, RGB32(205,215,255), 80 });
    for (size_t i = 0; i < g_lights.size(); ++i) ApplyLight(g_lights[i]);
    if (g_period == P_NIGHT) {
        for (int i = 0; i < 12; ++i) {
            int fx = (i * 53 + (int)(18 * sinf(g_tick * 0.012f + i))) % VW, fy = (i * 37 + 20 + (int)(14 * sinf(g_tick * 0.017f + i * 2))) % VH;
            if (((g_tick / 22 + i) % 5) > 1) { PutPx(fx, fy, RGB32(255,255,150)); ApplyLight({ fx, fy, 7, RGB32(255,255,140), 110 }); }
        }
    }
    if (showHint && ZoneAtPlayer() >= 0) DrawHintBubble(psx, psy);
}

// ------------------------------------------------------------------ interiors
static const uint32_t SKY[5] = { RGB32(170,215,245), RGB32(120,190,250), RGB32(240,200,130), RGB32(240,130,100), RGB32(20,25,70) };

static void Q(int ox, int oy, int a, int b, int w, int h, uint32_t c) { PxRect(ox + a * 2, oy + b * 2, w * 2, h * 2, c); }

static void DrawTableItems(int id) {
    switch (id) {
    case B_OTTOS: case B_MOCHERI: case B_MULLIRI: {
        int ox = 108, oy = 128, f = (g_tick / 20) & 1;
        Q(ox, oy, 0, 3, 6, 5, C_WHITE); Q(ox, oy, 6, 4, 1, 3, C_WHITE); Q(ox, oy, 1, 3, 4, 1, RGB32(110,60,30));
        Q(ox, oy, 2, f ? 0 : 1, 1, 2, RGB32(235,235,245)); Q(ox, oy, 4, f ? 1 : 0, 1, 2, RGB32(235,235,245));
        int cx = 182, cy = 138; uint32_t g = RGB32(232,170,72), gd = RGB32(180,120,40);
        Q(cx, cy, 0, 2, 2, 2, g); Q(cx, cy, 1, 1, 2, 2, g); Q(cx, cy, 3, 0, 3, 2, g); Q(cx, cy, 5, 1, 2, 2, g); Q(cx, cy, 6, 2, 2, 2, g);
        Q(cx, cy, 3, 1, 3, 1, gd); break; }
    case B_YAMATO: {
        int ox = 104, oy = 134;
        Q(ox, oy, 0, 3, 16, 2, RGB32(230,230,240));
        for (int i = 0; i < 4; ++i) { Q(ox, oy, 1 + i * 4, 1, 3, 2, RGB32(250,250,245)); Q(ox, oy, 1 + i * 4, 0, 3, 1, RGB32(250,140,100)); }
        PxRect(ox + 130, oy - 4, 2, 24, RGB32(150,100,60)); PxRect(ox + 136, oy - 4, 2, 24, RGB32(150,100,60)); break; }
    case B_PASTARIA: {
        int ox = 108, oy = 134;
        Q(ox, oy, 0, 3, 14, 2, C_WHITE); Q(ox, oy, 2, 1, 10, 2, RGB32(240,200,90)); Q(ox, oy, 3, 0, 8, 1, RGB32(240,200,90));
        Q(ox, oy, 4, 0, 2, 1, RGB32(200,50,40)); Q(ox, oy, 8, 1, 2, 1, RGB32(200,50,40)); Q(ox, oy, 6, 0, 1, 1, RGB32(60,150,60));
        PxRect(ox + 150, oy + 2, 2, 18, RGB32(200,200,210)); break; }
    case B_GOFISH: {
        int ox = 128, oy = 126;
        Q(ox, oy, 0, 1, 8, 8, RGB32(240,230,200)); Q(ox, oy, 0, 1, 8, 1, RGB32(200,190,160)); Q(ox, oy, 0, 5, 8, 1, RGB32(220,60,50));
        Q(ox, oy, 2, 0, 2, 1, RGB32(250,150,60)); Q(ox, oy, 5, 0, 2, 1, RGB32(250,200,70)); break; }
    default: {
        int ox = 136, oy = 130;
        Q(ox, oy, 0, 2, 8, 4, RGB32(250,170,190)); Q(ox, oy, 0, 1, 8, 1, C_WHITE); Q(ox, oy, 3, 0, 2, 1, RGB32(210,40,60));
        Q(ox, oy, 0, 6, 8, 1, RGB32(200,120,140)); break; }
    }
}

static void DrawPartyScene() {
    PxRect(0, 0, VW, VH, RGB32(40,20,70));
    int fy = 120;
    uint32_t pc[4] = { RGB32(150,30,120), RGB32(30,120,170), RGB32(190,160,40), RGB32(90,40,160) };
    for (int y = 0; (fy + y * 20) < VH; ++y) for (int x = 0; x * 20 < VW; ++x) PxRect(x * 20, fy + y * 20, 20, 20, pc[(x + y + g_tick / 15) & 3]);
    for (int x = 0; x < VW; x += 14) { int yy = 10 + (int)(6 * sinf(x * 0.05f)); PxRect(x, yy, 1, 2, RGB32(60,60,70)); PxRect(x - 1, yy + 2, 3, 3, pc[(x / 14 + g_tick / 10) & 3] | 0x303030); }
    PxRect(159, 0, 2, 22, RGB32(150,150,160)); FillCircle(160, 34, 14, RGB32(190,190,205));
    for (int i = 0; i < 14; ++i) PutPx(153 + (i * 7 + g_tick / 4) % 14, 26 + (i * 5) % 16, C_WHITE);
    // two guests
    for (int g = 0; g < 2; ++g) {
        int gx = g ? 252 : 48, bob = ((g_tick / 12 + g) & 1) * 2;
        FillCircle(gx + 8, 104 - bob, 9, RGB32(236,190,150)); PxRect(gx - 2, 112 - bob, 20, 34, g ? RGB32(60,150,120) : RGB32(200,150,40));
        PxRect(gx - 5, 114 - bob - (g_tick / 10 & 1) * 6, 4, 14, RGB32(236,190,150)); PxRect(gx + 17, 114 - bob + (g_tick / 10 & 1) * 6, 4, 14, RGB32(236,190,150));
    }
    int bob = ((g_tick / 10) & 1) * 3;
    DrawAmi(112, 98 - bob, DIR_DOWN, true, (float)g_tick, 3, CurLook());
}

static void DrawHomeScene() {
    PxRect(0, 0, VW, VH, RGB32(246,226,214)); PxRect(0, 128, VW, VH - 128, RGB32(206,160,110));
    for (int y = 128; y < VH; y += 12) PxRect(0, y, VW, 1, RGB32(170,124,80));
    PxRect(0, 122, VW, 6, RGB32(160,110,90));
    PxRect(24, 26, 96, 104, RGB32(120,76,44)); PxRect(28, 30, 88, 96, RGB32(70,44,28)); PxRect(70, 30, 4, 96, RGB32(120,76,44));
    PxRect(30, 52, 84, 2, RGB32(200,200,210));
    for (int i = 0; i < 4; ++i) {
        int x = 36 + i * 20;
        PxRect(x + 2, 56, 6, 2, RGB32(210,210,220)); PxRect(x, 58, 10, 9, DRESS_C1[i]); PxRect(x - 3, 67, 16, 26, DRESS_C1[i]); PxRect(x - 3, 91, 16, 2, DRESS_C2[i]);
    }
    PxRect(204, 22, 76, 108, RGB32(255,214,90)); PxRect(208, 26, 68, 100, RGB32(196,224,244));
    for (int i = 0; i < 6; ++i) PxRect(214 + i * 10, 30 + ((g_tick / 4 + i * 9) % 90), 3, 1, C_WHITE);
    PxRect(140, 70, 40, 60, RGB32(250,220,110)); FillCircle(160, 66, 10, RGB32(255,244,170));    // little lamp
    DrawAmi(176, 96, DIR_DOWN, false, 0, 3, CurLook());
}

static void DrawLakeView() {
    for (int y = 0; y < 92; ++y) PxRect(0, y, VW, 1, Mix(RGB32(90,60,130), RGB32(255,175,95), y * 256 / 92));
    FillCircle(160, 92, 28, RGB32(255,232,150));
    for (int i = 0; i < 12; ++i) PxRect(i * 30 - 10, 84 + (i * 7) % 6, 36, 8, RGB32(80,50,110));
    for (int y = 92; y < 172; ++y) {
        PxRect(0, y, VW, 1, Mix(RGB32(240,150,100), RGB32(60,60,130), (y - 92) * 256 / 80));
        if (((y + g_tick / 8) & 3) < 2) { int hw = 8 + (y - 92) / 4; PxRect(160 - hw, y, hw * 2, 1, Mix(RGB32(255,220,150), RGB32(150,110,120), (y - 92) * 256 / 80)); }
    }
    PxRect(0, 172, VW, VH - 172, RGB32(172,114,62));
    for (int y = 172; y < VH; y += 10) PxRect(0, y, VW, 1, RGB32(120,76,40));
    for (int x = 0; x < VW; x += 40) PxRect(x, 160, 6, 14, RGB32(90,58,30));
    PxRect(0, 160, VW, 3, RGB32(120,76,40));
    DrawAmi(112, 128, DIR_UP, false, 0, 3, CurLook());
    PxRect(80, 176, 100, 12, RGB32(184,128,74)); PxRect(84, 188, 6, 12, RGB32(90,58,30)); PxRect(170, 188, 6, 12, RGB32(90,58,30));
    int ox = 196, oy = 156, f = (g_tick / 20) & 1;
    Q(ox, oy, 0, 3, 6, 5, C_WHITE); Q(ox, oy, 6, 4, 1, 3, C_WHITE); Q(ox, oy, 1, 3, 4, 1, RGB32(110,60,30));
    Q(ox, oy, 2, f ? 0 : 1, 1, 2, RGB32(255,255,255)); Q(ox, oy, 4, f ? 1 : 0, 1, 2, RGB32(255,255,255));
    if (g_ch.gofish == 1) { Q(238, 150, 0, 1, 8, 8, RGB32(240,230,200)); Q(238, 150, 0, 5, 8, 1, RGB32(220,60,50)); Q(238, 150, 2, 0, 2, 1, RGB32(250,150,60)); }
}

static void DrawInterior(int id) {
    if (id == B_LAKE)    { DrawLakeView(); return; }
    if (id == B_CHARLES) { DrawPartyScene(); return; }
    if (id == B_HOME)    { DrawHomeScene(); return; }
    const BInfo& bi = BINFO[id];
    uint32_t wall = Mix(bi.wall, C_WHITE, 70);
    PxRect(0, 0, VW, VH, wall);
    int fy = 128;
    if (id == B_YAMATO) {
        PxRect(0, fy, VW, VH - fy, RGB32(196,188,118));
        for (int x = 0; x < VW; x += 64) PxRect(x, fy, 2, VH - fy, RGB32(120,110,60));
        for (int y = fy; y < VH; y += 32) PxRect(0, y, VW, 2, RGB32(120,110,60));
    } else if (id == B_PASTARIA) {
        for (int y = fy; y < VH; y += 16) for (int x = 0; x < VW; x += 16) PxRect(x, y, 16, 16, (((x >> 4) + (y >> 4)) & 1) ? RGB32(205,70,60) : RGB32(245,240,230));
    } else {
        PxRect(0, fy, VW, VH - fy, id == B_CUSTOM ? RGB32(214,194,234) : RGB32(160,110,70));
        for (int y = fy; y < VH; y += 10) PxRect(0, y, VW, 1, id == B_CUSTOM ? RGB32(190,168,214) : RGB32(124,82,48));
    }
    PxRect(0, fy - 6, VW, 6, Mix(wall, C_BLACK, 70));
    // windows
    int wxs[2] = { 22, 234 };
    for (int i = 0; i < 2; ++i) {
        PxRect(wxs[i] - 3, 25, 70, 66, C_WHITE); PxRect(wxs[i], 28, 64, 60, SKY[g_period]);
        PxRect(wxs[i] + 31, 28, 2, 60, C_WHITE); PxRect(wxs[i], 57, 64, 2, C_WHITE);
        FillCircle(wxs[i] + 16, 84, 12, RGB32(46,125,52)); PxRect(wxs[i] + 14, 84, 4, 4, RGB32(104,66,34));
        if (g_period == P_NIGHT) { PutPx(wxs[i] + 10, 36, C_WHITE); PutPx(wxs[i] + 44, 42, C_WHITE); PutPx(wxs[i] + 28, 34, C_WHITE); }
    }
    // counter + jars
    PxRect(0, 100, VW, 28, bi.roof); PxRect(0, 100, VW, 3, Mix(bi.roof, C_WHITE, 80));
    for (int i = 0; i < 10; ++i) { uint32_t c = (i & 1) ? bi.a1 : bi.a2; PxRect(104 + i * 11, 90, 8, 10, c); PxRect(105 + i * 11, 88, 6, 2, RGB32(90,90,100)); }
    // hanging sign
    std::string nm = BName(id);
    int w = TxtW(nm, 2);
    PxRect((VW - w) / 2 - 8, 8, w + 16, 22, RGB32(70,46,28)); PxRect((VW - w) / 2 - 6, 10, w + 12, 18, RGB32(110,74,44));
    TxtC(VW / 2, 12, nm, RGB32(255,238,200), 2, true);
    // Ami behind the table, then the table in front
    DrawAmi(136, 108, DIR_DOWN, false, 0, 3, CurLook());
    PxRect(96, 150, 128, 12, RGB32(176,120,70)); PxRect(96, 150, 128, 2, RGB32(206,150,96)); PxRect(100, 162, 120, 3, RGB32(110,70,40));
    PxRect(108, 165, 6, 28, RGB32(110,70,40)); PxRect(206, 165, 6, 28, RGB32(110,70,40));
    DrawTableItems(id);
}

// ------------------------------------------------------------------ UI
static void DrawBox(int x, int y, int w, int h) {
    PxRect(x, y, w, h, C_WHITE); PxRect(x + 1, y + 1, w - 2, h - 2, RGB32(16,18,60));
    PxRect(x + 2, y + 2, w - 4, h - 4, RGB32(70,90,200)); PxRect(x + 3, y + 3, w - 6, h - 6, RGB32(16,18,60));
}
static void DrawCursor(int x, int y) { PxRect(x, y, 1, 7, C_GOLD); PxRect(x + 1, y + 1, 1, 5, C_GOLD); PxRect(x + 2, y + 2, 1, 3, C_GOLD); PxRect(x + 3, y + 3, 1, 1, C_GOLD); }

static State g_state = ST_TITLE, g_viewState = ST_TITLE, g_afterFade = ST_WALK;
static int  g_interior = -1, g_fade = 0, g_cardT = 0, g_endPage = 0, g_nameGuard = 0;
static float g_chars = 0, g_endChars = 0;

static std::vector<std::string> g_pages; static int g_page = 0; static std::function<void()> g_dlgDone;
static std::string g_mPrompt; static std::vector<std::string> g_mOpts; static int g_mSel = 0; static std::function<void(int)> g_mDone;
static std::string g_nameBuf; static std::function<void(bool)> g_nameDone;
static std::function<void()> g_fadeMid, g_afterFadeCb;

static void Say(std::vector<std::string> pages, std::function<void()> done = nullptr) {
    g_pages = std::move(pages); g_page = 0; g_chars = 0; g_dlgDone = done; g_state = ST_DIALOG;
}
static void Ask(const std::string& prompt, std::vector<std::string> opts, std::function<void(int)> done) {
    g_mPrompt = prompt; g_mOpts = std::move(opts); g_mSel = 0; g_mDone = done; g_state = ST_MENU;
}
static void AskName(std::function<void(bool)> done, const std::string& initial = "") { g_nameBuf = initial; g_nameDone = done; g_nameGuard = 10; g_state = ST_NAMING; }
static void Transition(std::function<void()> mid, std::function<void()> after = nullptr) {
    g_viewState = g_state; g_fadeMid = mid; g_afterFadeCb = after; g_state = ST_FADE_OUT;
}

static void DrawDialog() {
    DrawBox(6, 166, 308, 68);
    const std::string& txt = g_pages[g_page];
    std::vector<std::string> lines = Wrap(txt, 47);
    int rem = (int)g_chars;
    for (size_t i = 0; i < lines.size() && i < 4; ++i) {
        int n = std::min((int)lines[i].size(), rem);
        if (n > 0) Txt(16, 176 + (int)i * 12, lines[i].substr(0, n), C_WHITE);
        rem -= (int)lines[i].size(); if (rem <= 0) break;
    }
    if ((int)g_chars >= (int)txt.size() && ((g_tick / 20) & 1) == 0) { PxRect(296, 222, 7, 1, C_GOLD); PxRect(297, 223, 5, 1, C_GOLD); PxRect(298, 224, 3, 1, C_GOLD); PxRect(299, 225, 1, 1, C_GOLD); }
}
static void DrawMenu() {
    std::vector<std::string> pl = Wrap(g_mPrompt, 47);
    int h = 12 + (int)pl.size() * 12 + 4 + (int)g_mOpts.size() * 13 + 6, y = VH - 6 - h;
    DrawBox(6, y, 308, h);
    for (size_t i = 0; i < pl.size(); ++i) Txt(16, y + 9 + (int)i * 12, pl[i], C_WHITE);
    int oy = y + 9 + (int)pl.size() * 12 + 6;
    for (size_t i = 0; i < g_mOpts.size(); ++i) {
        bool sel = ((int)i == g_mSel);
        if (sel) DrawCursor(20, oy + (int)i * 13 + 1);
        Txt(32, oy + (int)i * 13, g_mOpts[i], sel ? C_GOLD : RGB32(210,214,240));
    }
}
static void DrawNaming() {
    DrawBox(20, 60, 280, 110);
    TxtC(VW / 2, 72, "What will you call this place?", C_WHITE);
    PxRect(44, 98, 232, 20, RGB32(8,8,24)); PxRect(45, 99, 230, 18, RGB32(30,34,90));
    std::string shown = g_nameBuf + (((g_tick / 25) & 1) ? "_" : " ");
    Txt(52, 104, shown, C_GOLD, 1, false);
    TxtC(VW / 2, 126, "Up to 12 characters", RGB32(180,190,230));
    TxtC(VW / 2, 144, "Enter = OK      Esc = back", RGB32(180,190,230));
}

static std::string Objective() {
    std::string c = BName(B_CUSTOM);
    switch (g_period) {
    case P_MORNING:   return "Morning: Ottos or " + c + "?";
    case P_NOON:      return "Noon: Mocheri, Mulliri or " + c + "?";
    case P_AFTERNOON:
        if (!g_visited[B_YAMATO] && !g_visited[B_PASTARIA]) return "Afternoon: Yamato AND Pastaria - you pick the order";
        return g_visited[B_YAMATO] ? "Afternoon: now go to Pastaria" : "Afternoon: now go to Yamato";
    case P_EVENING:   return g_ch.gofish == 1 ? "Evening: take your dinner to the lake pier" : "Evening: the lake pier (GoFish is optional)";
    default:          return g_ch.dressed ? "Night: head to Charles!" : "Night: get dressed at home first";
    }
}
static void DrawHud() {
    DimRect(0, 0, VW, 12, 130);
    std::string o = Objective(); if (o.size() > 52) o.resize(52);
    Txt(4, 2, o, C_WHITE);
}

static void DrawCard() {
    PxRect(0, 0, VW, VH, C_BLACK);
    int p = g_period;
    uint32_t sunC[3] = { RGB32(255,215,90), RGB32(255,240,120), RGB32(255,170,60) };
    if (p <= 2) {
        FillCircle(160, 58, 14, sunC[p]);
        for (int k = 0; k < 8; ++k) { float a = k * 0.785398f; PxRect(160 + (int)(cosf(a) * 21) - 1, 58 + (int)(sinf(a) * 21) - 1, 3, 3, sunC[p]); }
    } else if (p == 3) {
        FillCircle(160, 66, 16, RGB32(255,120,60)); PxRect(120, 74, 80, 12, C_BLACK); PxRect(110, 74, 100, 2, RGB32(255,150,90));
    } else {
        FillCircle(160, 58, 14, RGB32(240,240,215)); FillCircle(167, 53, 12, C_BLACK);
        for (int i = 0; i < 20; ++i) if (((g_tick / 15 + i) % 4) != 0) PutPx((i * 61 + 7) % VW, (i * 29 + 11) % 100, C_WHITE);
    }
    TxtC(VW / 2, 98, PERIOD_NAME[p], RGB32(255,238,200), 3);
    static const char* sub[5] = { "", "The sun climbs high and the town is buzzing.", "A warm, golden afternoon. Time for something yummy.", "The sky turns orange and pink over the lake.", "Stars come out. Tonight there is a party!" };
    std::vector<std::string> ln = Wrap(sub[p], 44);
    for (size_t i = 0; i < ln.size(); ++i) TxtC(VW / 2, 140 + (int)i * 12, ln[i], RGB32(190,200,240));
    if (g_cardT > 40 && ((g_tick / 25) & 1)) TxtC(VW / 2, 210, "Press Z", RGB32(150,160,200));
}

static void DrawKissEmoji(int x, int y, int sc) {
    auto R = [&](int a, int b, int w, int h, uint32_t c) { PxRect(x + a * sc, y + b * sc, w * sc, h * sc, c); };
    for (int gy = 0; gy < 16; ++gy) for (int gx = 0; gx < 16; ++gx) {
        float dx = gx - 7.5f, dy = gy - 7.5f, d = sqrtf(dx * dx + dy * dy);
        if (d <= 7.5f) R(gx, gy, 1, 1, d > 6.4f ? RGB32(230,160,30) : RGB32(255,205,60));
    }
    R(3, 5, 4, 1, RGB32(90,50,20)); R(3, 4, 1, 1, RGB32(90,50,20));        // winking eye
    R(10, 4, 2, 3, RGB32(90,50,20));                                          // open eye
    R(7, 10, 2, 2, RGB32(230,70,100)); R(6, 11, 1, 1, RGB32(230,70,100));   // kissy mouth
    uint32_t hc = RGB32(235,50,90);                                          // blown heart
    R(12, 8, 2, 2, hc); R(15, 8, 2, 2, hc); R(12, 9, 5, 2, hc); R(13, 11, 3, 1, hc); R(14, 12, 1, 1, hc);
}

static std::vector<std::string> SummaryLines() {
    std::vector<std::string> L; std::string cn = g_ch.custom.empty() ? std::string("(unnamed)") : g_ch.custom;
    L.push_back(std::string("Morning:    ") + (g_ch.morning == 0 ? "Ottos" : cn.c_str()));
    L.push_back(std::string("Noon:       ") + (g_ch.noon == 0 ? "Mocheri" : (g_ch.noon == 1 ? "Mulliri" : cn.c_str())));
    L.push_back(std::string("Afternoon:  ") + (g_ch.aftFirst == 0 ? "Yamato, then Pastaria" : "Pastaria, then Yamato"));
    L.push_back(std::string("Evening:    Coffee by the lake") + (g_ch.gofish == 1 ? " + GoFish dinner" : ", no GoFish"));
    L.push_back(std::string("Night:      ") + (g_ch.dress >= 0 ? DRESS_NAME[g_ch.dress] : "Party") + " dress at Charles");
    L.push_back(std::string("            ") + (g_ch.acc >= 0 ? ACC_NAME[g_ch.acc] : "No accessory"));
    L.push_back(std::string("Your place: ") + cn);
    std::string v;
    for (int i = 0; i < 5; ++i) if (g_ch.customAt[i]) { if (!v.empty()) v += ", "; v += PERIOD_LOWER[i]; }
    L.push_back(std::string("Visited:    ") + (v.empty() ? "never" : v));
    return L;
}
static const char* LETTER_TEXT = "Let's enjoy this today together honey, cannot wait to do this every year with you and thank you for putting back the spark in my eyes,";

static void DrawEnding() {
    PxRect(0, 0, VW, VH, RGB32(14,12,40));
    for (int i = 0; i < 46; ++i) if (((g_tick / 20 + i) & 3) != 0) PutPx((i * 97 + 13) % VW, (i * 57 + 29) % VH, RGB32(220,220,255));
    if (g_endPage == 0) {
        DrawBox(10, 10, 300, 220);
        TxtC(VW / 2, 22, "AMI'S DAY - YOUR CHOICES", C_GOLD, 1);
        std::vector<std::string> L = SummaryLines();
        for (size_t i = 0; i < L.size(); ++i) Txt(24, 48 + (int)i * 16, L[i], C_WHITE);
        if ((g_tick / 25) & 1) TxtC(VW / 2, 212, "Press Z", RGB32(170,180,230));
    } else if (g_endPage == 1) {
        PxRect(16, 16, 288, 208, RGB32(120,80,50)); PxRect(19, 19, 282, 202, RGB32(250,240,220));
        std::vector<std::string> L = Wrap(LETTER_TEXT, 42);
        L.push_back(""); L.push_back("yours only,"); L.push_back("Ralvioli");
        int total = 0; for (size_t i = 0; i < L.size(); ++i) total += (int)L[i].size();
        int rem = (int)g_endChars;
        for (size_t i = 0; i < L.size(); ++i) {
            int n = std::min((int)L[i].size(), rem);
            if (n > 0) Txt(34, 36 + (int)i * 16, L[i].substr(0, n), RGB32(70,42,34), 1, false);
            rem -= (int)L[i].size(); if (rem <= 0) break;
        }
        if ((int)g_endChars >= total) {
            DrawKissEmoji(222, 160, 3);
            for (int i = 0; i < 6; ++i) { int hx = 40 + i * 40, hy = 204 - ((g_tick / 2 + i * 13) % 40); DrawIcon(ICON_HEART, hx, hy); }
            if ((g_tick / 25) & 1) TxtC(VW / 2, 208, "Press Z", RGB32(150,110,90), 1, false);
        }
    } else {
        TxtC(VW / 2, 80, "THE END", C_GOLD, 4);
        TxtC(VW / 2, 130, "Thank you for playing, Ami", RGB32(220,220,250));
        DrawKissEmoji(144, 160, 2);
        if ((g_tick / 25) & 1) TxtC(VW / 2, 210, "Press Z to play again", RGB32(170,180,230));
    }
}

// ------------------------------------------------------------------ game flow
static void Interact(int id);
static void FinishVisit(int id);
static void RunVisit(int id);
static void NextPeriod();
static void ChooseDress();
static void LakeFlow();

static void ResetGame() {
    g_ch = Choices(); memset(g_visited, 0, sizeof g_visited); g_renameAvail = true;
    g_period = P_MORNING; BuildScene(g_period); InitBirds(g_period); ResetPlayer();
    g_interior = -1; g_fade = 0; g_state = ST_TITLE; g_endPage = 0;
}

static bool PeriodDone() {
    switch (g_period) {
    case P_MORNING:   return g_visited[B_OTTOS] || g_visited[B_CUSTOM];
    case P_NOON:      return g_visited[B_MOCHERI] || g_visited[B_MULLIRI] || g_visited[B_CUSTOM];
    case P_AFTERNOON: return g_visited[B_YAMATO] && g_visited[B_PASTARIA];
    case P_EVENING:   return g_visited[B_LAKE];
    default:          return g_visited[B_CHARLES];
    }
}

static void PeriodIntro() {
    switch (g_period) {
    case P_NOON:      Say({ "It's noon already! Time for a little midday treat.", "Mocheri or Mulliri? Or maybe somewhere of your own?" }); break;
    case P_AFTERNOON: Say({ "Ami's tummy is rumbling. Yamato and Pastaria are both open today.", "Visit them both - you decide which comes first." }); break;
    case P_EVENING:   Say({ "The sky is glowing. How about a walk down to the lake?", "You can also grab dinner from GoFish." }); break;
    case P_NIGHT:     Say({ "Tonight is the party at Charles!", "But first, Ami should get dressed at home." }); break;
    default: break;
    }
}

static void NextPeriod() {
    if (g_period == P_NIGHT) {
        Transition([]() { g_interior = -1; g_endPage = 0; g_endChars = 0; g_state = ST_END; });
        return;
    }
    Transition([]() {
        g_period++; memset(g_visited, 0, sizeof g_visited); g_renameAvail = true;
        BuildScene(g_period); InitBirds(g_period); ResetPlayer();
        g_interior = -1; g_cardT = 0; g_state = ST_CARD;
    });
}

static void AfterVisit(int id) {
    if (PeriodDone()) { NextPeriod(); return; }
    if (g_period == P_AFTERNOON && (id == B_YAMATO || id == B_PASTARIA)) {
        int other = g_visited[B_YAMATO] ? B_PASTARIA : B_YAMATO;
        Say({ "That was delicious! Now for " + BName(other) + "." });
    } else if (id == B_HOME) {
        Say({ "Ami looks wonderful! Now off to Charles." });
    } else if (id == B_GOFISH) {
        Say({ "Dinner is packed and ready. Off to the lake pier!" });
    } else {
        Say({ "What a lovely stop. Is there somewhere else to go?" });
    }
}

static void FinishVisit(int id) { Transition([]() { g_interior = -1; }, [id]() { AfterVisit(id); }); }

static void Record(int id) {
    switch (g_period) {
    case P_MORNING:   if (id == B_OTTOS) g_ch.morning = 0; else if (id == B_CUSTOM) g_ch.morning = 1; break;
    case P_NOON:      if (id == B_MOCHERI) g_ch.noon = 0; else if (id == B_MULLIRI) g_ch.noon = 1; else if (id == B_CUSTOM) g_ch.noon = 2; break;
    case P_AFTERNOON: if ((id == B_YAMATO || id == B_PASTARIA) && g_ch.aftFirst < 0) g_ch.aftFirst = (id == B_YAMATO) ? 0 : 1; break;
    case P_EVENING:   if (id == B_GOFISH) g_ch.gofish = 1; break;
    default:          if (id == B_CHARLES) g_ch.charles = true; break;
    }
    if (id == B_CUSTOM) g_ch.customAt[g_period] = true;
}

static std::vector<std::string> VisitPages(int id) {
    switch (id) {
    case B_OTTOS:
        return { "Ami steps into Ottos. The smell of fresh coffee fills the air.", "She orders a warm latte and a buttery croissant, and picks a seat by the window.", "The first sip of the day. Perfect." };
    case B_CUSTOM: {
        static const char* when[5] = { "a gentle start to the morning", "a lovely break at noon", "a sunny afternoon treat", "a golden evening moment", "a cozy stop on the way to the party" };
        return { "Ami steps into " + g_ch.custom + ". It feels like a little corner of the world made just for her.", std::string("She spends a while here, enjoying ") + when[g_period] + "." };
    }
    case B_MOCHERI:
        return { "Ami walks into Mocheri. Sunlight pours over the cozy tables.", "She sips an iced mocha and nibbles a chocolate pastry.", "Sweet, cool, and just what she needed." };
    case B_MULLIRI:
        return { "Ami walks into Mulliri, an old mill turned coffee shop.", "She picks a cappuccino and a warm slice of apple cake.", "Every bite tastes like home." };
    case B_YAMATO:
        return { "Ami enters Yamato. Paper lanterns glow softly overhead.", "Fresh salmon nigiri, a dragon roll and a warm bowl of miso soup.", "Itadakimasu!" };
    case B_PASTARIA:
        return { "Ami enters Pastaria. It smells of garlic, basil and fresh bread.", "She twirls creamy fettuccine around her fork, with tiramisu to finish.", "Buon appetito!" };
    case B_GOFISH:
        return { "Ami stops by GoFish and orders at the little window.", "Crispy fish and chips, wrapped up warm to take away." };
    case B_CHARLES: {
        std::vector<std::string> p;
        p.push_back("Ami arrives at Charles. Music, lights and laughter spill out of the doors.");
        p.push_back(std::string("Dressed in her ") + DRESS_NAME[g_ch.dress] + " dress, she steps onto the dance floor." +
                    (g_ch.acc == 0 ? " Her pearls catch the light." : (g_ch.acc == 1 ? " The flower in her hair sways with every step." : "")));
        p.push_back("What a night!");
        return p;
    }
    default: return { "..." };
    }
}

static void RunVisit(int id) {
    Record(id); g_visited[id] = true;
    Transition([id]() { g_interior = id; Say(VisitPages(id), [id]() { FinishVisit(id); }); });
}

static void ChooseDress() {
    Ask("Pick a dress for the party tonight.", { DRESS_NAME[0], DRESS_NAME[1], DRESS_NAME[2], DRESS_NAME[3] }, [](int d) {
        if (d < 0) { Say({ "Ami decides to browse a little longer." }); return; }
        Ask("And an accessory to go with it?", { ACC_NAME[0], ACC_NAME[1], ACC_NAME[2] }, [d](int a) {
            if (a < 0) { ChooseDress(); return; }
            std::string q = std::string("Wear the ") + DRESS_NAME[d] + " dress with: " + ACC_NAME[a] + "?";
            Ask(q, { "Yes, perfect!", "No, let me change" }, [d, a](int c) {
                if (c != 0) { ChooseDress(); return; }
                g_ch.dress = d; g_ch.acc = a; g_ch.dressed = true; g_visited[B_HOME] = true;
                Transition([]() {
                    g_interior = B_HOME;
                    Say({ "Ami slips into the " + std::string(DRESS_NAME[g_ch.dress]) + " dress and twirls in front of the mirror.", "A little finishing touch, a deep breath... she is ready for the party!" }, []() { FinishVisit(B_HOME); });
                });
            });
        });
    });
}

static void LakeFlow() {
    auto go = []() {
        g_visited[B_LAKE] = true; g_ch.lake = true;
        Transition([]() {
            g_interior = B_LAKE;
            std::vector<std::string> p;
            p.push_back("Ami walks to the end of the pier and sits down. The lake shimmers in the evening light.");
            p.push_back("She sips a warm cup of coffee and watches the ripples spread across the water.");
            if (g_ch.gofish == 1) p.push_back("She unwraps her GoFish dinner. Crispy, golden, and perfect with the sunset.");
            p.push_back("What a peaceful evening.");
            Say(p, []() { FinishVisit(B_LAKE); });
        });
    };
    if (g_ch.gofish != -1) { go(); return; }
    Ask("Would you like to order dinner from GoFish to enjoy by the lake?", { "Yes, order from GoFish", "No, just coffee" }, [go](int c) {
        if (c < 0) return;
        g_ch.gofish = (c == 0) ? 1 : 0;
        if (c == 0) g_visited[B_GOFISH] = true;
        go();
    });
}

static std::string EnterPrompt(int id) {
    std::string n = BName(id);
    switch (id) {
    case B_OTTOS:    return "Spend your morning at Ottos?";
    case B_CUSTOM:   return g_period == P_MORNING ? "Spend your morning at " + n + "?" : (g_period == P_NOON ? "Spend your noon at " + n + "?" : "Stop by " + n + "?");
    case B_MOCHERI:  return "Have your noon coffee at Mocheri?";
    case B_MULLIRI:  return "Have your noon coffee at Mulliri?";
    case B_YAMATO:   return g_visited[B_PASTARIA] ? "Go to Yamato next?" : "Go to Yamato first?";
    case B_PASTARIA: return g_visited[B_YAMATO] ? "Go to Pastaria next?" : "Go to Pastaria first?";
    case B_GOFISH:   return "Order something from GoFish?";
    case B_HOME:     return "Go home and get dressed for the party?";
    case B_CHARLES:  return "Go to the party at Charles?";
    default:         return "Sit on the pier and have a coffee by the lake?";
    }
}

static void Interact(int id) {
    if (id == B_CUSTOM && g_ch.custom.empty()) {
        Ask("This building has no name yet. Would you like to name it?", { "Yes, name it", "Not now" }, [](int c) {
            if (c != 0) return;
            AskName([](bool ok) {
                if (!ok) return;
                g_ch.custom = g_nameBuf; g_renameAvail = false;
                Say({ "\"" + g_ch.custom + "\"... what a lovely name!" }, []() { Interact(B_CUSTOM); });
            });
        });
        return;
    }
    if (id == B_CUSTOM && g_renameAvail) {
        Ask("Would you like to rename " + g_ch.custom + "?", { "Keep the name", "Rename it" }, [](int c) {
            if (c < 0) return;
            if (c == 0) { g_renameAvail = false; Interact(B_CUSTOM); return; }
            AskName([](bool ok) {
                if (!ok) return;
                g_ch.custom = g_nameBuf; g_renameAvail = false;
                Say({ "\"" + g_ch.custom + "\"... what a lovely new name!" }, []() { Interact(B_CUSTOM); });
            }, g_ch.custom);
        });
        return;
    }
    if (g_visited[id] && id != B_LAKE) { Say({ "Ami has already been to " + BName(id) + " today." }); return; }
    if (id == B_CHARLES && !g_ch.dressed) { Say({ "Ami can't go to a party in her everyday dress!", "She should change at home first." }); return; }
    Ask(EnterPrompt(id), { "Yes", "No" }, [id](int c) {
        if (c != 0) return;
        if (id == B_HOME) ChooseDress();
        else if (id == B_LAKE) LakeFlow();
        else RunVisit(id);
    });
}

// ------------------------------------------------------------------ update
static void UpdateWalk() {
    float dx = (float)((int)Down(VK_RIGHT) - (int)Down(VK_LEFT)), dy = (float)((int)Down(VK_DOWN) - (int)Down(VK_UP));
    g_pl.moving = (dx != 0 || dy != 0);
    if (g_pl.moving) {
        float len = sqrtf(dx * dx + dy * dy), sp = 1.3f;
        float nx = g_pl.x + dx / len * sp, ny = g_pl.y + dy / len * sp;
        if (!Blocked(nx, g_pl.y)) g_pl.x = nx;
        if (!Blocked(g_pl.x, ny)) g_pl.y = ny;
        g_pl.anim += 1.0f;
        if (dx < 0) g_pl.dir = DIR_LEFT; else if (dx > 0) g_pl.dir = DIR_RIGHT; else if (dy < 0) g_pl.dir = DIR_UP; else g_pl.dir = DIR_DOWN;
    } else g_pl.anim = 0;
    if (Pressed('Z')) { int id = ZoneAtPlayer(); if (id >= 0) { Snd(SND_OK); Interact(id); } }
}

static void Update() {
    PollKeys(); ++g_tick;
    if (g_nameGuard > 0) --g_nameGuard;
    if (Pressed('M') && g_state != ST_NAMING) MusicToggle();
    bool worldVisible = (g_interior < 0) && g_state != ST_END && g_state != ST_CARD;
    UpdateBirds(); UpdateChirp(worldVisible);

    switch (g_state) {
    case ST_TITLE:
        if (Pressed('Z')) {
            Snd(SND_OK);
            Say({ "Hello Ami, let's start your day!", "Use the arrow keys to walk. Press Z to interact and X to cancel.",
                  "Where would you like to go this morning? Ottos smells wonderful... or you could name the other building yourself." });
        }
        break;
    case ST_WALK: UpdateWalk(); break;
    case ST_DIALOG: {
        const std::string& cur = g_pages[g_page];
        g_chars += 0.8f; if (g_chars > (float)cur.size()) g_chars = (float)cur.size();
        if (Pressed('Z')) {
            if (g_chars < (float)cur.size()) g_chars = (float)cur.size();
            else if (g_page + 1 < (int)g_pages.size()) { ++g_page; g_chars = 0; Snd(SND_BLIP); }
            else { std::function<void()> cb = g_dlgDone; g_dlgDone = nullptr; g_state = ST_WALK; if (cb) cb(); }
        }
        break; }
    case ST_MENU:
        if (Pressed(VK_UP))   { g_mSel = (g_mSel + (int)g_mOpts.size() - 1) % (int)g_mOpts.size(); Snd(SND_BLIP); }
        if (Pressed(VK_DOWN)) { g_mSel = (g_mSel + 1) % (int)g_mOpts.size(); Snd(SND_BLIP); }
        if (Pressed('Z')) { Snd(SND_OK); int s = g_mSel; auto cb = g_mDone; g_mDone = nullptr; g_state = ST_WALK; if (cb) cb(s); }
        else if (Pressed('X')) { Snd(SND_BACK); auto cb = g_mDone; g_mDone = nullptr; g_state = ST_WALK; if (cb) cb(-1); }
        break;
    case ST_NAMING: break;   // handled by WM_CHAR
    case ST_FADE_OUT:
        g_fade += 12;
        if (g_fade >= 255) {
            g_fade = 255;
            auto cb = g_fadeMid; g_fadeMid = nullptr;
            g_state = ST_FADE_IN; g_afterFade = ST_WALK;
            if (cb) cb();
            if (g_state == ST_CARD || g_state == ST_END) {
                if (g_state == ST_CARD) g_fade = 0;
                else { g_afterFade = ST_END; g_state = ST_FADE_IN; }
            } else if (g_state != ST_FADE_IN) { g_afterFade = g_state; g_state = ST_FADE_IN; }
        }
        break;
    case ST_FADE_IN:
        g_fade -= 12;
        if (g_fade <= 0) {
            g_fade = 0; g_state = g_afterFade;
            auto cb = g_afterFadeCb; g_afterFadeCb = nullptr; if (cb) cb();
        }
        break;
    case ST_CARD:
        ++g_cardT;
        if (g_cardT > 180 || (g_cardT > 40 && Pressed('Z'))) { g_state = ST_FADE_IN; g_fade = 255; g_afterFade = ST_WALK; g_afterFadeCb = PeriodIntro; }
        break;
    case ST_END:
        if (g_endPage == 1) { g_endChars += 0.6f; }
        if (Pressed('Z')) {
            if (g_endPage == 0) { g_endPage = 1; g_endChars = 0; Snd(SND_OK); }
            else if (g_endPage == 1) {
                int total = (int)strlen(LETTER_TEXT) + 11 + 8;
                if (g_endChars < (float)total) g_endChars = (float)total + 1; else { g_endPage = 2; Snd(SND_OK); }
            } else { Snd(SND_OK); ResetGame(); }
        }
        break;
    }
}

// ------------------------------------------------------------------ render
static void Render() {
    State vs = g_state;
    if (g_state == ST_FADE_OUT) vs = g_viewState; else if (g_state == ST_FADE_IN) vs = g_afterFade;
    if (vs == ST_END) DrawEnding();
    else if (vs == ST_CARD) DrawCard();
    else {
        if (g_interior >= 0) DrawInterior(g_interior);
        else RenderWorld(vs == ST_WALK);
        if (g_interior < 0 && (vs == ST_WALK || vs == ST_DIALOG || vs == ST_MENU || vs == ST_NAMING)) DrawHud();
        if (vs == ST_TITLE) {
            DimRect(0, 10, VW, 76, 100);
            TxtC(VW / 2, 22, "AMI'S DAY", C_GOLD, 4);
            TxtC(VW / 2, 62, "A little day out, just for you", C_WHITE);
            if ((g_tick / 30) & 1) TxtC(VW / 2, 156, "Press Z to start", C_WHITE);
            DimRect(0, 204, VW, 24, 110);
            TxtC(VW / 2, 208, "Arrows: move   Z: interact   X: cancel", RGB32(220,226,250));
            TxtC(VW / 2, 218, "M: music on/off      F11: fullscreen", RGB32(220,226,250));
        }
        if (vs == ST_DIALOG) DrawDialog();
        if (vs == ST_MENU) DrawMenu();
        if (vs == ST_NAMING) DrawNaming();
    }
    if (g_fade > 0) DimRect(0, 0, VW, VH, 256 - g_fade);
}

// ------------------------------------------------------------------ Win32 plumbing
static void Present(HDC dc) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) return;
    int w = cw, h = cw * VH / VW;
    if (h > ch) { h = ch; w = ch * VW / VH; }
    if (w < 1) w = 1; if (h < 1) h = 1;
    int ox = (cw - w) / 2, oy = (ch - h) / 2;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, ox, oy, w, h, 0, 0, VW, VH, g_pix, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
}

static void ToggleFullscreen() {
    DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
    if (!g_fullscreen) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(g_hwnd, &g_wp) && GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(g_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            g_fullscreen = true;
        }
    } else {
        SetWindowLong(g_hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &g_wp);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_fullscreen = false;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (g_state == ST_NAMING) { auto cb = g_nameDone; g_nameDone = nullptr; g_state = ST_WALK; Snd(SND_BACK); if (cb) cb(false); }
            else if (g_fullscreen) ToggleFullscreen();
            return 0;
        }
        if (wp == VK_F11) { ToggleFullscreen(); return 0; }
        break;
    case WM_CHAR:
        if (g_state == ST_NAMING && g_nameGuard <= 0) {
            char c = (char)wp;
            if (c == 13) {
                if (!g_nameBuf.empty()) { auto cb = g_nameDone; g_nameDone = nullptr; g_state = ST_WALK; Snd(SND_OK); if (cb) cb(true); }
            } else if (c == 8) {
                if (!g_nameBuf.empty()) g_nameBuf.pop_back();
            } else if (g_nameBuf.size() < 12 &&
                       ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '\'' || c == '-' || c == '&' || c == '.' ||
                        (c == ' ' && !g_nameBuf.empty()))) {
                g_nameBuf += c; Snd(SND_BLIP);
            }
        }
        return 0;
    case MM_MCINOTIFY:
        if (wp == MCI_NOTIFY_SUCCESSFUL && g_musicOpen && g_musicOn) mciSendStringA("play bgm from 0 notify", NULL, 0, hwnd);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: {
        HDC dc = GetDC(hwnd); RECT rc; GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); ReleaseDC(hwnd, dc);
        return 0; }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH)); Present(dc);
        EndPaint(hwnd, &ps); return 0; }
    case WM_DESTROY:
        MusicStop();
        g_running = false; PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    BuildTiles(); BuildSounds(); ResetGame();

    ZeroMemory(&g_bmi, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); g_bmi.bmiHeader.biWidth = VW; g_bmi.bmiHeader.biHeight = -VH;
    g_bmi.bmiHeader.biPlanes = 1; g_bmi.bmiHeader.biBitCount = 32; g_bmi.bmiHeader.biCompression = BI_RGB;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = "AmisDayWnd";
    RegisterClassA(&wc);
    RECT r = { 0, 0, VW * 3, VH * 3 };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA("AmisDayWnd", "Ami's Day", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;
    ShowWindow(g_hwnd, nShow);
    MusicStart(g_hwnd);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
    double acc = 0.0; const double step = 1.0 / 60.0;
    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (!g_running) break;
        QueryPerformanceCounter(&now);
        acc += (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart; last = now;
        if (acc > 0.25) acc = 0.25;
        bool focused = (GetForegroundWindow() == g_hwnd);
        while (acc >= step) { if (focused) Update(); acc -= step; }
        Render();
        HDC dc = GetDC(g_hwnd); Present(dc); ReleaseDC(g_hwnd, dc);
        Sleep(1);
    }
    return 0;
}
