# Lista zmian DFTv1.1

Wersja w `config.h`: `DFTv1.1`. Zawiera etapy a1-a3 oraz wszystko, co bylo zaplanowane dalej.
Stabilna kopia DFTv1 i etap a3 pozostaja nietkniete (osobne foldery / zip).

## Pulpity HOME (a1)
- EVERYDAY: Pomodoro, Smart Scroll, Volume, DaVinci, Photoshop, Lightroom.
- CREATIVE: Pixelmator Pro, Spotify, Media. (OBS Studio pozostaje ukryte - patrz nizej.)
- SYSTEM: Settings, KiCad.
- Obrot enkodera wybiera ikony przez wszystkie pulpity; klik/dotyk otwiera.
- Swipe lewo/prawo na HOME zmienia pulpit; z aplikacji swipe w prawo wraca na HOME.
- Dlugie przytrzymanie zawsze wraca na pierwszy pulpit. Nie zatrzymuje Pomodoro i nie zmienia aktywnego profilu.

## Smart Touch Ring (a2)
- Per profil: Touch mode (SMOOTH/STEP), Sensitivity, Inertia (OFF/LOW/MEDIUM/HIGH), Direction, Adaptive Touch, Touch ring ON/OFF.
- DaVinci domyslnie identyczny z jogiem DFTv1; stare ustawienia sa migrowane.
- Nowe profile: Photoshop (SMOOTH, ON), Lightroom (STEP, ON), Media (SMOOTH, ON), Pixelmator Pro (SMOOTH, ON).

## Smart Scroll (a3)
- SMART (akceleracja) / PRECISION, czulosc, bezwladnosc, kierunek, zapamietywana os.
- Zmiana kierunku zatrzymuje bezwladnosc; przewijanie konczy sie przy wyjsciu z profilu, dotknieciu ekranu, odlaczeniu USB.
- Ograniczenie: standardowe kolko myszy USB ma calkowita rozdzielczosc; plynnosc daja czeste zdarzenia i bezwladnosc.

## Nowe w DFTv1.1 (po a3)
### Host OS (Settings)
- Windows (Ctrl) albo macOS (Cmd). Dotyczy skrotow z modyfikatorem: DaVinci ZOOM, KiCad PAN X, Photoshop, Lightroom.
- Domyslnie Windows - tak jak dzialalo DFTv1/a3. **Na Macu ustaw macOS**, inaczej np. zoom w DaVinci wysle Ctrl zamiast Cmd.

