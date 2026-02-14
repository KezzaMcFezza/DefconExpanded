#!/usr/bin/env python3
import os
import geopandas as gpd
from shapely.geometry import Polygon, LineString, box

def polylines_to_polygons_with_frame(input_shp, output_shp):
    """
    Convert coastline polylines to polygons by adding a world frame.
    """
    print(f"Reading {input_shp}...")
    gdf = gpd.read_file(input_shp)
    
    print(f"Found {len(gdf)} total features")
    
    # Filter out lakes if featurec_1 exists
    if 'featurec_1' in gdf.columns:
        print("Filtering out lakes...")
        original = len(gdf)
        gdf = gdf[~gdf['featurec_1'].str.contains('Lake|Reservoir', case=False, na=False)]
        print(f"Removed {original - len(gdf)} lake features")
    
    # Get all coastline geometries
    all_lines = []
    for geom in gdf.geometry:
        if geom.geom_type == 'LineString':
            all_lines.append(geom)
        elif geom.geom_type == 'MultiLineString':
            all_lines.extend(list(geom.geoms))
    
    print(f"Processing {len(all_lines)} coastline segments...")
    
    # Add world boundary frame (WGS84: -180 to 180, -90 to 90)
    # Create a box around the entire world
    frame_coords = [
        (-180, -90), (180, -90),  # Bottom edge
        (180, 90), (-180, 90),    # Top edge  
        (-180, -90)               # Close the box
    ]
    world_frame = LineString(frame_coords)
    all_lines.append(world_frame)
    
    print("Added world boundary frame")
    print("Polygonizing...")
    
    # Now polygonize with the frame included
    from shapely.ops import polygonize
    polygons = list(polygonize(all_lines))
    
    print(f"Created {len(polygons)} polygons")
    
    if not polygons:
        print("ERROR: No polygons created!")
        return
    
    # The largest polygon will be the ocean - we want to keep everything EXCEPT that
    # Sort polygons by area
    polygons_with_area = [(p, p.area) for p in polygons]
    polygons_with_area.sort(key=lambda x: x[1], reverse=True)
    
    # Skip the largest (ocean) polygon
    land_polygons = [p for p, area in polygons_with_area[1:]]
    
    print(f"Filtered to {len(land_polygons)} land polygons (removed ocean)")
    
    if not land_polygons:
        print("ERROR: No land polygons found!")
        return
    
    # Create GeoDataFrame
    gdf_poly = gpd.GeoDataFrame(geometry=land_polygons, crs=gdf.crs)
    gdf_poly.to_file(output_shp)
    print(f"Saved to {output_shp}")

if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    input_path = os.path.join(script_dir, 'Shapefiles', 'coastlines', 'CoastlinesNew3.shp')
    output_path = os.path.join(script_dir, 'Shapefiles', 'coastlines', 'Coastlines_poly.shp')
    
    if not os.path.exists(input_path):
        print(f"Error: Input file not found at {input_path}")
    else:
        polylines_to_polygons_with_frame(input_path, output_path)