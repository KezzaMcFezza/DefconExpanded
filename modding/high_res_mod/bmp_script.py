#!/usr/bin/env python3
import os
import struct
import argparse
from typing import List, Tuple, Dict
from PIL import Image, ImageDraw

def parse_shp(path: str) -> List[Dict[str, list]]:
    """
    Parse a .shp file on disk and return a list of polygons.
    Each polygon record is a dict with keys 'outer' and 'holes', each containing lists of rings.
    """
    polygons = []
    with open(path, 'rb') as f:
        f.read(100)  # skip header
        while True:
            rec_header = f.read(8)
            if not rec_header or len(rec_header) < 8:
                break
            rec_num, rec_length = struct.unpack('>ii', rec_header)
            content = f.read(rec_length * 2)
            shape_type = struct.unpack('<i', content[:4])[0]
            if shape_type != 5:
                continue
            offset = 4 + 32  # skip shape type and bounding box
            num_parts, num_points = struct.unpack('<ii', content[offset:offset+8])
            offset += 8
            parts = [struct.unpack('<i', content[offset+i*4:offset+(i+1)*4])[0]
                     for i in range(num_parts)]
            parts.append(num_points)
            offset += num_parts * 4
            pts = [struct.unpack('<dd', content[offset+i*16:offset+(i+1)*16])
                   for i in range(num_points)]
            rings = [pts[parts[i]:parts[i+1]] for i in range(num_parts)]
            outer, holes = [], []
            for ring in rings:
                area = sum(
                    (ring[i][0] * ring[(i+1) % len(ring)][1] -
                     ring[(i+1) % len(ring)][0] * ring[i][1])
                    for i in range(len(ring))
                )
                if area < 0:
                    outer.append(ring)
                else:
                    holes.append(ring)
            polygons.append({'outer': outer, 'holes': holes})
    return polygons

def lonlat_to_pixel(lon: float, lat: float, width: int, height: int) -> Tuple[int,int]:
    x = int(round((lon + 180.0) / 360.0 * width))
    y = int(round((90.0 - lat) / 180.0 * height))
    return x, y

def rasterise(width: int, height: int,
              land_polys: List[Dict[str, list]],
              lakes_polys: List[Dict[str, list]],
              include_ice: bool,
              ice_polys: List[Dict[str, list]],
              out_path: str) -> None:
    """
    Draw land (and optionally ice) white on a black background; water/lakes remain black.
    """
    img = Image.new('1', (width, height), 0)
    draw = ImageDraw.Draw(img)

    def draw_ring(ring, fill_val):
        pts = [lonlat_to_pixel(x,y,width,height) for x,y in ring]
        draw.polygon(pts, fill=fill_val)

    # land polygons
    for poly in land_polys:
        for ring in poly['outer']:
            draw_ring(ring, 1)
        for ring in poly['holes']:
            draw_ring(ring, 0)

    # optional ice
    if include_ice:
        for poly in ice_polys:
            for ring in poly['outer']:
                draw_ring(ring, 1)
            for ring in poly['holes']:
                draw_ring(ring, 0)

    # lakes overwrite land (black)
    for lake in lakes_polys:
        for ring in lake['outer']:
            draw_ring(ring, 0)
        for ring in lake['holes']:
            draw_ring(ring, 1)  # island inside lake

    img.save(out_path, format='BMP')
    print(f"Saved {out_path}")

def main():
    parser = argparse.ArgumentParser(description="Rasterise shapefiles into BMPs.")
    parser.add_argument('--32k', action='store_true', help='Generate 32k BMP (32768×16384)')
    parser.add_argument('--16k', action='store_true', help='Generate 16k BMP (16384×8192)')
    parser.add_argument('--8k', action='store_true',  help='Generate 8k BMP (8192×4096)')
    parser.add_argument('--4k', action='store_true',  help='Generate 4k BMP (4096×2048)')
    parser.add_argument('--lakes', action='store_true', help='Include lakes (default: no)')
    parser.add_argument('--no-lakes', action='store_true', help='Exclude lakes')
    parser.add_argument('--yes-ice', action='store_true', help='Include Antarctic ice shelves')
    parser.add_argument('--no-ice', action='store_true', help='Exclude Antarctic ice shelves (default)')
    args = parser.parse_args()

    # Determine requested sizes
    size_map = {'4k': (4096, 2048),
                '8k': (8192, 4096),
                '16k': (16384, 8192),
                '32k': (32768, 16384)}
    resolutions = [res for res in size_map if getattr(args, res)]
    if not resolutions:
        parser.error("Select at least one resolution (--4k/--8k/--16k/--32k).")

    # Determine lakes inclusion
    include_lakes = args.lakes and not args.no_lakes

    # Determine ice inclusion
    include_ice = args.yes_ice and not args.no_ice

    # Paths to shapefiles (relative to script)
    base_dir = os.path.dirname(os.path.abspath(__file__))
    shp_dir = os.path.join(base_dir, 'Shapefiles')
    no_ice_dir = os.path.join(shp_dir, 'No Antartic Shelf + Land')
    with_ice_dir = os.path.join(shp_dir, 'Antartic Shelf + Land')
    lakes_dir = os.path.join(shp_dir, 'Lakes')

    # Load land polygons
    land_path = [f for f in os.listdir(no_ice_dir) if f.lower().endswith('.shp')][0]
    land_polys = parse_shp(os.path.join(no_ice_dir, land_path))

    # Load ice polygons only if needed
    ice_polys = []
    if include_ice:
        ice_path = [f for f in os.listdir(with_ice_dir) if f.lower().endswith('.shp')][0]
        ice_polys = parse_shp(os.path.join(with_ice_dir, ice_path))

    # Load lakes polygons only if needed
    lakes_polys = []
    if include_lakes:
        lakes_path = [f for f in os.listdir(lakes_dir) if f.lower().endswith('.shp')][0]
        lakes_polys = parse_shp(os.path.join(lakes_dir, lakes_path))

    # Prepare output directory
    out_dir = os.path.join(base_dir, 'bmp_result')
    os.makedirs(out_dir, exist_ok=True)

    # Rasterise each requested resolution separately (no downscaling)
    for res in resolutions:
        width, height = size_map[res]
        suffix = ''
        suffix += '_ice' if include_ice else '_noice'
        suffix += '_lakes' if include_lakes else '_nolakes'
        out_name = f'earth_{res}{suffix}.bmp'
        rasterise(width, height,
                  land_polys,
                  lakes_polys,
                  include_ice,
                  ice_polys,
                  out_path=os.path.join(out_dir, out_name))

if __name__ == '__main__':
    main()
