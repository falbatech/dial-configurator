// FalbaTech DIAL FT - DFTv1.1
// Smartwatch-style UI (Galaxy Watch inspired): swipeable HOME pages, rotary bezel navigation.
// Screens: 0 HOME (pages EVERYDAY / CREATIVE / SYSTEM) | 1 CONTROL (active profile) | 2 POMODORO | 3 SETTINGS | 4 RGB ring | 5 Touch ring
// USB: HID keyboard + mouse (wheel/pan) + consumer control, plus CDC serial ("STATUS", Now Playing od DIAL FT Helper).
#include <Arduino.h>
#include <FS.h>   // wymagane przez USBHostMSC.cpp (LDF nie dolacza go sam)
#include <Wire.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <time.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "USBHIDConsumerControl.h"
#include "USBCDC.h"
#include "driver/gpio.h"
#include "mbedtls/base64.h"
#if __has_include("tusb.h")
#include "tusb.h"
#define HAVE_TUD 1
#endif
#include "config.h"
#if __has_include("logo.h")
#include "logo.h"
#define HAVE_LOGO 1
#endif

static inline uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// RGB ring modes
enum RgbMode : uint8_t { RGB_OFF = 0, RGB_FOLLOW, RGB_AMBIENT, RGB_BREATHE, RGB_RAINBOW, RGB_MODES };
static const char *RGB_MODE_NAMES[RGB_MODES] = {"Off", "Follow", "Ambient", "Breathe", "Rainbow"};
static const int RGB_COLOR_STATES = 38;   // 0 = theme, 1..36 = hue 0..350 deg, 37 = white
static void rgbFlash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);
static bool micMuted;   // Spotkania: mikrofon wyciszony (pierscien na czerwono)
static uint32_t hiddenMask = 0;   // ukryte ikony HOME (konfigurator), bit = indeks w APPS
static bool autoSwitch = true;    // automatyczna zmiana profilu wg aktywnej aplikacji (od DIAL FT Helper)
static bool clockFace = true;     // tarcza zegarka w spoczynku (gdy znany jest czas)
static bool standup = true;       // przypomnienie o przerwie po STANDUP_MIN minutach pracy
static bool tourDone = false;     // samouczek przy pierwszym uruchomieniu
static void rgbAction(bool red = false);

// ===================== globals =====================
static TFT_eSPI tft;
static TFT_eSprite sprA(&tft), sprB(&tft);
static TFT_eSprite *gCur = &sprA, *gNxt = &sprB;
static USBHIDKeyboard kb;
static USBHIDMouse mouse;
static USBHIDConsumerControl cc;
static USBCDC cdc;   // jawny port CDC (makro Serial zalezy od flag plytki i trafialo na UART0)
static Preferences prefs;

static const char *initStage = "boot";
static String lastError = "none";
static bool dirty = true;

static int tile = 0;            // 0 home, 1 control, 2 pomodoro, 3 settings, 4 rgb
static int homePage = 0;        // biezacy pulpit HOME (indeks w tablicy pgList)
static int homeSel = 0;         // zaznaczona ikona na biezacym pulpicie
static int activeProfile = 0;
static uint8_t subSel[32] = {0};
static int setSel = 0, settingsTop = 0;
static int ledSel = 0, ledTop = 0;
static bool ledEdit = false;
// Touch jog - stan biezacy
static bool jogActive = false, jogInertiaOn = false;
static float jogLastAng = 0, jogShowAng = 0, jogAcc = 0, jogVel = 0;
static int jogDir = 1;
static uint32_t jogLastMs = 0, jogInertiaMs = 0;
static int bezelAng = 0;
static char lastAction[20] = "";
static uint32_t lastActionMs = 0;

// settings (persisted)
static uint8_t themeIdx = 0, briPct = 80, focusMin = POMO_FOCUS_DEFAULT, breakMin = POMO_BREAK_DEFAULT;
static bool scrollInv = false;
static uint8_t hostOs = 0;   // 0 = Windows/Linux (Ctrl), 1 = macOS (Cmd) - modyfikator skrotow w profilach
static const uint8_t BREAK_OPTS[4] = {3, 5, 10, 15};
// RGB settings (persisted); kolor pierscienia osobno dla kazdego motywu (rgbColorT[themeIdx])
static uint8_t rgbMode = RGB_FOLLOW, rgbLastMode = RGB_FOLLOW, rgbBri = 60;
static uint8_t rgbColorT[8] = {0};
static bool rgbPomo = true, rgbFlashOn = true, rgbIdleOff = true, rgbRev = false;
// Touch jog (DaVinci): kat na klatke wybierany z tablicy
// Smart Touch Ring: ustawienia kazdego profilu osobno (zapis w flash jako 1 bajt na profil)
enum TouchMode : uint8_t { TM_SMOOTH = 0, TM_STEP = 1 };
struct TouchCfg { uint8_t mode, sens, inertia; bool rev, adaptive, ring; };   // sens 0..2 (LOW/MED/HIGH), inertia 0..3 (OFF/LOW/MED/HIGH)
enum ProfId : uint8_t { PR_VOLUME = 0, PR_SCROLL, PR_DAVINCI, PR_KICAD, PR_PHOTOSHOP, PR_LIGHTROOM, PR_MEDIA, PR_PIXELMATOR, PR_SPOTIFY,
                        PR_MEETINGS, PR_SYSTEM, PR_PREMIERE, PR_FINALCUT, PR_FIGMA, PR_BLENDER, PR_FUSION, PR_CUSTOM0 };
static const int NCUSTOM = 12;                     // profile klienta (konfigurator w przegladarce), 2 pulpity CUSTOM
static const int NPROF = PR_CUSTOM0 + NCUSTOM;
static TouchCfg tcfg[NPROF];
static const char *TC_MODE_NAMES[2] = {"SMOOTH", "STEP"};
static const char *TC_SENS_NAMES[3] = {"LOW", "MEDIUM", "HIGH"};
static const char *TC_INERTIA_NAMES[4] = {"OFF", "LOW", "MEDIUM", "HIGH"};
static const float TC_SENS_MULT[3] = {0.6f, 1.0f, 1.6f};
static const float TC_INERTIA_K[4] = {0.0f, 0.88f, 0.93f, 0.965f};   // wspolczynnik wyhamowania na ~15 ms; MEDIUM = DFTv1
// kat palca na jedna "jednostke akcji" (jedno kliniecie enkodera) przy czulosci MEDIUM
//                                           Vol   Scroll DaVinci KiCad  PS     LR     Media  Pixel  Spotify Meet  System Prem   FCP    Figma  Blender Fusion
static const float TC_SMOOTH_DEG[PR_CUSTOM0] = {4.0f, 8.0f,  4.0f,   10.0f, 6.0f,  8.0f,  4.0f,  6.0f,  4.0f,   4.0f, 8.0f,  4.0f,  4.0f,  6.0f,  6.0f,   6.0f};
static const float TC_STEP_DEG[PR_CUSTOM0]   = {15.0f, 15.0f, 6.0f,  20.0f, 12.0f, 12.0f, 15.0f, 12.0f, 15.0f, 15.0f, 20.0f, 6.0f, 6.0f, 12.0f, 12.0f, 12.0f};   // DaVinci: patrz jogDegIdx
static float tcSmoothDeg(int p) { return p < PR_CUSTOM0 ? TC_SMOOTH_DEG[p] : 6.0f; }
static float tcStepDeg(int p) { return p < PR_CUSTOM0 ? TC_STEP_DEG[p] : 15.0f; }
static uint8_t jogDegIdx = 2;               // DaVinci STEP: kat na klatke z tablicy JOG_DEG (jak w DFTv1)
static const uint8_t JOG_DEG[5] = {3, 4, 6, 8, 12};
static bool tcRev(int p);                   // kierunek: dla Scroll wspolny z scrollInv

static void tcDefaults() {
  for (int p = 0; p < NPROF; p++) tcfg[p] = {TM_SMOOTH, 1, 0, false, true, true};
  tcfg[1].inertia = 2;                                   // Scroll: domyslnie bezwladnosc MEDIUM
  tcfg[2] = {TM_STEP, 1, 0, false, false, true};         // DaVinci: dokladnie jak jog w DFTv1
  tcfg[3] = {TM_STEP, 1, 0, false, false, false};        // KiCad: pierscien domyslnie wylaczony
  tcfg[PR_LIGHTROOM] = {TM_STEP, 1, 0, false, false, true};   // Lightroom: pierscien = dokladne kroki suwaka
  tcfg[PR_PREMIERE] = tcfg[PR_FINALCUT] = {TM_STEP, 1, 0, false, false, true};   // montaz: jog klatek jak DaVinci
  tcfg[PR_SYSTEM] = {TM_STEP, 1, 0, false, false, false};
}
static uint8_t tcPack(const TouchCfg &c) {
  return (c.mode & 1) | ((c.sens & 3) << 1) | ((c.inertia & 3) << 3) | (c.rev ? 0x20 : 0) | (c.adaptive ? 0x40 : 0) | (c.ring ? 0x80 : 0);
}
static void tcUnpack(TouchCfg &c, uint8_t v) {
  c.mode = v & 1; c.sens = (v >> 1) & 3; if (c.sens > 2) c.sens = 1;
  c.inertia = (v >> 3) & 3; c.rev = (v & 0x20) != 0; c.adaptive = (v & 0x40) != 0; c.ring = (v & 0x80) != 0;
}

static uint32_t lastInputMs = 0;
static bool dimmed = false;
static bool prefsPending = false;   // odlozony zapis ustawien (flash)
static uint32_t prefsAtMs = 0;

static struct {
  uint32_t frames = 0, clicks = 0, holds = 0, taps = 0, swipes = 0, hid = 0, tpErr = 0;
  int lx = -1, ly = -1;
  uint8_t tpG = 0, tpF = 0;
} st;

// ===================== themes =====================
struct Theme { const char *name; uint8_t r, g, b; };
static const Theme THEMES[] = {
  {"Ocean", 0, 190, 255}, {"Amber", 255, 160, 0}, {"Mint", 0, 220, 150},
  {"Violet", 170, 110, 255}, {"Rose", 255, 80, 130}, {"Mono", 225, 225, 225},
  {"Color", 225, 225, 225},    // Color: kazda aplikacja ma wlasny kolor (ICON_RGB), akcent wg aplikacji
  {"FalbaTech", 98, 178, 98}}; // zielen z logo FalbaTech (#518751, rozjasniona na wyswietlacz); domyslny motyw
static const int NTHEMES = sizeof(THEMES) / sizeof(THEMES[0]);
static const int THEME_COLOR = 6;
static const int THEME_DEFAULT = 7;   // FalbaTech
static uint16_t cAcc, cDim, cGray, cBtn, cBreak;
static uint8_t accR, accG, accB;   // biezacy akcent (w motywie Color zmienia sie z aplikacja)

// kolory aplikacji w motywie Color, wg numeru ikony
static const int NICONS = 20;
static const uint8_t ICON_RGB[NICONS][3] = {
  {175, 82, 222},   // 0 Volume - fioletowy
  {94, 92, 230},    // 1 Smart Scroll - indygo
  {255, 140, 40},   // 2 DaVinci - pomaranczowy
  {180, 220, 40},   // 3 KiCad - limonkowy
  {255, 69, 58},    // 4 Pomodoro - czerwony
  {170, 170, 185},  // 5 Settings - szary
  {49, 168, 255},   // 6 Photoshop - niebieski
  {64, 224, 208},   // 7 Lightroom - turkusowy
  {255, 55, 140},   // 8 Media - rozowy
  {94, 92, 230},    // 9 Smart Scroll (poziomo) - jak 1
  {255, 204, 0},    // 10 Pixelmator - zolty
  {30, 215, 96},    // 11 Spotify - zielony
  {10, 132, 255},   // 12 Spotkania - niebieski
  {200, 200, 210},  // 13 System - srebrny
  {153, 153, 255},  // 14 Premiere - lawendowy
  {191, 90, 242},   // 15 Final Cut - fioletowy
  {242, 78, 30},    // 16 Figma - czerwono-pomaranczowy
  {234, 140, 0},    // 17 Blender - pomaranczowy
  {0, 190, 150},    // 18 Fusion 360 - morski
  {230, 230, 230},  // 19 wlasny profil (kolor z konfiguratora)
};
static bool themeMulti() { return themeIdx == THEME_COLOR; }
static uint16_t iconColor(int icon) { return C565(ICON_RGB[icon][0], ICON_RGB[icon][1], ICON_RGB[icon][2]); }

static void setAccent(uint8_t r, uint8_t g, uint8_t b) {
  accR = r; accG = g; accB = b;
  cAcc = C565(r, g, b);
  cDim = C565(r / 6, g / 6, b / 6);
}
static void accentIcon(int icon) {   // tylko motyw Color
  if (themeMulti()) setAccent(ICON_RGB[icon][0], ICON_RGB[icon][1], ICON_RGB[icon][2]);
}

static void applyTheme() {
  const Theme &t = THEMES[themeIdx];
  setAccent(t.r, t.g, t.b);
  cGray = C565(150, 150, 150);
  cBtn = C565(26, 26, 26);
  cBreak = C565(0, 220, 150);
}

static void savePrefs() {
  prefs.putUChar("theme", themeIdx);
  prefs.putUChar("bri", briPct);
  prefs.putUChar("fmin", focusMin);
  prefs.putBool("inv", scrollInv);
  prefs.putUChar("host", hostOs);
  prefs.putUChar("bmin", breakMin);
  prefs.putUChar("rgbm", rgbMode);
  prefs.putUChar("rgbl", rgbLastMode);
  for (int t = 0; t < NTHEMES; t++) { char k[8]; snprintf(k, sizeof(k), "rgbc%d", t); prefs.putUChar(k, rgbColorT[t]); }
  prefs.putUChar("rgbb", rgbBri);
  prefs.putBool("rgbp", rgbPomo);
  prefs.putBool("rgbf", rgbFlashOn);
  prefs.putBool("rgbi", rgbIdleOff);
  prefs.putBool("rgbr", rgbRev);
  prefs.putUChar("jogd", jogDegIdx);
  prefs.putUChar("sax", subSel[1]);
  prefs.putUChar("tcv", 1);
  prefs.putULong("hide", hiddenMask);
  prefs.putBool("auto", autoSwitch);
  prefs.putBool("clk", clockFace);
  prefs.putBool("stand", standup);
  prefs.putBool("tour", tourDone);
  for (int p = 0; p < NPROF; p++) { char k[8]; snprintf(k, sizeof(k), "tc%d", p); prefs.putUChar(k, tcPack(tcfg[p])); }
}

static void setBacklight(uint8_t pct) { ledcWrite(PIN_BL, (uint32_t)pct * 255 / 100); }

static void markActive() {
  lastInputMs = millis();
  if (dimmed) { dimmed = false; setBacklight(briPct); }
}

static void setAction(const char *s) {
  strncpy(lastAction, s, sizeof(lastAction) - 1);
  lastAction[sizeof(lastAction) - 1] = 0;
  lastActionMs = millis();
  dirty = true;
}

// ===================== profiles =====================
// Skroty: "mod" = Ctrl (Windows/Linux) albo Cmd (macOS) wg Settings -> Host OS.
struct SubMode { const char *name; const char *hint; };
struct Profile { const char *name; uint8_t icon; uint8_t nsub; SubMode sub[6]; const char *tap; };
static const int DV_COLOR = 4;   // indeks trybu COLOR w profilu DaVinci
static Profile PROFILES[NPROF] = {
  {"Volume", 0, 1, {{"VOLUME", "turn: volume"}}, "click/tap: mute"},
  {"Scroll", 1, 2, {{"VERTICAL", "turn: scroll up/down"}, {"HORIZONTAL", "turn: scroll left/right"}}, "tap: invert direction"},
  {"DaVinci", 2, 5, {{"FRAME", "turn: frame step"}, {"ZOOM", "turn: timeline zoom"}, {"SCROLL", "turn: timeline scroll"},
                     {"JKL", "turn: J / L"}, {"COLOR", "turn: Resolve slider"}}, "tap: play | rim: jog"},
  {"KiCad", 3, 5, {{"ZOOM", "turn: zoom"}, {"PAN X", "turn: pan left/right"}, {"PAN Y", "turn: pan up/down"},
                   {"LAYER", "turn: layer + / -"}, {"ROTATE", "turn: rotate R / Shift+R"}}, "tap: Esc"},
  {"Photoshop", 6, 5, {{"BRUSH", "turn: brush size [ ]"}, {"HARDNESS", "turn: hardness"}, {"ZOOM", "turn: zoom in/out"},
                       {"HISTORY", "turn: undo / redo"}, {"LAYER", "turn: layer up/down"}}, "tap: fit on screen"},
  {"Lightroom", 7, 4, {{"ADJUST", "turn: slider - / +"}, {"SLIDER", "turn: prev/next slider"}, {"PHOTO", "turn: prev/next photo"},
                       {"ZOOM", "turn: zoom in/out"}}, "tap: before / after"},
  {"Media", 8, 2, {{"VOLUME", "turn: volume"}, {"TRACK", "turn: prev/next track"}}, "tap: play / pause"},
  // Pixelmator Pro (tylko macOS, zawsze Cmd): skroty z przewodnika Apple "Keyboard shortcuts in Pixelmator Pro"
  {"Pixelmator", 10, 6, {{"BRUSH", "turn: brush size [ ]"}, {"HARDNESS", "turn: softer / harder"}, {"OPACITY", "turn: opacity 10-100%"},
                         {"ZOOM", "turn: zoom out / in"}, {"HISTORY", "turn: undo / redo"}, {"LAYER", "turn: prev/next layer"}}, "tap: zoom to fit"},
  // Spotify: utwory i glosnosc klawiszami multimedialnymi (dzialaja w tle); SEEK i LIKE wymagaja aktywnego okna Spotify
  {"Spotify", 11, 3, {{"VOLUME", "turn: volume"}, {"TRACK", "turn: prev/next track"}, {"SEEK", "turn: seek back / fwd"}}, "tap: play / pause"},
  // Spotkania: tryb = aplikacja (inne skroty wyciszenia); obrot = glosnosc systemu, tap = mikrofon, przycisk CAM = kamera
  {"Meetings", 12, 3, {{"ZOOM", "turn: volume | tap: mic"}, {"TEAMS", "turn: volume | tap: mic"}, {"MEET", "turn: volume | tap: mic"}}, "tap: mute mic"},
  {"System", 13, 4, {{"APPS", "turn: switch app"}, {"DESKTOP", "turn: prev/next desktop"}, {"BRIGHTNESS", "turn: screen brightness"},
                     {"UNDO", "turn: undo / redo"}}, "tap: Mission Control"},
  {"Premiere", 14, 4, {{"FRAME", "turn: frame step"}, {"ZOOM", "turn: timeline zoom"}, {"EDIT", "turn: prev/next edit"},
                       {"JKL", "turn: J / L"}}, "tap: play | rim: jog"},
  // Final Cut Pro (tylko macOS, zawsze Cmd)
  {"Final Cut", 15, 4, {{"FRAME", "turn: frame step"}, {"ZOOM", "turn: timeline zoom"}, {"EDIT", "turn: prev/next edit"},
                        {"JKL", "turn: J / L"}}, "tap: play | rim: jog"},
  {"Figma", 16, 4, {{"ZOOM", "turn: zoom out / in"}, {"OPACITY", "turn: opacity 10-100%"}, {"LAYER", "turn: prev/next layer"},
                    {"HISTORY", "turn: undo / redo"}}, "tap: zoom to fit"},
  // Blender: klawisze numeryczne dzialaja, gdy kursor jest nad widokiem 3D
  {"Blender", 17, 5, {{"ZOOM", "turn: zoom view"}, {"ORBIT", "turn: orbit left/right"}, {"TILT", "turn: orbit up/down"},
                      {"FRAME", "turn: prev/next frame"}, {"HISTORY", "turn: undo / redo"}}, "tap: view all"},
  // Fusion 360: kursor nad widokiem; ORBIT / PAN = przeciaganie srodkowym przyciskiem (jak mysz)
  {"Fusion 360", 18, 4, {{"ZOOM", "turn: zoom"}, {"ORBIT", "turn: orbit"}, {"PAN", "turn: pan"}, {"HISTORY", "turn: undo / redo"}}, "tap: fit (F6)"},
};

