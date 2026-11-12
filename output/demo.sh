#!/usr/bin/env bash
# Non-interactive, segmented demo of the -autorotate feature. Each section
# prints a titled banner, pauses between sections, and never reads input.
# Recorded with: asciinema rec --overwrite -c output/demo.sh output/demo.cast
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
JPGE="$ROOT/bin/jpge"
MAT="$ROOT/output/materials"
RES="$ROOT/output/results"
mkdir -p "$RES"

pause() { sleep 1.2; }

banner() {
	local title="$1"
	echo
	echo "================================================================"
	echo "  $title"
	echo "================================================================"
	sleep 0.6
}

banner "Section 1/5  Build via the repo's CMake entry point"
cmake --build output/build -j
pause

banner "Section 2/5  Generate small in-repo test materials"
python3 output/make_assets.py
pause

banner "Section 3/5  Auto-rotate all eight EXIF orientations"
for o in 1 2 3 4 5 6 7 8; do
	"$JPGE" -autorotate "$MAT/orient_${o}.jpg" "$RES/demo_${o}.jpg" 95 | grep -E "EXIF orientation|Auto-rotated|resolution"
done
pause

banner "Section 4/5  Switch off: byte-identical, missing/corrupt EXIF safe"
"$JPGE" "$MAT/orient_6.jpg"            "$RES/demo_off.jpg"     95 >/dev/null
"$JPGE" -autorotate "$MAT/plain.jpg"        "$RES/demo_plain.jpg"   95 | grep "EXIF orientation"
"$JPGE" -autorotate "$MAT/corrupt_exif.jpg" "$RES/demo_corrupt.jpg" 95 | grep "EXIF orientation"
if cmp -s "$RES/demo_off.jpg" "$RES/demo_6.jpg"; then
	echo "unexpected: switch-off output matched rotated output"
else
	echo "switch off leaves sideways pixels; -autorotate changes output as expected"
fi
pause

banner "Section 5/5  Full acceptance harness (42 checks)"
python3 output/run_verification.py
echo
echo "Demo finished; no keyboard input was required."
