#!/usr/bin/env python3
"""
Generate gridlines_low.dat, gridlines_medium.dat, gridlines_high.dat from a bmp file.

Spacing matches globe_renderer.h:
  GLOBE_GRIDLINE_SPACING_DEG_LOW    = 20°
  GLOBE_GRIDLINE_SPACING_DEG_MEDIUM = 10°
  GLOBE_GRIDLINE_SPACING_DEG_HIGH   = 5°

BMP files are stored bottom-up; Defcon's WorldRenderer buffer has row 0 = bottom. 
PIL loads BMP and flips to top-down (row 0 = top). So we sample at py_pil = height - 1 - py_game.

Output format matches coastlines.dat and international.dat:
  "b" = new polyline (one ocean segment)
  "lon lat" = vertex (space separated degrees)

Example:
  python tools/python/generate_gridlines_dat.py localisation/data/earth/territory.bmp
  
Important:
  The BMP file must match Defcon's expected aspect ratio of 512 x 285. Just multiply the resolution by
  any amount. If you are using this script i assume you understand this already but just in case :)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Requires Pillow: pip install Pillow", file=sys.stderr)
    sys.exit(1)

# Match Defcon: land = (r,g,b) > 20
LAND_THRESHOLD = 20
SAMPLE_STEP_DEG = 0.25
BOUNDARY_EPSILON_DEG = 0.00001
MAX_BOUNDARY_ITER = 32
SPACING_DEG_LOW = 20.0
SPACING_DEG_MEDIUM = 10.0
SPACING_DEG_HIGH = 5.0


def pixel_to_lon_lat(px, py, width, height):
    lon = 360.0 * (px / (width - 1)) - 180.0 if width > 1 else 0.0
    lat = 200.0 * (py / (height - 1)) - 100.0 if height > 1 else 0.0
    return lon, lat


def lon_lat_to_pixel(lon, lat, width, height):
    px = (lon + 180.0) / 360.0 * (width - 1)
    py = (lat + 100.0) / 200.0 * (height - 1)
    return px, py


def is_ocean_at(img, lon, lat):
    w, h = img.size
    px = int((lon + 180.0) / 360.0 * w)
    py_game = int((lat + 100.0) / 200.0 * h)
    px = max(0, min(w - 1, px))
    py_game = max(0, min(h - 1, py_game))
    py_pil = h - 1 - py_game
    r, g, b = img.getpixel((px, py_pil))[:3]
    return not (r > LAND_THRESHOLD and g > LAND_THRESHOLD and b > LAND_THRESHOLD)


def find_ocean_boundary(img, lon_a, lat_a, lon_b, lat_b, a_is_ocean):
    lon_lo, lat_lo = lon_a, lat_a
    lon_hi, lat_hi = lon_b, lat_b
    for _ in range(MAX_BOUNDARY_ITER):
        if abs(lon_hi - lon_lo) + abs(lat_hi - lat_lo) <= BOUNDARY_EPSILON_DEG:
            break
        lon_mid = 0.5 * (lon_lo + lon_hi)
        lat_mid = 0.5 * (lat_lo + lat_hi)
        mid_ocean = is_ocean_at(img, lon_mid, lat_mid)
        if a_is_ocean:
            if mid_ocean:
                lon_lo, lat_lo = lon_mid, lat_mid
            else:
                lon_hi, lat_hi = lon_mid, lat_mid
        else:
            if mid_ocean:
                lon_hi, lat_hi = lon_mid, lat_mid
            else:
                lon_lo, lat_lo = lon_mid, lat_mid
    return (lon_lo, lat_lo) if a_is_ocean else (lon_hi, lat_hi)


def build_ocean_segments_along_line(img, fixed_coord, range_start, range_end, range_end_inclusive, is_meridian, sample_step):
    segments = []
    segment = []
    first = True
    was_ocean = False
    prev_lon, prev_lat = 0.0, 0.0

    t = range_start
    while (t <= range_end if range_end_inclusive else t < range_end):
        lon = fixed_coord if is_meridian else t
        lat = t if is_meridian else fixed_coord
        ocean = is_ocean_at(img, lon, lat)

        if ocean:
            if not first and not was_ocean:
                bx, by = find_ocean_boundary(img, prev_lon, prev_lat, lon, lat, False)
                segment.append((bx, by))
            segment.append((lon, lat))
        else:
            if was_ocean:
                bx, by = find_ocean_boundary(img, prev_lon, prev_lat, lon, lat, True)
                segment.append((bx, by))
                if len(segment) >= 2:
                    segments.append(segment)
                segment = []
        prev_lon, prev_lat = lon, lat
        was_ocean = ocean
        first = False
        t += sample_step

    if len(segment) >= 2:
        segments.append(segment)
    return segments


def generate_one_gridlines_file(img, out_path, spacing_deg, sample_step=SAMPLE_STEP_DEG):
    all_segments = []

    x = -180.0
    while x < 180:
        segs = build_ocean_segments_along_line(img, x, -90.0, 90.0, False, True, sample_step)
        all_segments.extend(segs)
        x += spacing_deg

    y = -90.0
    while y <= 90:
        segs = build_ocean_segments_along_line(img, y, -180.0, 180.0, True, False, sample_step)
        all_segments.extend(segs)
        y += spacing_deg

    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        for seg in all_segments:
            f.write("b\n")
            for lon, lat in seg:
                f.write(f"{lon} {lat}\n")

    print(f"  {out_path.name}: {len(all_segments)} segments (spacing {spacing_deg}°)")
    return out_path


def main():
    ap = argparse.ArgumentParser(description="Generate gridlines_low/medium/high.dat from any .bmp")
    ap.add_argument("-o", "--output-dir", default="../../",
                    help="Output directory for the 3 .dat files")
    args = ap.parse_args()

    if not Path(args.input).exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    img = Image.open(args.input).convert("RGB")
    w, h = img.size
    out_dir = Path(args.output_dir)

    print(f"Generating 3 gridline files from {args.input} ({w}x{h})")
    generate_one_gridlines_file(img, out_dir / "gridlines_low.dat", SPACING_DEG_LOW)
    generate_one_gridlines_file(img, out_dir / "gridlines_medium.dat", SPACING_DEG_MEDIUM)
    generate_one_gridlines_file(img, out_dir / "gridlines_high.dat", SPACING_DEG_HIGH)
    print("Done.")


if __name__ == "__main__":
    main()
