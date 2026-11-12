#!/usr/bin/env python3
# Acceptance harness for the -autorotate feature. Everything is driven through
# the repo's own jpge entry point; no third-party libraries are required.
#
# Checks:
#  1. All eight EXIF orientations decode to the expected upright pixels.
#  2. Switch off (default) produces byte-identical JPEG output.
#  3. Missing / corrupt / truncated EXIF is treated as normal orientation 1.
#  4. The memory compression path (-m) gives identical results to the file path.

import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.dirname(os.path.abspath(__file__))
MAT = os.path.join(HERE, "materials")
WORK = os.path.join(HERE, "results")
JPGE = os.path.join(ROOT, "bin", "jpge")
W, H = 16, 16

# Interior sample points per quadrant (kept clear of the midlines and outer
# edges, where 4:2:0 chroma subsampling can bleed neighboring colors).
SAMPLE_POINTS = [(3, 3), (12, 3), (3, 12), (12, 12)]

PASS, FAIL = 0, 0


def check(name, condition, detail=""):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  [PASS] {name}")
    else:
        FAIL += 1
        print(f"  [FAIL] {name} {detail}")


def read_tga(path):
    """Returns (width, height, rows) with rows normalized to top-to-bottom.
    Handles uncompressed and RLE TGAs at 24/32 bpp with either origin."""
    data = open(path, "rb").read()
    image_type = data[2]
    width = data[12] | (data[13] << 8)
    height = data[14] | (data[15] << 8)
    bpp = data[16]
    origin_bottom = (data[17] & 0x20) == 0
    assert bpp in (24, 32), f"unexpected bpp {bpp}"
    ch = bpp // 8
    assert image_type in (2, 10), f"unsupported TGA image type {image_type}"

    stream = data[18:]
    pos = 0

    def next_pixel_bytes():
        nonlocal pos
        px = stream[pos:pos + ch]
        pos += ch
        return px

    stored_rows = []
    if image_type == 2:  # uncompressed
        for _ in range(height):
            row = []
            for _ in range(width):
                b, g, r = next_pixel_bytes()[:3]
                row.append((r, g, b))
            stored_rows.append(row)
    else:  # RLE
        while len(stored_rows) < height:
            row = []
            while len(row) < width:
                header = stream[pos]
                pos += 1
                count = (header & 0x7F) + 1
                if header & 0x80:  # RLE packet
                    px = next_pixel_bytes()
                    for _ in range(count):
                        row.append((px[2], px[1], px[0]))
                else:  # raw packet
                    for _ in range(count):
                        b, g, r = next_pixel_bytes()[:3]
                        row.append((r, g, b))
            stored_rows.append(row)

    if origin_bottom:
        stored_rows.reverse()
    return width, height, stored_rows


def expected_pixels(orientation):
    """Reference orientation implementation, mirroring exif_orientation.h."""
    _, _, base = read_tga(os.path.join(MAT, "base.tga"))
    swaps = orientation in (5, 6, 7, 8)
    dw, dh = (H, W) if swaps else (W, H)
    out = [[None] * dw for _ in range(dh)]
    for y in range(H):
        for x in range(W):
            if orientation == 1:
                ox, oy = x, y
            elif orientation == 2:
                ox, oy = W - 1 - x, y
            elif orientation == 3:
                ox, oy = W - 1 - x, H - 1 - y
            elif orientation == 4:
                ox, oy = x, H - 1 - y
            elif orientation == 5:
                ox, oy = y, x
            elif orientation == 6:
                ox, oy = y, W - 1 - x
            elif orientation == 7:
                ox, oy = H - 1 - y, x
            else:  # 8
                ox, oy = H - 1 - y, W - 1 - x
            out[oy][ox] = base[y][x]
    return dw, dh, out


def run_jpge(src, dst, *extra):
    cmd = [JPGE, *extra, src, dst, "95"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr)
        raise AssertionError(f"jpge failed: {' '.join(cmd)}")
    return res.stdout


