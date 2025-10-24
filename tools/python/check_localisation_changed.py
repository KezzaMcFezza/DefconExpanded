#!/usr/bin/env python3

"""
Checksum-based change detection for localisation data files.

This script computes SHA256 checksums of all files in localisation/data/
and compares them with previously stored checksums to determine if a
rebuild of main.dat is necessary.

This prevents unnecessary rebuilds when rar creates archives with different
timestamps/metadata even when file content hasn't changed.

Exit codes:
  0 - Changes detected (or no previous checksums exist) - rebuild needed
  1 - No changes detected - rebuild not needed
  2 - Error occurred (e.g., data directory not found)

Usage:
  Called automatically by build_linux.sh with -r flag.
  To force a rebuild, delete localisation/.localisation_checksums
"""

import os
import sys
import hashlib
import json
from pathlib import Path


def compute_file_hash(filepath):
    """Compute SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    try:
        with open(filepath, "rb") as f:
            # Read in chunks to handle large files efficiently
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        return sha256_hash.hexdigest()
    except Exception as e:
        print(f"Error hashing {filepath}: {e}", file=sys.stderr)
        return None


def compute_directory_checksums(data_dir):
    """
    Recursively compute checksums for all files in the data directory.
    Returns a dictionary mapping relative paths to their SHA256 hashes.
    """
    checksums = {}
    data_path = Path(data_dir)
    
    if not data_path.exists():
        print(f"Error: Data directory not found: {data_dir}", file=sys.stderr)
        sys.exit(2)
    
    # Walk through all files recursively
    for filepath in sorted(data_path.rglob('*')):
        if filepath.is_file():
            # Use relative path from data directory as key
            relative_path = str(filepath.relative_to(data_path))
            file_hash = compute_file_hash(filepath)
            
            if file_hash is not None:
                checksums[relative_path] = file_hash
            else:
                # If we can't hash a file, treat it as a change to be safe
                print(f"Warning: Could not hash {relative_path}, assuming changed", file=sys.stderr)
                return None
    
    return checksums


def load_previous_checksums(checksum_file):
    """Load previously stored checksums from file."""
    if not os.path.exists(checksum_file):
        return None
    
    try:
        with open(checksum_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Warning: Could not load previous checksums: {e}", file=sys.stderr)
        return None


def save_checksums(checksums, checksum_file):
    """Save checksums to file."""
    try:
        with open(checksum_file, 'w') as f:
            json.dump(checksums, f, indent=2, sort_keys=True)
        return True
    except Exception as e:
        print(f"Error: Could not save checksums: {e}", file=sys.stderr)
        return False


def compare_checksums(old_checksums, new_checksums):
    """
    Compare two checksum dictionaries.
    Returns (changed, added, removed) file lists.
    """
    if old_checksums is None:
        # No previous checksums, treat everything as new
        return list(new_checksums.keys()), [], []
    
    old_files = set(old_checksums.keys())
    new_files = set(new_checksums.keys())
    
    # Files that exist in both but have different checksums
    changed = [f for f in old_files & new_files 
               if old_checksums[f] != new_checksums[f]]
    
    # Files that only exist in new
    added = list(new_files - old_files)
    
    # Files that only existed in old
    removed = list(old_files - new_files)
    
    return changed, added, removed


def main():
    # Determine paths
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent.parent
    localisation_dir = project_root / "localisation"
    data_dir = localisation_dir / "data"
    checksum_file = localisation_dir / ".localisation_checksums"
    
    print("Checking for changes in localisation/data/...")
    
    # Compute current checksums
    current_checksums = compute_directory_checksums(data_dir)
    if current_checksums is None:
        print("Error computing checksums, forcing rebuild", file=sys.stderr)
        sys.exit(0)  # Exit 0 means "rebuild needed"
    
    print(f"Computed checksums for {len(current_checksums)} files")
    
    # Load previous checksums
    previous_checksums = load_previous_checksums(checksum_file)
    
    if previous_checksums is None:
        print("No previous checksums found - rebuild needed")
        # Save current checksums for next time
        if save_checksums(current_checksums, checksum_file):
            print(f"Saved checksums to {checksum_file.relative_to(project_root)}")
        sys.exit(0)  # Rebuild needed
    
    # Compare checksums
    changed, added, removed = compare_checksums(previous_checksums, current_checksums)
    
    if changed or added or removed:
        print("\nChanges detected:")
        if changed:
            print(f"  Modified files: {len(changed)}")
            for f in changed[:5]:  # Show first 5
                print(f"    - {f}")
            if len(changed) > 5:
                print(f"    ... and {len(changed) - 5} more")
        
        if added:
            print(f"  Added files: {len(added)}")
            for f in added[:5]:
                print(f"    + {f}")
            if len(added) > 5:
                print(f"    ... and {len(added) - 5} more")
        
        if removed:
            print(f"  Removed files: {len(removed)}")
            for f in removed[:5]:
                print(f"    - {f}")
            if len(removed) > 5:
                print(f"    ... and {len(removed) - 5} more")
        
        print("\nRebuild of main.dat is required")
        
        # Save current checksums for next time
        if save_checksums(current_checksums, checksum_file):
            print(f"Saved new checksums to {checksum_file.relative_to(project_root)}")
        
        sys.exit(0)  # Rebuild needed
    else:
        print("No changes detected - main.dat rebuild not needed")
        sys.exit(1)  # No rebuild needed


if __name__ == "__main__":
    main()
