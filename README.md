# DIAL FT (FalbaTech) - DFTv1.3

Firmware dla Elecrow CrowPanel 1.28" (ESP32-S3, GC9A01 240x240, dotyk CST816, enkoder + przycisk, 5x WS2812).
Interfejs w stylu zegarka: pulpity HOME + obrotowy "bezel". Zmiany: `CHANGELOG_DFTv1.3.md`, `CHANGELOG_DFTv1.2.md`, `CHANGELOG_DFTv1.1.md`.

**Konfigurator i aktualizacje w przegladarce:** https://falbatech.click/dial-ft (Ecwid, kod: `web/ecwid-embed-short.html`; pliki na GitHub falbatech/dial-configurator, zawartosc `web/`) (zloz: `tools/build_web.sh`, wgraj na serwer HTTPS). **Helper (opcjonalny, macOS):** `tools/helper` - okladki Spotify, automatyczny profil, zegar.

## Ekrany
- HOME - pulpity: EVERYDAY (Pomodoro, Smart Scroll, Volume, DaVinci, Photoshop, Lightroom), CREATIVE (Pixelmator Pro, Spotify, Media), SYSTEM (Settings, KiCad).
- CONTROL - aktywny profil (obrot = akcja, klik = nastepny tryb, tap w srodek = akcja tap).
- POMODORO - obrot ustawia minuty (gdy gotowy), klik/tap = start/pauza, RESET dotykiem.
- SETTINGS -> RGB ring -> Touch ring (swipe w lewo). Motywy: Ocean, Amber, Mint, Violet, Rose, Mono, Color (kazda aplikacja we wlasnym kolorze).

## Sterowanie fizyczne
- Obrot: HOME wybor ikony (przez wszystkie pulpity) | CONTROL akcja | POMODORO minuty | listy: wybor wiersza.
- Klik: HOME otwiera | CONTROL zmienia tryb | POMODORO start/pauza | listy: zmienia wartosc.
- Przytrzymanie (700 ms): pierwszy pulpit HOME (Pomodoro i profil zostaja).