// ---- profile klienta (konfigurator w przegladarce, zapis w flash) ----
// Krok makra: t = 0 koniec, 1 klawisz (m = modyfikatory, c = kod klawisza Arduino), 2 multimedia (c = kod Consumer),
// 3 kolko pionowo (c = ilosc, ze znakiem), 4 kolko poziomo, 5 pauza (c = ms), 6 wpisz tekst (c = 0 / 1: tekst profilu),
// 7 nastepny tryb, 8 poprzedni tryb. m: 1 = Ctrl/Cmd wg Host OS, 2 Shift, 4 Alt/Option, 8 Ctrl, 16 Win/Cmd.
// Kazdy tryb ma 4 akcje (makra do NSTEP krokow): obrot w lewo, w prawo, tap, klik. Pusty klik = nastepny tryb.
struct Act { uint8_t t, m; int16_t c; };
static const int NSTEP = 6;
struct Seq { Act s[NSTEP]; };
struct CustomMode { char name[11]; Seq l, r, tap, click; };
struct CustomProf { uint8_t ver, used; char name[13]; uint8_t icon, cr, cg, cb, nsub; CustomMode m[4]; char app[40]; char text[2][33]; };
// format DFTv1.2 (migracja przy pierwszym uruchomieniu 1.3)
struct CustomModeV1 { char name[11]; Act l, r; };
struct CustomProfV1 { uint8_t used; char name[13]; uint8_t icon, cr, cg, cb, nsub; CustomModeV1 m[4]; Act tap; char app[40]; };
static CustomProf custom[NCUSTOM];
static const char *CUSTOM_HINT = "turn: custom action";

static void customToProfile(int i) {   // wypelnia PROFILES[PR_CUSTOM0 + i] wskaznikami na dane klienta
  Profile &P = PROFILES[PR_CUSTOM0 + i];
  CustomProf &c = custom[i];
  P.name = c.name;
  P.icon = c.icon < NICONS ? c.icon : 19;
  P.nsub = c.nsub < 1 ? 1 : (c.nsub > 4 ? 4 : c.nsub);
  for (int k = 0; k < 6; k++) P.sub[k] = {k < 4 ? c.m[k].name : "", CUSTOM_HINT};
  P.tap = "tap: custom action";
}

// ===================== HOME: aplikacje i pulpity =====================
// ready=false: ikona zarezerwowana, ale ukryta (nie pokazujemy funkcji, ktorych nie ma). Pulpit bez aplikacji jest pomijany.
// Klient moze ukryc dowolna ikone w konfiguratorze (hiddenMask, bit = indeks w APPS).
enum AppKind : uint8_t { APP_PROFILE, APP_POMO, APP_SETTINGS };
struct AppDef { const char *name; uint8_t page; AppKind kind; uint8_t profile; uint8_t icon; bool ready; };
static const int NPAGES = 6;
static const char *PAGE_NAMES[NPAGES] = {"EVERYDAY", "CREATIVE", "DESIGN", "SYSTEM", "CUSTOM", "CUSTOM 2"};
static const int NAPPS_BUILTIN = 20;
static AppDef APPS[NAPPS_BUILTIN + NCUSTOM] = {
  {"Pomodoro",     0, APP_POMO,     0,             4, true},
  {"Smart Scroll", 0, APP_PROFILE,  PR_SCROLL,     1, true},
  {"Volume",       0, APP_PROFILE,  PR_VOLUME,     0, true},
  {"Media",        0, APP_PROFILE,  PR_MEDIA,      8, true},
  {"Spotify",      0, APP_PROFILE,  PR_SPOTIFY,   11, true},
  {"Meetings",     0, APP_PROFILE,  PR_MEETINGS,  12, true},
  {"DaVinci",      1, APP_PROFILE,  PR_DAVINCI,    2, true},
  {"Premiere",     1, APP_PROFILE,  PR_PREMIERE,  14, true},
  {"Final Cut",    1, APP_PROFILE,  PR_FINALCUT,  15, true},
  {"Photoshop",    1, APP_PROFILE,  PR_PHOTOSHOP,  6, true},
  {"Lightroom",    1, APP_PROFILE,  PR_LIGHTROOM,  7, true},
  {"Pixelmator",   1, APP_PROFILE,  PR_PIXELMATOR, 10, true},
  {"Figma",        2, APP_PROFILE,  PR_FIGMA,     16, true},
  {"Blender",      2, APP_PROFILE,  PR_BLENDER,   17, true},
  {"Fusion 360",   2, APP_PROFILE,  PR_FUSION,    18, true},
  {"KiCad",        2, APP_PROFILE,  PR_KICAD,      3, true},
  {"System",       3, APP_PROFILE,  PR_SYSTEM,    13, true},
  {"Settings",     3, APP_SETTINGS, 0,             5, true},
  {"OBS Studio",   1, APP_PROFILE,  0,             0, false},   // ukryte: bez domyslnych skrotow; klient moze zrobic wlasny profil
  {"",             0, APP_PROFILE,  0,             0, false},   // rezerwa
};
static int NAPPS = NAPPS_BUILTIN;
static uint8_t pgList[NPAGES][8];  // indeksy w APPS dla kazdego (niepustego) pulpitu
static uint8_t pgCount[NPAGES];
static uint8_t pgId[NPAGES];       // numer pulpitu (nazwa w PAGE_NAMES)
static uint8_t nPages = 0;

static void customApps() {   // profile klienta jako ikony na pulpicie CUSTOM
  NAPPS = NAPPS_BUILTIN;
  for (int i = 0; i < NCUSTOM; i++) {
    customToProfile(i);
    APPS[NAPPS_BUILTIN + i] = {custom[i].name, (uint8_t)(i < 6 ? 4 : 5), APP_PROFILE, (uint8_t)(PR_CUSTOM0 + i), PROFILES[PR_CUSTOM0 + i].icon, custom[i].used != 0};
  }
  NAPPS = NAPPS_BUILTIN + NCUSTOM;
}

static uint16_t profColor565(int prof) { const CustomProf &c = custom[prof - PR_CUSTOM0]; return C565(c.cr, c.cg, c.cb); }
static void accentProfile(int prof) { const CustomProf &c = custom[prof - PR_CUSTOM0]; setAccent(c.cr, c.cg, c.cb); }

static void loadCustom() {
  for (int i = 0; i < NCUSTOM; i++) {
    char k[8]; snprintf(k, sizeof(k), "cp%d", i);
    CustomProf &c = custom[i];
    memset(&c, 0, sizeof(CustomProf));
    size_t len = prefs.getBytesLength(k);
    if (len == sizeof(CustomProf)) prefs.getBytes(k, &c, sizeof(CustomProf));
    else if (len == sizeof(CustomProfV1)) {   // migracja z DFTv1.2: jedna akcja na obrot, tap wspolny dla profilu
      static CustomProfV1 o;
      prefs.getBytes(k, &o, sizeof(o));
      c.ver = 2; c.used = o.used; memcpy(c.name, o.name, sizeof(c.name)); c.icon = o.icon; c.cr = o.cr; c.cg = o.cg; c.cb = o.cb;
      c.nsub = o.nsub; memcpy(c.app, o.app, sizeof(c.app));
      for (int m = 0; m < 4; m++) {
        memcpy(c.m[m].name, o.m[m].name, sizeof(c.m[m].name));
        c.m[m].l.s[0] = o.m[m].l; c.m[m].r.s[0] = o.m[m].r; c.m[m].tap.s[0] = o.tap;
      }
      prefs.putBytes(k, &c, sizeof(CustomProf));
    }
    c.name[12] = 0; c.app[39] = 0; c.text[0][32] = 0; c.text[1][32] = 0;
    for (int m = 0; m < 4; m++) c.m[m].name[10] = 0;
  }
}
static void saveCustom(int i) {
  char k[8]; snprintf(k, sizeof(k), "cp%d", i);
  if (custom[i].used) prefs.putBytes(k, &custom[i], sizeof(CustomProf)); else prefs.remove(k);
}

static void buildPages() {
  uint8_t cnt[NPAGES] = {0};
  uint8_t tmp[NPAGES][8];
  for (int i = 0; i < NAPPS; i++) {
    const AppDef &a = APPS[i];
    if (!a.ready || a.page >= NPAGES || cnt[a.page] >= 6) continue;
    if (i < 32 && (hiddenMask & (1UL << i)) && a.kind != APP_SETTINGS) continue;   // Settings nie da sie ukryc
    tmp[a.page][cnt[a.page]++] = i;
  }
  nPages = 0;
  for (int p = 0; p < NPAGES; p++) {
    if (!cnt[p]) continue;
    for (int j = 0; j < cnt[p]; j++) pgList[nPages][j] = tmp[p][j];
    pgCount[nPages] = cnt[p];
    pgId[nPages] = p;
    nPages++;
  }
  if (homePage >= nPages) homePage = 0;
  if (homeSel >= pgCount[homePage]) homeSel = 0;
}

// ===================== pomodoro =====================
enum PState : uint8_t { P_IDLE, P_FOCUS, P_BREAK, P_PAUSED };
static PState pState = P_IDLE;
static bool pBreak = false;
static uint32_t pEndMs = 0, pRemain = 0;
static uint16_t sessions = 0;

// statystyki Pomodoro: sesje dzis i w ostatnich 7 dniach (gdy znany jest czas; inaczej tylko od wlaczenia)
struct PomoStats { int32_t day; uint8_t cnt[7]; };   // cnt[0] = dzis, cnt[1] = wczoraj, ...
static PomoStats pstats = {0, {0}};
static int32_t today();
static void statsRoll() {
  int32_t d = today();
  if (d <= 0) return;
  if (pstats.day <= 0) { pstats.day = d; return; }
  int shift = d - pstats.day;
  if (shift <= 0) return;
  for (int i = 6; i >= 0; i--) pstats.cnt[i] = (i - shift >= 0) ? pstats.cnt[i - shift] : 0;
  pstats.day = d;
}
static void statsAdd() {
  statsRoll();
  if (pstats.day <= 0) return;
  if (pstats.cnt[0] < 255) pstats.cnt[0]++;
  prefs.putBytes("pst", &pstats, sizeof(pstats));
}
static int statsWeek() { int n = 0; for (int i = 0; i < 7; i++) n += pstats.cnt[i]; return n; }

