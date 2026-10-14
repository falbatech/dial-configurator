#!/usr/bin/env python3
"""Konwertuje logo (np. webp) do RGB565 -> src/logo.h (domyslnie 196x139).
Uzycie: python tools/convert_logo.py [wejscie] [wyjscie]
Wymaga: pip install pillow"""
import sys
from PIL import Image

src = sys.argv[1] if len(sys.argv) > 1 else "assets/falbatech-logo-original.webp"
dst = sys.argv[2] if len(sys.argv) > 2 else "src/logo.h"
W, H = 196, 139

img = Image.open(src).convert("RGBA")
img.thumbnail((W, H), Image.LANCZOS)
canvas = Image.new("RGB", (W, H), (0, 0, 0))
canvas.paste(img, ((W - img.width) // 2, (H - img.height) // 2), img)

vals = []
for r, g, b in canvas.getdata():
    vals.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

with open(dst, "w") as f:
    f.write(f"#pragma once\n#include <stdint.h>\nstatic const int LOGO_W = {W};\nstatic const int LOGO_H = {H};\n")
    f.write("static const uint16_t LOGO_DATA[] = {\n")
    for i in range(0, len(vals), 12):
        f.write("  " + ", ".join(f"0x{v:04X}" for v in vals[i:i + 12]) + ",\n")
    f.write("};\n")
print(f"OK: {dst} ({W}x{H})")
