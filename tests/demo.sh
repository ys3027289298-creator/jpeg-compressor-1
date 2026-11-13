#!/usr/bin/env bash
# Non-interactive demo used for the segmented screen recording.
# Each section prints a title banner, pauses, then runs one verification step.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

pause() { sleep 2; }

title() {
    echo
    echo "============================================================"
    echo "  $1"
    echo "============================================================"
    pause
}

title "Segment 1/5: Build the repository entry point (bin/jpge)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j4
pause

title "Segment 2/5: Generate small self-made CMYK/YCCK test assets"
/tmp/venv/bin/python tests/make_assets.py
pause

title "Segment 3/5: CMYK conversion vs stb_image reference (bounded error)"
c++ -O2 -std=c++11 -I. tests/verify_cmyk.cpp jpgd.cpp -o build/verify_cmyk
./build/verify_cmyk 4 tests/assets/cmyk.jpg tests/assets/ycck.jpg \
    tests/assets/rgb.jpg tests/assets/gray.jpg
pause

title "Segment 4/5: Illegal color-space marker degrades, does not crash"
c++ -O2 -std=c++11 -I. tests/test_degrade.cpp jpgd.cpp -o build/test_degrade
./build/test_degrade tests/assets/cmyk.jpg tests/assets/cmyk_badtag.jpg
pause

title "Segment 5/5: Zero regression on old RGB/grayscale paths + CLI modes"
./tests/regression.sh
./bin/jpge -d       tests/assets/cmyk.jpg        output/cmyk_rgb.tga
./bin/jpge -d -cmyk tests/assets/cmyk.jpg        output/cmyk_keep.tga
echo
echo "Artifacts in output/:"
ls -1 output/
pause

echo
echo "DEMO COMPLETE - no keyboard input required"
