#!/bin/bash
# Sklada strone konfiguratora (web/) do wgrania na serwer HTTPS (np. falba.tech/dial):
# kompiluje firmware, kopiuje pliki do web/firmware/, tworzy manifest.json dla ESP Web Tools i paczke helpera.
# Aktualizacja zapisuje bootloader, partycje i aplikacje - obszar NVS (ustawienia, profile klienta) zostaje.
set -e
cd "$(dirname "$0")/.."
PIO=${PIO:-$HOME/.platformio/penv/bin/pio}
"$PIO" run -e crowpanel128
VER=$(grep -o 'FW_VERSION "[^"]*"' src/config.h | cut -d'"' -f2)
B=.pio/build/crowpanel128
BOOTAPP=$(find "$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions" -name boot_app0.bin | head -1)
mkdir -p web/firmware web/helper
cp "$B/bootloader.bin" "$B/partitions.bin" "$B/firmware.bin" web/firmware/
cp "$BOOTAPP" web/firmware/boot_app0.bin
cat > web/manifest.json <<JSON
{
  "name": "DIAL FT",
  "version": "$VER",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware/bootloader.bin", "offset": 0 },
        { "path": "firmware/partitions.bin", "offset": 32768 },
        { "path": "firmware/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
JSON
cp docs/DIAL_FT_Instrukcja_PL.pdf docs/DIAL_FT_User_Guide_EN.pdf web/
rm -f web/helper/DIAL-FT-Helper-mac.zip
(cd tools/helper && zip -q -r ../../web/helper/DIAL-FT-Helper-mac.zip dialft_helper.py install_mac.sh README.md -x "__pycache__/*")
# kod do wklejenia w Ecwid ("Osadz kod / kod wlasny"): komponent wbudowany w jeden blok.
# BASE_URL = adres, pod ktorym leza pliki web/ (manifest, firmware, helper, PDF). Bez niego: tylko ustawienia i profile.
ATTR=""
if [ -n "$BASE_URL" ]; then
  ATTR=" manifest=\"$BASE_URL/manifest.json\" helper=\"$BASE_URL/helper/DIAL-FT-Helper-mac.zip\" guide-pl=\"$BASE_URL/DIAL_FT_Instrukcja_PL.pdf\" guide-en=\"$BASE_URL/DIAL_FT_User_Guide_EN.pdf\""
fi
{
  echo "<!-- DIAL FT Configurator ($VER) - wklej w Ecwid: Osadz kod / kod wlasny -->"
  echo "<dialft-configurator theme=\"light\" lang=\"auto\"$ATTR></dialft-configurator>"
  echo "<script>"
  cat web/dialft-configurator.js
  echo "</script>"
} > web/ecwid-snippet.html
echo "Gotowe: web/ ($VER). Kod do Ecwid: web/ecwid-snippet.html ($(wc -c < web/ecwid-snippet.html) B)."
