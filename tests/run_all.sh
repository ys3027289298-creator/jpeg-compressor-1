#!/usr/bin/env bash
# End-to-end verification driven through the repository's own jpge entry
# point plus small focused harnesses. Fully non-interactive.
# All generated artifacts land in output/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BOUND="${RGB_ERROR_BOUND:-4}"

echo "########## 1/5 build ##########"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j4

echo
echo "########## 2/5 test assets (small, self-made) ##########"
PYTHON_BIN="${PYTHON:-python3}"
if ! "$PYTHON_BIN" -c "import PIL" 2>/dev/null; then
    PYTHON_BIN=/tmp/venv/bin/python
fi
"$PYTHON_BIN" tests/make_assets.py

echo
echo "########## 3/5 color conversion vs. stb_image reference ##########"
c++ -O2 -std=c++11 -I. tests/verify_cmyk.cpp jpgd.cpp -o build/verify_cmyk
./build/verify_cmyk "$BOUND" \
    tests/assets/cmyk.jpg tests/assets/ycck.jpg \
    tests/assets/rgb.jpg tests/assets/gray.jpg | tee output/verify_cmyk.log

echo
echo "########## 4/5 illegal color-space marker degradation ##########"
c++ -O2 -std=c++11 -I. tests/test_degrade.cpp jpgd.cpp -o build/test_degrade
./build/test_degrade tests/assets/cmyk.jpg tests/assets/cmyk_badtag.jpg | tee output/test_degrade.log

echo
echo "########## 5/5 zero regression + CLI smoke tests ##########"
./tests/regression.sh | tee output/regression.log

./bin/jpge -d        tests/assets/cmyk.jpg       output/cmyk_rgb.tga      >>output/cli_smoke.log 2>&1
./bin/jpge -d -cmyk  tests/assets/cmyk.jpg       output/cmyk_keep.tga     >>output/cli_smoke.log 2>&1
./bin/jpge -d        tests/assets/ycck.jpg       output/ycck_rgb.tga      >>output/cli_smoke.log 2>&1
./bin/jpge -d        tests/assets/cmyk_badtag.jpg output/badtag_rgb.tga   >>output/cli_smoke.log 2>&1
./bin/jpge -d -s     tests/assets/cmyk.jpg       output/cmyk_stb_ref.tga  >>output/cli_smoke.log 2>&1
echo "CLI smoke tests wrote output/*.tga"

echo
echo "ALL CHECKS PASSED"
