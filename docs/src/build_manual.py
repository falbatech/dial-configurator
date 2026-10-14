#!/usr/bin/env python3
"""Instrukcja obslugi DIAL FT (PL + EN): HTML -> PDF przez Google Chrome (headless).

Uzycie: python3 docs/src/build_manual.py   -> docs/DIAL_FT_Instrukcja_PL.pdf, docs/DIAL_FT_User_Guide_EN.pdf
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.dirname(HERE)
VERSION = "DFTv1.3"
CONFIG_URL = "falbatech.click/dial-ft"   # strona konfiguratora w sklepie Ecwid
SITE = "falbatech.click"
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

CSS = """
@page { size: A4; margin: 16mm 15mm 16mm 15mm; }
@page :first { margin: 0; }
* { box-sizing: border-box; }
html { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
body { font-family: -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif; color: #1d1d1f; font-size: 10.2pt; line-height: 1.45; margin: 0; }
.cover { height: 297mm; width: 210mm; position: relative; background: #0f1410; color: #fff; overflow: hidden; page-break-after: always; }
.cover img.photo { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover; opacity: .78; }
.cover .shade { position: absolute; inset: 0; background: linear-gradient(180deg, rgba(10,16,10,.92) 0%, rgba(10,16,10,.35) 38%, rgba(10,16,10,.15) 60%, rgba(10,16,10,.9) 100%); }
.cover .top { position: absolute; top: 22mm; left: 18mm; right: 18mm; }
.cover .brand { font-size: 12pt; letter-spacing: 3px; color: #8fca8f; text-transform: uppercase; }
.cover h1 { font-size: 46pt; margin: 4mm 0 2mm; letter-spacing: 1px; }
.cover .subtitle { font-size: 16pt; color: #d9e8d9; }
.cover .bottom { position: absolute; bottom: 18mm; left: 18mm; right: 18mm; display: flex; justify-content: space-between; align-items: end; font-size: 10pt; color: #cfdccf; }
.cover .logo { background: #fff; border-radius: 10px; padding: 3mm 4mm; height: 24mm; }
h2 { font-size: 16pt; color: #2f5f2f; border-bottom: 2px solid #518751; padding-bottom: 1.5mm; margin: 0 0 3.5mm; page-break-after: avoid; }
h3 { font-size: 11.5pt; margin: 4.5mm 0 1.5mm; color: #1d1d1f; page-break-after: avoid; }
section { margin-bottom: 7mm; }
section.break { page-break-before: always; }
p { margin: 0 0 2.4mm; }
ul, ol { margin: 0 0 2.6mm; padding-left: 5.5mm; }
li { margin: .6mm 0; }
table { width: 100%; border-collapse: collapse; margin: 1.5mm 0 3.5mm; font-size: 9.4pt; page-break-inside: auto; }
tr { page-break-inside: avoid; }
th { background: #eaf3ea; color: #2f5f2f; text-align: left; font-weight: 600; padding: 1.6mm 2mm; border-bottom: 1px solid #b9d6b9; }
td { padding: 1.4mm 2mm; border-bottom: 1px solid #e5e5e7; vertical-align: top; }
td.k { font-weight: 600; white-space: nowrap; width: 26mm; }
.key { display: inline-block; font-family: "SF Mono", Menlo, monospace; font-size: 8.4pt; background: #f2f2f4; border: 1px solid #dcdce0; border-radius: 3px; padding: 0 1.2mm; }
.note { background: #eef6ee; border-left: 3px solid #518751; padding: 2.2mm 3mm; margin: 2mm 0 3.5mm; border-radius: 0 4px 4px 0; }
.warn { background: #fff7e6; border-left: 3px solid #e0a020; padding: 2.2mm 3mm; margin: 2mm 0 3.5mm; border-radius: 0 4px 4px 0; }
.grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 6mm; align-items: start; }
.figure img { width: 100%; border-radius: 6px; }
.figure .cap { font-size: 8.6pt; color: #6e6e73; margin-top: 1mm; }
.controls td.k { width: 30mm; }
.toc { columns: 2; column-gap: 8mm; font-size: 10pt; }
.toc div { margin: .8mm 0; }
.small { font-size: 8.8pt; color: #6e6e73; }
"""


def k(s):
    return "".join('<span class="key">%s</span>' % p if p not in ("+", " / ", "/") else p
                   for p in _split_keys(s))


def _split_keys(s):
    out, cur = [], ""
    for part in s.split(" / "):
        if out:
            out.append(" / ")
        keys = part.split("+")
        for i, kk in enumerate(keys):
            if i:
                out.append("+")
            out.append(kk)
    return out


# ------------------------------------------------------------------ profile (wspolne, tryby: nazwa, PL, EN)
# skroty: "Mod" = Ctrl (Windows) albo Cmd (macOS) wg Settings -> Host OS
PROFILES = [
    ("Volume", [("VOLUME", "głośność systemu", "system volume")], ("wycisz", "mute")),
    ("Smart Scroll", [("VERTICAL", "przewijanie góra / dół", "scroll up / down"),
                      ("HORIZONTAL", "przewijanie lewo / prawo", "scroll left / right")],
     ("odwróć kierunek", "invert direction")),
    ("Media", [("VOLUME", "głośność", "volume"), ("TRACK", "poprzedni / następny utwór", "previous / next track")],
     ("odtwarzaj / pauza", "play / pause")),
    ("Spotify", [("VOLUME", "głośność", "volume"), ("TRACK", "poprzedni / następny utwór", "previous / next track"),
                 ("SEEK", "przewijanie utworu (" + k("Shift+←/→") + ")", "seek in track (" + k("Shift+←/→") + ")")],
     ("odtwarzaj / pauza; przycisk LIKE = polub (" + k("Alt+Shift+B") + ")", "play / pause; LIKE button = like song (" + k("Alt+Shift+B") + ")")),
    ("Meetings", [("ZOOM / TEAMS / MEET", "głośność; tryb wybiera aplikację", "volume; the mode selects the app")],
     ("mikrofon wł./wył. (pierścień czerwony = wyciszony); przycisk CAM = kamera", "microphone on/off (red ring = muted); CAM button = camera")),
    ("DaVinci Resolve", [("FRAME", "klatka (" + k("←/→") + ")", "frame step (" + k("←/→") + ")"),
                         ("ZOOM", "powiększenie osi czasu (" + k("Mod+=/-") + ")", "timeline zoom (" + k("Mod+=/-") + ")"),
                         ("SCROLL", "przewijanie osi czasu", "timeline scroll"),
                         ("JKL", k("J / L"), k("J / L")),
                         ("COLOR", "najedź kursorem na wartość na stronie Color i kręć", "point the mouse at a value on the Color page and turn")],
     ("odtwarzaj / pauza (" + k("Space") + "); obrzeże ekranu = jog klatek", "play / pause (" + k("Space") + "); screen rim = frame jog")),
    ("Premiere Pro", [("FRAME", "klatka", "frame step"), ("ZOOM", "powiększenie osi czasu (" + k("= / -") + ")", "timeline zoom (" + k("= / -") + ")"),
                      ("EDIT", "poprzedni / następny punkt cięcia", "previous / next edit point"), ("JKL", k("J / L"), k("J / L"))],
     ("odtwarzaj / pauza; obrzeże = jog", "play / pause; rim = jog")),
    ("Final Cut Pro", [("FRAME", "klatka", "frame step"), ("ZOOM", "powiększenie (" + k("Cmd+=/-") + ")", "zoom (" + k("Cmd+=/-") + ")"),
                       ("EDIT", "poprzedni / następny punkt cięcia", "previous / next edit point"), ("JKL", k("J / L"), k("J / L"))],
     ("odtwarzaj / pauza; obrzeże = jog", "play / pause; rim = jog")),
    ("Photoshop", [("BRUSH", "rozmiar pędzla (" + k("[ / ]") + ")", "brush size (" + k("[ / ]") + ")"),
                   ("HARDNESS", "twardość pędzla", "brush hardness"), ("ZOOM", "powiększenie", "zoom"),
                   ("HISTORY", "cofnij / ponów", "undo / redo"), ("LAYER", "warstwa niżej / wyżej", "layer down / up")],
     ("dopasuj do ekranu (" + k("Mod+0") + ")", "fit on screen (" + k("Mod+0") + ")")),
    ("Lightroom Classic", [("ADJUST", "wybrany suwak - / + (szybki obrót = duże kroki)", "selected slider - / + (fast turn = big steps)"),
                           ("SLIDER", "poprzedni / następny suwak (" + k(", / .") + ")", "previous / next slider (" + k(", / .") + ")"),
                           ("PHOTO", "poprzednie / następne zdjęcie", "previous / next photo"), ("ZOOM", "powiększenie", "zoom")],
     ("przed / po (" + k("\\") + ")", "before / after (" + k("\\") + ")")),
    ("Pixelmator Pro", [("BRUSH", "rozmiar pędzla", "brush size"), ("HARDNESS", "miękkość / twardość", "softer / harder"),
                        ("OPACITY", "krycie 10-100%", "opacity 10-100%"), ("ZOOM", "powiększenie", "zoom"),
                        ("HISTORY", "cofnij / ponów", "undo / redo"), ("LAYER", "poprzednia / następna warstwa", "previous / next layer")],
     ("dopasuj do okna (" + k("Cmd+0") + ")", "zoom to fit (" + k("Cmd+0") + ")")),
    ("Figma", [("ZOOM", "powiększenie", "zoom"), ("OPACITY", "krycie warstwy 10-100%", "layer opacity 10-100%"),
               ("LAYER", "poprzednia / następna warstwa (" + k("Shift+Tab / Tab") + ")", "previous / next layer (" + k("Shift+Tab / Tab") + ")"),
               ("HISTORY", "cofnij / ponów", "undo / redo")],
     ("dopasuj (" + k("Shift+1") + ")", "zoom to fit (" + k("Shift+1") + ")")),
    ("Blender", [("ZOOM", "powiększenie widoku", "view zoom"), ("ORBIT", "obrót widoku lewo / prawo", "orbit left / right"),
                 ("TILT", "obrót widoku góra / dół", "orbit up / down"), ("FRAME", "klatka", "frame"),
                 ("HISTORY", "cofnij / ponów", "undo / redo")],
     ("pokaż wszystko (" + k("Home") + "); kursor nad widokiem 3D", "view all (" + k("Home") + "); mouse over the 3D view")),
    ("Fusion 360", [("ZOOM", "powiększenie", "zoom"), ("ORBIT", "obrót (jak Shift + środkowy przycisk)", "orbit (like Shift + middle mouse)"),
                    ("PAN", "przesuwanie (jak środkowy przycisk)", "pan (like middle mouse)"), ("HISTORY", "cofnij / ponów", "undo / redo")],
     ("dopasuj (" + k("F6") + "); kursor nad widokiem", "fit (" + k("F6") + "); mouse over the viewport")),
    ("KiCad", [("ZOOM", "powiększenie", "zoom"), ("PAN X", "przesuwanie lewo / prawo", "pan left / right"),
               ("PAN Y", "przesuwanie góra / dół", "pan up / down"), ("LAYER", "warstwa " + k("+ / -"), "layer " + k("+ / -")),
               ("ROTATE", "obrót elementu (" + k("R / Shift+R") + ")", "rotate item (" + k("R / Shift+R") + ")")],
     (k("Esc"), k("Esc"))),
    ("System", [("APPS", "przełączanie aplikacji", "switch apps"), ("DESKTOP", "poprzedni / następny pulpit", "previous / next desktop"),
                ("BRIGHTNESS", "jasność ekranu*", "screen brightness*"), ("UNDO", "cofnij / ponów", "undo / redo")],
     ("Mission Control / Widok zadań", "Mission Control / Task View")),
]

SETTINGS_ROWS = [
    ("Theme", "motyw kolorów: Ocean, Amber, Mint, Violet, Rose, Mono, Color (każda aplikacja w swoim kolorze), FalbaTech",
     "color theme: Ocean, Amber, Mint, Violet, Rose, Mono, Color (every app in its own color), FalbaTech"),
    ("Host OS", "Windows (skróty z Ctrl) albo macOS (skróty z Cmd) - ustaw zgodnie z komputerem",
     "Windows (Ctrl shortcuts) or macOS (Cmd shortcuts) - set it to match your computer"),
    ("Scroll dir", "kierunek przewijania", "scroll direction"),
    ("Brightness", "jasność ekranu DIAL", "DIAL screen brightness"),
    ("RGB ring", "szybkie włączanie / wyłączanie pierścienia RGB", "quick RGB ring on / off"),
    ("Pomo break", "długość przerwy Pomodoro: 3 / 5 / 10 / 15 min", "Pomodoro break: 3 / 5 / 10 / 15 min"),
    ("Touch jog ... Jog dir", "jog obrzeżem ekranu w DaVinci: włącz, szybkość, bezwładność, kierunek",
     "DaVinci rim jog: on/off, speed, inertia, direction"),
    ("Touch ring >", "Smart Touch Ring - ustawienia dla każdego profilu (patrz niżej)", "Smart Touch Ring - per-profile settings (see below)"),
    ("Auto profile", "automatyczna zmiana profilu z DIAL FT Helper", "automatic profile switching with DIAL FT Helper"),
    ("Clock face", "tarcza zegarka w spoczynku", "clock face when idle"),
    ("Stand-up", "przypomnienie o przerwie po 50 min pracy", "stand-up reminder after 50 min of work"),
    ("Tutorial >", "uruchamia ponownie samouczek", "runs the tutorial again"),
    ("Version", "wersja firmware", "firmware version"),
]

TXT = {
    "pl": {
        "lang": "pl", "file": "DIAL_FT_Instrukcja_PL", "title": "Instrukcja obsługi", "brand": "FalbaTech",
        "toc": "Spis treści",
        "s": {
            "start": "Pierwsze kroki", "controls": "Sterowanie", "home": "Ekran HOME", "profiles": "Profile",
            "color": "DaVinci Resolve: COLOR", "ring": "Smart Touch Ring i Smart Scroll", "pomo": "Pomodoro, przerwy i zegar",
            "rgb": "Pierścień RGB", "settings": "Ustawienia", "config": "Konfigurator i własne profile",
            "update": "Aktualizacja oprogramowania", "helper": "DIAL FT Helper (opcjonalny, macOS)",
            "trouble": "Rozwiązywanie problemów", "specs": "Dane techniczne i bezpieczeństwo",
        },
    },
    "en": {
        "lang": "en", "file": "DIAL_FT_User_Guide_EN", "title": "User Guide", "brand": "FalbaTech",
        "toc": "Contents",
        "s": {
            "start": "Getting started", "controls": "Controls", "home": "HOME screen", "profiles": "Profiles",
            "color": "DaVinci Resolve: COLOR", "ring": "Smart Touch Ring and Smart Scroll", "pomo": "Pomodoro, breaks and clock",
            "rgb": "RGB ring", "settings": "Settings", "config": "Configurator and custom profiles",
            "update": "Firmware update", "helper": "DIAL FT Helper (optional, macOS)",
            "trouble": "Troubleshooting", "specs": "Specifications and safety",
        },
    },
}


def body(L):
    pl = L == "pl"
    S = TXT[L]["s"]
    T = lambda p, e: p if pl else e
    h = []
    # ---- okladka
    h.append(f"""<div class="cover"><img class="photo" src="cover.jpg"><div class="shade"></div>
<div class="top"><div class="brand">FalbaTech</div><h1>DIAL FT</h1>
<div class="subtitle">{TXT[L]['title']} &middot; {VERSION}</div></div>
<div class="bottom"><div>{T('Inteligentne pokrętło z okrągłym ekranem dotykowym<br>do pracy twórczej, montażu i codziennej pracy.',
                           'A smart dial with a round touchscreen<br>for creative work, editing and everyday tasks.')}<br><br>{SITE}</div>
<img class="logo" src="logo.png"></div></div>""")
    # ---- spis tresci
    h.append(f"<section><h2>{TXT[L]['toc']}</h2><div class='toc'>" +
             "".join(f"<div>{i + 1}. {v}</div>" for i, v in enumerate(S.values())) + "</div></section>")
    # ---- 1. start
    h.append(f"""<section><h2>1. {S['start']}</h2>
<div class="grid2"><div>
<ol>
<li>{T('Podłącz DIAL FT kablem USB-C do komputera (macOS, Windows lub Linux). Nie trzeba instalować sterowników.',
       'Connect DIAL FT to your computer (macOS, Windows or Linux) with the USB-C cable. No drivers are needed.')}</li>
<li>{T('Po logo FalbaTech pojawi się krótki samouczek: wykonaj kolejno obrót, klik, przytrzymanie, przesunięcie palcem i tapnięcie.',
       'After the FalbaTech logo a short tutorial appears: turn, click, hold, swipe and tap, one after another.')}</li>
<li>{T('Otwórz <b>Settings → Host OS</b> i wybierz swój system (macOS albo Windows). Od tego zależy, czy skróty używają Cmd czy Ctrl.',
       'Open <b>Settings → Host OS</b> and choose your system (macOS or Windows). It decides whether shortcuts use Cmd or Ctrl.')}</li>
<li>{T('Wybierz aplikację na ekranie HOME obrotem i otwórz ją klikiem albo tapnięciem.',
       'Pick an app on the HOME screen by turning the dial and open it with a click or a tap.')}</li>
</ol>
<div class="note">{T('Wszystkie profile działają od razu, bez żadnego programu na komputerze. DIAL FT jest dla komputera klawiaturą i myszą USB. '
                     'Opcjonalny DIAL FT Helper dodaje okładki Spotify, automatyczną zmianę profilu i zegar.',
                     'Every profile works right away, with no software on your computer. To the computer DIAL FT is a USB keyboard and mouse. '
                     'The optional DIAL FT Helper adds Spotify cover art, automatic profile switching and the clock.')}</div>
</div><div class="figure"><img src="home.jpg"><div class="cap">{T('Ekran HOME, motyw Color.', 'HOME screen, Color theme.')}</div></div></div>
</section>""")
    # ---- 2. sterowanie
    rows = [
        (T("Obrót", "Turn"), T("wybór ikony / akcja profilu (np. głośność, klatka) / wybór wiersza w ustawieniach",
                               "select an icon / profile action (e.g. volume, frame) / select a row in settings")),
        (T("Klik", "Click"), T("otwiera aplikację; w profilu zmienia tryb (np. FRAME → ZOOM); w ustawieniach zmienia wartość",
                               "opens an app; in a profile switches the mode (e.g. FRAME → ZOOM); in settings changes the value")),
        (T("Przytrzymanie", "Hold"), T("powrót na pierwszy ekran HOME (Pomodoro i profil zostają)",
                                       "back to the first HOME page (Pomodoro and the profile keep running)")),
        (T("Przesunięcie palcem", "Swipe"), T("lewo / prawo: zmiana strony HOME; w profilu w prawo: powrót na HOME; Ustawienia → RGB → Touch ring",
                                              "left / right: change HOME page; in a profile swipe right: back to HOME; Settings → RGB → Touch ring")),
        (T("Tapnięcie", "Tap"), T("otwiera ikonę; w profilu akcja tap (np. wycisz, odtwarzaj); przyciski na ekranie (LIKE, CAM, RESET)",
                                  "opens an icon; in a profile the tap action (e.g. mute, play); on-screen buttons (LIKE, CAM, RESET)")),
        (T("Obrzeże ekranu", "Screen rim"), T("Smart Touch Ring: przesuwaj palec po krawędzi ekranu jak po pierścieniu - dokładna regulacja",
                                              "Smart Touch Ring: slide your finger around the edge like a ring - fine control")),
    ]
    h.append(f"<section><h2>2. {S['controls']}</h2><table class='controls'>" +
             "".join(f"<tr><td class='k'>{a}</td><td>{b}</td></tr>" for a, b in rows) + "</table>" +
             f"<p class='small'>{T('Ekran przyciemnia się po 45 s bezczynności (albo pokazuje zegar). Pierwszy obrót, klik lub dotyk tylko go budzi.', 'The screen dims after 45 s of inactivity (or shows the clock). The first turn, click or touch only wakes it.')}</p></section>")
    # ---- 3. home
    pages = [("EVERYDAY", "Pomodoro, Smart Scroll, Volume, Media, Spotify, Meetings"),
             ("CREATIVE", "DaVinci, Premiere, Final Cut, Photoshop, Lightroom, Pixelmator"),
             ("DESIGN", "Figma, Blender, Fusion 360, KiCad"), ("SYSTEM", "System, Settings"),
             ("CUSTOM / CUSTOM 2", T("Twoje profile z konfiguratora (do 12)", "your profiles from the configurator (up to 12)"))]
    h.append(f"<section><h2>3. {S['home']}</h2><p>{T('Ikony są ułożone na stronach. Kropki na dole pokazują bieżącą stronę. Obrót przechodzi przez wszystkie ikony na wszystkich stronach.', 'Icons are arranged in pages. The dots at the bottom show the current page. Turning walks through all icons on all pages.')}</p><table>" +
             "".join(f"<tr><td class='k'>{a}</td><td>{b}</td></tr>" for a, b in pages) + "</table>" +
             f"<p>{T('Niepotrzebne ikony ukryjesz w konfiguratorze (rozdział 10). Kropka przy ikonie oznacza aktywny profil.', 'Hide apps you do not use in the configurator (chapter 10). A dot next to an icon marks the active profile.')}</p></section>")
    # ---- 4. profile
    t = [f"<section><h2>4. {S['profiles']}</h2><p>{T('W profilu klik zmienia tryb (kropki pod nazwą pokazują, który). Obrót wykonuje akcję trybu, tapnięcie w środek - akcję tap. ', 'In a profile a click changes the mode (the dots under the name show which). Turning performs the mode action, tapping the center performs the tap action. ')}" +
         f"{k('Mod')} = {T('Ctrl w Windows, Cmd w macOS (Settings → Host OS).', 'Ctrl on Windows, Cmd on macOS (Settings → Host OS).')}</p>"]
    t.append(f"<table><tr><th>{T('Profil', 'Profile')}</th><th>{T('Tryb', 'Mode')}</th><th>{T('Obrót', 'Turn')}</th></tr>")
    for name, modes, tap in PROFILES:
        n = len(modes) + 1
        for i, (mn, mpl, men) in enumerate(modes):
            t.append("<tr>" + (f"<td class='k' rowspan='{n}'>{name}</td>" if i == 0 else "") +
                     f"<td>{mn}</td><td>{mpl if pl else men}</td></tr>")
        t.append(f"<tr><td><i>tap</i></td><td>{tap[0] if pl else tap[1]}</td></tr>")
    t.append("</table>")
    t.append(f"<p class='small'>* {T('Klawisze jasności działają, jeśli system je obsługuje (zwykle wbudowany ekran laptopa). Final Cut Pro i Pixelmator Pro są tylko na macOS. W Blenderze i Fusion 360 trzymaj kursor nad widokiem 3D.', 'Brightness keys work where the system supports them (usually a built-in laptop display). Final Cut Pro and Pixelmator Pro are macOS only. In Blender and Fusion 360 keep the mouse over the 3D view.')}</p>")
    t.append(f"<h3>Spotify</h3><p>{T('Utwory, głośność i odtwarzanie działają klawiszami multimedialnymi, także gdy Spotify jest w tle. SEEK i LIKE wymagają aktywnego okna Spotify. Z DIAL FT Helper ekran pokazuje okładkę, tytuł, wykonawcę i postęp utworu.', 'Tracks, volume and play/pause use media keys and work even with Spotify in the background. SEEK and LIKE need the Spotify window in front. With DIAL FT Helper the screen shows cover art, title, artist and track progress.')}</p>")
    t.append(f"<h3>Meetings</h3><p>{T('Klikiem wybierz aplikację: ZOOM, TEAMS albo MEET (Google Meet w przeglądarce). Tap wycisza mikrofon - pierścień RGB pulsuje wtedy na czerwono. Przycisk CAM włącza / wyłącza kamerę. DIAL zapamiętuje stan po swojej stronie; jeśli wyciszysz mikrofon myszą, tapnij dwa razy, by stan znów się zgadzał.', 'Click to choose the app: ZOOM, TEAMS or MEET (Google Meet in a browser). Tap mutes the microphone - the RGB ring then pulses red. The CAM button toggles the camera. DIAL keeps track of the state itself; if you mute with the mouse, tap twice to get back in sync.')}</p></section>")
    h.append("".join(t))
    # ---- 5. color
    h.append(f"""<section><h2>5. {S['color']}</h2>
<p>{T('Tryb COLOR w profilu DaVinci steruje prawdziwymi kontrolkami strony Color - bez żadnego programu, także w darmowym DaVinci Resolve.',
      'The COLOR mode in the DaVinci profile controls the real controls on the Color page - no software needed, also in the free DaVinci Resolve.')}</p>
<ol><li>{T('W profilu DaVinci klikaj, aż pojawi się <b>HOVER + TURN</b>.', 'In the DaVinci profile click until <b>HOVER + TURN</b> appears.')}</li>
<li>{T('Najedź myszą na wartość, np. Temp, Tint, Contrast, Saturation albo prążkowany pasek pod kołem Lift / Gamma / Gain / Offset.',
       'Point the mouse at a value, e.g. Temp, Tint, Contrast, Saturation or the ribbed bar under the Lift / Gamma / Gain / Offset wheel.')}</li>
<li>{T('Kręć pokrętłem. Tapnięcie zmienia krok: FINE / NORMAL / COARSE. Obrzeże ekranu daje najmniejsze kroki.',
       'Turn the dial. A tap changes the step: FINE / NORMAL / COARSE. The screen rim gives the smallest steps.')}</li></ol>
<div class="warn">{T('Obrót działa jak przeciąganie myszą. Kursor musi stać nad wartością - nad innym elementem (np. węzłem) przesunąłby go.',
                     'Turning works like dragging with the mouse. Keep the pointer over a value - over another item (e.g. a node) it would move it.')}</div></section>""")
    # ---- 6. ring
    h.append(f"""<section><h2>6. {S['ring']}</h2>
<p>{T('<b>Smart Touch Ring</b>: przesuwaj palec po krawędzi ekranu. Działa jak drugie, dokładniejsze pokrętło. Ustawienia dla każdego profilu osobno: <b>Settings → Touch ring</b> (albo przesuń w lewo z ekranu RGB ring).',
      '<b>Smart Touch Ring</b>: slide your finger around the screen edge. It works like a second, finer dial. Settings per profile: <b>Settings → Touch ring</b> (or swipe left from the RGB ring screen).')}</p>
<table><tr><td class="k">Touch mode</td><td>{T('SMOOTH (płynnie) albo STEP (kroki)', 'SMOOTH or STEP')}</td></tr>
<tr><td class="k">Sensitivity</td><td>LOW / MEDIUM / HIGH</td></tr>
<tr><td class="k">Inertia</td><td>{T('bezwładność po machnięciu: OFF / LOW / MEDIUM / HIGH', 'momentum after a flick: OFF / LOW / MEDIUM / HIGH')}</td></tr>
<tr><td class="k">Direction</td><td>NORMAL / REVERSE</td></tr>
<tr><td class="k">Adaptive</td><td>{T('szybszy ruch = większe zmiany', 'faster movement = bigger changes')}</td></tr>
<tr><td class="k">Touch ring</td><td>{T('włącz / wyłącz w tym profilu', 'on / off for this profile')}</td></tr></table>
<p>{T('<b>Smart Scroll</b>: przewijanie z przyspieszeniem (SMART) albo stałe (PRECISION), bezwładność i oś pionowa / pozioma (klik). Zmiana kierunku obrotu od razu zatrzymuje bezwładność; dotknięcie ekranu też.',
      '<b>Smart Scroll</b>: scrolling with acceleration (SMART) or constant (PRECISION), momentum and vertical / horizontal axis (click). Reversing the direction stops the momentum immediately; so does touching the screen.')}</p></section>""")
    # ---- 7. pomodoro
    h.append(f"""<section><h2>7. {S['pomo']}</h2>
<p>{T('<b>Pomodoro</b>: obrót ustawia czas skupienia (gdy licznik stoi), klik lub tap - start / pauza, RESET - od nowa. Przerwę ustawisz w Settings → Pomo break. Po zakończeniu ekran mruga, a pierścień błyska na biało. Licznik działa dalej, gdy używasz innych profili.',
      '<b>Pomodoro</b>: turn to set the focus time (when stopped), click or tap to start / pause, RESET to start over. Set the break in Settings → Pomo break. When a period ends the screen blinks and the ring flashes white. The timer keeps running while you use other profiles.')}</p>
<p>{T('<b>Statystyki</b>: liczba sesji dzisiaj i w ostatnich 7 dniach (gdy DIAL zna godzinę - patrz niżej).',
      '<b>Statistics</b>: sessions today and in the last 7 days (when DIAL knows the time - see below).')}</p>
<p>{T('<b>Przypomnienie o przerwie</b>: po 50 minutach pracy bez 5 minut przerwy DIAL pokazuje „TIME TO STAND UP”. Dowolny ruch je zamyka. Wyłączysz w Settings → Stand-up.',
      '<b>Stand-up reminder</b>: after 50 minutes of work without a 5-minute break DIAL shows “TIME TO STAND UP”. Any input dismisses it. Turn it off in Settings → Stand-up.')}</p>
<p>{T('<b>Tarcza zegarka</b>: w spoczynku DIAL pokazuje godzinę, datę, sekundnik, Pomodoro i tytuł utworu. DIAL nie ma zegara z baterią - godzinę ustawia konfigurator (przy połączeniu) albo DIAL FT Helper (na stałe). Bez tego ekran po prostu się przyciemnia.',
      '<b>Clock face</b>: when idle DIAL shows the time, date, seconds ring, Pomodoro and the song title. DIAL has no battery clock - the time is set by the configurator (when connected) or by DIAL FT Helper (permanently). Without it the screen simply dims.')}</p></section>""")
    # ---- 8. rgb
    h.append(f"""<section><h2>8. {S['rgb']}</h2>
<table><tr><td class="k">Mode</td><td>{T('Off, Follow (światło podąża za pokrętłem), Ambient, Breathe, Rainbow', 'Off, Follow (light follows the dial), Ambient, Breathe, Rainbow')}</td></tr>
<tr><td class="k">Color</td><td>{T('kolor motywu, 36 odcieni albo biały - zapamiętywany osobno dla każdego motywu', 'theme color, 36 hues or white - remembered separately for each theme')}</td></tr>
<tr><td class="k">Brightness</td><td>20-100%</td></tr>
<tr><td class="k">Pomodoro</td><td>{T('pierścień pokazuje postęp Pomodoro', 'the ring shows Pomodoro progress')}</td></tr>
<tr><td class="k">Action flash</td><td>{T('krótki błysk przy akcjach', 'short flash on actions')}</td></tr>
<tr><td class="k">Idle off</td><td>{T('gaśnie razem z ekranem', 'turns off with the screen')}</td></tr>
<tr><td class="k">Direction</td><td>{T('kierunek światła względem pokrętła', 'light direction relative to the dial')}</td></tr></table>
<p>{T('Czerwony puls oznacza wyciszony mikrofon w profilu Meetings.', 'A red pulse means the microphone is muted in the Meetings profile.')}</p></section>""")
    # ---- 9. ustawienia
    h.append(f"<section><h2>9. {S['settings']}</h2><table>" +
             "".join(f"<tr><td class='k'>{a}</td><td>{b if pl else c}</td></tr>" for a, b, c in SETTINGS_ROWS) +
             f"</table><p class='small'>{T('Zmiany zapisują się automatycznie w pamięci DIAL.', 'Changes are saved automatically in DIAL.')}</p></section>")
    # ---- 10. konfigurator
    h.append(f"""<section><h2>10. {S['config']}</h2>
<p>{T(f'Konfigurator to strona <b>{CONFIG_URL}</b>. Działa w przeglądarce Google Chrome albo Microsoft Edge na komputerze - nic nie trzeba instalować. Wszystko zapisuje się w DIAL, więc działa potem na każdym komputerze.',
      f'The configurator is the page <b>{CONFIG_URL}</b>. It works in Google Chrome or Microsoft Edge on a computer - nothing to install. Everything is saved in DIAL, so it then works on any computer.')}</p>
<ol><li>{T('Podłącz DIAL FT i kliknij <b>Connect DIAL FT</b>. W okienku wybierz DIAL FT.', 'Plug in DIAL FT and click <b>Connect DIAL FT</b>. Choose DIAL FT in the window.')}</li>
<li>{T('<b>General</b>: motyw, system, jasność, pierścień RGB, Pomodoro, automatyczny profil, zegar, przypomnienie.', '<b>General</b>: theme, system, brightness, RGB ring, Pomodoro, auto profile, clock, reminder.')}</li>
<li>{T('<b>Home screen</b>: odznacz aplikacje, których nie używasz.', '<b>Home screen</b>: untick apps you do not use.')}</li>
<li>{T('<b>Twoje profile</b>: do 12 profili dla dowolnych programów, z gotowych szablonów albo od zera.', '<b>Your own profiles</b>: up to 12 profiles for any program, from templates or from scratch.')}</li>
<li>{T('Kliknij <b>Save to DIAL FT</b>.', 'Click <b>Save to DIAL FT</b>.')}</li></ol>
<h3>{T('Własny profil', 'Custom profile')}</h3>
<ul><li>{T('Nazwa, ikona i kolor (widoczny w motywie Color). Szablon („Zacznij od szablonu”) wypełnia profil gotowymi skrótami, np. dla przeglądarki, DaVinci, Premiere, Photoshopa, Lightrooma, Figmy lub VS Code.',
       'Name, icon and color (shown in the Color theme). A template (“Start from template”) fills the profile with ready shortcuts, e.g. for a browser, DaVinci, Premiere, Photoshop, Lightroom, Figma or VS Code.')}</li>
<li>{T('Do 4 trybów. Każdy tryb ma 4 akcje: <b>obrót w lewo</b>, <b>obrót w prawo</b>, <b>tapnięcie</b> i <b>klik pokrętła</b> (pusty klik = następny tryb).',
       'Up to 4 modes. Every mode has 4 actions: <b>turn left</b>, <b>turn right</b>, <b>tap</b> and <b>dial click</b> (empty click = next mode).')}</li>
<li>{T('Każda akcja może być <b>makrem</b> - do 6 kroków po kolei: skrót klawiszowy (kliknij pole i naciśnij skrót), klawisz multimedialny, kółko myszy, pauza, wpisanie tekstu, następny / poprzedni tryb.',
       'Every action can be a <b>macro</b> - up to 6 steps in a row: keyboard shortcut (click the field and press it), media key, mouse wheel, pause, type text, next / previous mode.')}</li>
<li>{T('Dwa teksty na profil (do 32 znaków, bez polskich znaków) do kroku „Wpisz tekst”, np. podpis lub adres e-mail.',
       'Two texts per profile (up to 32 characters, ASCII) for the “Type text” step, e.g. a signature or an e-mail address.')}</li>
<li>{T('<b>Program</b> (opcjonalnie): identyfikator aplikacji do automatycznego profilu, np. com.apple.Safari (macOS) albo chrome.exe (Windows).',
       '<b>Program</b> (optional): app identifier for auto profile, e.g. com.apple.Safari (macOS) or chrome.exe (Windows).')}</li>
<li>{T('<b>Eksportuj / Importuj profile</b>: zapis wszystkich profili do pliku i wczytanie z pliku - kopia zapasowa lub wymiana profili.',
       '<b>Export / Import profiles</b>: save all profiles to a file and load them back - a backup or a way to share profiles.')}</li></ul>
<div class="note">{T('„Ctrl/Cmd” w skrótach jest wysyłane jako Cmd na Macu i Ctrl w Windows - według ustawienia Host OS. Dzięki temu jeden profil działa na obu systemach.',
                     '“Ctrl/Cmd” in shortcuts is sent as Cmd on Mac and Ctrl on Windows - following the Host OS setting. One profile works on both systems.')}</div></section>""")
    # ---- 11. aktualizacja
    h.append(f"""<section><h2>11. {S['update']}</h2>
<p>{T(f'Aktualizacje instalujesz na tej samej stronie ({CONFIG_URL}), w sekcji Firmware update. Ustawienia i własne profile zostają.',
      f'Install updates on the same page ({CONFIG_URL}), in the Firmware update section. Settings and custom profiles are kept.')}</p>
<ol><li>{T('Kliknij <b>1. Prepare</b> i wybierz DIAL FT. Ekran gaśnie - DIAL jest w trybie aktualizacji.', 'Click <b>1. Prepare</b> and choose DIAL FT. The screen goes dark - DIAL is in update mode.')}</li>
<li>{T('Kliknij <b>2. Install update</b> i wybierz port „USB JTAG/serial debug unit”.', 'Click <b>2. Install update</b> and choose the port “USB JTAG/serial debug unit”.')}</li>
<li>{T('Po komunikacie o zakończeniu odłącz i podłącz DIAL FT.', 'When it reports success, unplug and plug in DIAL FT.')}</li></ol>
<p class="small">{T('Jeśli DIAL nie przechodzi w tryb aktualizacji: odłącz kabel, przytrzymaj przycisk BOOT z tyłu płytki, podłącz kabel, puść przycisk i zacznij od kroku 2.',
                    'If DIAL does not enter update mode: unplug the cable, hold the BOOT button on the back of the board, plug the cable in, release the button and start from step 2.')}</p></section>""")
    # ---- 12. helper
    h.append(f"""<section><h2>12. {S['helper']}</h2>
<p>{T('Mały program działający w tle. Pobierzesz go z konfiguratora. Dodaje:', 'A small background program. Download it from the configurator. It adds:')}</p>
<ul><li>{T('okładkę, tytuł, wykonawcę i postęp utworu Spotify na ekranie DIAL,', 'Spotify cover art, title, artist and progress on the DIAL screen,')}</li>
<li>{T('automatyczną zmianę profilu, gdy przełączasz aplikacje (DaVinci, Premiere, Final Cut, Photoshop, Lightroom, Pixelmator, Spotify, Zoom, Teams, Figma, Blender, Fusion 360, KiCad oraz Twoje profile),',
       'automatic profile switching when you change apps (DaVinci, Premiere, Final Cut, Photoshop, Lightroom, Pixelmator, Spotify, Zoom, Teams, Figma, Blender, Fusion 360, KiCad and your profiles),')}</li>
<li>{T('godzinę dla tarczy zegarka i statystyk Pomodoro.', 'the time for the clock face and Pomodoro statistics.')}</li></ul>
<p>{T('Instalacja: rozpakuj archiwum i uruchom <span class="key">install_mac.sh</span> w Terminalu. Helper startuje sam po zalogowaniu. Przy pierwszym utworze macOS zapyta o zgodę na sterowanie Spotify - kliknij Zezwól. Odinstalowanie: <span class="key">install_mac.sh --uninstall</span>.',
      'Install: unzip the archive and run <span class="key">install_mac.sh</span> in Terminal. The helper starts automatically at login. On the first song macOS asks for permission to control Spotify - click Allow. Uninstall: <span class="key">install_mac.sh --uninstall</span>.')}</p>
<p class="small">{T('Automatyczny profil nie przełącza, gdy jesteś w Ustawieniach lub w Pomodoro. Wyłączysz go w Settings → Auto profile.', 'Auto profile does not switch while you are in Settings or Pomodoro. Turn it off in Settings → Auto profile.')}</p></section>""")
    # ---- 13. problemy
    tr = [
        (T("Skróty nie działają na Macu (albo w Windows)", "Shortcuts do not work on Mac (or Windows)"),
         T("Ustaw Settings → Host OS zgodnie z komputerem.", "Set Settings → Host OS to match your computer.")),
        (T("Przewijanie w złą stronę", "Scrolling goes the wrong way"),
         T("Settings → Scroll dir albo tapnięcie w profilu Smart Scroll.", "Settings → Scroll dir or a tap in the Smart Scroll profile.")),
        (T("Brak zegara w spoczynku", "No clock when idle"),
         T("Połącz konfigurator albo zainstaluj DIAL FT Helper (DIAL nie ma zegara z baterią).", "Connect the configurator or install DIAL FT Helper (DIAL has no battery clock).")),
        (T("COLOR przesuwa coś innego", "COLOR moves something else"),
         T("Kursor musi stać nad wartością w Resolve, nie nad węzłem ani klipem.", "Keep the pointer over a value in Resolve, not over a node or clip.")),
        (T("Nie widać okładki Spotify", "No Spotify cover art"),
         T("Uruchom DIAL FT Helper i zezwól mu na sterowanie Spotify (Ustawienia systemowe → Prywatność → Automatyzacja).", "Run DIAL FT Helper and allow it to control Spotify (System Settings → Privacy → Automation).")),
        (T("Konfigurator nie widzi DIAL", "The configurator cannot see DIAL"),
         T("Użyj Chrome lub Edge, zamknij inne programy korzystające z portu DIAL, podłącz kabel ponownie.", "Use Chrome or Edge, close other programs using the DIAL port, reconnect the cable.")),
        (T("DIAL nie reaguje", "DIAL does not respond"),
         T("Odłącz i podłącz kabel USB. Ustawienia nie zostaną utracone.", "Unplug and plug in the USB cable. Settings are not lost.")),
    ]
    h.append(f"<section><h2>13. {S['trouble']}</h2><table><tr><th>{T('Problem', 'Problem')}</th><th>{T('Rozwiązanie', 'Solution')}</th></tr>" +
             "".join(f"<tr><td>{a}</td><td>{b}</td></tr>" for a, b in tr) + "</table></section>")
    # ---- 14. dane
    h.append(f"""<section><h2>14. {S['specs']}</h2>
<div class="grid2"><table>
<tr><td class="k">{T('Ekran', 'Display')}</td><td>{T('okrągły 1,28″, 240×240, dotykowy', 'round 1.28″, 240×240, touch')}</td></tr>
<tr><td class="k">{T('Sterowanie', 'Input')}</td><td>{T('pokrętło z klikiem, ekran dotykowy', 'dial with click, touchscreen')}</td></tr>
<tr><td class="k">{T('Podświetlenie', 'Lighting')}</td><td>{T('pierścień 5 diod RGB', 'ring of 5 RGB LEDs')}</td></tr>
<tr><td class="k">{T('Procesor', 'Processor')}</td><td>ESP32-S3</td></tr>
<tr><td class="k">{T('Złącze', 'Connector')}</td><td>{T('USB-C, zasilanie i dane', 'USB-C, power and data')}</td></tr>
<tr><td class="k">{T('Systemy', 'Systems')}</td><td>macOS, Windows, Linux</td></tr>
<tr><td class="k">{T('Obudowa', 'Housing')}</td><td>{T('podstawa z drewna, metalowy pierścień', 'wooden base, metal ring')}</td></tr>
<tr><td class="k">Firmware</td><td>{VERSION}</td></tr></table>
<div><ul>
<li>{T('Używaj tylko w pomieszczeniach, chroń przed wodą i wilgocią.', 'Indoor use only; keep away from water and moisture.')}</li>
<li>{T('Drewno czyść suchą lub lekko wilgotną ściereczką, ekran - miękką ściereczką bez środków chemicznych.', 'Clean the wood with a dry or slightly damp cloth and the screen with a soft cloth, no chemicals.')}</li>
<li>{T('Nie otwieraj obudowy i nie zginaj kabla przy wtyczce.', 'Do not open the housing or bend the cable at the plug.')}</li>
<li>{T('Zużytego urządzenia nie wyrzucaj z odpadami domowymi - oddaj je do punktu zbiórki elektroniki.', 'Do not dispose of the device with household waste - take it to an electronics collection point.')}</li>
</ul><p class="small">FalbaTech &middot; {SITE}</p></div></div></section>""")
    return "\n".join(h)


def build(L):
    html = f"""<!doctype html><html lang="{TXT[L]['lang']}"><head><meta charset="utf-8">
<title>DIAL FT - {TXT[L]['title']}</title><style>{CSS}</style></head><body>{body(L)}</body></html>"""
    src = os.path.join(HERE, TXT[L]["file"] + ".html")
    with open(src, "w", encoding="utf-8") as f:
        f.write(html)
    out = os.path.join(DOCS, TXT[L]["file"] + ".pdf")
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--print-to-pdf=" + out,
                    "file://" + src], check=True, capture_output=True, timeout=120)
    print("ok", out)


if __name__ == "__main__":
    for lang in (sys.argv[1:] or ["pl", "en"]):
        build(lang)
