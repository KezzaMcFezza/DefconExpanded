#!/usr/bin/env python3
"""
Generate a high-resolution equirectangular image of coastlines or internationals from 
their respective .dat files.

Format (same as EarthData::LoadCoastlines):
  - Line starting with 'b' = begin new island/polyline
  - Other lines = "longitude latitude" (space-separated floats, degrees)
  - Longitude in [-180, 180], latitude in [-90, 90]

Equirectangular projection (standard):
  x_pixel = (longitude + 180) / 360 * width
  y_pixel = (90 - latitude) / 180 * height   (lat 90 = top, lat -90 = bottom)

Output: PNG with white coastlines on black. Use -W for resolution (e.g. 8192 -> 8192x4096).

Example (from repo root):
  python tools/python/coastlines_to_image.py localisation/data/earth/coastlines.dat -o coastlines.png -W 16384
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Requires Pillow: pip install Pillow", file=sys.stderr)
    sys.exit(1)


def parse_coastlines(path):
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(path)
    current = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line[0] == "b":
                if current:
                    yield current
                current = []
            else:
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        lon, lat = float(parts[0]), float(parts[1])
                        current.append((lon, lat))
                    except ValueError:
                        continue
    if current:
        yield current


def lon_lat_to_pixel(lon, lat, width, height):
    x = (lon + 180.0) / 360.0 * (width - 1)
    y = (90.0 - lat) / 180.0 * (height - 1)
    return (x, y)


def draw_coastlines(
    input_path,
    output_path,
    width=8192,
    height=None,
    line_width=1,
    white=(255, 255, 255),
    black=(0, 0, 0),
):
    if height is None:
        height = width // 2

    img = Image.new("RGB", (width, height), black)
    draw = ImageDraw.Draw(img)

    islands = list(parse_coastlines(input_path))
    segments_drawn = 0

    for points in islands:
        if len(points) < 2:
            continue
        for i in range(len(points) - 1):
            lon1, lat1 = points[i]
            lon2, lat2 = points[i + 1]
            x1, y1 = lon_lat_to_pixel(lon1, lat1, width, height)
            x2, y2 = lon_lat_to_pixel(lon2, lat2, width, height)

            if abs(lon2 - lon1) > 180:
                if lon1 > lon2:
                    lon1, lon2 = lon2, lon1
                    lat1, lat2 = lat2, lat1
                    x1, x2 = x2, x1
                    y1, y2 = y2, y1
                span = (180 - lon1) + (lon2 + 180)
                t_right = (180 - lon1) / span if span else 0.5
                lat_mid = lat1 + t_right * (lat2 - lat1)
                _, y_mid = lon_lat_to_pixel(180, lat_mid, width, height)
                draw.line([(x1, y1), (width - 1, y_mid)], fill=white, width=line_width)
                draw.line([(0, y_mid), (x2, y2)], fill=white, width=line_width)
            else:
                draw.line([(x1, y1), (x2, y2)], fill=white, width=line_width)
            segments_drawn += 1

    img.save(output_path, "PNG")
    print(f"Rendered {len(islands)} polylines, {segments_drawn} segments -> {output_path} ({width}x{height})")
    return output_path


def main():
    ap = argparse.ArgumentParser(
        description="Generate equirectangular coastline image from *.dat"
    )
    ap.add_argument(
        "input",
        nargs="?",
        default="coastlines.dat",
        help="Path to *.dat (default: coastlines.dat)",
    )
    ap.add_argument(
        "-o", "--output",
        default="coastlines.png",
        help="Output PNG path (default: coastlines.png)",
    )
    ap.add_argument(
        "-W", "--width",
        type=int,
        default=8192,
        help="Image width in pixels (default: 8192)",
    )
    ap.add_argument(
        "-H", "--height",
        type=int,
        default=None,
        help="Image height (default: width/2 for 2:1 equirectangular)",
    )
    ap.add_argument(
        "-w", "--line-width",
        type=int,
        default=1,
        help="Line width in pixels (default: 1)",
    )
    args = ap.parse_args()

    draw_coastlines(
        args.input,
        args.output,
        width=args.width,
        height=args.height,
        line_width=args.line_width,
    )


if __name__ == "__main__":
    main()