def decode(jpg_path, tga_path):
    subprocess.run([JPGE, "-d", jpg_path, tga_path], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    return read_tga(tga_path)


def pixels_close(got, expected, tol=30):
    """Composes only quadrant-interior sample points of both images."""
    if len(got) != len(expected) or len(got[0]) != len(expected[0]):
        return False
    for (x, y) in SAMPLE_POINTS:
        if any(abs(a - b) > tol for a, b in zip(got[y][x], expected[y][x])):
            return False
    return True


def main():
    os.makedirs(WORK, exist_ok=True)

    print("== Section 1: all eight EXIF orientations ==")
    for o in range(1, 9):
        src = os.path.join(MAT, f"orient_{o}.jpg")
        jpg = os.path.join(WORK, f"auto_{o}.jpg")
        tga = os.path.join(WORK, f"auto_{o}.tga")
        log = run_jpge(src, jpg, "-autorotate")
        dw, dh, rows = decode(jpg, tga)
        ew, eh, expected = expected_pixels(o)
        check(f"orientation {o}: dimensions {dw}x{dh} == {ew}x{eh}",
              (dw, dh) == (ew, eh))
        check(f"orientation {o}: pixels match upright reference",
              pixels_close(rows, expected))
        check(f"orientation {o}: log reports tag {o}",
              f"EXIF orientation: {o}" in log)

    print("== Section 2: big-endian Exif (orientation 6) ==")
    jpg = os.path.join(WORK, "auto_6_be.jpg")
    tga = os.path.join(WORK, "auto_6_be.tga")
    run_jpge(os.path.join(MAT, "orient_6_be.jpg"), jpg, "-autorotate")
    dw, dh, rows = decode(jpg, tga)
    ew, eh, expected = expected_pixels(6)
    check("big-endian orientation 6: dimensions", (dw, dh) == (ew, eh))
    check("big-endian orientation 6: pixels", pixels_close(rows, expected))

    print("== Section 3: switch off is byte-identical ==")
    for o in (1, 3, 6, 8):
        src = os.path.join(MAT, f"orient_{o}.jpg")
        off_jpg = os.path.join(WORK, f"off_{o}.jpg")
        on_jpg = os.path.join(WORK, f"on_compare_{o}.jpg")
        run_jpge(src, off_jpg)                       # default: switch absent
        run_jpge(src, on_jpg, "-autorotate")
        same = open(off_jpg, "rb").read() == open(on_jpg, "rb").read()
        if o == 1:
            check(f"orientation {o}: switch on == switch off (no-op)", same)
        else:
            check(f"orientation {o}: switch off output differs from rotated", not same)

    # Direct byte identity: running with/without the switch on orientation-1
    # material must produce the exact same output bytes.
    plain_off = os.path.join(WORK, "plain_off.jpg")
    plain_on = os.path.join(WORK, "plain_on.jpg")
    run_jpge(os.path.join(MAT, "plain.jpg"), plain_off)
    run_jpge(os.path.join(MAT, "plain.jpg"), plain_on, "-autorotate")
    check("plain input: zero byte difference with/without switch",
          open(plain_off, "rb").read() == open(plain_on, "rb").read())

    print("== Section 4: missing / corrupt EXIF treated as normal ==")
    for label, fname in (("missing", "plain.jpg"),
                         ("corrupt", "corrupt_exif.jpg"),
                         ("truncated", "truncated_exif.jpg")):
        on_jpg = os.path.join(WORK, f"{label}_on.jpg")
        off_jpg = os.path.join(WORK, f"{label}_off.jpg")
        log = run_jpge(os.path.join(MAT, fname), on_jpg, "-autorotate")
        run_jpge(os.path.join(MAT, fname), off_jpg)
        check(f"{label} EXIF: handled as orientation 1",
              "EXIF orientation: 1" in log)
        check(f"{label} EXIF: output byte-identical to switch off",
              open(on_jpg, "rb").read() == open(off_jpg, "rb").read())

    print("== Section 5: in-memory compression path (-m) ==")
    mem_jpg = os.path.join(WORK, "mem_auto_6.jpg")
    file_jpg = os.path.join(WORK, "file_auto_6.jpg")
    run_jpge(os.path.join(MAT, "orient_6.jpg"), mem_jpg, "-autorotate", "-m")
    run_jpge(os.path.join(MAT, "orient_6.jpg"), file_jpg, "-autorotate")
    check("-m memory path matches file path byte-for-byte",
          open(mem_jpg, "rb").read() == open(file_jpg, "rb").read())
    tga = os.path.join(WORK, "mem_auto_6.tga")
    dw, dh, rows = decode(mem_jpg, tga)
    ew, eh, expected = expected_pixels(6)
    check("-m output dimensions", (dw, dh) == (ew, eh))
    check("-m output pixels upright", pixels_close(rows, expected))

    # EXIF is parsed from the in-memory file buffer (see
    # exif::read_orientation_from_memory in exif_orientation.h), and the -m
    # run above exercises the fully memory-resident compression path.
    print("== Section 6: artifacts ==")
    check("all eight orientation assets exist",
          all(os.path.exists(os.path.join(MAT, f"orient_{o}.jpg")) for o in range(1, 9)))
    check("damaged metadata assets exist",
          all(os.path.exists(os.path.join(MAT, n)) for n in
              ("plain.jpg", "corrupt_exif.jpg", "truncated_exif.jpg")))

    print(f"\n{FAIL} failed, {PASS} passed")
    sys.exit(1 if FAIL else 0)


if __name__ == "__main__":
    main()
