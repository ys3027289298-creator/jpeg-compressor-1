#!/usr/bin/env bash
# Zero-regression guard: decode the RGB/grayscale assets with both the
# pristine (HEAD) jpgd and the modified jpgd, and require byte-identical
# output on the pre-existing 1- and 3-component paths.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "== building pristine jpgd from git HEAD =="
mkdir -p "$WORK/pristine"
git -C "$ROOT" archive HEAD jpgd.cpp jpgd.h jpgd_idct.h | tar -x -C "$WORK/pristine"

c++ -O2 -std=c++11 -I"$WORK/pristine" -I"$ROOT" \
    "$WORK/pristine/jpgd.cpp" "$ROOT/tests/regression_decode.cpp" \
    -o "$WORK/regress_old"
c++ -O2 -std=c++11 -I"$ROOT" \
    "$ROOT/jpgd.cpp" "$ROOT/tests/regression_decode.cpp" \
    -o "$WORK/regress_new"

fail=0
for img in "$ROOT/tests/assets/rgb.jpg" "$ROOT/tests/assets/gray.jpg"; do
    "$WORK/regress_old" "$img" "$WORK/old.raw"
    "$WORK/regress_new" "$img" "$WORK/new.raw"
    if cmp -s "$WORK/old.raw" "$WORK/new.raw"; then
        echo "[ OK ] $(basename "$img") byte-identical on old vs. new jpgd"
    else
        echo "[FAIL] $(basename "$img") differs"
        fail=1
    fi
done

if [ "$fail" -eq 0 ]; then
    echo
    echo "REGRESSION: old RGB/grayscale paths unchanged"
fi
exit "$fail"
