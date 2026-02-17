#!/usr/bin/env python3

import os
import sys
import shutil
import glob
from pathlib import Path


def copy_files(source_dir, target_dir, file_prefix, include_map=False):
    try:
        os.makedirs(target_dir, exist_ok=True)

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


def clean_target_directory(target_dir, file_prefix):
    try:
        if not os.path.exists(target_dir):
            return

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
    if len(sys.argv) != 4:
        print("Usage: python copy_files.py <build_type> <source_dir> <target_dir>")
        print("  build_type: 'release' or 'debug'")
        print("  source_dir: Source directory containing built files")
        print("  target_dir: Target directory to copy files to")
        sys.exit(1)

    build_type = sys.argv[1].lower()
    source_dir = sys.argv[2]
    target_dir = sys.argv[3]

    if build_type not in ['release', 'debug']:
        print("Error: build_type must be 'release' or 'debug'")
        sys.exit(1)

    file_prefix = 'defcon'
    include_map = (build_type == 'debug')

    clean_target_directory(target_dir, file_prefix)

    success = copy_files(source_dir, target_dir, file_prefix, include_map)

    if success:
        print("File copying completed successfully!")
        sys.exit(0)
    else:
        print("File copying failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()
