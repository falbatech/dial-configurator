# Lista zmian DFTv1.2

Wersja w `config.h`: `DFTv1.2`. Zawiera wszystko z DFTv1.1 (patrz `CHANGELOG_DFTv1.1.md`).

## Nowe profile (bez instalowania czegokolwiek)
- **Meetings**: tryb = aplikacja (ZOOM / TEAMS / MEET). Obrot = glosnosc, tap = mikrofon, przycisk CAM = kamera.
  Wyciszony mikrofon: pierscien RGB pulsuje na czerwono (stan wedlug DIAL - aplikacja go nie zwraca).
  Skroty: Zoom Cmd+Shift+A/V (Mac) / Alt+A/V (Win); Teams Cmd/Ctrl+Shift+M/O; Meet Cmd/Ctrl+D/E.
- **System**: APPS (przelaczanie aplikacji: Cmd/Alt+Tab trzymany podczas krecenia), DESKTOP (Ctrl+strzalka / Win+Ctrl+strzalka),
  BRIGHTNESS (klawisze jasnosci), UNDO. Tap = Mission Control / Widok zadan.
- **Premiere Pro**: FRAME / ZOOM (= -) / EDIT (gora/dol = poprzedni/nastepny punkt ciecia) / JKL; tap = play; obrzeze = jog klatek.
- **Final Cut Pro** (macOS): jak Premiere, ZOOM = Cmd+= / Cmd+-.
- **Figma**: ZOOM / OPACITY (cyfry 1-0) / LAYER (Tab, Shift+Tab) / HISTORY; tap = Shift+1 (dopasuj).
- **Blender**: ZOOM (kolko) / ORBIT (num 4/6) / TILT (num 8/2) / FRAME / HISTORY; tap = Home (pokaz wszystko). Kursor nad widokiem 3D.
- **Fusion 360**: ZOOM (kolko) / ORBIT (Shift + srodkowy przycisk, przeciaganie) / PAN (srodkowy przycisk) / HISTORY; tap = F6 (dopasuj).

## Pulpity HOME
- EVERYDAY: Pomodoro, Smart Scroll, Volume, Media, Spotify, Meetings
- CREATIVE: DaVinci, Premiere, Final Cut, Photoshop, Lightroom, Pixelmator
- DESIGN: Figma, Blender, Fusion 360, KiCad
- SYSTEM: System, Settings
- CUSTOM: profile klienta (pojawia sie, gdy istnieje jakis profil)
- Nowe ikony; w motywie Color kazda aplikacja ma wlasny kolor, profile klienta - kolor wybrany w konfiguratorze.

## Animacje
- Wybor ikony na HOME: zaznaczona ikona plynnie sie powieksza (27 -> 30 px, 170 ms).
- Zmiana pulpitu: subtelny impuls pierscienia RGB w kolorze motywu (w motywie Color - kolor zaznaczonej aplikacji). Wylacza go RGB ring -> Action flash.

## Konfigurator w przegladarce (`web/`)
- Strona dziala w Chrome / Edge (Web Serial), nic nie trzeba instalowac. Wszystko zapisuje sie w DIAL.
- Ustawienia ogolne, ukrywanie ikon HOME, do **6 wlasnych profili** (nazwa, ikona, kolor, do 4 trybow, akcja obrotu w lewo / w prawo,
  akcja tapniecia; akcje: skrot klawiszowy nagrywany z klawiatury, klawisz multimedialny, kolko pionowo / poziomo), program do auto-profilu.
- **Aktualizacja firmware** z tej samej strony (ESP Web Tools): zapisuje bootloader, partycje i aplikacje - ustawienia i profile zostaja.
- Ustawia zegar DIAL przy polaczeniu.
- Dziala w sklepie: https://falbatech.click/dial-ft (blok Ecwid "Osadz kod", krotki kod `web/ecwid-embed-short.html`); pliki pobierane z GitHub falbatech/dial-configurator przez jsDelivr. Komponent `web/dialft-configurator.js` (Shadow DOM, PL/EN, jasny/ciemny).
- `tools/build_web.sh` sklada strone z aktualnym firmware i paczka helpera; zawartosc `web/` trzeba wgrac na serwer z HTTPS
  (np. falba.tech/dial). Podglad bez urzadzenia: `index.html?demo`.
- Wyglad w kolorach FalbaTech (zielen z logo).

## DIAL FT Helper 1.3 (macOS, opcjonalny)
- **Automatyczny profil**: wysyla na DIAL identyfikator aplikacji na wierzchu; DIAL przelacza profil (DaVinci, Premiere, Final Cut,
  Photoshop, Lightroom, Pixelmator, Spotify, Zoom, Teams, Figma, Blender, Fusion 360, KiCad oraz profile klienta z polem "Program").
  Nie przelacza w Ustawieniach, Pomodoro i samouczku; ukryta ikona = profil wylaczony. Settings -> Auto profile.
- **Zegar**: wysyla czas co 10 minut.
- Now Playing Spotify jak w 1.1.

## Tarcza zegarka
- Po bezczynnosci (zamiast przyciemnionego menu) zegar: godzina, data, sekundnik na obrzezu, Pomodoro lub dzisiejsze sesje, tytul utworu.
- Wymaga czasu z helpera albo konfiguratora (DIAL nie ma zegara z bateria). Pierwszy obrot / klik / dotyk tylko budzi.
- Settings -> Clock face. Jasnosc: `CLOCK_DIM_PCT`.

## Samouczek
- Przy pierwszym uruchomieniu: TURN, CLICK, HOLD, SWIPE, TAP - kazdy krok trzeba wykonac. Ponownie: Settings -> Tutorial.

## Pomodoro
- Statystyki: sesje dzis i w ostatnich 7 dniach (gdy znany jest czas), na ekranie Pomodoro i na tarczy zegarka.

## Przypomnienie o przerwie
- Po `STANDUP_MIN` (50) minutach pracy bez 5 minut przerwy: ekran "TIME TO STAND UP", blysk pierscienia. Nie dziala w trakcie Pomodoro.
  Settings -> Stand-up.

## Motyw FalbaTech
- Nowy motyw w zieleni FalbaTech; domyslny dla nowych urzadzen (dotychczasowe ustawienia motywu zostaja).

## Instrukcja obslugi
- Nowa instrukcja PL i EN dla DFTv1.2 (`docs/`, generowana z `docs/src/build_manual.py` przez Chrome), linki w konfiguratorze.

## Protokol USB (dla wlasnych narzedzi)
`CFG GET` / `CFG SET {json}`, `TIME <epoch> <strefa_min>`, `APP <identyfikator>`, `STATUS`, `HELLO`.

## Do przetestowania na urzadzeniu
1. Samouczek po aktualizacji (pokazuje sie raz).
2. Konfigurator w Chrome: polaczenie, zapis ustawien, wlasny profil (nagrany skrot), ukrywanie ikon, aktualizacja firmware.
3. Auto profil z helperem: Resolve -> DaVinci, Figma -> Figma itd.
4. Tarcza zegarka po 45 s bezczynnosci (z helperem).
5. Meetings w Zoom / Teams / Meet; System: przelaczanie aplikacji i pulpitow, jasnosc.
6. Premiere / Final Cut / Figma / Blender / Fusion 360 (Fusion ORBIT i PAN przeciaganiem).
7. Przypomnienie o przerwie (do testu mozna zmniejszyc STANDUP_MIN).
8. Windows: skroty z Ctrl, profil System.
