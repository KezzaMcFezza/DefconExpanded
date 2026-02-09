#!/usr/bin/env python3

import os
import sys
import shutil
import glob
from pathlib import Path


def copy_files(source_dir, target_dir, file_prefix, include_map=False, exact_names=False):
    try:
        os.makedirs(target_dir, exist_ok=True)
        
        if exact_names:
            patterns = [
                f"{file_prefix}.js",
                f"{file_prefix}.wasm",
                f"{file_prefix}.data"
            ]
            if include_map:
                patterns.append(f"{file_prefix}.wasm.map")
        else:
            patterns = [
                f"{file_prefix}_*.js",
                f"{file_prefix}_*.wasm",
                f"{file_prefix}_*.data"
            ]
            if include_map:
                patterns.append(f"{file_prefix}_*.wasm.map")
        
        copied_files = []
        missing_files = []
        
        for pattern in patterns:
            source_pattern = os.path.join(source_dir, pattern)
            matching_files = glob.glob(source_pattern)
            
            if matching_files:
                for file_path in matching_files:
                    filename = os.path.basename(file_path)
                    target_path = os.path.join(target_dir, filename)
                    
                    try:
                        shutil.copy2(file_path, target_path)
                        copied_files.append(filename)
                        print(f"Copied: {filename}")
                    except Exception as e:
                        print(f"Error copying {filename}: {e}")
                        return False
            else:
                if not pattern.endswith("*.data"):
                    missing_files.append(pattern)
        
        if copied_files:
            print(f"Successfully copied {len(copied_files)} files")
        
        if missing_files:
            print(f"Warning: Missing files: {', '.join(missing_files)}")
            if any(not pattern.endswith("*.data") for pattern in missing_files):
                print("ERROR: Required files are missing!")
                return False
        
        return True
        
    except Exception as e:
        print(f"Something went very wrong, and it's my fault: {e}")
        return False


def clean_target_directory(target_dir, file_prefix, exact_names=False):
    try:
        if not os.path.exists(target_dir):
            return
        
        if exact_names:
            patterns = [
                f"{file_prefix}.js",
                f"{file_prefix}.wasm",
                f"{file_prefix}.data",
                f"{file_prefix}.wasm.map"
            ]
        else:
            patterns = [
                f"{file_prefix}_*.js",
                f"{file_prefix}_*.wasm",
                f"{file_prefix}_*.data",
                f"{file_prefix}_*.wasm.map"
            ]
        
        for pattern in patterns:
            pattern_path = os.path.join(target_dir, pattern)
            for file_path in glob.glob(pattern_path):
                os.remove(file_path)
                    
    except Exception as e:
        print(f"Target directory cleaning failed! {e}")


def main():
    if len(sys.argv) != 5:
        print("Usage: python copy_files.py <project_type> <build_type> <source_dir> <target_dir>")
        print("  project_type: 'replay', 'sync', or 'vanilla'")
        print("  build_type: 'release' or 'debug'")
        print("  source_dir: Source directory containing built files")
        print("  target_dir: Target directory to copy files to")
        sys.exit(1)
    
    project_type = sys.argv[1].lower()
    build_type = sys.argv[2].lower()
    source_dir = sys.argv[3]
    target_dir = sys.argv[4]
    
    if project_type not in ['replay', 'sync', 'vanilla']:
        print("Error: project_type must be 'replay', 'sync', or 'vanilla'")
        sys.exit(1)
    
    if build_type not in ['release', 'debug']:
        print("Error: build_type must be 'release' or 'debug'")
        sys.exit(1)
    
    if project_type == 'replay':
        file_prefix = 'replay_viewer'
        exact_names = False
    elif project_type == 'sync':
        file_prefix = 'sync_practice'
        exact_names = False
    else:
        file_prefix = 'defcon'
        exact_names = False
    
    include_map = (build_type == 'debug')
    
    clean_target_directory(target_dir, file_prefix, exact_names)
    
    success = copy_files(source_dir, target_dir, file_prefix, include_map, exact_names)
    
    if success:
        print("File copying completed successfully!")
        sys.exit(0)
    else:
        print("File copying failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()