# EXIF auto-rotate verification artifacts

Everything here is produced through the repository's own `jpge` entry point
(built from `tga2jpg.cpp`). No third-party Python packages are required.

## Source change

- `../exif_orientation.h`: allocation-free EXIF orientation parser plus an
  in-place orientation fix (cycle-leader permutation; pixels never get copied
  into a second image buffer).
- `../tga2jpg.cpp`: new `-autorotate` option (default off).

## How to reproduce

```sh
cmake -S . -B output/build
cmake --build output/build -j
python3 output/make_assets.py
python3 output/run_verification.py
g++ -std=c++11 -I. output/test_rotation_unit.cpp -o /tmp/test_rotation && /tmp/test_rotation
asciinema rec --overwrite -c "bash output/demo.sh" output/demo.cast
```

## Layout

- `make_assets.py` - builds `materials/`: base TGA/JPG, eight oriented JPEGs,
  a big-endian variant, and missing/corrupt/truncated EXIF cases.
- `run_verification.py` - acceptance harness; outputs under `results/`.
- `test_rotation_unit.cpp` - C++ bijection/parser unit tests.
- `demo.sh` - non-interactive, segmented demo with titled sections and pauses.
- `demo.cast` / `demo.gif` - screen recording of the full run.
- `materials/` - small generated inputs (each JPEG is well under 1 KB).
- `results/verification.log` - captured 42-check run; `unit_test.log`.

## Acceptance coverage

- All eight EXIF orientations decode to the expected upright pixels, including
  a big-endian (Motorola byte-order) Exif sample.
- Switch absent: output is byte-identical to the original tool (verified
  against a clean pre-change build) for normal, oriented, and corrupt inputs.
- Missing/corrupt/truncated EXIF is treated as orientation 1, no failure.
- `-m` memory compression path rotates identically to the file path.
- Rotation works inside the decoded buffer with one pixel-sized scratch and an
  index table; no whole-image pixel copy.
