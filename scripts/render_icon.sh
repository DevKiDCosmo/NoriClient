#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input.svg> <output.png>" >&2
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
OUTPUT_DIR="$(dirname "$OUTPUT")"
mkdir -p "$OUTPUT_DIR"

if command -v python3 >/dev/null 2>&1; then
  if python3 - <<'PY' >/dev/null 2>&1
import cairosvg
PY
  then
    python3 - "$INPUT" "$OUTPUT" <<'PY'
import sys
from pathlib import Path
import cairosvg

source = Path(sys.argv[1])
target = Path(sys.argv[2])
# Render at a large size so the output works well for app/status icons.
cairosvg.svg2png(url=str(source), write_to=str(target), output_width=1024, output_height=1024)
print(f"Rendered {source} -> {target}")
PY
    exit 0
  fi
fi

if command -v rsvg-convert >/dev/null 2>&1; then
  rsvg-convert -w 1024 -h 1024 "$INPUT" -o "$OUTPUT"
  echo "Rendered $INPUT -> $OUTPUT using rsvg-convert"
  exit 0
fi

if command -v magick >/dev/null 2>&1; then
  magick "$INPUT" -resize 1024x1024 "$OUTPUT"
  echo "Rendered $INPUT -> $OUTPUT using ImageMagick"
  exit 0
fi

if command -v qlmanage >/dev/null 2>&1; then
  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TMP_DIR"' EXIT
  qlmanage -t -s 1024 -o "$TMP_DIR" "$INPUT" >/dev/null 2>&1 || true
  GENERATED="$(find "$TMP_DIR" -name '*.png' | head -n 1 || true)"
  if [[ -n "${GENERATED:-}" && -f "$GENERATED" ]]; then
    cp "$GENERATED" "$OUTPUT"
    echo "Rendered $INPUT -> $OUTPUT using qlmanage"
    exit 0
  fi
fi

echo "Unable to render SVG. Install one of: cairosvg (python3), rsvg-convert, ImageMagick, or use qlmanage on macOS." >&2
exit 1

