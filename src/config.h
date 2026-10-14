#pragma once
#define FW_VERSION "DFTv1.3"

// ---- Piny ----
#define PIN_ENC_A 45
#define PIN_ENC_B 42
#define PIN_BTN   41
#define PIN_BL    46
#define PIN_PWR_1 1      // HIGH przed inicjalizacja LCD (kod fabryczny Elecrow)
#define PIN_PWR_2 2

// Dotyk CST816 (I2C wg README Elecrow). RST = -1 jesli nie chcesz resetowac.
#define PIN_TP_SDA 6
#define PIN_TP_SCL 7
#define PIN_TP_RST 13    // SPRAWDZ w factory_soucecode
#define PIN_TP_INT 5     // impuls przed resetem; -1 = pomin
#define TP_ADDR    0x15

// Mapowanie dotyku -> ekran (jesli swipe/tap w zla strone, zmien)
#define TOUCH_SWAP_XY 0
#define TOUCH_FLIP_X  0
#define TOUCH_FLIP_Y  0
#define SWIPE_MIN     40   // px
#define TAP_MAX       16   // px

// ---- Enkoder / przycisk ----
#define ENC_STEPS_PER_DETENT 4
#define ENC_INVERT 0
#define BTN_DEBOUNCE_MS 25
#define BTN_HOLD_MS     700   // przytrzymanie = powrot na pulpit glowny

// ---- Wyswietlacz ----
#define LOGO_MS      1800
#define LCD_ROTATION 0
// #define LCD_MADCTL_OVERRIDE 0x48
#define IDLE_DIM_MS  45000    // sciemnianie po bezczynnosci
#define IDLE_DIM_PCT 15
#define CLOCK_DIM_PCT 35      // jasnosc tarczy zegarka w spoczynku
#define STANDUP_MIN  50       // przypomnienie o przerwie po tylu minutach ciaglej pracy

// ---- Pomodoro ----
#define POMO_FOCUS_DEFAULT 25
#define POMO_BREAK_DEFAULT 5

// ---- Pierscien RGB (5x WS2812, GRB) ----
#define PIN_RGB    48
#define RGB_COUNT  5
#define RGB_DETENTS_PER_REV 24   // ile krokow enkodera = pelny obrot swiatelka; zmien, jesli za wolno/szybko

// ---- Touch jog (DaVinci) ----
#define JOG_R_MIN       78    // promien (px) od srodka, od ktorego zaczyna sie strefa jogu
#define JOG_FRAMES_CAP  4     // max klatek na jeden odczyt dotyku

// ---- COLOR (DaVinci Resolve): najedz kursorem na wartosc i krec ----

#define HOVER_STEP 2   // COLOR hover: ruch myszy (jednostki HID) na kliniecie enkodera w trybie NORMAL
