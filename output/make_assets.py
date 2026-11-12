#!/usr/bin/env python3
# Generates tiny self-contained test assets (no third-party libraries):
#  - base.tga: a 10x8 RGB image with four distinct corner colors and an
#    asymmetric marker so every EXIF orientation is visually unique
#  - base.jpg: reference JPEG produced by the repo's own jpge entry point
#  - orient_1.jpg .. orient_8.jpg: base.jpg with a hand-built Exif APP1
#    carrying each orientation tag (both TIFF endiannesses for orient_6)
#  - corrupt_exif.jpg: Exif APP1 whose payload is truncated/garbage
#  - plain.jpg: JPEG without any APP1 metadata (orientation missing)

import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "materials")
W, H = 16, 16


def write_tga(path, width, height, rgb_rows):
    """rgb_rows: list of rows, each a row-major sequence of (r,g,b) tuples,
    given top-to-bottom. Writes an uncompressed 24bpp TGA, bottom-left origin."""
    header = bytearray(18)
    header[2] = 2            # uncompressed true-color
    header[12] = width & 0xFF
    header[13] = (width >> 8) & 0xFF
    header[14] = height & 0xFF
    header[15] = (height >> 8) & 0xFF
    header[16] = 24          # bits per pixel
    header[17] = 0x00        # bottom-left origin
    with open(path, "wb") as f:
        f.write(bytes(header))
        for row in reversed(rgb_rows):  # TGA is bottom-up for this origin
            for (r, g, b) in row:
                f.write(bytes((b, g, r)))


def build_base_image():
    # Four large solid quadrants with distinct, JPEG-friendly colors. Solid
    # blocks survive 4:2:0 chroma subsampling cleanly away from the midlines,
    # and every one of the eight EXIF orientations lands in a unique corner.
    quad = {
        "tl": (200, 40, 40),    # red
        "tr": (40, 180, 60),    # green
        "br": (40, 80, 200),    # blue
        "bl": (200, 180, 40),   # yellow
    }
    rows = []
    for y in range(H):
        row = []
        for x in range(W):
            top = y < H // 2
            left = x < W // 2
            if top and left:
                px = quad["tl"]
            elif top and not left:
                px = quad["tr"]
            elif not top and left:
                px = quad["bl"]
            else:
                px = quad["br"]
            row.append(px)
        rows.append(row)
    return rows


def exif_app1(orientation, little_endian=True):
    """Builds a minimal but well-formed Exif APP1 segment (marker included)."""
    endian = b"II" if little_endian else b"MM"
    bo = "<" if little_endian else ">"

    # TIFF header: endian + magic 0x002A + IFD0 offset 8.
    tiff = endian + struct.pack(bo + "H", 0x002A) + struct.pack(bo + "I", 8)
    # IFD0 with one entry (Orientation), followed by next-IFD offset 0.
    ifd = struct.pack(bo + "H", 1)
    # tag=0x0112 Orientation, type=3 SHORT, count=1, value inline in first 2 bytes.
    ifd += struct.pack(bo + "HHI", 0x0112, 3, 1)
    ifd += struct.pack(bo + "H", orientation) + b"\x00\x00"
    ifd += struct.pack(bo + "I", 0)
    payload = b"Exif\x00\x00" + tiff + ifd

    seg = b"\xFF\xE1" + struct.pack(">H", len(payload) + 2) + payload
    return seg


def inject_app1(jpeg_bytes, app1):
    assert jpeg_bytes[:2] == b"\xFF\xD8"
    return jpeg_bytes[:2] + app1 + jpeg_bytes[2:]


def main():
    os.makedirs(OUT, exist_ok=True)
    jpge = os.path.join(ROOT, "bin", "jpge")
    if not os.path.exists(jpge):
        sys.exit("bin/jpge not found; build the project first")

    rows = build_base_image()
    tga_path = os.path.join(OUT, "base.tga")
    write_tga(tga_path, W, H, rows)

    base_jpg = os.path.join(OUT, "base.jpg")
    subprocess.run([jpge, tga_path, base_jpg, "95"], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    raw = open(base_jpg, "rb").read()

    # Eight orientations, all little-endian.
    for o in range(1, 9):
        with open(os.path.join(OUT, f"orient_{o}.jpg"), "wb") as f:
            f.write(inject_app1(raw, exif_app1(o, True)))

    # Extra big-endian variant of the common phone orientation 6.
    with open(os.path.join(OUT, "orient_6_be.jpg"), "wb") as f:
        f.write(inject_app1(raw, exif_app1(6, False)))

    # Corrupt Exif: APP1 with the Exif signature but a broken TIFF header.
    bad_payload = b"Exif\x00\x00" + b"XX" + b"\x00" * 16
    bad_seg = b"\xFF\xE1" + struct.pack(">H", len(bad_payload) + 2) + bad_payload
    with open(os.path.join(OUT, "corrupt_exif.jpg"), "wb") as f:
        f.write(inject_app1(raw, bad_seg))

    # Corrupt Exif: valid length/header but an IFD0 offset that points past the
    # segment. The JPEG itself stays decodable; only the metadata is unusable.
    bo = "<"
    bad_tiff = b"II" + struct.pack(bo + "H", 0x002A) + struct.pack(bo + "I", 9999)
    bad2_payload = b"Exif\x00\x00" + bad_tiff + b"\x00" * 8
    trunc_seg = b"\xFF\xE1" + struct.pack(">H", len(bad2_payload) + 2) + bad2_payload
    with open(os.path.join(OUT, "truncated_exif.jpg"), "wb") as f:
        f.write(inject_app1(raw, trunc_seg))

    # Missing EXIF: the plain reference JPEG has no APP1 at all.
    with open(os.path.join(OUT, "plain.jpg"), "wb") as f:
        f.write(raw)

    print(f"Wrote assets to {OUT}")
    for name in sorted(os.listdir(OUT)):
        print(f"  {name}: {os.path.getsize(os.path.join(OUT, name))} bytes")


if __name__ == "__main__":
    main()
