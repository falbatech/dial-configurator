/* DIAL FT Configurator - komponent <dialft-configurator> (FalbaTech).
 *
 * Dziala w Chrome / Edge na komputerze (Web Serial). Shadow DOM: style sklepu (np. Ecwid) nie mieszaja sie z konfiguratorem.
 * Atrybuty (wszystkie opcjonalne):
 *   theme="light|dark"        wyglad (domyslnie light)
 *   lang="auto|pl|en"         jezyk (auto = jezyk strony, potem przegladarki)
 *   manifest="URL"            manifest aktualizacji ESP Web Tools; bez niego sekcja aktualizacji jest ukryta
 *   helper="URL"              paczka DIAL FT Helper (macOS)
 *   guide-pl="URL" guide-en="URL"   instrukcje PDF
 */
(function () {
  "use strict";
  if (customElements.get("dialft-configurator")) return;

  const ESP_WEB_TOOLS = "https://unpkg.com/esp-web-tools@10.0.1/dist/web/install-button.js?module";

  // ---------------------------------------------------------------- tlumaczenia
  const I18N = {
    en: {
      title: "DIAL FT Configurator", sub: "Settings, your own profiles and updates - right in the browser",
      connect: "Connect DIAL FT", disconnect: "Disconnect", notConnected: "not connected", connecting: "connecting…",
      connected: "connected", saved: "saved", failed: "connection failed", notDial: "This device did not answer as DIAL FT.",
      oldFw: "DIAL FT did not send its settings - please update the firmware below.", lost: "DIAL FT disconnected",
      noSupportT: "Open this page in Chrome or Edge on a computer",
      noSupport: "Talking to DIAL FT over USB works in Google Chrome or Microsoft Edge on macOS, Windows or Linux. Safari, Firefox and phones cannot do it.",
      introT: "Welcome", intro: "Plug in DIAL FT with the USB cable and click <b>Connect DIAL FT</b>. In the window that opens choose <b>DIAL FT</b>. All changes are saved in the device, so they work on every computer without this page.",
      general: "General", firmware: "Device firmware", theme: "Theme", computer: "Computer", winOpt: "Windows / Linux (Ctrl)", macOpt: "macOS (Cmd)",
      bri: "Screen brightness", rgb: "RGB ring", rgbBri: "RGB brightness", focus: "Pomodoro focus (min)", brk: "Pomodoro break (min)",
      auto: "Auto profile (with DIAL FT Helper)", clock: "Clock face when idle", standup: "Stand-up reminder", inv: "Invert scroll",
      home: "Home screen", homeHint: "Untick apps you do not use - their icons disappear from DIAL FT. Settings always stays.",
      profiles: "Your own profiles",
      profilesHint: "Make DIAL FT work with any program. Each profile has up to 4 modes (click the dial on the device to switch). For every mode choose what turning <b>left</b> and <b>right</b> does. Click a key field and press the shortcut on your keyboard. <b>Ctrl/Cmd</b> is sent as Cmd on Mac and Ctrl on Windows (set \"Computer\" above). Profiles appear on the CUSTOM page.",
      profile: "Profile", name: "Name (12)", icon: "Icon", color: "Color", program: "Program (for auto profile, optional)",
      programPh: "e.g. com.apple.Safari or chrome.exe", modeName: "Mode name", left: "Turn left", right: "Turn right", addMode: "+ Add mode",
      removeMode: "Remove mode", tap: "Tap on screen:", clickKeys: "click & press keys", pressKeys: "press keys…", keyNo: "key not supported",
      notches: "notches", none: "—",
      actNone: "Nothing", actKey: "Key", actMedia: "Media", actWheel: "Scroll ↕", actPan: "Scroll ↔",
      actDelay: "Pause", actText: "Type text", actNext: "Next mode", actPrev: "Previous mode", ms: "ms",
      aLeft: "Turn left", aRight: "Turn right", aTap: "Tap screen", aClick: "Click dial", clickDefault: "next mode (default)",
      addStep: "+ step", empty: "nothing", texts: "Texts for “Type text” (ASCII)", text1: "Text 1", text2: "Text 2",
      template: "Start from template…", templateAsk: "Replace this profile with the template?",
      exportB: "Export profiles", importB: "Import profiles", importErr: "This file is not a DIAL FT profile export.",
      macroNote: "Update DIAL FT to DFTv1.3 or newer to use macros, tap and click per mode.",
      seqHint: "Each action can be a macro: up to {n} steps run one after another.",
      clockT: "Clock", clockHint: "DIAL FT has no battery clock. This page sets the time when you connect; DIAL FT Helper keeps it updated.",
      setClock: "Set clock now", clockSet: "Clock set to",
      updT: "Firmware update", updHint: "Your settings and profiles stay on the device.",
      upd1: "Click <b>1. Prepare</b> and choose DIAL FT. The screen goes dark: it is now in update mode.",
      upd2: "Click <b>2. Install update</b> and choose the port that appears (<i>USB JTAG/serial debug unit</i>).",
      upd3: "When it says it is done, unplug and plug in DIAL FT again.",
      prepare: "1. Prepare", install: "2. Install update", available: "Available:", prepared: "DIAL FT is in update mode. Now click 2. Install update.",
      cancelled: "Cancelled.",
      helperT: "DIAL FT Helper (optional, macOS)", helperHint: "A small background program that adds Spotify cover art and song title, automatic profile when you switch apps, and the clock. Everything else works without it.",
      download: "Download for macOS", guideT: "User guide", guideHint: "Controls, every profile, the configurator, updates and troubleshooting.",
      noChanges: "All changes saved", unsaved: "Unsaved changes", undo: "Undo", save: "Save to DIAL FT",
      icons: ["Speaker", "Arrows", "Color wheels", "Chip", "Timer", "Sliders", "Brush", "Photo", "Media", "Arrows (horizontal)", "Pixel grid", "Spotify", "Microphone", "Monitor", "Film", "Clapper", "Figma dots", "Orbit", "Cube", "Star"],
      media: ["Play / Pause", "Next track", "Previous track", "Volume up", "Volume down", "Mute", "Brightness up", "Brightness down"],
    },
    pl: {
      title: "Konfigurator DIAL FT", sub: "Ustawienia, własne profile i aktualizacje - prosto w przeglądarce",
      connect: "Połącz DIAL FT", disconnect: "Rozłącz", notConnected: "nie połączono", connecting: "łączenie…",
      connected: "połączono", saved: "zapisano", failed: "nie udało się połączyć", notDial: "To urządzenie nie odpowiada jak DIAL FT.",
      oldFw: "DIAL FT nie wysłał ustawień - zaktualizuj oprogramowanie poniżej.", lost: "DIAL FT odłączony",
      noSupportT: "Otwórz tę stronę w Chrome lub Edge na komputerze",
      noSupport: "Połączenie z DIAL FT przez USB działa w Google Chrome lub Microsoft Edge na macOS, Windows lub Linux. Safari, Firefox i telefony tego nie potrafią.",
      introT: "Witaj", intro: "Podłącz DIAL FT kablem USB i kliknij <b>Połącz DIAL FT</b>. W okienku wybierz <b>DIAL FT</b>. Zmiany zapisują się w urządzeniu, więc działają na każdym komputerze, także bez tej strony.",
      general: "Ogólne", firmware: "Oprogramowanie urządzenia", theme: "Motyw", computer: "Komputer", winOpt: "Windows / Linux (Ctrl)", macOpt: "macOS (Cmd)",
      bri: "Jasność ekranu", rgb: "Pierścień RGB", rgbBri: "Jasność RGB", focus: "Pomodoro - skupienie (min)", brk: "Pomodoro - przerwa (min)",
      auto: "Automatyczny profil (z DIAL FT Helper)", clock: "Zegar w spoczynku", standup: "Przypomnienie o przerwie", inv: "Odwróć przewijanie",
      home: "Ekran HOME", homeHint: "Odznacz aplikacje, których nie używasz - ich ikony znikną z DIAL FT. Ustawienia zostają zawsze.",
      profiles: "Twoje profile",
      profilesHint: "DIAL FT może współpracować z dowolnym programem. Każdy profil ma do 4 trybów (zmieniasz je klikiem pokrętła). Dla każdego trybu wybierz, co robi obrót w <b>lewo</b> i w <b>prawo</b>. Kliknij pole klawisza i naciśnij skrót na klawiaturze. <b>Ctrl/Cmd</b> jest wysyłane jako Cmd na Macu i Ctrl w Windows (ustawienie „Komputer” wyżej). Profile pojawiają się na stronie CUSTOM.",
      profile: "Profil", name: "Nazwa (12)", icon: "Ikona", color: "Kolor", program: "Program (do automatycznego profilu, opcjonalnie)",
      programPh: "np. com.apple.Safari albo chrome.exe", modeName: "Nazwa trybu", left: "Obrót w lewo", right: "Obrót w prawo", addMode: "+ Dodaj tryb",
      removeMode: "Usuń tryb", tap: "Tapnięcie ekranu:", clickKeys: "kliknij i naciśnij skrót", pressKeys: "naciśnij klawisze…", keyNo: "klawisz nieobsługiwany",
      notches: "ząbki", none: "—",
      actNone: "Nic", actKey: "Klawisz", actMedia: "Multimedia", actWheel: "Kółko ↕", actPan: "Kółko ↔",
      actDelay: "Pauza", actText: "Wpisz tekst", actNext: "Następny tryb", actPrev: "Poprzedni tryb", ms: "ms",
      aLeft: "Obrót w lewo", aRight: "Obrót w prawo", aTap: "Tapnięcie", aClick: "Klik pokrętła", clickDefault: "następny tryb (domyślnie)",
      addStep: "+ krok", empty: "nic", texts: "Teksty do kroku „Wpisz tekst” (bez polskich znaków)", text1: "Tekst 1", text2: "Tekst 2",
      template: "Zacznij od szablonu…", templateAsk: "Zastąpić ten profil szablonem?",
      exportB: "Eksportuj profile", importB: "Importuj profile", importErr: "To nie jest plik z profilami DIAL FT.",
      macroNote: "Zaktualizuj DIAL FT do DFTv1.3 lub nowszego, aby używać makr oraz tapnięcia i kliku w każdym trybie.",
      seqHint: "Każda akcja może być makrem: do {n} kroków wykonywanych po kolei.",
      clockT: "Zegar", clockHint: "DIAL FT nie ma zegara z baterią. Ta strona ustawia godzinę przy połączeniu, a DIAL FT Helper aktualizuje ją na bieżąco.",
      setClock: "Ustaw zegar teraz", clockSet: "Ustawiono godzinę",
      updT: "Aktualizacja oprogramowania", updHint: "Ustawienia i Twoje profile zostają w urządzeniu.",
      upd1: "Kliknij <b>1. Przygotuj</b> i wybierz DIAL FT. Ekran zgaśnie - urządzenie jest w trybie aktualizacji.",
      upd2: "Kliknij <b>2. Zainstaluj aktualizację</b> i wybierz port, który się pojawi (<i>USB JTAG/serial debug unit</i>).",
      upd3: "Po komunikacie o zakończeniu odłącz i podłącz DIAL FT.",
      prepare: "1. Przygotuj", install: "2. Zainstaluj aktualizację", available: "Dostępna:", prepared: "DIAL FT jest w trybie aktualizacji. Kliknij 2. Zainstaluj aktualizację.",
      cancelled: "Anulowano.",
      helperT: "DIAL FT Helper (opcjonalny, macOS)", helperHint: "Mały program w tle: okładki i tytuły Spotify, automatyczna zmiana profilu przy przełączaniu aplikacji oraz zegar. Wszystko inne działa bez niego.",
      download: "Pobierz na macOS", guideT: "Instrukcja obsługi", guideHint: "Sterowanie, wszystkie profile, konfigurator, aktualizacje i rozwiązywanie problemów.",
      noChanges: "Wszystko zapisane", unsaved: "Niezapisane zmiany", undo: "Cofnij", save: "Zapisz w DIAL FT",
      icons: ["Głośnik", "Strzałki", "Koła kolorów", "Układ scalony", "Minutnik", "Suwaki", "Pędzel", "Zdjęcie", "Multimedia", "Strzałki (poziomo)", "Siatka pikseli", "Spotify", "Mikrofon", "Monitor", "Taśma filmowa", "Klaps", "Kropki Figma", "Orbita", "Sześcian", "Gwiazda"],
      media: ["Odtwarzaj / pauza", "Następny utwór", "Poprzedni utwór", "Głośniej", "Ciszej", "Wycisz", "Jaśniej", "Ciemniej"],
    },
  };
  const MEDIA_CODES = [0xCD, 0xB5, 0xB6, 0xE9, 0xEA, 0xE2, 0x6F, 0x70];
  // szablony profili (kroki [typ, modyfikatory, kod]); M = Ctrl/Cmd wg ustawienia "Komputer"
  const K = (code, m = 0) => [[1, m, code]], M = 1, SH = 2, AL = 4;
  const LEFT = 0xD8, RIGHT = 0xD7, UP = 0xDA, DOWN = 0xD9, TAB = 0xB3, PGUP = 0xD3, PGDN = 0xD6, SPACE = 32;
  const TEMPLATES = [
    { name: "Browser", icon: 13, color: "#4285f4", app: "com.google.Chrome", modes: [
      { name: "TABS", l: K(TAB, M | SH), r: K(TAB, M), tap: K(116, M) },
      { name: "SCROLL", l: [[3, 0, -2]], r: [[3, 0, 2]], tap: K(SPACE) },
      { name: "ZOOM", l: K(45, M), r: K(61, M), tap: K(48, M) },
      { name: "HISTORY", l: K(91, M), r: K(93, M), tap: K(114, M) }] },
    { name: "DaVinci edit", icon: 2, color: "#ff8c28", app: "com.blackmagic-design.DaVinciResolve", modes: [
      { name: "FRAME", l: K(LEFT), r: K(RIGHT), tap: K(SPACE) },
      { name: "ZOOM", l: K(45, M), r: K(61, M), tap: K(122, SH) },
      { name: "EDIT", l: K(UP), r: K(DOWN), tap: K(98, M) },
      { name: "JKL", l: K(106), r: K(108), tap: K(107) }] },
    { name: "Premiere", icon: 14, color: "#9999ff", app: "com.adobe.PremierePro", modes: [
      { name: "FRAME", l: K(LEFT), r: K(RIGHT), tap: K(SPACE) },
      { name: "ZOOM", l: K(45), r: K(61), tap: K(92) },
      { name: "EDIT", l: K(UP), r: K(DOWN), tap: K(107, M) },
      { name: "JKL", l: K(106), r: K(108), tap: K(107) }] },
    { name: "Photoshop", icon: 6, color: "#31a8ff", app: "com.adobe.Photoshop", modes: [
      { name: "BRUSH", l: K(91), r: K(93), tap: K(120) },
      { name: "ZOOM", l: K(45, M), r: K(61, M), tap: K(48, M) },
      { name: "HISTORY", l: K(122, M), r: K(122, M | SH), tap: K(115, M) },
      { name: "LAYER", l: K(91, AL), r: K(93, AL), tap: K(106, M) }] },
    { name: "Lightroom", icon: 7, color: "#40e0d0", app: "com.adobe.LightroomClassic", modes: [
      { name: "ADJUST", l: K(0xDE), r: K(0xDF), tap: K(92) },
      { name: "SLIDER", l: K(44), r: K(46), tap: K(92) },
      { name: "PHOTO", l: K(LEFT, M), r: K(RIGHT, M), tap: K(112) },
      { name: "RATING", l: K(48), r: K(53), tap: K(117) }] },
    { name: "Figma", icon: 16, color: "#f24e1e", app: "com.figma.Desktop", modes: [
      { name: "ZOOM", l: K(45, M), r: K(61, M), tap: K(49, SH) },
      { name: "LAYER", l: K(TAB, SH), r: K(TAB), tap: K(103, M | AL) },
      { name: "HISTORY", l: K(122, M), r: K(122, M | SH), tap: K(100, M) }] },
    { name: "VS Code", icon: 3, color: "#007acc", app: "com.microsoft.VSCode", modes: [
      { name: "TABS", l: K(PGUP, M), r: K(PGDN, M), tap: K(112, M | SH) },
      { name: "LINES", l: K(UP, AL), r: K(DOWN, AL), tap: K(47, M) },
      { name: "ZOOM", l: K(45, M), r: K(61, M), tap: K(98, M) },
      { name: "HISTORY", l: K(122, M), r: K(122, M | SH), tap: K(115, M) }] },
    { name: "Signature", icon: 19, color: "#518751", app: "", texts: ["Best regards,", "Your name"], modes: [
      { name: "TYPE", l: [[6, 0, 0]], r: [[6, 0, 0], [1, 0, 0xB0], [6, 0, 1]], tap: [[6, 0, 0], [1, 0, 0xB0], [6, 0, 1]] }] },
  ];
  // KeyboardEvent.code -> kod klawisza Arduino USBHIDKeyboard
  const KEYMAP = (() => {
    const m = {};
    for (let i = 0; i < 26; i++) m["Key" + String.fromCharCode(65 + i)] = 97 + i;
    for (let i = 0; i < 10; i++) m["Digit" + i] = 48 + i;
    Object.assign(m, { Minus: 45, Equal: 61, BracketLeft: 91, BracketRight: 93, Backslash: 92, Semicolon: 59, Quote: 39, Comma: 44,
      Period: 46, Slash: 47, Backquote: 96, Space: 32, Enter: 0xB0, NumpadEnter: 0xE0, Escape: 0xB1, Backspace: 0xB2, Tab: 0xB3,
      Insert: 0xD1, Delete: 0xD4, Home: 0xD2, End: 0xD5, PageUp: 0xD3, PageDown: 0xD6, ArrowUp: 0xDA, ArrowDown: 0xD9,
      ArrowLeft: 0xD8, ArrowRight: 0xD7, NumpadAdd: 0xDF, NumpadSubtract: 0xDE, NumpadMultiply: 0xDD, NumpadDivide: 0xDC,
      NumpadDecimal: 0xEB, Numpad0: 0xEA });
    for (let i = 1; i <= 9; i++) m["Numpad" + i] = 0xE0 + i;
    for (let i = 1; i <= 12; i++) m["F" + i] = 0xC1 + i;
    for (let i = 13; i <= 24; i++) m["F" + i] = 0xF0 + (i - 13);
    return m;
  })();
  const KEYNAME = Object.fromEntries(Object.entries(KEYMAP).map(([k, v]) => [v, k.replace("Arrow", "").replace("Numpad", "Num ")]));
  const MODS = [[1, "Ctrl/Cmd"], [8, "Ctrl"], [16, "Win/Cmd"], [4, "Alt/Opt"], [2, "Shift"]];

  const CSS = `
  :host { all: initial; display: block; font: 15px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
  .wrap { --acc: #518751; --acc2: #3f6d3f; --accbg: #eaf3ea; --accdk: #cfe4cf;
          --bg: #ffffff; --panel: #ffffff; --panel2: #f5f6f5; --line: #e2e5e2; --text: #1d1f1d; --muted: #6b716b; --err: #c8372f; --ok: #3f7d3f;
          background: var(--bg); color: var(--text); padding: 4px 0 70px; position: relative; }
  .wrap.dark { --acc: #5a9a5a; --acc2: #8fca8f; --accbg: #1f2d1f; --accdk: #1d3a1d;
          --bg: #0d0d0f; --panel: #17171a; --panel2: #1f1f23; --line: #2c2c31; --text: #ececef; --muted: #9a9aa3; --err: #ff6b62; --ok: #8fca8f; }
  * { box-sizing: border-box; }
  .top { display: flex; align-items: center; gap: 14px; padding: 14px 4px 18px; flex-wrap: wrap; }
  .logo { width: 34px; height: 34px; border-radius: 50%; border: 3px solid var(--acc); box-shadow: 0 0 0 3px var(--accdk) inset; flex: none; }
  h1 { font-size: 20px; margin: 0; } .sub { color: var(--muted); font-size: 13px; }
  .grow { flex: 1; }
  .status { font-size: 13px; color: var(--muted); } .status.ok { color: var(--ok); } .status.err { color: var(--err); }
  section { background: var(--panel); border: 1px solid var(--line); border-radius: 14px; padding: 18px 20px; margin-bottom: 16px; }
  h2 { font-size: 16px; margin: 0 0 4px; }
  p.hint { color: var(--muted); margin: 0 0 14px; font-size: 13px; }
  button, a.btn { background: var(--acc); color: #fff; border: 0; border-radius: 10px; padding: 9px 16px; font-family: inherit; font-weight: 600; font-size: 14px; line-height: 1.2; cursor: pointer; text-decoration: none; display: inline-block; }
  button.ghost { background: transparent; color: var(--text); border: 1px solid var(--line); }
  button.small { padding: 5px 10px; font-size: 12px; border-radius: 8px; }
  button:disabled { opacity: .45; cursor: default; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 12px 18px; }
  label.field { display: flex; flex-direction: column; gap: 5px; font-size: 13px; color: var(--muted); }
  input, select { background: var(--panel2); color: var(--text); border: 1px solid var(--line); border-radius: 8px; padding: 7px 9px; font-family: inherit; font-size: 14px; min-width: 0; }
  input[type=color] { padding: 2px; height: 34px; width: 52px; }
  input[type=range] { padding: 0; accent-color: var(--acc); }
  input[type=checkbox] { width: 18px; height: 18px; accent-color: var(--acc); flex: none; margin: 0; }
  .switch { display: flex; align-items: center; gap: 9px; font-size: 14px; color: var(--text); }
  .row { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
  .apps { display: grid; grid-template-columns: repeat(auto-fill, minmax(160px, 1fr)); gap: 8px; }
  .app { display: flex; align-items: center; gap: 8px; padding: 8px 10px; background: var(--panel2); border: 1px solid var(--line); border-radius: 10px; font-size: 14px; }
  .app small { color: var(--muted); margin-left: auto; font-size: 11px; }
  .prof { border: 1px solid var(--line); border-radius: 12px; padding: 14px; margin-top: 12px; background: var(--panel2); }
  .prof.off { opacity: .6; }
  .head { display: flex; gap: 12px; align-items: end; flex-wrap: wrap; }
  .modes { margin-top: 8px; display: flex; flex-direction: column; gap: 8px; }
  .mode, .colhead { display: grid; grid-template-columns: 120px minmax(0, 1fr) minmax(0, 1fr) 28px; gap: 8px; align-items: center; }
  .colhead { color: var(--muted); font-size: 12px; margin-top: 12px; }
  .act { min-width: 0; display: flex; gap: 6px; align-items: center; background: var(--panel); border: 1px solid var(--line); border-radius: 8px; padding: 4px 6px; min-height: 36px; }
  .act select { padding: 4px 6px; font-size: 13px; }
  .rec { flex: 1; text-align: center; font: 12px ui-monospace, Menlo, monospace; padding: 5px 6px; border-radius: 6px; background: var(--accbg); cursor: pointer; color: var(--acc2); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .rec.listening { outline: 2px solid var(--acc); color: var(--text); }
  .act input[type=number] { width: 64px; }
  .muted { color: var(--muted); font-size: 13px; }
  .modecard { border: 1px solid var(--line); border-radius: 10px; padding: 10px; margin-top: 10px; background: var(--panel); }
  .actgrid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; margin-top: 8px; }
  .actlabel { font-size: 12px; color: var(--muted); margin-bottom: 4px; }
  .seq { display: flex; flex-direction: column; gap: 5px; }
  label.btn.ghost { background: transparent; color: var(--text); border: 1px solid var(--line); }
  label.btn.small { padding: 5px 10px; font-size: 12px; border-radius: 8px; }
  @media (max-width: 760px) { .actgrid { grid-template-columns: 1fr; } }
  ol { margin: 6px 0 12px; padding-left: 20px; color: var(--muted); font-size: 14px; } li { margin: 4px 0; }
  .savebar { position: sticky; bottom: 12px; margin-left: auto; width: max-content; max-width: 100%; display: flex; gap: 10px; align-items: center;
             background: var(--panel); border: 1px solid var(--line); padding: 10px 12px; border-radius: 14px; box-shadow: 0 8px 30px rgba(0,0,0,.18); z-index: 3; }
  .warnbox { background: #fff7e6; color: #5a4200; border-color: #f0d9a0; }
  .dark .warnbox { background: #2b2410; color: #ffd98a; border-color: #4a3d16; }
  .hidden { display: none !important; }
  @media (max-width: 760px) { .mode { grid-template-columns: 1fr; } .colhead { display: none; } .mode > button { justify-self: end; } }
  .wrap { overflow-x: hidden; }
  `;

  class DialFtConfigurator extends HTMLElement {
    connectedCallback() {
      if (this._init) return;
      this._init = true;
      const lang = (this.getAttribute("lang") || "auto").toLowerCase();
      const pageLang = (document.documentElement.lang || navigator.language || "en").toLowerCase();
      this.L = I18N[lang === "pl" || lang === "en" ? lang : (pageLang.startsWith("pl") || /[/?=]pl([/&#]|$)/.test(location.href) ? "pl" : "en")];
      this.port = null; this.writer = null; this.reader = null; this.rxBuf = ""; this.waiters = [];
      this.cfg = null; this.original = null;
      this.root = this.attachShadow({ mode: "open" });
      this.render0();
    }
    $(id) { return this.root.getElementById(id); }
    t(k) { return this.L[k]; }

    // ------------------------------------------------------------ szkielet
    render0() {
      const L = this.L, a = n => this.getAttribute(n);
      const manifest = a("manifest"), helper = a("helper"), gpl = a("guide-pl"), gen = a("guide-en");
      this.root.innerHTML = `<style>${CSS}</style>
      <div class="wrap ${a("theme") === "dark" ? "dark" : ""}">
        <div class="top"><div class="logo"></div><div><h1>${L.title}</h1><div class="sub">${L.sub}</div></div>
          <div class="grow"></div><span id="status" class="status">${L.notConnected}</span><button id="connect">${L.connect}</button></div>
        <section id="nosupport" class="warnbox hidden"><h2>${L.noSupportT}</h2><p class="hint" style="color:inherit">${L.noSupport}</p></section>
        <section id="intro"><h2>${L.introT}</h2><p class="hint">${L.intro}</p></section>
        <div id="ui" class="hidden">
          <section><h2>${L.general}</h2><p class="hint">${L.firmware}: <b id="fw">-</b></p>
            <div class="grid">
              <label class="field">${L.theme}<select id="s_theme"></select></label>
              <label class="field">${L.computer}<select id="s_host"><option value="0">${L.winOpt}</option><option value="1">${L.macOpt}</option></select></label>
              <label class="field">${L.bri}<span><input id="s_bri" type="range" min="10" max="100" step="5"> <span id="s_bri_v" class="muted"></span></span></label>
              <label class="field">${L.rgb}<select id="s_rgbMode"></select></label>
              <label class="field">${L.rgbBri}<span><input id="s_rgbBri" type="range" min="10" max="100" step="5"> <span id="s_rgbBri_v" class="muted"></span></span></label>
              <label class="field">${L.focus}<input id="s_focus" type="number" min="5" max="90" step="5"></label>
              <label class="field">${L.brk}<input id="s_break" type="number" min="1" max="60"></label>
            </div>
            <div class="row" style="margin-top:14px;gap:22px">
              <label class="switch"><input id="s_autoSwitch" type="checkbox">${L.auto}</label>
              <label class="switch"><input id="s_clock" type="checkbox">${L.clock}</label>
              <label class="switch"><input id="s_standup" type="checkbox">${L.standup}</label>
              <label class="switch"><input id="s_scrollInv" type="checkbox">${L.inv}</label>
            </div></section>
          <section><h2>${L.home}</h2><p class="hint">${L.homeHint}</p><div id="apps" class="apps"></div></section>
          <section><h2>${L.profiles}</h2><p class="hint">${L.profilesHint} <span id="seqhint"></span></p>
            <div class="row"><button id="exportp" class="small ghost">${L.exportB}</button>
              <label class="btn small ghost" style="cursor:pointer">${L.importB}<input id="importp" type="file" accept=".json,application/json" class="hidden"></label>
              <span id="macronote" class="muted hidden">${L.macroNote}</span></div>
            <div id="profiles"></div></section>
          <section><h2>${L.clockT}</h2><p class="hint">${L.clockHint}</p><button id="synctime" class="ghost">${L.setClock}</button> <span id="timeinfo" class="muted"></span></section>
          <div id="savebar" class="savebar"><span id="dirty" class="muted"></span><button id="revert" class="ghost">${L.undo}</button><button id="save">${L.save}</button></div>
        </div>
        ${manifest ? `<section><h2>${L.updT}</h2><p class="hint">${L.updHint}</p><ol><li>${L.upd1}</li><li>${L.upd2}</li><li>${L.upd3}</li></ol>
          <div class="row"><button id="prepare" class="ghost">${L.prepare}</button>
          <esp-web-install-button id="ewt" manifest="${manifest}"><button slot="activate">${L.install}</button><span slot="unsupported" class="muted">${L.noSupport}</span></esp-web-install-button>
          <span id="fwinfo" class="muted"></span></div></section>` : ""}
        ${helper ? `<section><h2>${L.helperT}</h2><p class="hint">${L.helperHint}</p><a class="btn" href="${helper}">${L.download}</a></section>` : ""}
        ${gpl || gen ? `<section><h2>${L.guideT}</h2><p class="hint">${L.guideHint}</p><div class="row">
          ${gpl ? `<a class="btn" href="${gpl}" target="_blank" rel="noopener">Polski (PDF)</a>` : ""}${gen ? `<a class="btn" href="${gen}" target="_blank" rel="noopener">English (PDF)</a>` : ""}</div></section>` : ""}
      </div>`;
      const supported = "serial" in navigator;
      if (!supported) { this.$("nosupport").classList.remove("hidden"); this.$("connect").disabled = true; if (this.$("prepare")) this.$("prepare").disabled = true; }
      this.$("connect").addEventListener("click", () => this.connect());
      this.$("save").addEventListener("click", () => this.save());
      this.$("revert").addEventListener("click", () => { this.cfg = JSON.parse(this.original); this.render(); this.markDirty(); });
      this.$("synctime").addEventListener("click", () => this.port && this.syncTime());
      this.$("exportp").addEventListener("click", () => this.cfg && this.exportProfiles());
      this.$("importp").addEventListener("change", e => { if (this.cfg && e.target.files[0]) this.importProfiles(e.target.files[0]); e.target.value = ""; });
      this.bindSettings();
      if (manifest) {
        if (!customElements.get("esp-web-install-button")) { const s = document.createElement("script"); s.type = "module"; s.src = ESP_WEB_TOOLS; document.head.appendChild(s); }
        this.$("prepare").addEventListener("click", () => this.prepareUpdate());
        fetch(manifest).then(r => r.json()).then(m => { this.$("fwinfo").textContent = L.available + " " + m.version; }).catch(() => {});
      }
      if (this.hasAttribute("demo")) this.loadDemo();
    }
    setStatus(txt, cls = "") { const s = this.$("status"); s.textContent = txt; s.className = "status " + cls; }

    // ------------------------------------------------------------ USB
    async connect() {
      if (this.port) { await this.disconnect(); return; }
      const L = this.L;
      try {
        this.port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });
        await this.port.open({ baudRate: 115200 });
        await this.port.setSignals({ dataTerminalReady: true, requestToSend: false });
        this.writer = this.port.writable.getWriter();
        this.readLoop();
        this.setStatus(L.connecting);
        const hello = await this.request("HELLO", l => l.startsWith("DIAL FT"), 2000);
        if (!hello) throw new Error(L.notDial);
        await this.syncTime();
        await this.load();
        this.$("connect").textContent = L.disconnect;
        this.setStatus(L.connected, "ok");
      } catch (e) {
        console.error(e);
        if (e && e.name === "NotFoundError") { this.setStatus(L.notConnected); await this.disconnect(true); return; }
        this.setStatus(e.message && e.message.length < 90 ? e.message : L.failed, "err");
        await this.disconnect(true);
      }
    }
    async disconnect(keepStatus) {
      try { if (this.reader) await this.reader.cancel(); } catch (e) {}
      try { if (this.writer) this.writer.releaseLock(); } catch (e) {}
      try { if (this.port) await this.port.close(); } catch (e) {}
      this.port = this.reader = this.writer = null;
      this.$("connect").textContent = this.L.connect;
      if (!keepStatus) this.setStatus(this.L.notConnected);
    }
    async readLoop() {
      const dec = new TextDecoder();
      while (this.port && this.port.readable) {
        this.reader = this.port.readable.getReader();
        try {
          for (;;) {
            const { value, done } = await this.reader.read();
            if (done) break;
            this.rxBuf += dec.decode(value);
            let i;
            while ((i = this.rxBuf.indexOf("\n")) >= 0) {
              const line = this.rxBuf.slice(0, i).replace(/\r$/, "");
              this.rxBuf = this.rxBuf.slice(i + 1);
              for (const w of this.waiters.slice()) if (w.match(line)) { this.waiters.splice(this.waiters.indexOf(w), 1); w.resolve(line); }
            }
          }
        } catch (e) { break; } finally { try { this.reader.releaseLock(); } catch (e) {} }
      }
      if (this.port) { this.setStatus(this.L.lost, "err"); this.port = null; this.$("connect").textContent = this.L.connect; }
    }
    send(line) { return this.writer.write(new TextEncoder().encode(line + "\n")); }
    request(line, match, timeout = 3000) {
      return new Promise(async resolve => {
        const w = { match, resolve };
        this.waiters.push(w);
        setTimeout(() => { const i = this.waiters.indexOf(w); if (i >= 0) { this.waiters.splice(i, 1); resolve(null); } }, timeout);
        await this.send(line);
      });
    }
    async syncTime() {
      const now = new Date();
      await this.send(`TIME ${Math.floor(now.getTime() / 1000)} ${-now.getTimezoneOffset()}`);
      this.$("timeinfo").textContent = this.L.clockSet + " " + now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    }

    // ------------------------------------------------------------ dane
    normalize(c) {   // jednolity format (makra) niezaleznie od wersji firmware; puste sloty dostaja domyslne pola
      this.v2 = (c.cfgv || 1) >= 2;
      this.maxSteps = this.v2 ? (c.maxSteps || 6) : 1;
      const seq = v => !v ? [] : (typeof v[0] === "number" ? (v[0] ? [v.slice(0, 3)] : []) : v.filter(x => x && x[0]).map(x => x.slice(0, 3)));
      for (const p of c.custom) {
        const oldTap = seq(p.tap);
        if (!p.modes || !p.modes.length) p.modes = [{ name: "MODE" }];
        for (const m of p.modes) {
          m.l = seq(m.l); m.r = seq(m.r);
          m.tap = m.tap ? seq(m.tap) : oldTap.slice();
          m.click = seq(m.click);
        }
        delete p.tap;
        if (!p.texts) p.texts = ["", ""];
        if (!p.color) p.color = "#518751";
        if (p.icon == null) p.icon = 19;
        if (p.app == null) p.app = "";
      }
      return c;
    }
    async load() {
      const line = await this.request("CFG GET", l => l.startsWith("CFG {"), 4000);
      if (!line) throw new Error(this.L.oldFw);
      this.cfg = this.normalize(JSON.parse(line.slice(4)));
      this.showUi();
    }
    showUi() {
      this.original = JSON.stringify(this.cfg);
      this.render();
      this.$("intro").classList.add("hidden");
      this.$("ui").classList.remove("hidden");
      this.markDirty();
    }
    async save() {
      const c = this.cfg, L = this.L;
      const prof = p => ({ used: p.used ? 1 : 0, name: p.name, icon: p.icon, color: p.color, app: p.app, texts: p.texts,
        modes: p.modes.map(m => ({ name: m.name, l: m.l, r: m.r, tap: m.tap, click: m.click })) });
      this.$("save").disabled = true;
      const ok = r => r && r.startsWith("CFG OK");
      const ask = line => this.request(line, l => l.startsWith("CFG OK") || l.startsWith("CFG ERR"), 6000);
      let r;
      if (this.v2) {   // ustawienia, potem kazdy profil osobnym wierszem (makra sa dlugie)
        r = await ask("CFG SET " + JSON.stringify({ settings: c.settings, hidden: c.apps.filter(a => a.hidden).map(a => a.i) }));
        for (let i = 0; ok(r) && i < c.custom.length; i++) r = await ask("CFG SET " + JSON.stringify({ slot: i, profile: prof(c.custom[i]) }));
      } else {         // firmware 1.2: jedna akcja na obrot, tap wspolny dla profilu
        const first = q => q.length ? q[0] : [0, 0, 0];
        r = await ask("CFG SET " + JSON.stringify({ settings: c.settings, hidden: c.apps.filter(a => a.hidden).map(a => a.i),
          custom: c.custom.map(p => ({ used: p.used ? 1 : 0, name: p.name, icon: p.icon, color: p.color, app: p.app,
            tap: first(p.modes[0].tap), modes: p.modes.map(m => ({ name: m.name, l: first(m.l), r: first(m.r) })) })) }));
      }
      if (ok(r)) { this.original = JSON.stringify(this.cfg); this.setStatus(L.saved, "ok"); }
      else this.setStatus(r || L.failed, "err");
      this.markDirty();
    }
    exportProfiles() {
      const data = JSON.stringify({ dialft: 2, custom: this.cfg.custom }, null, 1);
      const a = document.createElement("a");
      a.href = URL.createObjectURL(new Blob([data], { type: "application/json" }));
      a.download = "dialft-profiles.json";
      a.click();
      setTimeout(() => URL.revokeObjectURL(a.href), 2000);
    }
    importProfiles(file) {
      const rd = new FileReader();
      rd.onload = () => {
        try {
          const d = JSON.parse(rd.result);
          if (!d || !Array.isArray(d.custom)) throw 0;
          const n = this.cfg.custom.length;
          const tmp = this.normalize({ cfgv: this.v2 ? 2 : 1, maxSteps: this.maxSteps, custom: d.custom.slice(0, n) }).custom;
          for (let i = 0; i < n; i++) if (tmp[i]) this.cfg.custom[i] = tmp[i];
          this.render(); this.markDirty();
        } catch (e) { alert(this.L.importErr); }
      };
      rd.readAsText(file);
    }
    markDirty() {
      const d = JSON.stringify(this.cfg) !== this.original;
      this.$("dirty").textContent = d ? this.L.unsaved : this.L.noChanges;
      this.$("dirty").style.color = d ? "var(--acc)" : "";
      this.$("save").disabled = !d; this.$("revert").disabled = !d;
    }

    // ------------------------------------------------------------ widok
    el(tag, attrs = {}, ...kids) {
      const e = document.createElement(tag);
      for (const [k, v] of Object.entries(attrs)) {
        if (k === "class") e.className = v; else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
        else if (["checked", "value", "disabled", "selected"].includes(k)) e[k] = v; else e.setAttribute(k, v);
      }
      for (const c of kids) if (c != null) e.append(c);
      return e;
    }
    render() {
      const s = this.cfg.settings, el = this.el.bind(this), $ = this.$.bind(this);
      $("fw").textContent = this.cfg.fw;
      $("s_theme").replaceChildren(...this.cfg.themes.map((t, i) => el("option", { value: i, selected: i === s.theme }, t)));
      $("s_rgbMode").replaceChildren(...this.cfg.rgbModes.map((t, i) => el("option", { value: i, selected: i === s.rgbMode }, t)));
      $("s_host").value = s.host;
      for (const k of ["bri", "rgbBri", "focus", "break"]) $("s_" + k).value = s[k];
      for (const k of ["autoSwitch", "clock", "standup", "scrollInv"]) $("s_" + k).checked = !!s[k];
      $("s_bri_v").textContent = s.bri + "%"; $("s_rgbBri_v").textContent = s.rgbBri + "%";
      $("apps").replaceChildren(...this.cfg.apps.map(a => el("label", { class: "app" },
        el("input", { type: "checkbox", checked: !a.hidden, disabled: !!a.fixed, onchange: e => { a.hidden = e.target.checked ? 0 : 1; this.markDirty(); } }),
        a.name, el("small", {}, this.cfg.pages[a.page] || ""))));
      $("profiles").replaceChildren(...this.cfg.custom.map((c, i) => this.renderProfile(c, i)));
      $("seqhint").textContent = this.v2 ? this.L.seqHint.replace("{n}", this.maxSteps) : "";
      $("macronote").classList.toggle("hidden", this.v2);
    }
    renderProfile(c, i) {
      const L = this.L, el = this.el.bind(this), ch = () => this.markDirty();
      const box = el("div", { class: "prof" + (c.used ? "" : " off") });
      const redraw = () => box.replaceWith(this.renderProfile(c, i));
      const tpl = el("select", { style: "max-width:190px", onchange: e => {
        const t = TEMPLATES[+e.target.value]; e.target.value = "";
        if (!t || (c.used && !confirm(L.templateAsk))) return;
        Object.assign(c, JSON.parse(JSON.stringify({ used: 1, name: t.name.slice(0, 12), icon: t.icon, color: t.color, app: t.app || "",
          texts: t.texts || ["", ""], modes: t.modes.map(m => ({ name: m.name, l: m.l || [], r: m.r || [], tap: m.tap || [], click: m.click || [] })) })));
        ch(); redraw();
      } }, el("option", { value: "" }, L.template), ...TEMPLATES.map((t, k) => el("option", { value: k }, t.name)));
      box.append(el("div", { class: "head" },
        el("label", { class: "switch" }, el("input", { type: "checkbox", checked: !!c.used, onchange: e => { c.used = e.target.checked ? 1 : 0; ch(); redraw(); } }), L.profile + " " + (i + 1)),
        el("label", { class: "field" }, L.name, el("input", { value: c.name || "", maxlength: 12, oninput: e => { c.name = e.target.value; ch(); } })),
        el("label", { class: "field" }, L.icon, el("select", { onchange: e => { c.icon = +e.target.value; ch(); } },
          ...L.icons.map((n, k) => el("option", { value: k, selected: k === c.icon }, n)))),
        el("label", { class: "field" }, L.color, el("input", { type: "color", value: c.color, oninput: e => { c.color = e.target.value; ch(); } })),
        el("label", { class: "field", style: "flex:1;min-width:200px" }, L.program,
          el("input", { value: c.app || "", maxlength: 39, placeholder: L.programPh, oninput: e => { c.app = e.target.value; ch(); } })),
        tpl));
      if (!c.used) return box;
      if (this.v2) box.append(el("div", { class: "row", style: "margin-top:10px" },
        el("span", { class: "muted" }, L.texts),
        el("input", { value: c.texts[0], maxlength: 32, placeholder: L.text1, style: "flex:1;min-width:150px", oninput: e => { c.texts[0] = e.target.value; ch(); } }),
        el("input", { value: c.texts[1], maxlength: 32, placeholder: L.text2, style: "flex:1;min-width:150px", oninput: e => { c.texts[1] = e.target.value; ch(); } })));
      c.modes.forEach((m, k) => {
        const card = el("div", { class: "modecard" });
        card.append(el("div", { class: "row" },
          el("input", { value: m.name, maxlength: 10, style: "width:140px;font-weight:600", oninput: e => { m.name = e.target.value.toUpperCase(); e.target.value = m.name; ch(); } }),
          el("span", { class: "grow" }),
          c.modes.length > 1 ? el("button", { class: "small ghost", title: L.removeMode, onclick: () => { c.modes.splice(k, 1); ch(); redraw(); } }, "×") : null));
        const acts = [["l", L.aLeft, "←"], ["r", L.aRight, "→"], ["tap", L.aTap, "●"]];
        if (this.v2) acts.push(["click", L.aClick, "◉"]);
        const g = el("div", { class: "actgrid" });
        for (const [key, label, sym] of acts) {
          if (!this.v2 && key === "tap" && k > 0) continue;   // firmware 1.2: tap wspolny = z pierwszego trybu
          g.append(el("div", { class: "actcell" }, el("div", { class: "actlabel" }, sym + " " + label),
            this.seqEditor(m[key], v => { m[key] = v; ch(); }, key === "click" ? L.clickDefault : L.empty)));
        }
        card.append(g);
        box.append(card);
      });
      box.append(el("div", { class: "row", style: "margin-top:10px" },
        c.modes.length < 4 ? el("button", { class: "small ghost", onclick: () => { c.modes.push({ name: "MODE " + (c.modes.length + 1), l: [], r: [], tap: [], click: [] }); ch(); redraw(); } }, L.addMode) : null));
      return box;
    }
    seqEditor(steps, set, emptyText) {   // makro: lista krokow, kazdy krok to edytor akcji
      const L = this.L, el = this.el.bind(this);
      steps = steps.map(x => x.slice());
      const wrap = el("div", { class: "seq" });
      const draw = () => {
        wrap.replaceChildren();
        steps.forEach((st, i) => wrap.append(el("div", { class: "row", style: "gap:6px;flex-wrap:nowrap" },
          el("div", { style: "flex:1;min-width:0" }, this.actEditor(st, v => { if (v[0]) steps[i] = v; else steps.splice(i, 1); set(steps.map(x => x.slice())); if (!v[0]) draw(); })),
          el("button", { class: "small ghost", title: "×", onclick: () => { steps.splice(i, 1); set(steps.map(x => x.slice())); draw(); } }, "×"))));
        if (!steps.length) wrap.append(el("span", { class: "muted", style: "font-size:12px" }, emptyText));
        if (steps.length < this.maxSteps)
          wrap.append(el("button", { class: "small ghost", style: "align-self:flex-start", onclick: () => { steps.push([1, 0, 0]); set(steps.map(x => x.slice())); draw(); } }, L.addStep));
      };
      draw();
      return wrap;
    }
    actLabel(a) {
      if (a[0] !== 1) return "";
      const mods = MODS.filter(([b]) => a[1] & b).map(([, n]) => n);
      const k = a[2] > 32 && a[2] < 127 ? String.fromCharCode(a[2]).toUpperCase() : (KEYNAME[a[2]] || "key " + a[2]);
      return (mods.length ? mods.join("+") + "+" : "") + (a[2] ? k : "?");
    }
    actEditor(a, set, arrow) {
      const L = this.L, el = this.el.bind(this);
      a = a ? a.slice() : [0, 0, 0];
      const types = [[0, L.actNone], [1, L.actKey], [2, L.actMedia], [3, L.actWheel], [4, L.actPan]];
      if (this.v2) types.push([5, L.actDelay], [6, L.actText], [7, L.actNext], [8, L.actPrev]);
      const wrap = el("div", { class: "act" });
      const type = el("select", { onchange: () => { const t = +type.value; a = [t, 0, t === 2 ? MEDIA_CODES[0] : (t === 3 || t === 4 ? 1 : (t === 5 ? 100 : 0))]; set(a); fill(); } },
        ...types.map(([v, n]) => el("option", { value: v, selected: v === a[0] }, n)));
      const body = el("span", { style: "display:flex;flex:1;gap:6px;align-items:center;min-width:0" });
      if (arrow) wrap.append(el("span", { class: "muted", style: "flex:none;font-size:15px", title: arrow === "←" ? L.left : L.right }, arrow));
      wrap.append(type, body);
      const fill = () => {
        body.replaceChildren();
        if (a[0] === 1) {
          const rec = el("span", { class: "rec", tabindex: 0 }, this.actLabel(a) || L.clickKeys);
          rec.addEventListener("click", () => { rec.classList.add("listening"); rec.textContent = L.pressKeys; rec.focus(); });
          rec.addEventListener("blur", () => { rec.classList.remove("listening"); rec.textContent = this.actLabel(a) || L.clickKeys; });
          rec.addEventListener("keydown", e => {
            if (!rec.classList.contains("listening")) return;
            e.preventDefault(); e.stopPropagation();
            if (/^(Shift|Control|Alt|Meta)/.test(e.code)) return;
            let code = KEYMAP[e.code];
            if (!code && e.key && e.key.length === 1 && e.key.charCodeAt(0) > 32 && e.key.charCodeAt(0) < 127) code = e.key.toLowerCase().charCodeAt(0);
            if (!code && KEYMAP[e.key]) code = KEYMAP[e.key];
            if (!code) { rec.textContent = L.keyNo; return; }
            let m = 0;
            if (e.ctrlKey || e.metaKey) m |= 1;
            if (e.altKey) m |= 4;
            if (e.shiftKey) m |= 2;
            a = [1, m, code]; set(a);
            rec.classList.remove("listening"); rec.textContent = this.actLabel(a); rec.blur();
          });
          body.append(rec);
        } else if (a[0] === 2) {
          body.append(el("select", { style: "flex:1", onchange: e => { a = [2, 0, +e.target.value]; set(a); } },
            ...MEDIA_CODES.map((v, i) => el("option", { value: v, selected: v === a[2] }, L.media[i]))));
        } else if (a[0] === 3 || a[0] === 4) {
          body.append(el("input", { type: "number", min: -20, max: 20, value: a[2] || 1, oninput: e => { a = [a[0], 0, +e.target.value || 1]; set(a); } }),
            el("span", { class: "muted" }, L.notches + (a[0] === 3 ? " (+ ↓)" : " (+ →)")));
        } else if (a[0] === 5) {
          body.append(el("input", { type: "number", min: 10, max: 2000, step: 10, value: a[2] || 100, oninput: e => { a = [5, 0, Math.max(10, Math.min(2000, +e.target.value || 100))]; set(a); } }),
            el("span", { class: "muted" }, L.ms));
        } else if (a[0] === 6) {
          body.append(el("select", { style: "flex:1", onchange: e => { a = [6, 0, +e.target.value]; set(a); } },
            el("option", { value: 0, selected: a[2] === 0 }, L.text1), el("option", { value: 1, selected: a[2] === 1 }, L.text2)));
        } else body.append(el("span", { class: "muted" }, L.none));
      };
      fill();
      return wrap;
    }
    bindSettings() {
      const num = (id, key) => this.$(id).addEventListener("input", e => {
        this.cfg.settings[key] = +e.target.value;
        if (this.$(id + "_v")) this.$(id + "_v").textContent = e.target.value + "%";
        this.markDirty();
      });
      num("s_theme", "theme"); num("s_host", "host"); num("s_rgbMode", "rgbMode"); num("s_bri", "bri"); num("s_rgbBri", "rgbBri");
      num("s_focus", "focus"); num("s_break", "break");
      for (const k of ["autoSwitch", "clock", "standup", "scrollInv"])
        this.$("s_" + k).addEventListener("change", e => { this.cfg.settings[k] = e.target.checked; this.markDirty(); });
    }

    // ------------------------------------------------------------ aktualizacja: 1200 bodow -> bootloader
    async prepareUpdate() {
      try {
        if (this.port) await this.disconnect(true);
        const p = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });
        await p.open({ baudRate: 1200 });
        await p.setSignals({ dataTerminalReady: false });
        await new Promise(r => setTimeout(r, 250));
        try { await p.close(); } catch (e) {}
        this.$("fwinfo").textContent = this.L.prepared;
      } catch (e) { this.$("fwinfo").textContent = this.L.cancelled; }
    }

    loadDemo() {   // podglad bez urzadzenia (atrybut demo)
      this.cfg = this.normalize({ fw: "DFTv1.3 (demo)", cfgv: 2, maxSteps: 6, settings: { theme: 7, host: 1, bri: 80, scrollInv: false, rgbMode: 1, rgbBri: 60, break: 5, focus: 25, autoSwitch: true, clock: true, standup: true },
        themes: ["Ocean", "Amber", "Mint", "Violet", "Rose", "Mono", "Color", "FalbaTech"], rgbModes: ["Off", "Follow", "Ambient", "Breathe", "Rainbow"],
        pages: ["EVERYDAY", "CREATIVE", "DESIGN", "SYSTEM", "CUSTOM"],
        apps: [{ i: 0, name: "Pomodoro", page: 0, hidden: 0 }, { i: 1, name: "Smart Scroll", page: 0, hidden: 0 }, { i: 6, name: "DaVinci", page: 1, hidden: 0 }, { i: 17, name: "Settings", page: 3, hidden: 0, fixed: true }],
        custom: [{ used: 1, name: "Safari", icon: 19, color: "#518751", app: "com.apple.Safari", tap: [1, 1, 114], modes: [{ name: "TABS", l: [1, 3, 0xB3], r: [1, 1, 0xB3] }, { name: "ZOOM", l: [1, 1, 45], r: [1, 1, 61] }] },
          { used: 0, name: "", icon: 19, color: "#518751", app: "", tap: [0, 0, 0], modes: [] }] });
      this.showUi();
    }
  }
  customElements.define("dialft-configurator", DialFtConfigurator);
})();