## Profile ("mod" = Ctrl albo Cmd wg Settings -> Host OS)
- Volume: obrot = glosnosc, klik/tap = mute.
- Smart Scroll: VERTICAL / HORIZONTAL, SMART / PRECISION, bezwladnosc; tap = odwroc kierunek.
- DaVinci: FRAME (strzalki) / ZOOM (mod +/-) / SCROLL (poziomy) / JKL / COLOR; tap = spacja; obrzeze = jog.
- KiCad: ZOOM / PAN X (mod+kolko) / PAN Y (Shift+kolko) / LAYER (+/-) / ROTATE (R, Shift+R); tap = Esc.
- Photoshop: BRUSH / HARDNESS / ZOOM / HISTORY (undo/redo) / LAYER; tap = dopasuj do ekranu.
- Lightroom Classic (Develop): ADJUST (+/- numeryczne) / SLIDER (. ,) / PHOTO / ZOOM; tap = przed/po (`\`).
- Pixelmator Pro (zawsze Cmd): BRUSH / HARDNESS / OPACITY / ZOOM / HISTORY / LAYER; tap = Zoom to Fit.
- Spotify: VOLUME / TRACK / SEEK; tap = play/pause, przycisk LIKE = polub utwor. Z DIAL FT Helper (`tools/helper`, macOS): okladka, tytul, wykonawca, postep.
- Media: VOLUME / TRACK; tap = play/pause.
Skroty edytujesz w `doTurn()` / `tapAction()` w `src/main.cpp`.

## COLOR (DaVinci Resolve)
Tryb COLOR w profilu DaVinci: najedz kursorem na dowolna wartosc na stronie Color i krec pokretlem (dziala jak przeciaganie wartosci mysza).
Bez aplikacji na komputerze, rowniez w darmowym Resolve. Tap = krok FINE / NORMAL / COARSE, obrzeze ekranu = najdrobniej.

## Budowanie
1. `pip install pillow && python tools/convert_logo.py` (opcjonalnie; bez logo.h pokaze sie tekst).
2. `tools/upload.sh` (kompilacja, przejscie do bootloadera przez USB, wgranie, restart). Pierwszy raz albo po zawieszeniu: przytrzymaj BOOT i podlacz USB.
3. Port szeregowy USB: `STATUS`.

## Programowanie nowego kontrolera
- Przegladarka: https://falbatech.click/dial-ft -> Aktualizacja oprogramowania -> "2. Zainstaluj aktualizacje" (przy fabrycznym oprogramowaniu Elecrow krok "1. Przygotuj" nie jest potrzebny). Nie laczy sie: odlacz, przytrzymaj BOOT, podlacz, pusc BOOT.
- Terminal (seria, bez PlatformIO): `tools/flash_new.sh` (jeden) albo `tools/flash_new.sh --loop` (kolejne kontrolery). Czysci pamiec i wgrywa pliki z `web/firmware/`. Wymaga esptool (`pip3 install esptool`).

## Strojenie (src/config.h)
- Swipe/tap w zla strone: `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y`.
- Dotyk nie dziala: sprawdz `PIN_TP_RST` (STATUS pokazuje `touch err`).
- Obraz lustrzany: `LCD_ROTATION` lub `LCD_MADCTL_OVERRIDE`; kolory: `TFT_RGB_ORDER=TFT_BGR`.

## Zmiany DFTv1 (historia)
- Dotyk: osobne odczyty 0x01, 0x02 i 0x03-0x06; init z impulsem INT, 300 ms po resecie, zapis 0xFF do 0xFE; I2C 100 kHz; blad odczytu zeruje stan gestu.
- Pomodoro: alarm 5 mignięć bez delay() (petla dalej obsluguje wejscia).
- Zapis minut Pomodoro do flash 1,5 s po ostatnim kroku enkodera.

## Blad `FS.h: No such file` w USBHostMSC.cpp
Naprawia go `tools/framework_include.py` (extra_scripts w platformio.ini): dodaje katalog biblioteki FS do sciezek include.

## Potwierdzona dzialajaca konfiguracja
Wersja z `-DUSE_HSPI_PORT=1` (TFT_eSPI), `tools/framework_include.py` i `#include <FS.h>` w main.cpp dziala na urzadzeniu (ekran + build).

## Pierscien RGB (DFTv1)
5x WS2812 na GPIO48 (kolejnosc GRB). Piaty pulpit (przesun w lewo z SETTINGS) = panel "RGB ring".
- **Mode**: Off / Follow (swiatelko z ogonem podaza za obrotem pokretla) / Ambient / Breathe / Rainbow.
- **Color** (osobno dla kazdego motywu): Theme (kolor motywu), 36 odcieni co 10 stopni albo White. Klik wchodzi w edycje, obrot zmienia kolor, klik konczy.
- **Brightness** 20-100%, **Direction** Normal/Reverse (jesli swiatelko krazy w przeciwna strone niz pokretlo).
- **Pomodoro**: pierscien pokazuje postep (przerwa na zielono, pauza pulsuje, koniec = bialy blysk).
- **Action flash**: krotki blysk przy akcjach (Mute na czerwono, zmiana trybu/profilu w kolorze pierscienia).
- **Idle off**: pierscien gasnie razem ze sciemnieniem ekranu.
- Szybki wlacznik: SETTINGS -> "RGB ring" (klik = On/Off). Wszystko zapisuje sie w pamieci flash.
- Obrot swiatelka kalibrujesz `RGB_DETENTS_PER_REV` w `config.h`.
- Test sprzetu: `pio run -e diag -t upload` zapala pierscien w tym samym kolorze co ekran.

## Touch jog (DFTv1)
W profilu DaVinci przesuwaj palec po obrzezu ekranu (promien >= `JOG_R_MIN`): kazde `Jog speed` stopni = 1 klatka (strzalka prawo/lewo), zgodnie z ruchem wskazowek = do przodu.
Pokretlo = skoki zgrubne, obrzeze = dokladne przesuwanie. Kropka z ogonem podaza za palcem, pierscien RGB (Follow) tez.
Ustawienia w SETTINGS: Touch jog (ON/OFF), Jog speed (3/4/6/8/12 stopni), Jog inertia (bezwladnosc), Jog dir (kierunek).
Swipe zmieniajacy ekran w profilu DaVinci zaczynaj ze srodka ekranu. `JOG_FRAMES_CAP` ogranicza liczbe klatek na odczyt dotyku.
Dokumentacja dla klientow: `docs/` (PL i EN).
