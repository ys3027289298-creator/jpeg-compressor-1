#!/usr/bin/env python3
"""Generate tiny, self-made JPEG test assets (no external downloads).

Outputs into tests/assets/:
  rgb.jpg        - ordinary 3-component RGB JPEG (regression guard)
  gray.jpg       - 1-component grayscale JPEG (regression guard)
  cmyk.jpg       - 4-component Adobe CMYK JPEG (APP14 transform = 0)
  ycck.jpg       - same payload as cmyk.jpg but APP14 transform = 2 (YCCK)
  cmyk_badtag.jpg- APP14 with an illegal transform byte (must degrade, not crash)
"""
import os
import struct

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "assets")


def main():
    os.makedirs(ASSETS, exist_ok=True)

    # 8x8 RGB image with a couple of distinct colors.
    rgb = Image.new("RGB", (8, 8))
    rgb.putdata([
        (200, 40, 60), (200, 40, 60), (30, 180, 90), (30, 180, 90),
        (20, 30, 220), (20, 30, 220), (240, 240, 240), (240, 240, 240),
    ] * 8)
    rgb.save(os.path.join(ASSETS, "rgb.jpg"), "JPEG", quality=95)

    gray = rgb.convert("L")
    gray.save(os.path.join(ASSETS, "gray.jpg"), "JPEG", quality=95)

    # 8x8 CMYK image with known ink values (C, M, Y, K in 0..255 ink amount).
    cmyk_colors = [
        (0, 120, 200, 10),
        (200, 0, 90, 30),
        (40, 40, 40, 200),
        (0, 0, 0, 0),
    ]
    cmyk = Image.new("CMYK", (8, 8))
    cmyk.putdata([cmyk_colors[(x // 2) % 4] for _ in range(8) for x in range(8)])
    cmyk_path = os.path.join(ASSETS, "cmyk.jpg")
    cmyk.save(cmyk_path, "JPEG", quality=95)

    data = bytearray(open(cmyk_path, "rb").read())

    def patch_app14_transform(buf, value):
        marker = buf.find(b"Adobe\x00")
        if marker < 0:
            raise RuntimeError("no Adobe APP14 marker found")
        # Layout within segment: "Adobe\0" (6) + version(1) + flags0(2) +
        # flags1(2) + color_transform(1).
        buf[marker + 11] = value

    ycck = bytearray(data)
    patch_app14_transform(ycck, 2)
    open(os.path.join(ASSETS, "ycck.jpg"), "wb").write(ycck)

    bad = bytearray(data)
    patch_app14_transform(bad, 9)  # illegal: only 0/1/2 are defined
    open(os.path.join(ASSETS, "cmyk_badtag.jpg"), "wb").write(bad)

    for name in ("rgb.jpg", "gray.jpg", "cmyk.jpg", "ycck.jpg", "cmyk_badtag.jpg"):
        path = os.path.join(ASSETS, name)
        print("%-16s %5d bytes" % (name, os.path.getsize(path)))


if __name__ == "__main__":
    main()