### Profile
- **DaVinci**: FRAME / ZOOM / SCROLL / JKL / **COLOR** (nowy, patrz nizej). Tap = spacja, obrzeze = jog klatek.
- **KiCad**: ZOOM (kolko) / PAN X (mod+kolko) / **PAN Y** (Shift+kolko) / LAYER (+/-) / ROTATE (R w lewo, **Shift+R w prawo**). Tap = Esc.
- **Photoshop** (nowy): BRUSH `[ ]` / HARDNESS `Shift+[ ]` / ZOOM `mod+= / mod+-` / HISTORY `mod+Z / mod+Shift+Z` / LAYER `Alt+[ ]`. Tap = dopasuj do ekranu `mod+0`.
- **Lightroom Classic** (nowy, modul Develop): ADJUST (`+ / -` z klawiatury numerycznej; szybki obrot = Shift, duze kroki) / SLIDER (`. / ,` nastepny/poprzedni suwak panelu Basic) / PHOTO (`mod+strzalka`) / ZOOM (`mod+= / mod+-`). Tap = `\` przed/po.
- **Pixelmator Pro** (nowy, macOS - zawsze Cmd niezaleznie od Host OS; skroty z przewodnika Apple): BRUSH `[ ]` / HARDNESS `Shift+[ ]` / OPACITY (cyfry 1-0 = 10-100%: krycie pedzla w Paint/Erase albo warstwy w narzedziu Style; DIAL pamieta wlasna wartosc, startuje od 100%) / ZOOM `Cmd+= / Cmd+-` / HISTORY `Cmd+Z / Cmd+Shift+Z` / LAYER `Option+] / Option+[` (nastepna/poprzednia warstwa). Tap = Zoom to Fit `Cmd+0`.
- **Spotify** (nowy): VOLUME / TRACK (klawisze multimedialne - dzialaja tez, gdy Spotify jest w tle) / SEEK (`Shift+strzalka`, wymaga aktywnego okna Spotify). Tap = play/pause, przycisk LIKE = `Alt+Shift+B` (polub utwor; skrot z oficjalnej listy Spotify, wymaga aktywnego okna).
- **Spotify - Now Playing** (z DIAL FT Helper na macOS, `tools/helper`): okladka 112x112, tytul, wykonawca, postep utworu na obrzezu, znak pauzy. Dane z wlaczonej aplikacji Spotify (AppleScript), bez logowania. Bez helpera profil dziala jak wczesniej.
- **Media** (nowy): VOLUME / TRACK (nastepny/poprzedni utwor, jeden krok na klikniecie). Tap = play/pause. Dziala w kazdym odtwarzaczu obslugujacym klawisze multimedialne.
- **OBS Studio**: nadal ukryte. OBS nie ma domyslnych skrotow, a przelaczania scen nie da sie zrobic bez recznego przypisywania skrotow albo WebSocket. Nie pokazujemy ikony, ktora nie dziala od razu.

### COLOR (DaVinci Resolve) - najedz i krec
- Tryb COLOR w profilu DaVinci: najedz kursorem na dowolna wartosc na stronie Color (Temp, Tint, Contrast, Saturation, paski Lift/Gamma/Gain/Offset pod kolami, Pivot, Hue...) i krec pokretlem. To prawdziwe kontrolki Resolve, nie przyblizenia.
- **Bez aplikacji na komputerze**, bez konfiguracji, takze w darmowym Resolve; dziala na macOS, Windows i Linux w kazdej rozdzielczosci. DIAL jako mysz USB wciska przycisk w miejscu kursora, przesuwa w poziomie (jak przeciaganie wartosci w Resolve), po chwili bez obrotu puszcza i cofa kursor.
- Tap na ekranie: krok FINE / NORMAL / COARSE (`HOVER_STEP` w `config.h`). Obrzeze ekranu: najmniejsze kroki.
- Uwaga dla uzytkownika: obrot dziala jak przeciaganie mysza - kursor musi stac nad wartoscia (nad innym elementem przeciagnalby go).
- Odrzucone po testach: skrypt CDL przez API Resolve (w darmowym Resolve brak zewnetrznego API, CDL nie daje prawdziwych Lift/Gamma/Gain/Temp/Tint), sterowanie z helpera z zapamietanymi pozycjami pol (wymagalo konfiguracji u klienta, przeskoki), emulacja paneli Tangent/Blackmagic (cudze identyfikatory USB).

### Motyw Color
- Siodmy motyw w Settings -> Theme. Kazda aplikacja ma wlasny kolor ikony na HOME: Smart Scroll indygo, DaVinci pomaranczowy, KiCad limonkowy, Pomodoro czerwony, Settings szary, Photoshop niebieski, Lightroom turkusowy, Media rozowy, Pixelmator zolty, Spotify zielony; Volume fioletowy.
- Akcent ekranu (obwodka, zaznaczenie, nazwy trybow) przyjmuje kolor zaznaczonej / aktywnej aplikacji. Pierscien RGB z kolorem "Theme" tez swieci kolorem aplikacji. Kolor w `ICON_RGB` w `src/main.cpp`.

### Smart Scroll - ikona
- Ikona Smart Scroll (ekran profilu i HOME) pokazuje os: strzalki gora/dol dla VERTICAL, lewo/prawo dla HORIZONTAL.

### Pierscien RGB
- **Kolor zapisywany osobno dla kazdego motywu** (zmiana motywu przywraca kolor ustawiony dla tego motywu). Tytul ekranu pokazuje, dla ktorego motywu ustawiasz kolor. Migracja: dotychczasowy kolor trafia do biezacego motywu.
- Bez zmian: Off/Follow/Ambient/Breathe/Rainbow, jasnosc, kierunek, Pomodoro, blyski akcji, wylaczanie przy sciemnieniu.

### Pomodoro
- Dlugosc przerwy ustawiana w Settings -> Pomo break (3/5/10/15 min), zapisywana w pamieci. Ekran READY pokazuje fokus i przerwe.

### Port szeregowy
- **Naprawa:** od DFTv1 port CDC przez USB nie istnial (flagi plytki nadpisywaly `ARDUINO_USB_CDC_ON_BOOT`, `Serial` trafial na UART0). Teraz firmware uzywa jawnego `USBCDC`, wiec port jest zawsze widoczny (np. `/dev/cu.usbmodem...`), a kolejne wgrania nie wymagaja przycisku BOOT.
- `STATUS` pokazuje tez Host OS i stan helpera. `HELLO` zwraca wersje (identyfikacja portu).

## Do przetestowania na urzadzeniu (nic z DFTv1.1 nie bylo uruchomione na sprzecie)
1. Aktualizacja z a3: ustawienia (motyw, kolor RGB, touch ring, os scrolla) zostaja zachowane.
2. HOME: 6 ikon na EVERYDAY, pulpit CREATIVE z Media, przejscia enkoderem i swipe.
3. Host OS = macOS: DaVinci ZOOM, Photoshop ZOOM/HISTORY/FIT, Lightroom PHOTO/ZOOM.
4. Lightroom ADJUST: czy `+/-` z klawiatury numerycznej zmienia wybrany suwak (na Twojej wersji LR).
5. KiCad ROTATE w obie strony, PAN Y.
6. Pixelmator Pro: wszystkie 6 trybow; OPACITY z narzedziem Paint i Style.
7. Spotify: SEEK i LIKE przy aktywnym oknie Spotify; utwory w tle.
8. Spotify Now Playing: `tools/helper/install_mac.sh`, okladka i tytul po zmianie utworu, pauza, postep.
9. Media: next/prev/play w Spotify / Music / przegladarce.
10. COLOR: Temp/Tint/Saturation/paski kol w FINE/NORMAL/COARSE, powrot kursora; Windows.
