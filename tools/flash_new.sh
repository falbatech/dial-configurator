#!/bin/bash
# Programowanie NOWEGO kontrolera DIAL FT (Elecrow CrowPanel 1.28" ESP32-S3) gotowymi plikami z web/firmware/.
# Bez PlatformIO i bez kompilacji. Czysci cala pamiec (ustawienia fabryczne), potem wgrywa DIAL FT.
#
# Uzycie:  tools/flash_new.sh            (jeden kontroler)
#          tools/flash_new.sh --loop     (seria: po kazdym podlacz kolejny kontroler)
# Wymaga:  esptool (z PlatformIO albo: pip3 install esptool)
# Gdy kontroler sie nie laczy: odlacz, przytrzymaj BOOT, podlacz, pusc BOOT.
set -e
cd "$(dirname "$0")/.."
FW=web/firmware
for f in bootloader.bin partitions.bin boot_app0.bin firmware.bin; do
  [ -f "$FW/$f" ] || { echo "Brak $FW/$f - uruchom tools/build_web.sh albo pobierz web/ z GitHuba."; exit 1; }
done
if [ -x "$HOME/.platformio/penv/bin/python" ] && [ -f "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ]; then
  ESPTOOL=("$HOME/.platformio/penv/bin/python" "$HOME/.platformio/packages/tool-esptoolpy/esptool.py")
elif command -v esptool >/dev/null; then ESPTOOL=(esptool)
elif command -v esptool.py >/dev/null; then ESPTOOL=(esptool.py)
else ESPTOOL=(python3 -m esptool); fi

# tylko porty Espressif (VID 0x303A) - inne urzadzenia USB (np. klawiatury) sa pomijane
esp_port() {
  local PYX="$HOME/.platformio/penv/bin/python"; [ -x "$PYX" ] || PYX=python3
  "$PYX" -c "import serial.tools.list_ports as l; p=[x.device for x in l.comports() if x.vid==0x303A]; print(p[0] if p else '')" 2>/dev/null
}

flash_one() {
  echo "Czekam na kontroler (USB)..."
  until PORT=$(esp_port) && [ -n "$PORT" ]; do sleep 1; done
  sleep 1
  echo "Port: $PORT"
  "${ESPTOOL[@]}" --chip esp32s3 --port "$PORT" --baud 921600 --before default_reset --after no_reset erase_flash
  "${ESPTOOL[@]}" --chip esp32s3 --port "$PORT" --baud 921600 --before default_reset --after watchdog_reset write_flash -z \
    0x0 "$FW/bootloader.bin" 0x8000 "$FW/partitions.bin" 0xe000 "$FW/boot_app0.bin" 0x10000 "$FW/firmware.bin"
  echo "Gotowe: DIAL FT $(grep -o '"version": "[^"]*"' web/manifest.json | cut -d'"' -f4) wgrany. Na ekranie pojawi sie logo i samouczek."
}

if [ "$1" = "--loop" ]; then
  while true; do
    flash_one
    echo "Odlacz ten kontroler i podlacz nastepny (Ctrl+C = koniec)."
    while [ -n "$(esp_port)" ]; do sleep 1; done
  done
else
  flash_one
fi
