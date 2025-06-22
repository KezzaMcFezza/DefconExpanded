import numpy as np
from PIL import Image
import os
import sys

def hex_to_rgb(hex_color):
    """Convert hex color string to RGB tuple"""
    hex_color = hex_color.lstrip('#')
    if len(hex_color) == 6:
        return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
    else:
        # Handle possible typo/incorrect format
        print(f"Warning: Invalid hex color format: {hex_color}. Using green instead.")
        return (0, 255, 0)

def color_map(image_path, colors, output_base="scotland"):
    """
    Change the color of all non-transparent pixels in the image to each of the colors
    
    Args:
        image_path: Path to the outline image
        colors: List of hex color codes
        output_base: Base name for output files (e.g., "scotland", "england", etc.)
    """
    try:
        # Load the image with alpha channel
        img = Image.open(image_path).convert('RGBA')
        img_array = np.array(img)
        
        # Print image information
        height, width, channels = img_array.shape
        print(f"Image dimensions: {width}x{height}, Channels: {channels}")
        
        # Create a mask of non-transparent pixels
        # Any pixel with alpha > 0 is considered visible
        visible_mask = img_array[:,:,3] > 0
        
        # Count visible pixels for verification
        visible_count = np.sum(visible_mask)
        print(f"Found {visible_count} visible pixels to colorize")
        
        # Process each color
        for hex_color in colors:
            # Convert hex to RGB
            rgb_color = hex_to_rgb(hex_color)
            
            print(f"Processing color: {hex_color} (RGB: {rgb_color})")
            
            # Create a copy of the original image
            result_array = img_array.copy()
            
            # Set RGB channels of all visible pixels to the new color
            # While preserving the original alpha channel
            result_array[visible_mask, 0] = rgb_color[0]  # R
            result_array[visible_mask, 1] = rgb_color[1]  # G
            result_array[visible_mask, 2] = rgb_color[2]  # B
            # Alpha channel remains unchanged
            
            # Convert back to image
            result_img = Image.fromarray(result_array)
            
            # Save with appropriate filename
            output_filename = f"{output_base}{hex_color}.png"
            result_img.save(output_filename)
            print(f"Saved {output_filename}")
        
        print("All colors processed successfully!")
        return True
    
    except Exception as e:
        print(f"Error processing image: {e}")
        import traceback
        traceback.print_exc()
        return False

def print_usage():
    print("\nUsage:")
    print("python colorchanger.py <image_path> [output_base]")
    print("\nExample:")
    print("python colorchanger.py england.png england")
    print("\nThis will create: england00bf00.png, england00e5ff.png, etc.")
    print("\nIf output_base is not provided, it will default to 'scotland'")

# Main execution
if __name__ == "__main__":
    # List of colors to use
    colors = [
        "00bf00",  # Green
        "00e5ff",  # Cyan
        "3d5cff",  # Blue
        "e5cb00",  # Yellow
        "ff4949",  # Red
        "ffa700"   # Orange
    ]
    
    # Get command line arguments
    if len(sys.argv) < 2:
        # No arguments provided, ask for inputs
        print_usage()
        image_path = input("Enter the path to the image: ")
        output_base = input("Enter base name for output files (default: scotland): ")
        if not output_base:
            output_base = "scotland"
    elif len(sys.argv) == 2:
        # Only image path provided
        image_path = sys.argv[1]
        output_base = "scotland"  # Default
    else:
        # Both image path and output base provided
        image_path = sys.argv[1]
        output_base = sys.argv[2]
    
    # Validate image path
    if not os.path.exists(image_path):
        print(f"Error: The file {image_path} does not exist.")
    else:
        print(f"Processing {image_path} with output base name: {output_base}")
        color_map(image_path, colors, output_base)