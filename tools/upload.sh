#!/bin/bash
# Wgrywanie DIAL FT bez przycisku BOOT: 1200 bodow -> bootloader, upload, restart do aplikacji.
# Uzycie: tools/upload.sh   (z katalogu projektu)
set -e
PIO=${PIO:-$HOME/.platformio/penv/bin/pio}
PY=${PY:-$HOME/.platformio/penv/bin/python}
ESPTOOL=$HOME/.platformio/packages/tool-esptoolpy/esptool.py
# tylko porty Espressif (VID 0x303A): DIAL FT albo bootloader; inne urzadzenia USB (np. klawiatury) sa pomijane
dial_port() { "$PY" -c "import serial.tools.list_ports as l; p=[x.device for x in l.comports() if x.vid==0x303A]; print(p[0] if p else '')"; }
PORT=$(dial_port)
[ -z "$PORT" ] && { echo "Brak DIAL FT. Pierwszy raz: przytrzymaj BOOT i podlacz USB."; exit 1; }
"$PIO" run -e crowpanel128
if ! "$PY" -c "import sys,serial.tools.list_ports as l; sys.exit(0 if any(p.device=='$PORT' and 'JTAG' in (p.product or '') for p in l.comports()) else 1)"; then
  "$PY" -c "import serial,time; s=serial.Serial(); s.port='$PORT'; s.baudrate=1200; s.open(); s.dtr=False; time.sleep(0.2); s.close()" 2>/dev/null || true   # port znika przy resecie - to normalne
  sleep 4
  PORT=$(dial_port)
fi
"$PIO" run -e crowpanel128 -t upload --upload-port "$PORT"
sleep 3
# po wgraniu plytka czasem zostaje w bootloaderze: wtedy restart watchdogiem
if [ -e "$PORT" ] && "$PY" -c "import sys,serial.tools.list_ports as l; sys.exit(0 if any(p.device=='$PORT' and 'JTAG' in (p.product or '') for p in l.comports()) else 1)"; then
  "$PY" "$ESPTOOL" --chip esp32s3 --port "$PORT" --after watchdog_reset read_mac >/dev/null
fi
echo "Gotowe: DIAL FT uruchomiony."
