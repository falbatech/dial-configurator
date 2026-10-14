#!/bin/bash
# Instaluje DIAL FT Helper jako LaunchAgent (start przy logowaniu, restart po awarii).
# Odinstalowanie: ./install_mac.sh --uninstall
set -e
LABEL=tech.falba.dialft.helper
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
DEST="$HOME/Library/Application Support/FalbaTech/DIAL FT"
if [ "$1" = "--uninstall" ]; then
  launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
  rm -f "$PLIST"; rm -rf "$DEST"
  echo "DIAL FT Helper odinstalowany."; exit 0
fi
mkdir -p "$DEST" "$HOME/Library/LaunchAgents"
cp "$(dirname "$0")/dialft_helper.py" "$DEST/dialft_helper.py"
cat > "$PLIST" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key><array><string>/usr/bin/python3</string><string>-u</string><string>$DEST/dialft_helper.py</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$DEST/helper.log</string>
  <key>StandardErrorPath</key><string>$DEST/helper.log</string>
</dict></plist>
PL
launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
launchctl bootstrap "gui/$(id -u)" "$PLIST"
echo "DIAL FT Helper zainstalowany i uruchomiony. Log: $DEST/helper.log"
echo "Przy pierwszym utworze macOS zapyta o zgode na sterowanie Spotify - zezwol."
