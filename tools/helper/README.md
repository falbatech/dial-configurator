# DIAL FT Helper (macOS)

Opcjonalny program w tle, ktory pokazuje na DIAL FT, co gra w aplikacji **Spotify** (tytul, wykonawca, okladka, postep utworu, pauza).
Reszta DIAL FT (wszystkie profile, takze COLOR w DaVinci) dziala bez niego.

Czyta dane z wlaczonej aplikacji Spotify przez AppleScript - bez logowania i bez API Spotify w internecie.
Okladka jest pobierana z adresu podanego przez Spotify i skalowana systemowym `sips` do 112x112.
Tylko biblioteka standardowa Pythona (`/usr/bin/python3` z macOS).

## Instalacja (start przy logowaniu)
```
tools/helper/install_mac.sh
```
Przy pierwszym utworze macOS zapyta o zgode na sterowanie Spotify (Ustawienia > Prywatnosc > Automatyzacja).
Odinstalowanie: `tools/helper/install_mac.sh --uninstall`. Recznie: `python3 tools/helper/dialft_helper.py`.

## Na DIAL
Profil Spotify: okladka, tytul, wykonawca, obrzeze ekranu = postep utworu, dwie belki = pauza.
Bez helpera profil dziala dalej (klawisze multimedialne), tylko bez okladki.

## Protokol
Helper -> DIAL: `NP META <grane 0/1> <poz_s> <dl_s> <id>\t<tytul>\t<wykonawca>`, `NP OFF`,
okladka na zadanie: `NP ART <id> 112 112 <n>`, `NP A <nr> <base64 192 B>`, `NP END <id>` (RGB565 LE).
DIAL -> helper: `NP NEED <id>`.

Windows: jeszcze nie (Spotify nie ma tam AppleScript; potrzebne systemowe API "Media Transport Controls").