static bool pomoRunning() { return pState == P_FOCUS || pState == P_BREAK; }
static uint32_t pomoRemain() {
  if (pomoRunning()) { int32_t r = (int32_t)(pEndMs - millis()); return r > 0 ? (uint32_t)r : 0; }
  if (pState == P_PAUSED) return pRemain;
  return (uint32_t)focusMin * 60000UL;
}
static void pomoStart(bool brk) {
  pBreak = brk;
  pRemain = (uint32_t)(brk ? breakMin : focusMin) * 60000UL;
  pEndMs = millis() + pRemain;
  pState = brk ? P_BREAK : P_FOCUS;
}
static void pomoToggle() {
  if (pState == P_IDLE) pomoStart(false);
  else if (pState == P_PAUSED) { pEndMs = millis() + pRemain; pState = pBreak ? P_BREAK : P_FOCUS; }
  else { pRemain = pomoRemain(); pState = P_PAUSED; }
  dirty = true;
}
static void pomoReset() { pState = P_IDLE; pBreak = false; dirty = true; }
// Alarm Pomodoro: 5 mignięć podswietlenia, bez delay() - nie blokuje petli
static bool alertActive = false;
static uint32_t alertStartMs = 0;
static void alertStart() { alertActive = true; alertStartMs = millis(); }
static void alertTick() {
  if (!alertActive) return;
  uint32_t step = (millis() - alertStartMs) / 120;
  if (step >= 10) { alertActive = false; setBacklight(briPct); return; }
  setBacklight(step % 2 == 0 ? 0 : briPct);
}
static void pomoTick() {
  if (pomoRunning() && (int32_t)(millis() - pEndMs) >= 0) {
    bool wasFocus = (pState == P_FOCUS);
    if (wasFocus) { sessions++; statsAdd(); }
    pomoStart(wasFocus);
    alertStart();
    if (rgbPomo) rgbFlash(255, 255, 255, 1500);
    markActive();
    dirty = true;
  }
}
static void fmtTime(char *b, size_t n, uint32_t ms) {
  uint32_t s = (ms + 999) / 1000;
  snprintf(b, n, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// ===================== RGB ring (5x WS2812, GPIO48) =====================
static Adafruit_NeoPixel ring(RGB_COUNT, PIN_RGB, NEO_GRB + NEO_KHZ800);
static float rgbPos = 0.0f;                 // pozycja "swiatelka" w jednostkach LED (0..RGB_COUNT)
static float rgbLevel[RGB_COUNT] = {0};     // poswiata / ogon
static uint32_t rgbTurnMs = 0;
static uint8_t flashR = 0, flashG = 0, flashB = 0;
static uint32_t flashStartMs = 0;
static uint16_t flashDur = 0;

static void hsv2rgb(uint16_t h, uint8_t &r, uint8_t &g, uint8_t &b) {  // S = V = 1
  h %= 360;
  uint8_t region = h / 60;
  float f = (h % 60) / 60.0f;
  uint8_t q = (uint8_t)(255 * (1.0f - f)), t = (uint8_t)(255 * f);
  switch (region) {
    case 0: r = 255; g = t; b = 0; break;
    case 1: r = q; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = t; break;
    case 3: r = 0; g = q; b = 255; break;
    case 4: r = t; g = 0; b = 255; break;
    default: r = 255; g = 0; b = q; break;
  }
}

static uint8_t &rgbColor() { return rgbColorT[themeIdx]; }   // kolor pierscienia dla biezacego motywu

static void rgbBaseColor(uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t c = rgbColor();
  if (c == 0) { r = accR; g = accG; b = accB; }   // kolor motywu (w motywie Color: kolor aplikacji)
  else if (c >= RGB_COLOR_STATES - 1) { r = g = b = 255; }
  else hsv2rgb((uint16_t)(c - 1) * 10, r, g, b);
}
static uint16_t rgbSwatch565() { uint8_t r, g, b; rgbBaseColor(r, g, b); return C565(r, g, b); }
static void rgbColorName(char *b, size_t n) {
  uint8_t c = rgbColor();
  if (c == 0) snprintf(b, n, "Theme");
  else if (c >= RGB_COLOR_STATES - 1) snprintf(b, n, "White");
  else snprintf(b, n, "%d deg", (c - 1) * 10);
}

static void rgbFlash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
  flashR = r; flashG = g; flashB = b; flashStartMs = millis(); flashDur = ms;
}
static void rgbPagePulse() {
  if (!rgbFlashOn || rgbMode == RGB_OFF) return;
  uint8_t r, g, b;
  rgbBaseColor(r, g, b);
  rgbFlash(r / 2, g / 2, b / 2, 380);
}
static void rgbAction(bool red) {
  if (!rgbFlashOn) return;
  uint8_t r, g, b;
  if (red) { r = 255; g = 0; b = 0; } else rgbBaseColor(r, g, b);
  rgbFlash(r, g, b, 300);
}

// obrot pokretla -> przesuniecie swiatelka po pierscieniu (kierunek wg RGB Direction)
static void rgbOnTurn(int d) {
  rgbPos += (float)d * ((float)RGB_COUNT / RGB_DETENTS_PER_REV) * (rgbRev ? -1.0f : 1.0f);
  while (rgbPos < 0.0f) rgbPos += RGB_COUNT;
  while (rgbPos >= (float)RGB_COUNT) rgbPos -= RGB_COUNT;
  rgbTurnMs = millis();
}

static void rgbTick() {
  static uint32_t last = 0;
  static bool lastBlack = false;
  uint32_t now = millis();
  if (now - last < 20) return;
  last = now;

  float cr[RGB_COUNT], cg[RGB_COUNT], cb[RGB_COUNT];
  for (int i = 0; i < RGB_COUNT; i++) cr[i] = cg[i] = cb[i] = 0.0f;

  bool off = (rgbMode == RGB_OFF) || (dimmed && rgbIdleOff);
  if (!off) {
    uint8_t br, bg, bb;
    rgbBaseColor(br, bg, bb);
    bool flashing = flashDur && (now - flashStartMs) < flashDur;
    if (flashing) {                                   // krotki blysk (akcja / koniec Pomodoro)
      float f = 1.0f - (float)(now - flashStartMs) / flashDur;
      for (int i = 0; i < RGB_COUNT; i++) { cr[i] = flashR * f; cg[i] = flashG * f; cb[i] = flashB * f; }
    } else if (micMuted) {                            // Spotkania: wyciszony mikrofon = czerwony, wolny puls
      float lvl = 0.55f + 0.25f * sinf(now / 400.0f);
      for (int i = 0; i < RGB_COUNT; i++) { cr[i] = 255 * lvl; cg[i] = 0; cb[i] = 0; }
    } else if (rgbPomo && (pomoRunning() || pState == P_PAUSED)) {   // pasek postepu Pomodoro
      uint32_t total = (uint32_t)(pBreak ? breakMin : focusMin) * 60000UL;
      uint32_t rem = pomoRemain();
      float frac = total ? 1.0f - (float)(rem > total ? total : rem) / (float)total : 0.0f;
      float lit = frac * RGB_COUNT;
      float pulse = (pState == P_PAUSED) ? (0.45f + 0.35f * sinf(now / 250.0f)) : 1.0f;
      float r = pBreak ? 0 : br, g = pBreak ? 220 : bg, b = pBreak ? 150 : bb;
      for (int i = 0; i < RGB_COUNT; i++) {
        float v = fminf(1.0f, fmaxf(lit - i, 0.0f));
        v = fmaxf(v, 0.06f);
        cr[i] = r * v * pulse; cg[i] = g * v * pulse; cb[i] = b * v * pulse;
      }
    } else {
      switch (rgbMode) {
        case RGB_FOLLOW: {
          bool turning = (now - rgbTurnMs) < 120;
          for (int i = 0; i < RGB_COUNT; i++) {
            float dist = fabsf(rgbPos - i);
            if (dist > RGB_COUNT / 2.0f) dist = RGB_COUNT - dist;
            float v = fmaxf(0.0f, 1.0f - dist);
            rgbLevel[i] = fmaxf(rgbLevel[i] * 0.84f, turning ? v : 0.0f);
            float lvl = fmaxf(rgbLevel[i], 0.07f);
            cr[i] = br * lvl; cg[i] = bg * lvl; cb[i] = bb * lvl;
          }
          break;
        }
        case RGB_AMBIENT:
          for (int i = 0; i < RGB_COUNT; i++) { cr[i] = br; cg[i] = bg; cb[i] = bb; }
          break;
        case RGB_BREATHE: {
          float lvl = 0.15f + 0.85f * (0.5f + 0.5f * sinf(now / 636.6f));
          for (int i = 0; i < RGB_COUNT; i++) { cr[i] = br * lvl; cg[i] = bg * lvl; cb[i] = bb * lvl; }
          break;
        }
        case RGB_RAINBOW:
          for (int i = 0; i < RGB_COUNT; i++) {
            uint8_t r, g, b;
            hsv2rgb((uint16_t)((now / 15 + i * (360 / RGB_COUNT)) % 360), r, g, b);
            cr[i] = r; cg[i] = g; cb[i] = b;
          }
          break;
        default: break;
      }
    }
  }

  float scale = rgbBri / 100.0f;
  bool allZero = true;
  for (int i = 0; i < RGB_COUNT; i++) {
    uint8_t r = (uint8_t)fminf(255.0f, cr[i] * scale);
    uint8_t g = (uint8_t)fminf(255.0f, cg[i] * scale);
    uint8_t b = (uint8_t)fminf(255.0f, cb[i] * scale);
    if (r | g | b) allZero = false;
    ring.setPixelColor(i, ring.Color(r, g, b));
  }
  if (allZero && lastBlack) return;
  lastBlack = allZero;
  ring.show();
}

// ===================== encoder (ISR) =====================
static volatile int32_t encAccum = 0;
static volatile uint8_t encPrev = 0;
static volatile int8_t encSub = 0;

static void IRAM_ATTR encISR() {
  static DRAM_ATTR const int8_t T[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t cur = (gpio_get_level((gpio_num_t)PIN_ENC_A) << 1) | gpio_get_level((gpio_num_t)PIN_ENC_B);
  if (cur == encPrev) return;
  encSub += T[(encPrev << 2) | cur];
  encPrev = cur;
  if (encSub >= ENC_STEPS_PER_DETENT) { encSub = 0; encAccum += 1; }
  else if (encSub <= -ENC_STEPS_PER_DETENT) { encSub = 0; encAccum -= 1; }
}
static int32_t takeEncoder() {
  noInterrupts();
  int32_t v = encAccum;
  encAccum = 0;
  interrupts();
  return ENC_INVERT ? -v : v;
}

// ===================== USB actions =====================
static bool usbReady() {
#ifdef HAVE_TUD
  return tud_mounted();
#else
  return true;
#endif
}
static void sendConsumer(uint16_t code) {
  cc.press(code); delay(2); cc.release(); st.hid++;
}
static void keyTap(uint8_t key) { kb.write(key); st.hid++; }
static void chord(uint8_t mod, uint8_t key) {
  kb.press(mod); kb.press(key); delay(3); kb.releaseAll(); st.hid++;
}
static void chord2(uint8_t m1, uint8_t m2, uint8_t key) {
  kb.press(m1); kb.press(m2); kb.press(key); delay(3); kb.releaseAll(); st.hid++;
}
static uint8_t modKey() { return hostOs ? KEY_LEFT_GUI : KEY_LEFT_CTRL; }   // Ctrl albo Cmd
static void wheel(int w, int pan = 0) {
  while (w || pan) {
    int a = constrain(w, -127, 127), p = constrain(pan, -127, 127);
    mouse.move(0, 0, a, p);
    w -= a; pan -= p;
  }
  st.hid++;
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// ===================== Smart Scroll =====================
// Silnik przewijania: akceleracja zalezna od predkosci obrotu, bezwladnosc, natychmiastowe zatrzymanie przy zmianie kierunku.
// Zadnych opoznien: kazde kliniecie od razu wysyla raport kolka (wiele "zebow" w jednym raporcie).
static const float SC_SENS[3] = {0.5f, 1.0f, 2.0f};            // zebow kola na kliniecie enkodera (MEDIUM = DFTv1)
static const float SC_DECAY[4] = {0.0f, 0.90f, 0.94f, 0.97f};  // wyhamowanie na ~16 ms (LOW/MEDIUM/HIGH)
static float scVel = 0, scFrac = 0, scRate = 0;                // predkosc bezwladnosci [zeby/s], reszta ulamkowa, tempo obrotu [klikniecia/s]
static uint32_t scLastEnc = 0, scTickMs = 0;

static void scrollStop() { scVel = 0; scFrac = 0; scRate = 0; }

static void scrollAdd(float t) {   // t > 0: w dol / w prawo (przed uwzglednieniem Direction)
  scFrac += t;
  int n = (int)scFrac;
  if (!n) return;
  scFrac -= n;
  if (n > 120) n = 120; else if (n < -120) n = -120;
  int s = scrollInv ? -n : n;
  if (subSel[1] == 0) wheel(-s); else wheel(0, s);
}

static void scrollEncoder(int d) {
  const TouchCfg &c = tcfg[1];
  int dir = d > 0 ? 1 : -1, n = abs(d);
  if ((dir > 0 && scVel < 0) || (dir < 0 && scVel > 0) || scFrac * dir < 0) { scVel = 0; scFrac = 0; }   // zmiana kierunku: stop
  uint32_t now = millis();
  float dt = (float)(now - scLastEnc);
  if (dt < 1.0f) dt = 1.0f;
  float inst = n * 1000.0f / dt;
  if (inst > 80.0f) inst = 80.0f;
  scRate = (dt > 400.0f) ? inst : 0.5f * scRate + 0.5f * inst;
  scLastEnc = now;
  float m = 1.0f;
  if (c.adaptive) m = 1.0f + 5.0f * clamp01((scRate - 5.0f) / 30.0f);   // SMART: do 6x szybciej; PRECISION: bez przyspieszenia
  scrollAdd(dir * n * SC_SENS[c.sens] * m);
  if (c.inertia && scRate >= 10.0f) scVel = dir * scRate * SC_SENS[c.sens] * m;
  else scVel = 0;
}

static void scrollTick() {
  if (!(tile == 1 && activeProfile == 1) || !usbReady()) {   // nigdy nie przewijaj poza profilem Scroll
    if (scVel != 0 || scFrac != 0) scrollStop();
    return;
  }
  uint32_t now = millis();
  if (scVel == 0 || now - scLastEnc < 60) { scTickMs = now; return; }
  float dt = (now - scTickMs) / 1000.0f;
  if (dt < 0.008f) return;
  if (dt > 0.05f) dt = 0.05f;
  scTickMs = now;
  scrollAdd(scVel * dt);
  scVel *= powf(SC_DECAY[tcfg[1].inertia ? tcfg[1].inertia : 2], dt * 62.5f);
  if (fabsf(scVel) < 2.0f) { scVel = 0; scFrac = 0; }
}

// ===================== COLOR (DaVinci Resolve) =====================
static bool colorActive() { return tile == 1 && activeProfile == PR_DAVINCI && subSel[PR_DAVINCI] == DV_COLOR; }

// ---- HOVER ADJUST: najedz kursorem na pole w Resolve i krec (bez aplikacji na komputerze) ----
// Obrot = DIAL jako mysz wciska lewy przycisk w miejscu kursora i przesuwa go w poziomie (Resolve zmienia wartosc
// jak przy przeciaganiu); po HOVER_RELEASE_MS bez obrotu puszcza przycisk i cofa kursor tymi samymi krokami.
static const int HOVER_RELEASE_MS = 450;
static bool hvHeld = false;
static int hvSteps = 0;          // suma krokow (do cofniecia), znak = kierunek
static int8_t hvStepSize = HOVER_STEP;
static uint32_t hvLastMs = 0;

static uint8_t hvButton = MOUSE_LEFT, hvMod = 0;
static int hvProfile = -1;
// units: kliniecie enkodera = 4, jednostka pierscienia = 1; button / mod: np. Fusion 360 orbit = srodkowy + Shift
static void hoverAdd(int units, uint8_t button = MOUSE_LEFT, uint8_t mod = 0) {
  if (!usbReady() || !units) return;
  if (hvHeld && (button != hvButton || mod != hvMod || hvProfile != activeProfile)) { hvLastMs = 0; }   // inny tryb: najpierw pusc
  if (hvHeld && hvLastMs == 0) { mouse.release(hvButton); if (hvMod) kb.release(hvMod); hvHeld = false; hvSteps = 0; }
  int reports = abs(units) >= 4 ? abs(units) / 4 : 1;   // jeden raport na kliniecie (pierscien: na jednostke)
  int8_t step = (int8_t)(units > 0 ? hvStepSize : -hvStepSize);
  if (abs(units) < 4) step = units > 0 ? 1 : -1;        // pierscien: najmniejszy krok
  if (!hvHeld) {   // wcisniecie, pauza (Resolve musi zobaczyc klik przed ruchem), potem przeciaganie
    hvButton = button; hvMod = mod; hvProfile = activeProfile;
    if (mod) { kb.press(mod); delay(15); }
    mouse.press(button); hvHeld = true; hvSteps = 0;
    delay(40);
  }
  for (int i = 0; i < reports; i++) { mouse.move(step, 0); hvSteps += step; if (reports > 1) delay(4); }
  hvLastMs = millis();
  st.hid++;
}
static void hoverTick() {
  if (!hvHeld) return;
  if (millis() - hvLastMs < HOVER_RELEASE_MS && tile == 1 && activeProfile == hvProfile) return;
  delay(20);
  mouse.release(hvButton);
  if (hvMod) { kb.release(hvMod); hvMod = 0; }
  hvHeld = false;
  delay(30);
  while (hvSteps) {   // cofniecie tymi samymi malymi krokami (podobne przyspieszenie systemu w obie strony)
    int8_t b = (int8_t)constrain(-hvSteps, -hvStepSize, hvStepSize);
    mouse.move(b, 0);
    hvSteps += b;
  }
}

// System APPS: Cmd (macOS) / Alt (Windows) przytrzymany podczas krecenia, Tab / Shift+Tab, puszczenie po chwili
static bool swHeld = false;
static uint32_t swLastMs = 0;
static void switcherStep(int dir) {
  uint8_t m = hostOs ? KEY_LEFT_GUI : KEY_LEFT_ALT;
  if (!swHeld) { kb.press(m); swHeld = true; delay(15); }
  if (dir < 0) kb.press(KEY_LEFT_SHIFT);
  kb.press(KEY_TAB); delay(3); kb.release(KEY_TAB);
  if (dir < 0) kb.release(KEY_LEFT_SHIFT);
  swLastMs = millis();
  st.hid++;
}
static void switcherTick() {
  if (swHeld && (millis() - swLastMs > 700 || !(tile == 1 && activeProfile == PR_SYSTEM))) { kb.releaseAll(); swHeld = false; }
}

// Spotkania: stan mikrofonu wedlug DIAL (aplikacja go nie zwraca); pierscien swieci na czerwono, gdy wyciszony
static void meetingToggle(bool camera) {
  uint8_t sub = subSel[PR_MEETINGS];
  bool mac = hostOs;
  if (sub == 0) {          // Zoom: Cmd+Shift+A / Alt+A (mikrofon), Cmd+Shift+V / Alt+V (kamera)
    if (mac) chord2(KEY_LEFT_GUI, KEY_LEFT_SHIFT, camera ? 'v' : 'a'); else chord(KEY_LEFT_ALT, camera ? 'v' : 'a');
  } else if (sub == 1) {   // Teams: Cmd/Ctrl+Shift+M (mikrofon), Cmd/Ctrl+Shift+O (kamera)
    chord2(modKey(), KEY_LEFT_SHIFT, camera ? 'o' : 'm');
  } else {                 // Google Meet: Cmd/Ctrl+D (mikrofon), Cmd/Ctrl+E (kamera)
    chord(modKey(), camera ? 'e' : 'd');
  }
  if (!camera) { micMuted = !micMuted; setAction(micMuted ? "MIC OFF" : "MIC ON"); rgbAction(micMuted); }
  else { setAction("CAMERA"); rgbAction(false); }
}

// akcja z profilu klienta
static int customIdx() { return activeProfile - PR_CUSTOM0; }
static void runAct(const Act &a) {
  if (!a.t) return;
  if (a.t == 5) { delay(constrain((int)a.c, 0, 2000)); return; }
  if (a.t == 6) {   // wpisz tekst (tylko znaki ASCII; uklad klawiatury komputera: US / polski programisty)
    if (activeProfile >= PR_CUSTOM0) { kb.print(custom[customIdx()].text[a.c & 1]); st.hid++; }
    return;
  }
  if (a.t == 7 || a.t == 8) {
    const Profile &P = PROFILES[activeProfile];
    if (P.nsub > 1) { subSel[activeProfile] = (subSel[activeProfile] + (a.t == 7 ? 1 : P.nsub - 1)) % P.nsub; rgbAction(false); dirty = true; }
    return;
  }
  if (a.t == 1) {
    if (a.m & 1) kb.press(modKey());
    if (a.m & 2) kb.press(KEY_LEFT_SHIFT);
    if (a.m & 4) kb.press(KEY_LEFT_ALT);
    if (a.m & 8) kb.press(KEY_LEFT_CTRL);
    if (a.m & 16) kb.press(KEY_LEFT_GUI);
    if (a.c) { kb.press((uint8_t)a.c); delay(3); }
    kb.releaseAll();
    st.hid++;
  } else if (a.t == 2) sendConsumer((uint16_t)a.c);
  else if (a.t == 3) wheel(a.c);
  else if (a.t == 4) wheel(0, a.c);
}
static bool seqEmpty(const Seq &q) { return q.s[0].t == 0; }
static void runSeq(const Seq &q) {   // makro: kroki po kolei do pierwszego pustego
  for (int i = 0; i < NSTEP && q.s[i].t; i++) runAct(q.s[i]);
}

static uint8_t pxOpacity = 10;   // Pixelmator OPACITY: 1..10 = 10..100% (wlasny stan; aplikacja nie zwraca wartosci)

static void doTurn(int d) {
  int n = abs(d) > 6 ? 6 : abs(d);
  int dir = d > 0 ? 1 : -1;
  uint8_t sub = subSel[activeProfile];
  uint8_t M = modKey();
  switch (activeProfile) {
    case PR_VOLUME:
      for (int i = 0; i < n; i++) sendConsumer(dir > 0 ? CONSUMER_CONTROL_VOLUME_INCREMENT : CONSUMER_CONTROL_VOLUME_DECREMENT);
      setAction(dir > 0 ? "VOL +" : "VOL -");
      break;
    case PR_SCROLL:  // Smart Scroll: clockwise = down / right (akceleracja, bezwladnosc, patrz scrollEncoder)
      scrollEncoder(d);
      break;
    case PR_DAVINCI:
      if (sub == DV_COLOR) { hoverAdd(d * 4); break; }
      for (int i = 0; i < n; i++) {
        if (sub == 0) keyTap(dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
        else if (sub == 1) chord(M, dir > 0 ? '=' : '-');
        else if (sub == 2) wheel(0, dir);
        else keyTap(dir > 0 ? 'l' : 'j');
      }
      break;
    case PR_KICAD:
      for (int i = 0; i < n; i++) {
        if (sub == 0) wheel(dir);
        else if (sub == 1) { kb.press(M); wheel(-dir); kb.releaseAll(); }
        else if (sub == 2) { kb.press(KEY_LEFT_SHIFT); wheel(-dir); kb.releaseAll(); }
        else if (sub == 3) { kb.write(dir > 0 ? '+' : '-'); st.hid++; }   // jak w DFTv1
        else if (dir > 0) chord(KEY_LEFT_SHIFT, 'r');   // KiCad 7+: Shift+R = obrot zgodnie z ruchem wskazowek
        else keyTap('r');
      }
      break;
    case PR_PHOTOSHOP:
      for (int i = 0; i < n; i++) {
        if (sub == 0) keyTap(dir > 0 ? ']' : '[');
        else if (sub == 1) chord(KEY_LEFT_SHIFT, dir > 0 ? ']' : '[');
        else if (sub == 2) chord(M, dir > 0 ? '=' : '-');
        else if (sub == 3) { if (dir > 0) chord2(M, KEY_LEFT_SHIFT, 'z'); else chord(M, 'z'); }
        else chord(KEY_LEFT_ALT, dir > 0 ? ']' : '[');   // Alt/Option + ] = warstwa wyzej
      }
      setAction(sub == 3 ? (dir > 0 ? "REDO" : "UNDO") : (dir > 0 ? "+" : "-"));
      break;
    case PR_LIGHTROOM:
      if (sub == 0) {   // szybki obrot = duze kroki (Shift)
        for (int i = 0; i < n; i++) {
          if (abs(d) >= 3) chord(KEY_LEFT_SHIFT, dir > 0 ? KEY_KP_PLUS : KEY_KP_MINUS);
          else keyTap(dir > 0 ? KEY_KP_PLUS : KEY_KP_MINUS);
        }
        setAction(dir > 0 ? "+" : "-");
      } else {
        for (int i = 0; i < n; i++) {
          if (sub == 1) keyTap(dir > 0 ? '.' : ',');
          else if (sub == 2) chord(M, dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
          else chord(M, dir > 0 ? '=' : '-');
        }
      }
      break;
    case PR_PIXELMATOR: {
      const uint8_t C = KEY_LEFT_GUI;   // Pixelmator Pro jest tylko na macOS
      if (sub == 2) {   // cyfra ustawia krycie pedzla (Paint/Erase) albo warstwy (narzedzie Style): 1 = 10% ... 0 = 100%
        pxOpacity = (uint8_t)constrain((int)pxOpacity + dir, 1, 10);
        keyTap(pxOpacity == 10 ? '0' : (char)('0' + pxOpacity));
        char b[16]; snprintf(b, sizeof(b), "OPACITY %u%%", pxOpacity * 10); setAction(b);
        break;
      }
      for (int i = 0; i < n; i++) {
        if (sub == 0) keyTap(dir > 0 ? ']' : '[');
        else if (sub == 1) chord(KEY_LEFT_SHIFT, dir > 0 ? ']' : '[');
        else if (sub == 3) chord(C, dir > 0 ? '=' : '-');
        else if (sub == 4) { if (dir > 0) chord2(C, KEY_LEFT_SHIFT, 'z'); else chord(C, 'z'); }
        else chord(KEY_LEFT_ALT, dir > 0 ? ']' : '[');   // Option+] = nastepna warstwa na liscie
      }
      setAction(sub == 4 ? (dir > 0 ? "REDO" : "UNDO") : (dir > 0 ? "+" : "-"));
      break;
    }
    case PR_SPOTIFY:
      if (sub == 2) {   // Shift+strzalka = przewijanie w utworze (okno Spotify musi byc aktywne)
        for (int i = 0; i < n; i++) chord(KEY_LEFT_SHIFT, dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
        setAction(dir > 0 ? "SEEK >>" : "<< SEEK");
        break;
      }
      // VOLUME / TRACK: jak Media
    case PR_MEDIA:
      if (sub == 0) {
        for (int i = 0; i < n; i++) sendConsumer(dir > 0 ? CONSUMER_CONTROL_VOLUME_INCREMENT : CONSUMER_CONTROL_VOLUME_DECREMENT);
        setAction(dir > 0 ? "VOL +" : "VOL -");
      } else {   // utwory: jeden krok na kliniecie, bez serii przy szybkim obrocie
        sendConsumer(dir > 0 ? CONSUMER_CONTROL_SCAN_NEXT : CONSUMER_CONTROL_SCAN_PREVIOUS);
        setAction(dir > 0 ? "NEXT" : "PREVIOUS");
      }
      break;
    case PR_MEETINGS:
      for (int i = 0; i < n; i++) sendConsumer(dir > 0 ? CONSUMER_CONTROL_VOLUME_INCREMENT : CONSUMER_CONTROL_VOLUME_DECREMENT);
      setAction(dir > 0 ? "VOL +" : "VOL -");
      break;
    case PR_SYSTEM:
      if (sub == 0) switcherStep(dir);
      else if (sub == 1) {   // pulpity: Ctrl+strzalka (macOS) / Win+Ctrl+strzalka (Windows), jeden krok na kliniecie
        if (hostOs) chord(KEY_LEFT_CTRL, dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
        else chord2(KEY_LEFT_GUI, KEY_LEFT_CTRL, dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
      } else if (sub == 2) {
        for (int i = 0; i < n; i++) sendConsumer(dir > 0 ? CONSUMER_CONTROL_BRIGHTNESS_INCREMENT : CONSUMER_CONTROL_BRIGHTNESS_DECREMENT);
        setAction(dir > 0 ? "BRIGHTER" : "DIMMER");
      } else {
        for (int i = 0; i < n; i++) { if (dir > 0) chord2(M, KEY_LEFT_SHIFT, 'z'); else chord(M, 'z'); }
        setAction(dir > 0 ? "REDO" : "UNDO");
      }
      break;
    case PR_PREMIERE:
    case PR_FINALCUT: {
      uint8_t C = activeProfile == PR_FINALCUT ? KEY_LEFT_GUI : M;   // Final Cut: tylko macOS
      for (int i = 0; i < n; i++) {
        if (sub == 0) keyTap(dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
        else if (sub == 1) {   // Premiere: = / -, Final Cut: Cmd+= / Cmd+-
          if (activeProfile == PR_PREMIERE) keyTap(dir > 0 ? '=' : '-'); else chord(C, dir > 0 ? '=' : '-');
        }
        else if (sub == 2) keyTap(dir > 0 ? KEY_DOWN_ARROW : KEY_UP_ARROW);   // nastepny / poprzedni punkt ciecia
        else keyTap(dir > 0 ? 'l' : 'j');
      }
      break;
    }
    case PR_FIGMA:
      if (sub == 1) {   // cyfra = krycie zaznaczonej warstwy: 1 = 10% ... 0 = 100%
        pxOpacity = (uint8_t)constrain((int)pxOpacity + dir, 1, 10);
        keyTap(pxOpacity == 10 ? '0' : (char)('0' + pxOpacity));
        char b[16]; snprintf(b, sizeof(b), "OPACITY %u%%", pxOpacity * 10); setAction(b);
        break;
      }
      for (int i = 0; i < n; i++) {
        if (sub == 0) chord(M, dir > 0 ? '=' : '-');
        else if (sub == 2) { if (dir > 0) keyTap(KEY_TAB); else chord(KEY_LEFT_SHIFT, KEY_TAB); }
        else { if (dir > 0) chord2(M, KEY_LEFT_SHIFT, 'z'); else chord(M, 'z'); }
      }
      break;
    case PR_BLENDER:
      for (int i = 0; i < n; i++) {
        if (sub == 0) wheel(dir);                                              // kursor nad widokiem 3D
        else if (sub == 1) keyTap(dir > 0 ? KEY_KP_6 : KEY_KP_4);              // obrot widoku o 15 stopni
        else if (sub == 2) keyTap(dir > 0 ? KEY_KP_8 : KEY_KP_2);
        else if (sub == 3) keyTap(dir > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW); // klatka
        else { if (dir > 0) chord2(M, KEY_LEFT_SHIFT, 'z'); else chord(M, 'z'); }
      }
      break;
    case PR_FUSION:
      if (sub == 0) { for (int i = 0; i < n; i++) wheel(dir); }
      else if (sub == 1) hoverAdd(d * 4, MOUSE_MIDDLE, KEY_LEFT_SHIFT);   // orbit: Shift + srodkowy przycisk
      else if (sub == 2) hoverAdd(d * 4, MOUSE_MIDDLE, 0);                // pan: srodkowy przycisk
      else { for (int i = 0; i < n; i++) chord(M, dir > 0 ? 'y' : 'z'); setAction(dir > 0 ? "REDO" : "UNDO"); }
      break;
    default:
      if (activeProfile >= PR_CUSTOM0) {
        const CustomProf &c = custom[activeProfile - PR_CUSTOM0];
        const Seq &q = dir > 0 ? c.m[sub & 3].r : c.m[sub & 3].l;
        for (int i = 0; i < n; i++) runSeq(q);
      }
      break;
  }
}

static void tapAction() {
  rgbAction(activeProfile == PR_VOLUME);
  switch (activeProfile) {
    case PR_VOLUME: sendConsumer(CONSUMER_CONTROL_MUTE); setAction("MUTE"); break;
    case PR_SCROLL: scrollStop(); scrollInv = !scrollInv; savePrefs(); setAction(scrollInv ? "INVERTED" : "NORMAL"); break;
    case PR_DAVINCI:
      if (subSel[PR_DAVINCI] != DV_COLOR) { keyTap(' '); setAction("PLAY/PAUSE"); }
      break;
    case PR_KICAD: keyTap(KEY_ESC); setAction("ESC"); break;
    case PR_PHOTOSHOP: chord(modKey(), '0'); setAction("FIT"); break;
    case PR_LIGHTROOM: keyTap('\\'); setAction("BEFORE/AFTER"); break;
    case PR_MEDIA:
    case PR_SPOTIFY: sendConsumer(CONSUMER_CONTROL_PLAY_PAUSE); setAction("PLAY/PAUSE"); break;
    case PR_PIXELMATOR: chord(KEY_LEFT_GUI, '0'); setAction("FIT"); break;
    case PR_MEETINGS: meetingToggle(false); break;
    case PR_SYSTEM:   // Mission Control (Ctrl+strzalka w gore) / Widok zadan (Win+Tab)
      if (hostOs) chord(KEY_LEFT_CTRL, KEY_UP_ARROW); else chord(KEY_LEFT_GUI, KEY_TAB);
      setAction("OVERVIEW");
      break;
    case PR_PREMIERE:
    case PR_FINALCUT: keyTap(' '); setAction("PLAY/PAUSE"); break;
    case PR_FIGMA: chord(KEY_LEFT_SHIFT, '1'); setAction("FIT"); break;
    case PR_BLENDER: keyTap(KEY_HOME); setAction("VIEW ALL"); break;
    case PR_FUSION: keyTap(KEY_F6); setAction("FIT"); break;
    default:
      if (activeProfile >= PR_CUSTOM0) {
        const Seq &q = custom[customIdx()].m[subSel[activeProfile] & 3].tap;
        if (!seqEmpty(q)) { runSeq(q); setAction("ACTION"); }
      }
      break;
  }
  dirty = true;
}

static int tcSel = 0, tcTop = 0;
static uint8_t tcProf = 0;
static void goTile(int t);
static const int NSET = 16;
static void tourStart();
static void settingsCycle(int row) {
  if (row == 0) { themeIdx = (themeIdx + 1) % NTHEMES; applyTheme(); }
  else if (row == 1) hostOs ^= 1;
  else if (row == 2) { scrollStop(); scrollInv = !scrollInv; }
  else if (row == 3) { briPct += 20; if (briPct > 100) briPct = 20; setBacklight(briPct); }
  else if (row == 4) {   // szybki wlacznik pierscienia RGB
    if (rgbMode != RGB_OFF) { rgbLastMode = rgbMode; rgbMode = RGB_OFF; }
    else rgbMode = (rgbLastMode == RGB_OFF) ? RGB_FOLLOW : rgbLastMode;
  }
  else if (row == 5) {   // dlugosc przerwy Pomodoro (biezaca przerwa nie jest skracana)
    int i = 0; while (i < 3 && BREAK_OPTS[i] != breakMin) i++;
    breakMin = BREAK_OPTS[(i + 1) % 4];
  }
  else if (row == 6) tcfg[2].ring = !tcfg[2].ring;
  else if (row == 7) jogDegIdx = (jogDegIdx + 1) % 5;
  else if (row == 8) tcfg[2].inertia = tcfg[2].inertia ? 0 : 2;
  else if (row == 9) tcfg[2].rev = !tcfg[2].rev;
  else if (row == 10) { tcProf = activeProfile; tcSel = 0; tcTop = 0; goTile(5); return; }   // Smart Touch Ring - ustawienia profili
  else if (row == 11) autoSwitch = !autoSwitch;
  else if (row == 12) clockFace = !clockFace;
  else if (row == 13) standup = !standup;
  else if (row == 14) { tourStart(); return; }
  else return;
  savePrefs();
  dirty = true;
}

static void ledCycle(int row) {
  switch (row) {
    case 0: rgbMode = (rgbMode + 1) % RGB_MODES; if (rgbMode != RGB_OFF) rgbLastMode = rgbMode; break;
    case 1: ledEdit = !ledEdit; break;
    case 2: rgbBri += 20; if (rgbBri > 100) rgbBri = 20; break;
    case 3: rgbPomo = !rgbPomo; break;
    case 4: rgbFlashOn = !rgbFlashOn; break;
    case 5: rgbIdleOff = !rgbIdleOff; break;
    case 6: rgbRev = !rgbRev; break;
  }
  savePrefs();
  dirty = true;
}

// ===================== icons / widgets =====================
static void drawIcon(TFT_eSprite &s, int id, int cx, int cy, uint16_t c) {
  switch (id) {
    case 0:  // speaker
      s.fillRect(cx - 10, cy - 4, 6, 8, c);
      s.fillTriangle(cx - 4, cy - 4, cx + 4, cy - 11, cx + 4, cy + 11, c);
      s.fillTriangle(cx - 4, cy - 4, cx - 4, cy + 4, cx + 4, cy + 11, c);
      s.drawFastVLine(cx + 8, cy - 4, 9, c);
      s.drawFastVLine(cx + 12, cy - 7, 15, c);
      break;
    case 1:  // scroll arrows (gora / dol)
      s.fillTriangle(cx, cy - 12, cx - 8, cy - 2, cx + 8, cy - 2, c);
      s.fillTriangle(cx, cy + 12, cx - 8, cy + 2, cx + 8, cy + 2, c);
      break;
    case 9:  // scroll arrows (lewo / prawo)
      s.fillTriangle(cx - 12, cy, cx - 2, cy - 8, cx - 2, cy + 8, c);
      s.fillTriangle(cx + 12, cy, cx + 2, cy - 8, cx + 2, cy + 8, c);
      break;
    case 2:  // color wheels
      s.drawCircle(cx, cy, 12, c);
      s.fillCircle(cx, cy - 5, 3, c);
      s.fillCircle(cx - 5, cy + 4, 3, c);
      s.fillCircle(cx + 5, cy + 4, 3, c);
      break;
    case 3:  // chip
      s.drawRect(cx - 8, cy - 8, 17, 17, c);
      for (int i = -1; i <= 1; i++) {
        s.drawFastHLine(cx - 13, cy + i * 5, 5, c);
        s.drawFastHLine(cx + 9, cy + i * 5, 5, c);
        s.drawFastVLine(cx + i * 5, cy - 13, 5, c);
        s.drawFastVLine(cx + i * 5, cy + 9, 5, c);
      }
      break;
    case 4:  // timer
      s.drawCircle(cx, cy + 2, 11, c);
      s.drawLine(cx, cy + 2, cx, cy - 6, c);
      s.drawLine(cx, cy + 2, cx + 5, cy + 5, c);
      s.fillRect(cx - 3, cy - 13, 7, 3, c);
      break;
    case 5:  // settings (sliders)
      for (int i = -1; i <= 1; i++) {
        s.drawFastHLine(cx - 11, cy + i * 8, 23, c);
        s.fillCircle(cx + i * 6, cy + i * 8, 3, c);
      }
      break;
    case 6:  // brush (Photoshop)
      for (int k = -1; k <= 1; k++) s.drawLine(cx + 11 + k, cy - 12, cx - 1 + k, cy + 2, c);
      s.fillCircle(cx - 4, cy + 6, 5, c);
      s.fillTriangle(cx - 9, cy + 6, cx - 13, cy + 13, cx - 4, cy + 11, c);
      break;
    case 7:  // photo (Lightroom): ramka, gory, slonce
      s.drawRoundRect(cx - 13, cy - 10, 27, 21, 3, c);
      s.fillTriangle(cx - 10, cy + 7, cx - 2, cy - 3, cx + 5, cy + 7, c);
      s.fillTriangle(cx + 1, cy + 7, cx + 6, cy + 1, cx + 11, cy + 7, c);
      s.fillCircle(cx + 7, cy - 4, 2, c);
      break;
    case 10:  // Pixelmator: siatka pikseli
      for (int r = 0; r < 3; r++)
        for (int q = 0; q < 3; q++) {
          int x = cx - 12 + q * 9, y = cy - 12 + r * 9;
          if ((r + q) % 2 == 0) s.fillRoundRect(x, y, 7, 7, 2, c); else s.drawRoundRect(x, y, 7, 7, 2, c);
        }
      break;
    case 11:  // Spotify: kolo z trzema lukami
      s.fillCircle(cx, cy, 13, c);
      for (int k = 0; k < 3; k++) {   // luki o wspolnym srodku pod ikona (kat 180 = gora w drawSmoothArc)
        int r = 15 - k * 4;
        s.drawSmoothArc(cx, cy + 12, r, r - 2, 140, 220, TFT_BLACK, c);
      }
      break;
    case 12:  // mikrofon
      s.fillRoundRect(cx - 5, cy - 13, 10, 17, 5, c);
      s.drawSmoothArc(cx, cy - 2, 9, 7, 270, 360, c, TFT_BLACK);   // luk "U" pod mikrofonem (kat 0 = dol)
      s.drawSmoothArc(cx, cy - 2, 9, 7, 0, 90, c, TFT_BLACK);
      s.fillRect(cx - 1, cy + 8, 3, 4, c);
      s.fillRect(cx - 6, cy + 11, 13, 2, c);
      break;
    case 13:  // monitor (System)
      s.drawRoundRect(cx - 13, cy - 10, 27, 18, 2, c);
      s.fillRect(cx - 2, cy + 8, 5, 4, c);
      s.fillRect(cx - 7, cy + 12, 15, 2, c);
      break;
    case 14:  // Premiere: tasma filmowa
      s.drawRoundRect(cx - 12, cy - 11, 24, 22, 3, c);
      for (int k = -1; k <= 1; k++) { s.fillRect(cx - 10, cy - 3 + k * 6, 3, 3, c); s.fillRect(cx + 8, cy - 3 + k * 6, 3, 3, c); }
      s.fillTriangle(cx - 3, cy - 5, cx - 3, cy + 5, cx + 5, cy, c);
      break;
    case 15:  // Final Cut: klaps
      s.fillRect(cx - 12, cy - 3, 24, 15, c);
      s.fillTriangle(cx - 12, cy - 6, cx + 12, cy - 12, cx + 12, cy - 7, c);
      s.fillTriangle(cx - 12, cy - 6, cx - 12, cy - 4, cx + 12, cy - 7, c);
      break;
    case 16:  // Figma: piec kolek
      s.fillCircle(cx - 4, cy - 9, 4, c); s.fillCircle(cx + 4, cy - 9, 4, c);
      s.fillCircle(cx - 4, cy, 4, c); s.drawCircle(cx + 4, cy, 4, c);
      s.fillCircle(cx - 4, cy + 9, 4, c);
      break;
    case 17:  // Blender: kolo z orbita
      s.fillCircle(cx + 2, cy + 1, 7, c);
      s.fillCircle(cx + 2, cy + 1, 3, TFT_BLACK);
      s.drawLine(cx - 12, cy - 6, cx - 3, cy - 2, c); s.drawLine(cx - 12, cy - 5, cx - 3, cy - 1, c);
      s.drawLine(cx - 10, cy + 2, cx - 4, cy + 3, c);
      break;
    case 18:  // Fusion 360: szescian
      s.drawLine(cx, cy - 12, cx + 11, cy - 6, c); s.drawLine(cx, cy - 12, cx - 11, cy - 6, c);
      s.drawLine(cx - 11, cy - 6, cx, cy, c); s.drawLine(cx + 11, cy - 6, cx, cy, c);
      s.drawLine(cx - 11, cy - 6, cx - 11, cy + 7, c); s.drawLine(cx + 11, cy - 6, cx + 11, cy + 7, c);
      s.drawLine(cx, cy, cx, cy + 13, c); s.drawLine(cx - 11, cy + 7, cx, cy + 13, c); s.drawLine(cx + 11, cy + 7, cx, cy + 13, c);
      break;
    case 19:  // wlasny profil: gwiazda
      for (int k = 0; k < 5; k++) {
        float a1 = (-90 + k * 72) * 0.0174533f, a2 = (-90 + k * 72 + 144) * 0.0174533f;
        s.drawWideLine(cx + 12 * cosf(a1), cy + 12 * sinf(a1), cx + 12 * cosf(a2), cy + 12 * sinf(a2), 2.0f, c, TFT_BLACK);
      }
      break;
    case 8:  // media: play + pause
      s.fillTriangle(cx - 11, cy - 9, cx - 11, cy + 9, cx + 2, cy, c);
      s.fillRect(cx + 5, cy - 9, 3, 18, c);
      s.fillRect(cx + 10, cy - 9, 3, 18, c);
      break;
  }
}

static int scrollIcon(int icon) { return (icon == 1 && subSel[PR_SCROLL]) ? 9 : icon; }   // Smart Scroll: ikona wg osi

static void drawRing(TFT_eSprite &s) { s.drawSmoothArc(120, 120, 119, 115, 0, 360, cDim, TFT_BLACK); }
static void drawBezelMark(TFT_eSprite &s, int ang) {
  int a = ((ang % 360) + 360) % 360, e = a + 24;
  if (e <= 360) s.drawSmoothArc(120, 120, 119, 113, a, e, cAcc, TFT_BLACK);
  else {
    s.drawSmoothArc(120, 120, 119, 113, a, 360, cAcc, TFT_BLACK);
    s.drawSmoothArc(120, 120, 119, 113, 0, e - 360, cAcc, TFT_BLACK);
  }
}
static void drawDots(TFT_eSprite &s, int n, int idx) {   // n kropek, aktywna idx
  if (n < 2) return;
  for (int i = 0; i < n; i++) {
    int x = 120 + (int)((i - (n - 1) / 2.0f) * 14);
    if (i == idx) s.fillCircle(x, 227, 3, cAcc); else s.fillCircle(x, 227, 2, C565(90, 90, 90));
  }
}

// ===================== tiles =====================
// pierscien jogu (DaVinci): kropki na obrzezu + kropka z ogonem za palcem
static void drawJog(TFT_eSprite &s) {
  for (int a = 0; a < 360; a += 10) {
    float r = a * 0.0174533f;
    s.fillCircle(120 + (int)(106 * cosf(r)), 120 + (int)(106 * sinf(r)), 1, C565(70, 70, 70));
  }
  if (jogActive || jogInertiaOn) {
    for (int k = 5; k >= 0; k--) {
      float r = (jogShowAng - jogDir * k * 8.0f) * 0.0174533f;
      int rad = 7 - k; if (rad < 2) rad = 2;
      s.fillCircle(120 + (int)(106 * cosf(r)), 120 + (int)(106 * sinf(r)), rad, k == 0 ? TFT_WHITE : cAcc);
    }
  }
}

// pozycje ikon na pulpicie: 1-2 ikony w jednym rzedzie, 3-4 w siatce 2x2, 5-6 w siatce 3+2/3
static void homePos(int n, int i, int &x, int &y) {
  if (n <= 2) { x = (n == 1) ? 120 : (i == 0 ? 82 : 158); y = 116; }
  else if (n <= 4) { x = (n == 3 && i == 2) ? 120 : ((i % 2 == 0) ? 84 : 156); y = (i < 2) ? 90 : 150; }
  else {
    if (i < 3) { x = 62 + 58 * i; y = 88; }
    else { int b = n - 3, j = i - 3; x = (b == 3) ? 62 + 58 * j : (b == 2 ? 91 + 58 * j : 120); y = 162; }
  }
}

// animacja wyboru ikony: promien 27 -> 30 px w SEL_ANIM_MS (ease-out); start przy zmianie zaznaczenia
static const uint32_t SEL_ANIM_MS = 170;
static uint32_t selAnimMs = 0;
static bool uiAnimating = false;
static int selRadius() {
  uint32_t dt = millis() - selAnimMs;
  if (dt >= SEL_ANIM_MS) return 30;
  float t = (float)dt / SEL_ANIM_MS;
  uiAnimating = true;
  return 27 + (int)(3.0f * (1.0f - (1.0f - t) * (1.0f - t)) + 0.5f);
}

static void tileHome(TFT_eSprite &s) {
  static int lastSel = -1, lastPage = -1;
  if (homeSel != lastSel || homePage != lastPage) { lastSel = homeSel; lastPage = homePage; selAnimMs = millis(); }
  s.fillSprite(TFT_BLACK);
  drawRing(s);
  s.setTextDatum(MC_DATUM);
  if (pomoRunning() || pState == P_PAUSED) {
    char b[8]; fmtTime(b, sizeof(b), pomoRemain());
    s.setTextColor(pBreak ? cBreak : cAcc, TFT_BLACK);
    s.drawString(b, 120, 32, 2);
  } else {
    s.setTextColor(cGray, TFT_BLACK);
    s.drawString(PAGE_NAMES[pgId[homePage]], 120, 32, 2);
  }
  int n = pgCount[homePage];
  for (int i = 0; i < n; i++) {
    const AppDef &a = APPS[pgList[homePage][i]];
    int x, y; homePos(n, i, x, y);
    bool sel = (i == homeSel);
    int ic = scrollIcon(a.icon);
    uint16_t own = !themeMulti() ? C565(200, 200, 200)
                   : (a.profile >= PR_CUSTOM0 && a.kind == APP_PROFILE) ? profColor565(a.profile) : iconColor(ic);
    int r = sel ? selRadius() : 27;   // zaznaczona ikona plynnie sie powieksza
    s.fillCircle(x, y, r, sel ? cDim : cBtn);
    if (sel) { s.drawCircle(x, y, r, cAcc); s.drawCircle(x, y, r - 1, cAcc); }
    drawIcon(s, ic, x, y, sel ? cAcc : own);
    if (a.kind == APP_PROFILE && a.profile == activeProfile) s.fillCircle(x + 20, y - 20, 3, cAcc);
  }
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString(APPS[pgList[homePage][homeSel]].name, 120, 200, 4);
  drawDots(s, nPages, homePage);
}

static void drawSubDots(TFT_eSprite &s, const Profile &P) {
  if (P.nsub < 2) return;
  for (int i = 0; i < P.nsub; i++) {
    int x = 120 + (int)((i - (P.nsub - 1) / 2.0f) * 10);
    if (i == subSel[activeProfile]) s.fillCircle(x, 204, 3, cAcc); else s.fillCircle(x, 204, 2, C565(90, 90, 90));
  }
}

// DaVinci COLOR: najedz kursorem na wartosc w Resolve i krec (bez aplikacji na komputerze)
static void tileColor(TFT_eSprite &s, const Profile &P) {
  s.setTextDatum(MC_DATUM);
  s.setTextColor(cGray, TFT_BLACK);
  s.drawString("DaVinci  COLOR", 120, 34, 2);
  s.setTextColor(cAcc, TFT_BLACK);
  s.drawString("HOVER + TURN", 120, 80, 4);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString("point the mouse at any", 120, 112, 2);
  s.drawString("value in Resolve, turn", 120, 130, 2);
  bool recent = lastAction[0] && (millis() - lastActionMs < 1200);
  const char *sz = hvStepSize == 1 ? "FINE" : hvStepSize == 2 ? "NORMAL" : "COARSE";
  s.fillRoundRect(80, 150, 80, 26, 13, cBtn);
  s.setTextColor(cAcc, cBtn);
  s.drawString(recent ? lastAction : sz, 120, 163, 2);
  s.setTextColor(C565(110, 110, 110), TFT_BLACK);
  s.drawString("tap: step size | rim: finest", 120, 188, 1);
  drawSubDots(s, P);
}


// ===================== Now Playing (Spotify przez DIAL FT Helper) =====================
// Helper (tools/helper, macOS) wysyla: "NP META <grane 0/1> <poz_s> <dl_s> <id>\t<tytul>\t<wykonawca>", "NP OFF",
// okladke na zadanie "NP NEED <id>": "NP ART <id> <w> <h> <n>", "NP A <nr> <base64>", "NP END <id>".
static const int ART_W = 112, ART_H = 112, ART_CHUNK = 192;
static const int ART_CHUNKS = (ART_W * ART_H * 2 + ART_CHUNK - 1) / ART_CHUNK;
static bool npPlaying = false;
static uint32_t npSeenMs = 0, npPosAtMs = 0, npNeedMs = 0;
static uint16_t npPos = 0, npDur = 0;
static char npId[24] = "", npTitle[44] = "", npArtist[44] = "";
static uint16_t *artBuf = nullptr, *artRx = nullptr;   // gotowa okladka / odbierana
static char artId[24] = "", rxId[24] = "";
static uint8_t rxSeen[(ART_CHUNKS + 7) / 8];
static int rxGot = 0;

static bool npOnline() { return npSeenMs && millis() - npSeenMs < 5000 && npId[0]; }
static bool artReady() { return artBuf && artId[0] && strcmp(artId, npId) == 0; }
static uint32_t npPosNow() {
  uint32_t p = npPos + (npPlaying ? (millis() - npPosAtMs) / 1000 : 0);
  return (npDur && p > npDur) ? npDur : p;
}
static bool spotifyShown() { return tile == 1 && activeProfile == PR_SPOTIFY; }

static void npLine(char *line) {   // line zaczyna sie od "NP "
  char *a = line + 3;
  if (!strncmp(a, "META ", 5)) {
    char *tab1 = strchr(a, '\t'), *tab2 = tab1 ? strchr(tab1 + 1, '\t') : nullptr;
    if (!tab1 || !tab2) return;
    *tab1 = 0; *tab2 = 0;
    int pl = 0, pos = 0, dur = 0; char id[24] = "";
    if (sscanf(a + 5, "%d %d %d %23s", &pl, &pos, &dur, id) < 4) return;
    bool changed = strcmp(id, npId) != 0 || (bool)pl != npPlaying || abs((int)npPosNow() - pos) > 2;
    npPlaying = pl; npPos = pos; npDur = dur; npPosAtMs = millis(); npSeenMs = millis();
    strncpy(npId, id, sizeof(npId) - 1);
    strncpy(npTitle, tab1 + 1, sizeof(npTitle) - 1); npTitle[sizeof(npTitle) - 1] = 0;
    strncpy(npArtist, tab2 + 1, sizeof(npArtist) - 1); npArtist[sizeof(npArtist) - 1] = 0;
    if (strcmp(npId, "-") == 0) npId[0] = 0;
    if (npId[0] && artBuf && !artReady() && millis() - npNeedMs > 3000) {   // brak okladki: popros helpera
      npNeedMs = millis();
      cdc.printf("NP NEED %s\n", npId);
    }
    if (changed && spotifyShown()) dirty = true;
  } else if (!strncmp(a, "OFF", 3)) {
    npSeenMs = 0; npId[0] = 0;
    if (spotifyShown()) dirty = true;
  } else if (!strncmp(a, "ART ", 4)) {
    char id[24] = ""; int w = 0, h = 0, n = 0;
    if (sscanf(a + 4, "%23s %d %d %d", id, &w, &h, &n) < 4 || w != ART_W || h != ART_H || n != ART_CHUNKS || !artRx) { rxId[0] = 0; return; }
    strncpy(rxId, id, sizeof(rxId) - 1);
    memset(rxSeen, 0, sizeof(rxSeen)); rxGot = 0;
  } else if (!strncmp(a, "A ", 2) && rxId[0]) {
    char *sp = strchr(a + 2, ' ');
    if (!sp) return;
    int idx = atoi(a + 2);
    if (idx < 0 || idx >= ART_CHUNKS) return;
    uint8_t tmp[ART_CHUNK]; size_t olen = 0;
    if (mbedtls_base64_decode(tmp, sizeof(tmp), &olen, (const unsigned char *)sp + 1, strlen(sp + 1)) != 0) return;
    size_t off = (size_t)idx * ART_CHUNK, total = ART_W * ART_H * 2;
    if (off + olen > total) olen = total - off;
    memcpy((uint8_t *)artRx + off, tmp, olen);
    if (!(rxSeen[idx >> 3] & (1 << (idx & 7)))) { rxSeen[idx >> 3] |= 1 << (idx & 7); rxGot++; }
  } else if (!strncmp(a, "END ", 4) && rxId[0]) {
    if (rxGot == ART_CHUNKS && strcmp(a + 4, rxId) == 0) {   // komplet: zamiana buforow
      uint16_t *t = artBuf; artBuf = artRx; artRx = t;
      strncpy(artId, rxId, sizeof(artId) - 1);
      if (spotifyShown()) dirty = true;
    } else npNeedMs = 0;   // zgubione kawalki: kolejny META poprosi ponownie
    rxId[0] = 0;
  }
}

static void drawArc360(TFT_eSprite &s, int r, int ir, float frac, uint16_t col) {   // od godziny 12, zgodnie z zegarem
  int a0 = 180, a1 = 180 + (int)(frac * 360.0f);
  if (a1 <= a0) return;
  if (a1 <= 360) s.drawSmoothArc(120, 120, r, ir, a0, a1, col, TFT_BLACK);
  else { s.drawSmoothArc(120, 120, r, ir, a0, 360, col, TFT_BLACK); s.drawSmoothArc(120, 120, r, ir, 0, a1 - 360, col, TFT_BLACK); }
}

static void drawFit(TFT_eSprite &s, const char *txt, int y, int maxW, uint16_t col) {   // tekst przyciety do szerokosci
  char b[48]; strncpy(b, txt, sizeof(b) - 1); b[sizeof(b) - 1] = 0;
  int n = strlen(b);
  if (s.textWidth(b, 2) > maxW) {
    while (n > 1 && (b[n] = 0, s.textWidth(b, 2) + s.textWidth("..", 2) > maxW)) n--;
    strcat(b, "..");
  }
  s.setTextColor(col, TFT_BLACK);
  s.drawString(b, 120, y, 2);
}

// ekran Spotify z okladka (gdy helper wysyla dane)
static void tileSpotify(TFT_eSprite &s, const Profile &P) {
  if (npDur) drawArc360(s, 119, 114, (float)npPosNow() / npDur, cAcc);   // postep utworu na obrzezu
  if (jogActive || jogInertiaOn) drawJog(s);
  const int ax = 120 - ART_W / 2, ay = 30;
  if (artReady()) {   // bufor: RGB565 little endian od helpera; sprite oczekuje odwrotnej kolejnosci bajtow
    bool sw = s.getSwapBytes();
    s.setSwapBytes(true);
    s.pushImage(ax, ay, ART_W, ART_H, artBuf);
    s.setSwapBytes(sw);
  }
  else { s.fillRoundRect(ax, ay, ART_W, ART_H, 10, cBtn); drawIcon(s, 11, 120, ay + ART_H / 2, cAcc); }
  if (!npPlaying) {   // pauza: dwie belki na okladce
    s.fillRoundRect(106, ay + 40, 10, 32, 3, TFT_WHITE);
    s.fillRoundRect(124, ay + 40, 10, 32, 3, TFT_WHITE);
  }
  s.setTextDatum(MC_DATUM);
  drawFit(s, npTitle, 156, 190, TFT_WHITE);
  drawFit(s, npArtist, 174, 170, cGray);
  bool recent = lastAction[0] && (millis() - lastActionMs < 1200);
  s.setTextColor(recent ? TFT_WHITE : cAcc, TFT_BLACK);
  s.drawString(recent ? lastAction : P.sub[subSel[PR_SPOTIFY]].name, 120, 192, 2);
  s.fillRoundRect(96, 202, 48, 20, 10, cBtn);
  s.setTextColor(cAcc, cBtn);
  s.drawString("LIKE", 120, 212, 2);
}

static void spotifyLike() {
  chord2(KEY_LEFT_ALT, KEY_LEFT_SHIFT, 'b');   // Spotify: Alt+Shift+B = polub / usun z polubionych
  rgbAction(false);
  setAction("LIKE");
}

static void tileControl(TFT_eSprite &s) {
  const Profile &P = PROFILES[activeProfile];
  const SubMode &M = P.sub[subSel[activeProfile]];
  s.fillSprite(TFT_BLACK);
  drawRing(s);
  drawBezelMark(s, bezelAng);
  if (tcfg[activeProfile].ring) drawJog(s);
  if (activeProfile == PR_DAVINCI && subSel[PR_DAVINCI] == DV_COLOR) { tileColor(s, P); return; }
  if (activeProfile == PR_SPOTIFY && npOnline()) { tileSpotify(s, P); return; }
  s.setTextDatum(MC_DATUM);
  s.setTextColor(cGray, TFT_BLACK);
  s.drawString(P.name, 120, 34, 2);
  s.fillCircle(120, 80, 27, cDim);
  s.drawCircle(120, 80, 27, cAcc);
  drawIcon(s, scrollIcon(P.icon), 120, 80, cAcc);
  s.setTextColor(cAcc, TFT_BLACK);
  s.drawString(M.name, 120, 130, 4);
  bool recent = lastAction[0] && (millis() - lastActionMs < 1200);
  s.setTextColor(recent ? TFT_WHITE : cGray, TFT_BLACK);
  char hb[32];
  const char *hint = M.hint;
  if (activeProfile == 1) {   // Smart Scroll: tryb przyspieszenia i bezwladnosc zamiast statycznej podpowiedzi
    snprintf(hb, sizeof(hb), "%s | inertia %s", tcfg[1].adaptive ? "SMART" : "PRECISION", TC_INERTIA_NAMES[tcfg[1].inertia]);
    hint = hb;
  }
  s.drawString(recent ? lastAction : hint, 120, 158, 2);
  s.setTextColor(C565(110, 110, 110), TFT_BLACK);
  if (activeProfile == PR_MEETINGS) {   // stan mikrofonu + przycisk kamery
    s.fillRoundRect(92, 168, 56, 24, 12, cBtn);
    s.setTextColor(cAcc, cBtn);
    s.drawString("CAM", 120, 180, 2);
  } else if (activeProfile == PR_SPOTIFY) {   // przycisk LIKE zamiast podpowiedzi tap
    s.fillRoundRect(92, 168, 56, 24, 12, cBtn);
    s.setTextColor(cAcc, cBtn);
    s.drawString("LIKE", 120, 180, 2);
  } else s.drawString(P.tap, 120, 180, 2);
  drawSubDots(s, P);
}

static void tilePomo(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);
  uint32_t rem = pomoRemain();
  uint32_t total = (uint32_t)(pBreak ? breakMin : focusMin) * 60000UL;
  uint16_t col = pBreak ? cBreak : cAcc;
  s.drawSmoothArc(120, 120, 119, 109, 0, 360, cDim, TFT_BLACK);
  if (pState != P_IDLE && total) {
    uint32_t e = 360UL * (total - (rem > total ? total : rem)) / total;
    if (e > 2) s.drawSmoothArc(120, 120, 119, 109, 0, e > 360 ? 360 : e, col, TFT_BLACK);
  }
  s.setTextDatum(MC_DATUM);
  const char *label = pState == P_IDLE ? "READY" : pState == P_PAUSED ? "PAUSED" : pBreak ? "BREAK" : "FOCUS";
  s.setTextColor(pState == P_PAUSED ? cGray : col, TFT_BLACK);
  s.drawString(label, 120, 58, 4);
  char b[8]; fmtTime(b, sizeof(b), rem);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString(b, 120, 112, 6);
  char l[28];
  s.setTextColor(cGray, TFT_BLACK);
  if (pState == P_IDLE && pstats.day > 0 && (millis() / 3000) % 2) { statsRoll(); snprintf(l, sizeof(l), "today %u | week %d", pstats.cnt[0], statsWeek()); }
  else if (pState == P_IDLE) snprintf(l, sizeof(l), "turn: %u min | break %u", focusMin, breakMin);
  else if (pstats.day > 0) { statsRoll(); snprintf(l, sizeof(l), "today %u | week %d", pstats.cnt[0], statsWeek()); }
  else snprintf(l, sizeof(l), "sessions: %u", sessions);
  s.drawString(l, 120, 152, 2);
  if (pState != P_IDLE) {
    s.fillRoundRect(92, 172, 56, 24, 12, cBtn);
    s.setTextColor(TFT_WHITE, cBtn);
    s.drawString("RESET", 120, 184, 2);
  } else {
    s.setTextColor(C565(110, 110, 110), TFT_BLACK);
    s.drawString("click/tap: start", 120, 184, 2);
  }
}

static void drawRows(TFT_eSprite &s, const char *title, int n, const char *const *labels, char (*vals)[16],
                     int sel, int &top, int swatchRow, uint16_t swatchCol, bool editing) {
  if (sel < top) top = sel;
  if (sel > top + 3) top = sel - 3;
  s.fillSprite(TFT_BLACK);
  drawRing(s);
  s.setTextDatum(MC_DATUM);
  s.setTextColor(cGray, TFT_BLACK);
  s.drawString(title, 120, 32, 2);
  for (int v = 0; v < 4 && top + v < n; v++) {
    int i = top + v, y = 68 + v * 40;
    bool on = (i == sel);
    uint16_t bg = on ? cDim : cBtn;
    s.fillRoundRect(35, y - 17, 170, 34, 12, bg);
    if (on) s.drawRoundRect(35, y - 17, 170, 34, 12, cAcc);
    if (on && editing) s.drawRoundRect(36, y - 16, 168, 32, 11, cAcc);
    s.setTextDatum(ML_DATUM);
    s.setTextColor(TFT_WHITE, bg);
    s.drawString(labels[i], 48, y, 2);
    s.setTextDatum(MR_DATUM);
    s.setTextColor(on ? cAcc : cGray, bg);
    s.drawString(vals[i], i == swatchRow ? 176 : 192, y, 2);
    if (i == swatchRow) s.fillCircle(188, y, 6, swatchCol);
  }
  if (top > 0) s.fillTriangle(114, 52, 126, 52, 120, 46, cGray);
  if (top + 4 < n) s.fillTriangle(114, 210, 126, 210, 120, 216, cGray);
}

static void tileSettings(TFT_eSprite &s) {
  const char *labels[NSET] = {"Theme", "Host OS", "Scroll dir", "Brightness", "RGB ring", "Pomo break",
                              "Touch jog", "Jog speed", "Jog inertia", "Jog dir", "Touch ring",
                              "Auto profile", "Clock face", "Stand-up", "Tutorial", "Version"};
  char vals[NSET][16];
  snprintf(vals[0], 16, "%s", THEMES[themeIdx].name);
  snprintf(vals[1], 16, "%s", hostOs ? "macOS" : "Windows");
  snprintf(vals[2], 16, "%s", scrollInv ? "Inverted" : "Normal");
  snprintf(vals[3], 16, "%u%%", briPct);
  snprintf(vals[4], 16, "%s", RGB_MODE_NAMES[rgbMode]);
  snprintf(vals[5], 16, "%u min", breakMin);
  snprintf(vals[6], 16, "%s", tcfg[2].ring ? "ON" : "OFF");
  snprintf(vals[7], 16, "%u deg", JOG_DEG[jogDegIdx]);
  snprintf(vals[8], 16, "%s", TC_INERTIA_NAMES[tcfg[2].inertia]);
  snprintf(vals[9], 16, "%s", tcfg[2].rev ? "Reverse" : "Normal");
  snprintf(vals[10], 16, ">");
  snprintf(vals[11], 16, "%s", autoSwitch ? "ON" : "OFF");
  snprintf(vals[12], 16, "%s", clockFace ? "ON" : "OFF");
  snprintf(vals[13], 16, "%s", standup ? "ON" : "OFF");
  snprintf(vals[14], 16, ">");
  snprintf(vals[15], 16, "%s", FW_VERSION);
  drawRows(s, "Settings", NSET, labels, vals, setSel, settingsTop, 0, cAcc, false);
  drawDots(s, 3, 0);
}

static void tileLed(TFT_eSprite &s) {
  const char *labels[7] = {"Mode", "Color", "Brightness", "Pomodoro", "Action flash", "Idle off", "Direction"};
  char vals[7][16];
  char cn[12];
  rgbColorName(cn, sizeof(cn));
  snprintf(vals[0], 16, "%s", RGB_MODE_NAMES[rgbMode]);
  if (ledEdit) snprintf(vals[1], 16, "< %s >", cn); else snprintf(vals[1], 16, "%s", cn);
  snprintf(vals[2], 16, "%u%%", rgbBri);
  snprintf(vals[3], 16, "%s", rgbPomo ? "ON" : "OFF");
  snprintf(vals[4], 16, "%s", rgbFlashOn ? "ON" : "OFF");
  snprintf(vals[5], 16, "%s", rgbIdleOff ? "ON" : "OFF");
  snprintf(vals[6], 16, "%s", rgbRev ? "Reverse" : "Normal");
  char title[24]; snprintf(title, sizeof(title), "RGB ring - %s", THEMES[themeIdx].name);   // kolor zapisywany dla motywu
  drawRows(s, title, 7, labels, vals, ledSel, ledTop, 1, rgbSwatch565(), ledEdit);
  drawDots(s, 3, 1);
}

static bool tcRev(int p) { return p == 1 ? scrollInv : tcfg[p].rev; }
static int tcRowCount() { return tcProf == 1 ? 8 : 7; }

static void tileTouch(TFT_eSprite &s) {
  const TouchCfg &c = tcfg[tcProf];
  const char *labels[8] = {"Profile", "Touch mode", "Sensitivity", "Inertia", "Direction", tcProf == 1 ? "Acceleration" : "Adaptive", "Touch ring", "Axis"};
  char vals[8][16];
  snprintf(vals[0], 16, "%s", PROFILES[tcProf].name);
  snprintf(vals[1], 16, "%s", TC_MODE_NAMES[c.mode]);
  snprintf(vals[2], 16, "%s", TC_SENS_NAMES[c.sens]);
  snprintf(vals[3], 16, "%s", TC_INERTIA_NAMES[c.inertia]);
  snprintf(vals[4], 16, "%s", tcRev(tcProf) ? "REVERSE" : "NORMAL");
  snprintf(vals[5], 16, "%s", c.adaptive ? "ON" : "OFF");
  snprintf(vals[6], 16, "%s", c.ring ? "ON" : "OFF");
  snprintf(vals[7], 16, "%s", subSel[1] ? "HORIZONTAL" : "VERTICAL");
  drawRows(s, "Touch ring", tcRowCount(), labels, vals, tcSel, tcTop, -1, 0, false);
  drawDots(s, 3, 2);
}

static void tcCycle(int row) {
  TouchCfg &c = tcfg[tcProf];
  switch (row) {
    case 0:
      do tcProf = (tcProf + 1) % NPROF; while (tcProf >= PR_CUSTOM0 && !custom[tcProf - PR_CUSTOM0].used);   // tylko zdefiniowane profile klienta
      if (tcSel >= tcRowCount()) tcSel = tcRowCount() - 1;
      break;
    case 1: c.mode ^= 1; break;
    case 2: c.sens = (c.sens + 1) % 3; break;
    case 3: c.inertia = (c.inertia + 1) % 4; break;
    case 4: if (tcProf == 1) scrollInv = !scrollInv; else c.rev = !c.rev; break;
    case 5: c.adaptive = !c.adaptive; break;
    case 6: c.ring = !c.ring; break;
    case 7: if (tcProf == 1) subSel[1] ^= 1; break;
    default: return;
  }
  savePrefs();
  dirty = true;
}

static void renderTile(TFT_eSprite &s, int t) {
  if (themeMulti()) {   // akcent ekranu = kolor zaznaczonej / aktywnej aplikacji
    const AppDef &ha = APPS[pgList[homePage][homeSel]];
    if (t == 0) { if (ha.kind == APP_PROFILE && ha.profile >= PR_CUSTOM0) accentProfile(ha.profile); else accentIcon(scrollIcon(ha.icon)); }
    else if (t == 1) { if (activeProfile >= PR_CUSTOM0) accentProfile(activeProfile); else accentIcon(scrollIcon(PROFILES[activeProfile].icon)); }
    else if (t == 2) accentIcon(4);
    else accentIcon(5);
  }
  switch (t) {
    case 0: tileHome(s); break;
    case 1: tileControl(s); break;
    case 2: tilePomo(s); break;
    case 3: tileSettings(s); break;
    case 4: tileLed(s); break;
    default: tileTouch(s); break;
  }
}

static void tileOverlay(TFT_eSprite &s);
static bool overlayShown();
static void redraw() {
  if (overlayShown()) tileOverlay(*gCur); else renderTile(*gCur, tile);
  gCur->pushSprite(0, 0);
  st.frames++;
  dirty = false;
}

static void slideTo(int dir) {   // gCur = ekran biezacy, gNxt = docelowy; animacja i zamiana
  const int N = 7;
  for (int k = 1; k <= N; k++) {
    float r = 1.0f - (float)k / N;
    int off = (int)(240 * (1.0f - r * r));
    if (dir > 0) { gCur->pushSprite(-off, 0); gNxt->pushSprite(240 - off, 0); }
    else { gCur->pushSprite(off, 0); gNxt->pushSprite(off - 240, 0); }
  }
  TFT_eSprite *tmp = gCur; gCur = gNxt; gNxt = tmp;
  dirty = false;
  st.frames++;
}

static void goTile(int t) {
  t = constrain(t, 0, 5);
  if (t == tile) return;
  if (t != 1) scrollStop();
  renderTile(*gCur, tile);
  renderTile(*gNxt, t);
  int dir = (t > tile) ? 1 : -1;
  tile = t;
  if (tile != 4) ledEdit = false;
  slideTo(dir);
}

// zmiana pulpitu HOME (tile musi byc 0); sel = zaznaczona ikona na nowym pulpicie
static void goPage(int p, int sel, int dir) {
  p = constrain(p, 0, (int)nPages - 1);
  sel = constrain(sel, 0, (int)pgCount[p] - 1);
  if (p == homePage) { homeSel = sel; dirty = true; return; }
  renderTile(*gCur, 0);
  homePage = p; homeSel = sel;
  renderTile(*gNxt, 0);
  rgbPagePulse();   // subtelny impuls pierscienia w kolorze motywu (po renderze: w motywie Color = kolor nowej ikony)
  slideTo(dir);
}

static bool tourActive();
static void tourInput(int kind);
static bool overlayConsume();

// ===================== touch (CST816) =====================
static bool tpReadReg(uint8_t reg, uint8_t *buf, size_t n) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) return false;
  if (Wire.requestFrom((uint16_t)TP_ADDR, (size_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static bool touchInit() {
  if (PIN_TP_INT >= 0) {            // impuls na INT przed resetem
    pinMode(PIN_TP_INT, OUTPUT);
    digitalWrite(PIN_TP_INT, LOW); delay(5);
    digitalWrite(PIN_TP_INT, HIGH); delay(5);
  }
  if (PIN_TP_RST >= 0) {
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW); delay(10);
    digitalWrite(PIN_TP_RST, HIGH); delay(300);
  }
  if (PIN_TP_INT >= 0) pinMode(PIN_TP_INT, INPUT);
  Wire.begin(PIN_TP_SDA, PIN_TP_SCL, 100000);   // 100 kHz
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0xFE);                 // DisAutoSleep
  Wire.write(0xFF);
  return Wire.endTransmission() == 0;
}

static bool touchRead(bool &down, int &x, int &y) {
  uint8_t g = 0, f = 0, c[4];
  // gest (0x01), liczba palcow (0x02) i wspolrzedne (0x03-0x06): osobne transakcje
  if (!tpReadReg(0x01, &g, 1) || !tpReadReg(0x02, &f, 1)) { st.tpErr++; return false; }
  st.tpG = g; st.tpF = f;
  down = (f & 0x0F) > 0;
  x = 0; y = 0;
  if (!down) return true;
  if (!tpReadReg(0x03, c, 4)) { st.tpErr++; return false; }
  int rx = ((c[0] & 0x0F) << 8) | c[1];
  int ry = ((c[2] & 0x0F) << 8) | c[3];
  if (TOUCH_SWAP_XY) { int t = rx; rx = ry; ry = t; }
  if (TOUCH_FLIP_X) rx = 239 - rx;
  if (TOUCH_FLIP_Y) ry = 239 - ry;
  x = rx; y = ry;
  return true;
}

static void openApp(int appIdx) {
  const AppDef &a = APPS[appIdx];
  if (a.kind == APP_PROFILE) {
    scrollStop(); activeProfile = a.profile; rgbAction(false); goTile(1);
  }
  else if (a.kind == APP_POMO) goTile(2);
  else goTile(3);
}
static void openHomeItem(int i) { openApp(pgList[homePage][i]); }

// nawigacja gestem: dir = +1 palec w lewo, -1 palec w prawo
static void onSwipe(int dir) {
  switch (tile) {
    case 0:
      if (homePage + dir >= 0 && homePage + dir < nPages) goPage(homePage + dir, 0, dir);
      break;
    case 1:
      if (dir < 0) goTile(0);
      break;
    case 2:
      if (dir < 0) goTile(0);
      break;
    case 3:
      goTile(dir > 0 ? 4 : 0);
      break;
    case 4:
      goTile(dir > 0 ? 5 : 3);
      break;
    case 5:
      if (dir < 0) goTile(4);
      break;
  }
}

static void onTap(int x, int y) {
  st.taps++;
  st.lx = x; st.ly = y;
  int cx = x - 120, cy = y - 120;
  switch (tile) {
    case 0:
      for (int i = 0; i < pgCount[homePage]; i++) {
        int px, py; homePos(pgCount[homePage], i, px, py);
        int dx = x - px, dy = y - py;
        if (dx * dx + dy * dy <= 31 * 31) { homeSel = i; openHomeItem(i); return; }
      }
      break;
    case 1:
      if (colorActive()) { hvStepSize = hvStepSize >= 4 ? 1 : hvStepSize * 2; setAction(hvStepSize == 1 ? "FINE" : hvStepSize == 2 ? "NORMAL" : "COARSE"); break; }
      else if (activeProfile == PR_SPOTIFY && npOnline() && x > 88 && x < 152 && y > 196 && y < 230) spotifyLike();
      else if (activeProfile == PR_SPOTIFY && !npOnline() && x > 85 && x < 155 && y > 162 && y < 198) spotifyLike();
      else if (activeProfile == PR_MEETINGS && x > 85 && x < 155 && y > 162 && y < 198) meetingToggle(true);
      else if (cx * cx + cy * cy < 70 * 70) tapAction();
      break;
    case 2:
      if (pState != P_IDLE && x > 85 && x < 155 && y > 166 && y < 202) pomoReset();
      else if (cx * cx + cy * cy < 80 * 80) pomoToggle();
      break;
    case 3:
      for (int i = 0; i < 4; i++) {
        int yy = 68 + i * 40;
        if (y > yy - 18 && y < yy + 18 && settingsTop + i < NSET) { setSel = settingsTop + i; settingsCycle(setSel); break; }
      }
      break;
    case 4:
      for (int i = 0; i < 4; i++) {
        int yy = 68 + i * 40;
        if (y > yy - 18 && y < yy + 18 && ledTop + i < 7) { ledSel = ledTop + i; ledCycle(ledSel); break; }
      }
      break;
    case 5:
      for (int i = 0; i < 4; i++) {
        int yy = 68 + i * 40;
        if (y > yy - 18 && y < yy + 18 && tcTop + i < tcRowCount()) { tcSel = tcTop + i; tcCycle(tcSel); break; }
      }
      break;
  }
  dirty = true;
}

// ===================== touch jog (DaVinci) =====================
static bool jogAllowed() { return tile == 1 && tcfg[activeProfile].ring; }

// Smart Touch Ring: kat palca na obrzezu -> jednostki akcji (jak kliniecia enkodera) -> akcja profilu
static void tcEmit(int p, int n) {   // n > 0: zgodnie z ruchem wskazowek
  if (p == 1) { scrollAdd((float)(n > 40 ? 40 : (n < -40 ? -40 : n))); return; }   // Scroll: kierunek stosuje silnik (scrollInv)
  if (tcfg[p].rev) n = -n;
  int cnt = abs(n);
  if (cnt > JOG_FRAMES_CAP) cnt = JOG_FRAMES_CAP;
  if (p == PR_DAVINCI && subSel[PR_DAVINCI] == DV_COLOR) { for (int i = 0; i < abs(n) && i < 8; i++) hoverAdd(n > 0 ? 1 : -1); return; }   // COLOR: dokladna regulacja
  if (p == 2 || p == PR_PREMIERE || p == PR_FINALCUT) {   // montaz: jog = klatki (strzalki), niezaleznie od trybu - jak w DFTv1
    for (int i = 0; i < cnt; i++) keyTap(n > 0 ? KEY_RIGHT_ARROW : KEY_LEFT_ARROW);
  } else {
    doTurn(n > 0 ? cnt : -cnt);
  }
}

static float tcDegPerUnit(int p) {
  const TouchCfg &c = tcfg[p];
  float base = (c.mode == TM_STEP) ? ((p == 2) ? (float)JOG_DEG[jogDegIdx] : tcStepDeg(p)) : tcSmoothDeg(p);
  return base / TC_SENS_MULT[c.sens];
}

static void tcFeed(float dDeg) {
  int p = activeProfile;
  const TouchCfg &c = tcfg[p];
  float v = fabsf(jogVel), f = 1.0f;
  if (c.mode == TM_SMOOTH) f *= 1.0f + 1.5f * clamp01((v - 90.0f) / 600.0f);   // SMOOTH: szybszy ruch = wieksze tempo
  if (c.adaptive) f *= 1.0f + 3.0f * clamp01((v - 60.0f) / 540.0f);            // Adaptive Touch: dodatkowe dopasowanie do predkosci
  float per = tcDegPerUnit(p) / f;
  jogAcc += dDeg;
  int n = (int)(jogAcc / per);
  if (!n) return;
  jogAcc -= n * per;
  tcEmit(p, n);
}

static float angleOf(int x, int y) { return atan2f((float)(y - 120), (float)(x - 120)) * 57.29578f; }

static void rgbFollowAngle(float ang) {   // pierscien RGB podaza za palcem
  float u = ang / 360.0f * RGB_COUNT;
  if (rgbRev) u = -u;
  while (u < 0.0f) u += RGB_COUNT;
  while (u >= (float)RGB_COUNT) u -= RGB_COUNT;
  rgbPos = u;
  rgbTurnMs = millis();
}

static void jogCancel() { jogActive = false; jogInertiaOn = false; jogAcc = 0; jogVel = 0; }

static void jogBegin(int x, int y) {
  jogActive = true; jogInertiaOn = false;
  jogLastAng = angleOf(x, y); jogShowAng = jogLastAng;
  jogAcc = 0; jogVel = 0; jogLastMs = millis();
  dirty = true;
}

static void jogMove(int x, int y) {
  if (!jogAllowed()) { jogCancel(); return; }
  float a = angleOf(x, y);
  float d = a - jogLastAng;
  while (d > 180.0f) d -= 360.0f;
  while (d <= -180.0f) d += 360.0f;
  jogLastAng = a; jogShowAng = a;
  uint32_t now = millis();
  float dt = (now - jogLastMs) / 1000.0f;
  jogLastMs = now;
  if (dt <= 0.0f || fabsf(d) > 70.0f) return;   // odrzuc skok z zaklocen dotyku
  if (d != 0.0f) jogDir = d > 0 ? 1 : -1;
  jogVel = 0.6f * jogVel + 0.4f * (d / dt);
  tcFeed(d);
  rgbFollowAngle(a);
  dirty = true;
}

static void jogEnd() {
  jogActive = false;
  if (jogAllowed() && tcfg[activeProfile].inertia && fabsf(jogVel) > 120.0f) { jogInertiaOn = true; jogInertiaMs = millis(); }
  dirty = true;
}

static void jogTick() {   // bezwladnosc po machnieciu (poziom wg ustawien profilu)
  if (!jogInertiaOn) return;
  if (!jogAllowed()) { jogCancel(); return; }
  uint32_t now = millis();
  if (now - jogInertiaMs < 15) return;
  float dt = (now - jogInertiaMs) / 1000.0f;
  jogInertiaMs = now;
  int lvl = tcfg[activeProfile].inertia;
  float dd = jogVel * dt;
  jogShowAng += dd;
  jogVel *= TC_INERTIA_K[lvl ? lvl : 2];
  tcFeed(dd);
  if (fabsf(jogVel) < 30.0f) jogInertiaOn = false;
  dirty = true;
}

static bool ignoreTouch = false;
static void handleTouch() {
  static uint32_t last = 0;
  static bool was = false;
  static int sx, sy, lx, ly;
  static uint32_t t0;
  if (millis() - last < 12) return;
  last = millis();
  bool down; int x, y;
  if (!touchRead(down, x, y)) { if (jogActive) jogEnd(); was = false; return; }
  if (down) {
    if (!was) {
      sx = x; sy = y; t0 = millis();
      ignoreTouch = !tourActive() && overlayConsume();   // dotkniecie tylko budzi
      markActive();
      if (tile == 1 && activeProfile == 1) scrollStop();
      jogInertiaOn = false;
      int rx = x - 120, ry = y - 120;
      if (!ignoreTouch && !tourActive() && jogAllowed() && rx * rx + ry * ry >= JOG_R_MIN * JOG_R_MIN) jogBegin(x, y);
    } else if (jogActive) {
      jogMove(x, y);
    }
    lx = x; ly = y;
  } else if (was && (ignoreTouch || tourActive())) {
    if (tourActive() && !ignoreTouch) {
      int dx = lx - sx, dy = ly - sy;
      if (abs(dx) > SWIPE_MIN && abs(dx) > abs(dy)) tourInput(3);
      else if (abs(dx) < TAP_MAX && abs(dy) < TAP_MAX) tourInput(4);
    }
    if (jogActive) jogCancel();
    ignoreTouch = false;
  } else if (was) {
    if (jogActive) {
      jogEnd();
    } else {
      int dx = lx - sx, dy = ly - sy;
      uint32_t dt = millis() - t0;
      if (abs(dx) > SWIPE_MIN && abs(dx) > abs(dy)) {
        st.swipes++;
        onSwipe(dx < 0 ? 1 : -1);
      } else if (abs(dx) < TAP_MAX && abs(dy) < TAP_MAX && dt < 700) {
        onTap(lx, ly);
      }
    }
  }
  was = down;
}

// ===================== encoder + button =====================
// obrot na HOME: kolejne ikony przez wszystkie pulpity (po ostatniej ikonie wraca na pierwszy pulpit)
static void homeStep(int d) {
  int total = 0, flat = 0;
  for (int p = 0; p < nPages; p++) total += pgCount[p];
  for (int p = 0; p < homePage; p++) flat += pgCount[p];
  flat = (((flat + homeSel + d) % total) + total) % total;
  int p = 0;
  while (flat >= pgCount[p]) { flat -= pgCount[p]; p++; }
  if (p != homePage) goPage(p, flat, d > 0 ? 1 : -1);
  else homeSel = flat;
}

static void handleEncoder() {
  int32_t d = takeEncoder();
  if (!d) return;
  if (tourActive()) { markActive(); tourInput(0); return; }
  if (overlayConsume()) { markActive(); return; }
  markActive();
  rgbOnTurn((int)d);
  switch (tile) {
    case 0: homeStep((int)d); break;
    case 1: bezelAng += d * 14; doTurn((int)d); break;
    case 2:
      if (pState == P_IDLE) {
        focusMin = (uint8_t)constrain((int)focusMin + (int)d * 5, 5, 60);
        prefsPending = true; prefsAtMs = millis();
      }
      break;
    case 3: setSel = constrain(setSel + (int)d, 0, NSET - 1); break;
    case 4:
      if (ledEdit && ledSel == 1) {
        rgbColor() = (uint8_t)((((int)rgbColor() + (int)d) % RGB_COLOR_STATES + RGB_COLOR_STATES) % RGB_COLOR_STATES);
        prefsPending = true; prefsAtMs = millis();
      } else ledSel = constrain(ledSel + (int)d, 0, 6);
      break;
    case 5: tcSel = constrain(tcSel + (int)d, 0, tcRowCount() - 1); break;
  }
  dirty = true;
}

static void onClick() {
  st.clicks++;
  switch (tile) {
    case 0: openHomeItem(homeSel); break;   // klik otwiera zaznaczona aplikacje
    case 1:
      if (activeProfile >= PR_CUSTOM0 && !seqEmpty(custom[customIdx()].m[subSel[activeProfile] & 3].click)) {   // klik z makrem klienta
        runSeq(custom[customIdx()].m[subSel[activeProfile] & 3].click); setAction("ACTION");
      }
      else if (PROFILES[activeProfile].nsub > 1) {
        subSel[activeProfile] = (subSel[activeProfile] + 1) % PROFILES[activeProfile].nsub; rgbAction(false);
        if (activeProfile == PR_SCROLL) { scrollStop(); prefsPending = true; prefsAtMs = millis(); }
      }
      else tapAction();
      break;
    case 2: pomoToggle(); break;
    case 3: settingsCycle(setSel); break;
    case 4: ledCycle(ledSel); break;
    case 5: tcCycle(tcSel); break;
  }
  dirty = true;
}

static void onHold() {
  st.holds++;
  // zawsze pierwszy pulpit HOME; nie rusza Pomodoro ani aktywnego profilu
  if (tile != 0) { homePage = 0; homeSel = 0; goTile(0); }
  else if (homePage != 0) goPage(0, 0, -1);
}

static void handleButton() {
  static bool down = false, holdFired = false;
  static uint32_t tDown = 0, tChange = 0;
  bool raw = digitalRead(PIN_BTN) == LOW;
  uint32_t now = millis();
  if (raw != down && now - tChange >= BTN_DEBOUNCE_MS) {
    tChange = now;
    down = raw;
    if (down) { tDown = now; holdFired = false; if (!tourActive() && overlayConsume()) holdFired = true; markActive(); }   // klik tylko budzi
    else if (!holdFired) { if (tourActive()) tourInput(1); else onClick(); }
  }
  if (down && !holdFired && now - tDown >= BTN_HOLD_MS) { holdFired = true; if (tourActive()) tourInput(2); else onHold(); }
}

// ===================== serial =====================
static void printStatus() {
  cdc.printf("DIAL FT %s | host=%s | page=%d/%d | stage=%s | err=%s | usb=%s | tile=%d profile=%s sub=%u | theme=%s | frames=%lu clicks=%lu holds=%lu taps=%lu swipes=%lu hid=%lu | touch err=%lu f=%u g=0x%02X last=(%d,%d) | pomo=%d | rgb=%s | np=%s art=%s\n",
                FW_VERSION, hostOs ? "macOS" : "Windows", homePage + 1, (int)nPages, initStage, lastError.c_str(), usbReady() ? "ready" : "wait", tile,
                PROFILES[activeProfile].name, subSel[activeProfile], THEMES[themeIdx].name,
                (unsigned long)st.frames, (unsigned long)st.clicks, (unsigned long)st.holds,
                (unsigned long)st.taps, (unsigned long)st.swipes, (unsigned long)st.hid,
                (unsigned long)st.tpErr, st.tpF, st.tpG, st.lx, st.ly, (int)pState, RGB_MODE_NAMES[rgbMode], npOnline() ? npTitle : "-", artReady() ? "ok" : (artBuf ? "none" : "no-mem"));
}
// ===================== czas (od helpera albo konfiguratora) =====================
static bool timeValid = false;
static int64_t epochAtBoot = 0;   // czas lokalny (s) w chwili millis() == 0
static void timeSet(int64_t epochUtc, int tzMin) { epochAtBoot = epochUtc + (int64_t)tzMin * 60 - (int64_t)(millis() / 1000); timeValid = true; }
static bool localNow(struct tm &t) {
  if (!timeValid) return false;
  time_t now = (time_t)(epochAtBoot + millis() / 1000);
  gmtime_r(&now, &t);
  return true;
}

static int32_t today() { return timeValid ? (int32_t)((epochAtBoot + millis() / 1000) / 86400) : 0; }

// ===================== nakladki: tarcza zegarka, przypomnienie o przerwie, samouczek =====================
static bool clockShown = false, standShown = false;
static uint32_t activeSinceMs = 0, lastActMs = 0;   // poczatek ciaglej pracy (bez 5 min przerwy)
static int tourStep = -1;                           // -1 = samouczek nieaktywny
static const char *TOUR_TITLE[5] = {"TURN", "CLICK", "HOLD", "SWIPE", "TAP"};
static const char *TOUR_TEXT[5][2] = {{"turn the dial", "to select apps"}, {"click the dial", "to open / change mode"},
                                      {"hold the dial", "to go back HOME"}, {"swipe the screen", "left or right"},
                                      {"tap the screen", "to finish"}};
static void tourStart() { tourStep = 0; dirty = true; }
static bool tourActive() { return tourStep >= 0; }
static void tourInput(int kind) {   // kind: 0 obrot, 1 klik, 2 przytrzymanie, 3 swipe, 4 tap
  if (kind != tourStep) return;
  rgbAction(false);
  if (++tourStep >= 5) { tourStep = -1; tourDone = true; prefs.putBool("tour", true); }
  dirty = true;
}
static bool overlayShown() { return tourActive() || standShown || clockShown; }
// pierwsze dotkniecie / obrot / klik tylko zamyka zegar lub przypomnienie (bez akcji w profilu)
static bool overlayConsume() {
  if (standShown) { standShown = false; activeSinceMs = millis(); dirty = true; return true; }
  if (clockShown) { clockShown = false; dirty = true; return true; }
  return false;
}

static void tileOverlay(TFT_eSprite &s) {
  s.fillSprite(TFT_BLACK);
  s.setTextDatum(MC_DATUM);
  if (tourActive()) {
    drawArc360(s, 119, 114, (tourStep + 1) / 5.0f, cAcc);
    s.setTextColor(cGray, TFT_BLACK);
    s.drawString("WELCOME TO DIAL FT", 120, 48, 2);
    s.setTextColor(cAcc, TFT_BLACK);
    s.drawString(TOUR_TITLE[tourStep], 120, 100, 4);
    s.setTextColor(TFT_WHITE, TFT_BLACK);
    s.drawString(TOUR_TEXT[tourStep][0], 120, 138, 2);
    s.drawString(TOUR_TEXT[tourStep][1], 120, 156, 2);
    drawDots(s, 5, tourStep);
    return;
  }
  if (standShown) {
    drawRing(s);
    s.setTextColor(cAcc, TFT_BLACK);
    s.drawString("TIME TO", 120, 90, 4);
    s.drawString("STAND UP", 120, 122, 4);
    char b[28]; snprintf(b, sizeof(b), "%u min without a break", (unsigned)((millis() - activeSinceMs) / 60000));
    s.setTextColor(cGray, TFT_BLACK);
    s.drawString(b, 120, 160, 2);
    s.setTextColor(C565(110, 110, 110), TFT_BLACK);
    s.drawString("any input: OK", 120, 184, 2);
    return;
  }
  struct tm t;
  if (!localNow(t)) return;
  for (int i = 0; i < 60; i++) {   // sekundnik na obrzezu
    float a = (i * 6 - 90) * 0.0174533f;
    int r1 = (i % 5 == 0) ? 108 : 112;
    uint16_t c = (i <= t.tm_sec) ? cAcc : C565(45, 45, 45);
    s.drawLine(120 + (int)(r1 * cosf(a)), 120 + (int)(r1 * sinf(a)), 120 + (int)(117 * cosf(a)), 120 + (int)(117 * sinf(a)), c);
  }
  static const char *WD[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *MO[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char b[24];
  snprintf(b, sizeof(b), "%s %d %s", WD[t.tm_wday], t.tm_mday, MO[t.tm_mon]);
  s.setTextColor(cGray, TFT_BLACK);
  s.drawString(b, 120, 66, 2);
  snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  s.setTextColor(TFT_WHITE, TFT_BLACK);
  s.drawString(b, 120, 112, 6);
  if (pomoRunning() || pState == P_PAUSED) {
    char p[8]; fmtTime(p, sizeof(p), pomoRemain());
    snprintf(b, sizeof(b), "%s %s", pBreak ? "BREAK" : "FOCUS", p);
    s.setTextColor(pBreak ? cBreak : cAcc, TFT_BLACK);
    s.drawString(b, 120, 156, 2);
  } else if (pstats.day > 0) {
    statsRoll();
    snprintf(b, sizeof(b), "pomodoro today: %u", pstats.cnt[0]);
    s.setTextColor(C565(110, 110, 110), TFT_BLACK);
    s.drawString(b, 120, 156, 2);
  }
  if (npOnline() && npTitle[0]) drawFit(s, npTitle, 180, 150, cAcc);
}

// ===================== automatyczny profil wg aktywnej aplikacji =====================
// Helper wysyla "APP <identyfikator>" (macOS: bundle id, Windows: nazwa .exe). Dopasowanie po poczatku, bez wielkosci liter.
struct AppMap { const char *prefix; uint8_t profile; int8_t sub; };
static const AppMap APP_MAP[] = {
  {"com.blackmagic-design.DaVinciResolve", PR_DAVINCI, -1}, {"Resolve.exe", PR_DAVINCI, -1},
  {"com.adobe.PremierePro", PR_PREMIERE, -1}, {"Adobe Premiere Pro.exe", PR_PREMIERE, -1},
  {"com.apple.FinalCut", PR_FINALCUT, -1},
  {"com.adobe.Photoshop", PR_PHOTOSHOP, -1}, {"Photoshop.exe", PR_PHOTOSHOP, -1},
  {"com.adobe.LightroomClassic", PR_LIGHTROOM, -1}, {"Lightroom.exe", PR_LIGHTROOM, -1},
  {"com.pixelmatorteam.pixelmator", PR_PIXELMATOR, -1},
  {"com.spotify.client", PR_SPOTIFY, -1}, {"Spotify.exe", PR_SPOTIFY, -1},
  {"us.zoom.xos", PR_MEETINGS, 0}, {"Zoom.exe", PR_MEETINGS, 0},
  {"com.microsoft.teams", PR_MEETINGS, 1}, {"ms-teams.exe", PR_MEETINGS, 1}, {"Teams.exe", PR_MEETINGS, 1},
  {"com.figma.Desktop", PR_FIGMA, -1}, {"Figma.exe", PR_FIGMA, -1},
  {"org.blenderfoundation.blender", PR_BLENDER, -1}, {"blender.exe", PR_BLENDER, -1},
  {"com.autodesk.fusion360", PR_FUSION, -1}, {"Fusion360.exe", PR_FUSION, -1},
  {"org.kicad", PR_KICAD, -1}, {"kicad.exe", PR_KICAD, -1}, {"pcbnew.exe", PR_KICAD, -1}, {"eeschema.exe", PR_KICAD, -1},
};
static char lastApp[48] = "";
static void appChanged(const char *id) {
  strncpy(lastApp, id, sizeof(lastApp) - 1); lastApp[sizeof(lastApp) - 1] = 0;
  if (!autoSwitch || !(tile == 0 || tile == 1) || tourActive() || standShown) return;   // nie przeszkadzaj w Ustawieniach / Pomodoro / samouczku
  int prof = -1, sub = -1;
  for (int i = 0; i < NCUSTOM && prof < 0; i++)   // profile klienta maja pierwszenstwo
    if (custom[i].used && custom[i].app[0] && !strncasecmp(id, custom[i].app, strlen(custom[i].app))) prof = PR_CUSTOM0 + i;
  for (size_t i = 0; i < sizeof(APP_MAP) / sizeof(APP_MAP[0]) && prof < 0; i++)
    if (!strncasecmp(id, APP_MAP[i].prefix, strlen(APP_MAP[i].prefix))) { prof = APP_MAP[i].profile; sub = APP_MAP[i].sub; }
  if (prof < 0) return;
  for (int i = 0; i < NAPPS_BUILTIN; i++)   // ukryta ikona = klient tego profilu nie chce
    if (APPS[i].kind == APP_PROFILE && APPS[i].profile == prof && (hiddenMask & (1UL << i))) return;
  if (prof == activeProfile && tile == 1 && (sub < 0 || subSel[prof] == sub)) return;
  scrollStop();
  activeProfile = prof;
  if (sub >= 0) subSel[prof] = sub;
  if (tile != 1) goTile(1);
  setAction("AUTO");
  dirty = true;
}

// ===================== konfiguracja przez USB (konfigurator w przegladarce) =====================
// "CFG GET" -> "CFG {json}" ; "CFG SET {json}" -> "CFG OK" / "CFG ERR <powod>". Pola w SET sa opcjonalne.
// makro w JSON: [[t,m,c], [t,m,c], ...]; dla zgodnosci z konfiguratorem 1.2 przyjmuje tez pojedyncza akcje [t,m,c]
static void seqToJson(JsonArray a, const Seq &q) {
  for (int i = 0; i < NSTEP && q.s[i].t; i++) { JsonArray x = a.add<JsonArray>(); x.add(q.s[i].t); x.add(q.s[i].m); x.add(q.s[i].c); }
}
static void seqFromJson(JsonVariantConst v, Seq &q) {
  memset(&q, 0, sizeof(q));
  JsonArrayConst a = v.as<JsonArrayConst>();
  if (a.isNull() || !a.size()) return;
  if (a[0].is<int>()) { if (a.size() >= 3) { q.s[0].t = a[0] | 0; q.s[0].m = a[1] | 0; q.s[0].c = a[2] | 0; } return; }
  int n = 0;
  for (JsonArrayConst x : a) {
    if (n >= NSTEP) break;
    if (x.size() < 3 || !(x[0] | 0)) continue;
    q.s[n].t = x[0] | 0; q.s[n].m = x[1] | 0; q.s[n].c = x[2] | 0; n++;
  }
}
static void cfgGet() {
  JsonDocument d;
  d["fw"] = FW_VERSION;
  JsonObject st_ = d["settings"].to<JsonObject>();
  st_["theme"] = themeIdx; st_["host"] = hostOs; st_["bri"] = briPct; st_["scrollInv"] = scrollInv;
  st_["rgbMode"] = rgbMode; st_["rgbBri"] = rgbBri; st_["break"] = breakMin; st_["focus"] = focusMin;
  st_["autoSwitch"] = autoSwitch; st_["clock"] = clockFace; st_["standup"] = standup;
  JsonArray th = d["themes"].to<JsonArray>();
  for (int i = 0; i < NTHEMES; i++) th.add(THEMES[i].name);
  JsonArray rm = d["rgbModes"].to<JsonArray>();
  for (int i = 0; i < RGB_MODES; i++) rm.add(RGB_MODE_NAMES[i]);
  JsonArray pg = d["pages"].to<JsonArray>();
  for (int i = 0; i < NPAGES; i++) pg.add(PAGE_NAMES[i]);
  JsonArray ap = d["apps"].to<JsonArray>();
  for (int i = 0; i < NAPPS_BUILTIN; i++) {
    if (!APPS[i].ready) continue;
    JsonObject o = ap.add<JsonObject>();
    o["i"] = i; o["name"] = APPS[i].name; o["page"] = APPS[i].page; o["icon"] = APPS[i].icon;
    o["hidden"] = (hiddenMask >> i) & 1; o["fixed"] = APPS[i].kind == APP_SETTINGS;
  }
  d["cfgv"] = 2;   // 2 = makra (do NSTEP krokow), akcje l / r / tap / click w kazdym trybie, teksty
  d["maxSteps"] = NSTEP;
  JsonArray cu = d["custom"].to<JsonArray>();
  for (int i = 0; i < NCUSTOM; i++) {
    const CustomProf &c = custom[i];
    JsonObject o = cu.add<JsonObject>();
    o["used"] = c.used; o["name"] = c.name; o["icon"] = c.icon; o["app"] = c.app;
    char col[8]; snprintf(col, sizeof(col), "#%02x%02x%02x", c.cr, c.cg, c.cb); o["color"] = col;
    JsonArray tx = o["texts"].to<JsonArray>(); tx.add(c.text[0]); tx.add(c.text[1]);
    JsonArray ms = o["modes"].to<JsonArray>();
    for (int m = 0; m < c.nsub && m < 4; m++) {
      JsonObject mo = ms.add<JsonObject>();
      mo["name"] = c.m[m].name;
      seqToJson(mo["l"].to<JsonArray>(), c.m[m].l);
      seqToJson(mo["r"].to<JsonArray>(), c.m[m].r);
      seqToJson(mo["tap"].to<JsonArray>(), c.m[m].tap);
      seqToJson(mo["click"].to<JsonArray>(), c.m[m].click);
    }
  }
  cdc.print("CFG ");
  serializeJson(d, cdc);
  cdc.print("\n");
}
static void profileFromJson(JsonObjectConst o, CustomProf &c) {
  memset(&c, 0, sizeof(c));
  c.used = (o["used"] | 0) ? 1 : 0;
  strlcpy(c.name, o["name"] | "Custom", sizeof(c.name));
  strlcpy(c.app, o["app"] | "", sizeof(c.app));
  c.icon = o["icon"] | 19;
  unsigned rgb = 0xE6E6E6; const char *col = o["color"] | "#e6e6e6";
  if (col[0] == '#') rgb = strtoul(col + 1, nullptr, 16);
  c.cr = rgb >> 16; c.cg = rgb >> 8; c.cb = rgb;
  c.ver = 2;
  JsonArrayConst tx = o["texts"];
  if (!tx.isNull()) { strlcpy(c.text[0], tx[0] | "", sizeof(c.text[0])); strlcpy(c.text[1], tx[1] | "", sizeof(c.text[1])); }
  Seq oldTap; seqFromJson(o["tap"], oldTap);   // konfigurator 1.2: tap wspolny dla profilu
  JsonArrayConst ms = o["modes"];
  c.nsub = 0;
  for (JsonObjectConst mo : ms) {
    if (c.nsub >= 4) break;
    CustomMode &m = c.m[c.nsub++];
    strlcpy(m.name, mo["name"] | "MODE", sizeof(m.name));
    seqFromJson(mo["l"], m.l); seqFromJson(mo["r"], m.r);
    if (mo["tap"].is<JsonArrayConst>()) seqFromJson(mo["tap"], m.tap); else m.tap = oldTap;
    seqFromJson(mo["click"], m.click);
  }
  if (!c.nsub) { c.nsub = 1; strlcpy(c.m[0].name, "MODE", sizeof(c.m[0].name)); }
}

static void cfgSet(const char *json) {
  JsonDocument d;
  DeserializationError e = deserializeJson(d, json);
  if (e) { cdc.printf("CFG ERR %s\n", e.c_str()); return; }
  JsonObjectConst st_ = d["settings"];
  if (!st_.isNull()) {
    if (st_["theme"].is<int>()) { themeIdx = (uint8_t)(st_["theme"].as<int>() % NTHEMES); applyTheme(); }
    if (st_["host"].is<int>()) hostOs = st_["host"].as<int>() & 1;
    if (st_["bri"].is<int>()) { briPct = constrain(st_["bri"].as<int>(), 10, 100); setBacklight(briPct); }
    if (st_["scrollInv"].is<bool>()) scrollInv = st_["scrollInv"];
    if (st_["rgbMode"].is<int>()) { rgbMode = st_["rgbMode"].as<int>() % RGB_MODES; if (rgbMode != RGB_OFF) rgbLastMode = rgbMode; }
    if (st_["rgbBri"].is<int>()) rgbBri = constrain(st_["rgbBri"].as<int>(), 10, 100);
    if (st_["break"].is<int>()) breakMin = constrain(st_["break"].as<int>(), 1, 60);
    if (st_["focus"].is<int>()) focusMin = constrain(st_["focus"].as<int>(), 5, 90);
    if (st_["autoSwitch"].is<bool>()) autoSwitch = st_["autoSwitch"];
    if (st_["clock"].is<bool>()) clockFace = st_["clock"];
    if (st_["standup"].is<bool>()) standup = st_["standup"];
  }
  JsonArrayConst hid = d["hidden"];
  if (!hid.isNull()) {
    hiddenMask = 0;
    for (JsonVariantConst v : hid) { int i = v | -1; if (i >= 0 && i < NAPPS_BUILTIN && APPS[i].kind != APP_SETTINGS) hiddenMask |= 1UL << i; }
  }
  JsonArrayConst cu = d["custom"];
  if (!cu.isNull()) {
    int i = 0;
    for (JsonObjectConst o : cu) {
      if (i >= NCUSTOM) break;
      profileFromJson(o, custom[i]);
      saveCustom(i);
      i++;
    }
  }
  if (d["slot"].is<int>() && d["profile"].is<JsonObjectConst>()) {   // jeden profil na wiersz (duze makra)
    int i = d["slot"].as<int>();
    if (i >= 0 && i < NCUSTOM) { profileFromJson(d["profile"], custom[i]); saveCustom(i); }
  }
  savePrefs();
  customApps();
  buildPages();
  if (activeProfile >= PR_CUSTOM0 && !custom[activeProfile - PR_CUSTOM0].used) { activeProfile = PR_VOLUME; if (tile == 1) goTile(0); }
  dirty = true;
  cdc.print("CFG OK\n");
}

static void handleSerial() {
  static char buf[6144];
  static int len = 0;
  static bool overflow = false;
  while (cdc.available()) {
    char c = cdc.read();
    if (c == '\n' || c == '\r') {
      buf[len] = 0;
      if (len && !overflow) {
        if (!strncmp(buf, "NP ", 3)) npLine(buf);   // najczestsze i najdluzsze wiersze: bez String
        else {
          String line(buf);
          line.trim();
          if (line.equalsIgnoreCase("STATUS")) printStatus();
          else if (line.equalsIgnoreCase("CFG GET")) cfgGet();
          else if (line.startsWith("CFG SET ")) cfgSet(buf + 8);
          else if (line.startsWith("APP ")) { String id = line.substring(4); id.trim(); appChanged(id.c_str()); }
          else if (line.startsWith("TIME ")) {   // "TIME <epoch UTC> <przesuniecie strefy w minutach>"
            long long ep = 0; int tz = 0;
            if (sscanf(buf + 5, "%lld %d", &ep, &tz) >= 1 && ep > 1600000000LL) timeSet(ep, tz);
          }
          else if (line.equalsIgnoreCase("HELLO")) cdc.printf("DIAL FT %s\n", FW_VERSION);   // identyfikacja portu przez helpera
          else cdc.println("Commands: STATUS, HELLO, CFG GET, CFG SET {json}, APP <id>, TIME <epoch> <tzmin>");
        }
      }
      len = 0; overflow = false;
    } else if (len < (int)sizeof(buf) - 1) buf[len++] = c;
    else overflow = true;   // za dlugi wiersz: odrzuc w calosci
  }
}

// ===================== setup / loop =====================
static void showLogo() {
  tft.fillScreen(TFT_BLACK);
#ifdef HAVE_LOGO
  tft.setSwapBytes(true);
  tft.pushImage((240 - LOGO_W) / 2, (240 - LOGO_H) / 2, LOGO_W, LOGO_H, LOGO_DATA);
#else
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FalbaTech", 120, 120, 4);
#endif
  delay(LOGO_MS);
}

void setup() {
  initStage = "gpio";
  pinMode(PIN_PWR_1, OUTPUT); digitalWrite(PIN_PWR_1, HIGH);
  pinMode(PIN_PWR_2, OUTPUT); digitalWrite(PIN_PWR_2, HIGH);
  ledcAttach(PIN_BL, 5000, 8);
  ledcWrite(PIN_BL, 255);   // podswietlenie od razu: jesli swieci, firmware wystartowal
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  encPrev = (gpio_get_level((gpio_num_t)PIN_ENC_A) << 1) | gpio_get_level((gpio_num_t)PIN_ENC_B);
  attachInterrupt(PIN_ENC_A, encISR, CHANGE);
  attachInterrupt(PIN_ENC_B, encISR, CHANGE);

  initStage = "prefs";
  prefs.begin("dialft", false);
  themeIdx = prefs.getUChar("theme", THEME_DEFAULT) % NTHEMES;
  briPct = prefs.getUChar("bri", 80);
  focusMin = prefs.getUChar("fmin", POMO_FOCUS_DEFAULT);
  scrollInv = prefs.getBool("inv", false);
  hostOs = prefs.getUChar("host", 0) & 1;
  breakMin = prefs.getUChar("bmin", POMO_BREAK_DEFAULT);
  if (breakMin < 1 || breakMin > 60) breakMin = POMO_BREAK_DEFAULT;
  rgbMode = prefs.getUChar("rgbm", RGB_FOLLOW) % RGB_MODES;
  rgbLastMode = prefs.getUChar("rgbl", RGB_FOLLOW) % RGB_MODES;
  {
    uint8_t legacy = prefs.getUChar("rgbc", 0) % RGB_COLOR_STATES;   // DFTv1: jeden kolor dla wszystkich motywow
    uint8_t legacyTheme = prefs.getUChar("theme", THEME_DEFAULT) % NTHEMES;
    for (int t = 0; t < NTHEMES; t++) {
      char k[8]; snprintf(k, sizeof(k), "rgbc%d", t);
      rgbColorT[t] = prefs.getUChar(k, t == legacyTheme ? legacy : 0) % RGB_COLOR_STATES;
    }
  }
  rgbBri = prefs.getUChar("rgbb", 60);
  rgbPomo = prefs.getBool("rgbp", true);
  rgbFlashOn = prefs.getBool("rgbf", true);
  rgbIdleOff = prefs.getBool("rgbi", true);
  rgbRev = prefs.getBool("rgbr", false);
  jogDegIdx = prefs.getUChar("jogd", 2) % 5;
  subSel[1] = prefs.getUChar("sax", 0) & 1;
  tcDefaults();
  if (prefs.isKey("tcv")) {
    for (int p = 0; p < NPROF; p++) {
      char k[8]; snprintf(k, sizeof(k), "tc%d", p);
      if (prefs.isKey(k)) tcUnpack(tcfg[p], prefs.getUChar(k, tcPack(tcfg[p])));
    }
  } else {   // migracja z DFTv1: dotychczasowe ustawienia jogu DaVinci
    tcfg[2].ring = prefs.getBool("jogo", true);
    tcfg[2].inertia = prefs.getBool("jogi", false) ? 2 : 0;
    tcfg[2].rev = prefs.getBool("jogr", false);
  }
  applyTheme();
  if (prefs.getBytesLength("pst") == sizeof(pstats)) prefs.getBytes("pst", &pstats, sizeof(pstats));
  hiddenMask = prefs.getULong("hide", 0);
  autoSwitch = prefs.getBool("auto", true);
  clockFace = prefs.getBool("clk", true);
  standup = prefs.getBool("stand", true);
  tourDone = prefs.getBool("tour", false);
  loadCustom();
  customApps();
  buildPages();
  ring.begin();
  ring.clear();
  ring.show();

  initStage = "usb";
  USB.manufacturerName("FalbaTech");
  USB.productName("DIAL FT");
  kb.begin();
  mouse.begin();
  cc.begin();
  cdc.setRxBufferSize(8192);   // okladka przychodzi seria wierszy
  cdc.begin(115200);
  artBuf = (uint16_t *)heap_caps_malloc(ART_W * ART_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  artRx = (uint16_t *)heap_caps_malloc(ART_W * ART_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!artBuf || !artRx) lastError = "art alloc failed";
  USB.begin();

  initStage = "lcd";
  tft.init();
  tft.setRotation(LCD_ROTATION);
#ifdef LCD_MADCTL_OVERRIDE
  tft.writecommand(0x36);
  tft.writedata(LCD_MADCTL_OVERRIDE);
#endif
  tft.fillScreen(TFT_BLACK);
  setBacklight(briPct);

  initStage = "touch";
  if (!touchInit()) lastError = "touch not found";

  initStage = "logo";
  showLogo();

  initStage = "sprites";
  sprA.setColorDepth(16);
  sprB.setColorDepth(16);
  if (!sprA.createSprite(240, 240) || !sprB.createSprite(240, 240)) lastError = "sprite alloc failed";

  lastInputMs = millis();
  if (!tourDone) tourStep = 0;   // pierwsze uruchomienie: samouczek
  initStage = "ready";
  redraw();
}

void loop() {
  handleEncoder();
  handleButton();
  handleTouch();
  handleSerial();
  pomoTick();
  alertTick();
  rgbTick();
  jogTick();
  scrollTick();
  hoverTick();
  switcherTick();
  if (prefsPending && millis() - prefsAtMs > 1500) { savePrefs(); prefsPending = false; }

  static uint32_t lastSec = 0, lastDraw = 0, lastClockSec = 0;
  static bool usbPrev = false;
  uint32_t sec = millis() / 1000;
  if (sec != lastSec) {
    lastSec = sec;
    if ((pomoRunning()) && (tile == 0 || tile == 2)) dirty = true;
    if (spotifyShown() && npOnline() && npPlaying) dirty = true;   // pasek postepu utworu
  }
  bool u = usbReady();
  if (u != usbPrev) { usbPrev = u; dirty = true; }
  if (lastActionMs && millis() - lastActionMs > 1250 && lastAction[0]) { lastAction[0] = 0; dirty = true; }
  if (!dimmed && millis() - lastInputMs > IDLE_DIM_MS) {
    dimmed = true;
    if (clockFace && timeValid && !tourActive()) { clockShown = true; setBacklight(CLOCK_DIM_PCT); dirty = true; }   // tarcza zegarka
    else setBacklight(IDLE_DIM_PCT);
  }
  // przypomnienie o przerwie: STANDUP_MIN minut pracy bez 5 minut przerwy (Pomodoro ma wlasne przerwy)
  if (lastInputMs != lastActMs) {
    if (!activeSinceMs || lastInputMs - lastActMs > 5UL * 60000UL) activeSinceMs = lastInputMs;
    lastActMs = lastInputMs;
  }
  if (standup && !standShown && !tourActive() && !pomoRunning() && activeSinceMs && millis() - lastInputMs < 60000UL &&
      millis() - activeSinceMs > (uint32_t)STANDUP_MIN * 60000UL) {
    standShown = true; clockShown = false; dirty = true;
    alertStart(); rgbFlash(0, 200, 255, 1500);
  }
  if (clockShown && sec != lastClockSec) { lastClockSec = sec; dirty = true; }

  if (uiAnimating) dirty = true;
  if (dirty && millis() - lastDraw >= ((jogActive || jogInertiaOn) ? 50 : (uiAnimating ? 16 : 33))) {
    uiAnimating = false;   // renderTile ustawi ponownie, jesli animacja trwa
    lastDraw = millis();
    redraw();
  }
  delay(1);
}
