#!/usr/bin/env python3
"""DIAL FT Helper (macOS) - Now Playing ze Spotify, automatyczny profil i zegar dla DIAL FT.

Wysyla tez: "APP <bundle id>" po zmianie aplikacji na wierzchu (DIAL przelacza profil) i "TIME <epoch> <strefa_min>".

Co sekunde pyta Spotify (AppleScript) o utwor, wykonawce, pozycje i okladke, wysyla na DIAL:
    NP META <stan 0/1> <pozycja_s> <dlugosc_s> <id>\t<tytul>\t<wykonawca>
    NP OFF                                   (Spotify nie dziala)
Okladka 112x112 RGB565 tylko na zadanie DIAL ("NP NEED <id>"):
    NP ART <id> <w> <h> <liczba_kawalkow>, NP A <nr> <base64>, NP END <id>
Port 127.0.0.1:47811: przekaznik wierszy do DIAL (dla wlasnych narzedzi; "STATUS" zwraca stan DIAL).

Tylko biblioteka standardowa + narzedzia systemu (osascript, sips).
Uruchomienie: python3 dialft_helper.py   (instalacja w tle: install_mac.sh)
Pierwsze uruchomienie: macOS zapyta o zgode na sterowanie aplikacja Spotify - zezwol.
"""
import base64
import glob
import os
import select
import socket
import subprocess
import sys
import tempfile
import time
import unicodedata
import urllib.request

VERSION = "1.3.1"
RELAY_PORT = 47811
ART_W = ART_H = 112
CHUNK = 192
SEP = "\x1f"

APPLESCRIPT = '''
if application "Spotify" is running then
  tell application "Spotify"
    set s to (ASCII character 31)
    try
      set t to current track
      return (player state as text) & s & ((player position * 1000) as integer) & s & (duration of t) & s & (id of t) & s & (name of t) & s & (artist of t) & s & (artwork url of t)
    on error
      return "stopped"
    end try
  end tell
else
  return "off"
end if
'''


def log(*a):
    print(time.strftime("%H:%M:%S"), "[DIAL FT]", *a, flush=True)


# ---------------------------------------------------------------- serial
class PosixSerial:
    def __init__(self, path):
        import termios
        import tty
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        attrs = termios.tcgetattr(self.fd)
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.name = path

    def read(self, n=4096):
        try:
            return os.read(self.fd, n)
        except BlockingIOError:
            return b""

    def write(self, data):
        view = memoryview(data)
        t0 = time.time()
        while view:
            try:
                k = os.write(self.fd, view)
                view = view[k:]
            except BlockingIOError:
                if time.time() - t0 > 2.0:
                    raise OSError("write timeout")
                time.sleep(0.002)

    def close(self):
        try:
            os.close(self.fd)
        except OSError:
            pass


def find_dial():
    for path in sorted(glob.glob("/dev/cu.usbmodem*")):
        try:
            s = PosixSerial(path)
        except OSError:
            continue
        try:
            s.write(b"HELLO\n")
            t0, buf = time.time(), b""
            while time.time() - t0 < 1.0:
                buf += s.read()
                if b"DIAL FT" in buf:
                    return s
                time.sleep(0.02)
        except OSError:
            pass
        s.close()
    return None


# ---------------------------------------------------------------- Spotify
def ascii_text(s, n=40):
    """Font DIAL nie ma polskich znakow: transliteracja do ASCII."""
    s = s.replace("ł", "l").replace("Ł", "L").replace("\t", " ")
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode()
    return s[:n]


def spotify_now():
    try:
        out = subprocess.run(["osascript", "-e", APPLESCRIPT], capture_output=True, text=True, timeout=3).stdout.strip()
    except Exception:
        return None
    if out in ("off", ""):
        return None
    if out == "stopped":
        return {"playing": False, "pos": 0, "dur": 0, "id": "", "title": "", "artist": "", "art": ""}
    p = out.split(SEP)
    if len(p) < 7:
        return None
    try:
        return {"playing": p[0] == "playing", "pos": int(p[1]) // 1000, "dur": int(p[2]) // 1000,
                "id": p[3].split(":")[-1][:22], "title": ascii_text(p[4]), "artist": ascii_text(p[5]), "art": p[6]}
    except ValueError:
        return None


_art_cache = {}


def artwork_rgb565(url):
    """Pobiera okladke, skaluje sips-em do 112x112 BMP i zamienia na RGB565 (little endian)."""
    if url in _art_cache:
        return _art_cache[url]
    tmp = tempfile.mkdtemp(prefix="dialft_")
    jpg, bmp = os.path.join(tmp, "a.jpg"), os.path.join(tmp, "a.bmp")
    try:
        with urllib.request.urlopen(url, timeout=5) as r, open(jpg, "wb") as f:
            f.write(r.read())
        subprocess.run(["sips", "-s", "format", "bmp", "-z", str(ART_H), str(ART_W), jpg, "--out", bmp],
                       capture_output=True, timeout=10, check=True)
        d = open(bmp, "rb").read()
        off = int.from_bytes(d[10:14], "little")
        w = int.from_bytes(d[18:22], "little", signed=True)
        h = int.from_bytes(d[22:26], "little", signed=True)
        bpp = int.from_bytes(d[28:30], "little")
        if w != ART_W or abs(h) != ART_H or bpp not in (24, 32):
            raise ValueError("bmp %dx%d %d" % (w, h, bpp))
        bpx = bpp // 8
        stride = (w * bpx + 3) & ~3
        out = bytearray(w * abs(h) * 2)
        o = 0
        for y in range(abs(h)):
            row = off + (y if h < 0 else abs(h) - 1 - y) * stride
            for x in range(w):
                b, g, r = d[row + x * bpx], d[row + x * bpx + 1], d[row + x * bpx + 2]
                v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                out[o] = v & 0xFF
                out[o + 1] = v >> 8
                o += 2
        _art_cache.clear()
        _art_cache[url] = bytes(out)
        return _art_cache[url]
    except Exception as e:
        log("okladka:", e)
        return None
    finally:
        for f in (jpg, bmp):
            try:
                os.remove(f)
            except OSError:
                pass
        try:
            os.rmdir(tmp)
        except OSError:
            pass


# ---------------------------------------------------------------- aktywna aplikacja / czas
def front_bundle():
    """Identyfikator aplikacji na wierzchu (bundle id), przez systemowe lsappinfo."""
    try:
        asn = subprocess.run(["lsappinfo", "front"], capture_output=True, text=True, timeout=1).stdout.strip()
        out = subprocess.run(["lsappinfo", "info", "-only", "bundleid", asn], capture_output=True, text=True, timeout=1).stdout
        return out.split("=")[-1].strip().strip('"')
    except Exception:
        return ""


def time_line():
    now = time.time()
    tz_min = int(round(time.localtime(now).tm_gmtoff / 60))   # z czasem letnim (CEST = +120, CET = +60)
    return "TIME %d %d" % (int(now), tz_min)


# ---------------------------------------------------------------- main
def main():
    log("DIAL FT Helper %s" % VERSION)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind(("127.0.0.1", RELAY_PORT))
    except OSError:
        sys.exit("Helper juz dziala (port %d zajety)." % RELAY_PORT)
    srv.listen(4)
    srv.setblocking(False)
    clients = {}           # socket -> bufor
    ser, buf = None, b""
    last_search = last_np = 0.0
    now_playing = None
    sent_off = False
    last_time = last_front = 0.0
    front = ""

    def to_dial(line):
        nonlocal ser
        if ser is None:
            return
        try:
            ser.write((line + "\n").encode())
        except OSError:
            log("DIAL FT odlaczony")
            ser.close()
            ser = None

    def to_clients(line):
        for c in list(clients):
            try:
                c.sendall((line + "\n").encode())
            except OSError:
                c.close()
                clients.pop(c, None)

    def send_art(track_id):
        if not now_playing or now_playing["id"] != track_id or not now_playing["art"]:
            return
        data = artwork_rgb565(now_playing["art"])
        if not data:
            return
        n = (len(data) + CHUNK - 1) // CHUNK
        to_dial("NP ART %s %d %d %d" % (track_id, ART_W, ART_H, n))
        for i in range(n):
            to_dial("NP A %d %s" % (i, base64.b64encode(data[i * CHUNK:(i + 1) * CHUNK]).decode()))
            if i % 8 == 7:
                time.sleep(0.004)   # tempo: bufor odbiorczy DIAL ma 8 kB
        to_dial("NP END %s" % track_id)

    while True:
        now = time.time()
        if ser is None and now - last_search > 2.0:
            last_search = now
            ser = find_dial()
            if ser:
                buf = b""
                sent_off = False
                last_time = 0.0
                front = ""
                log("polaczono z DIAL FT:", ser.name)

        rlist = [srv] + list(clients) + ([ser.fd] if ser else [])
        try:
            ready, _, _ = select.select(rlist, [], [], 0.05)
        except (OSError, ValueError):
            ready = []
        for r in ready:
            if r is srv:
                try:
                    c, _ = srv.accept()
                    c.setblocking(False)
                    clients[c] = b""
                except OSError:
                    pass
            elif ser and r == ser.fd:
                data = ser.read()
                if not data:
                    log("DIAL FT odlaczony")
                    ser.close()
                    ser = None
                    continue
                buf += data
            elif r in clients:
                try:
                    data = r.recv(4096)
                except OSError:
                    data = b""
                if not data:
                    r.close()
                    clients.pop(r, None)
                    continue
                clients[r] += data
                while b"\n" in clients[r]:
                    line, clients[r] = clients[r].split(b"\n", 1)
                    line = line.decode(errors="ignore").strip()
                    if line == "HELLO":
                        r.sendall(b"DIAL FT relay\n")
                    elif line == "STATUS":
                        to_dial(line)

        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode(errors="ignore").strip()
            if line.startswith("DIAL FT "):
                to_clients(line)
            elif line.startswith("NP NEED "):
                send_art(line[8:].strip())

        if ser and now - last_time > 600:   # zegar DIAL (tarcza, statystyki Pomodoro)
            last_time = now
            to_dial(time_line())
        if ser and now - last_front > 0.8:   # automatyczny profil wg aplikacji na wierzchu
            last_front = now
            f = front_bundle()
            if f and f != front:
                front = f
                to_dial("APP " + f)

        if now - last_np > 1.0:
            last_np = now
            np_ = spotify_now()
            if np_ is None:
                now_playing = None
                if not sent_off:
                    to_dial("NP OFF")
                    sent_off = True
            else:
                sent_off = False
                now_playing = np_
                to_dial("NP META %d %d %d %s\t%s\t%s" % (np_["playing"], np_["pos"], np_["dur"], np_["id"] or "-",
                                                          np_["title"], np_["artist"]))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
