import re
import sys
from pathlib import Path

def extract_version_from_cmake():
    cmake_file = Path(__file__).parent.parent / "CMakeLists.txt"
    
    if not cmake_file.exists():
        print(f"ERROR: CMakeLists.txt not found at {cmake_file}")
        return None
    
    try:
        with open(cmake_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        pattern = r'set\(SYNC_PRACTICE_VERSION\s+"([^"]+)"'
        match = re.search(pattern, content)
        
        if match:
            version = match.group(1)
            print(f"Found version in CMakeLists.txt: {version}")
            return version
        else:
            print("ERROR: Could not find SYNC_PRACTICE_VERSION in CMakeLists.txt")
            return None
            
    except Exception as e:
        print(f"ERROR: Failed to read CMakeLists.txt: {e}")
        return None

def find_html_files():
    sync_dir = Path(__file__).parent.parent / "website" / "sync_practice"
    
    if not sync_dir.exists():
        print(f"ERROR: Sync practice directory not found: {sync_dir}")
        return []
    
    html_files = list(sync_dir.glob("sync_practice_*.html"))
    print(f"Found {len(html_files)} HTML files: {[f.name for f in html_files]}")
    return html_files

def verify_build_files_exist(version):
    sync_dir = Path(__file__).parent.parent / "website" / "sync_practice"
    
    expected_files = [
        f"sync_practice_{version}.js",
        f"sync_practice_{version}.wasm"
    ]
    
    missing_files = []
    for filename in expected_files:
        file_path = sync_dir / filename
        if not file_path.exists():
            missing_files.append(filename)
    
    if missing_files:
        print(f"WARNING: Expected build output files not found: {missing_files}")
        print("Make sure the build completed successfully before running this script.")
        return False
    else:
        print(f"✓ Build output files found for version {version}")
        return True

def update_script_tag(html_file, new_version):
    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        current_pattern = rf'src\s*=\s*"sync_practice_{re.escape(new_version)}\.js"'
        if re.search(current_pattern, content):
            print(f"✓ Script tag in {html_file.name} already points to sync_practice_{new_version}.js")
            return True
        
        pattern = r'(src\s*=\s*")sync_practice_[^"]*\.js(")'
        replacement = rf'\1sync_practice_{new_version}.js\2'
        
        new_content = re.sub(pattern, replacement, content)
        
        if new_content == content:
            print(f"WARNING: No script tag found or updated in {html_file.name}")
            print("Looking for patterns like: src=\"sync_practice_*.js\"")
            
            script_tags = re.findall(r'<script[^>]*src="[^"]*"[^>]*>', content)
            if script_tags:
                print("Found script tags with src attributes:")
                for i, tag in enumerate(script_tags):
                    print(f"  {i+1}: {tag}")
            else:
                print("No script tags with src attributes found")
            
            sync_refs = re.findall(r'sync_practice_[^"]*\.js', content)
            if sync_refs:
                print(f"Found sync_practice references: {sync_refs}")
            
            return False
        
        with open(html_file, 'w', encoding='utf-8') as f:
            f.write(new_content)
        
        print(f"✓ Updated script tag in {html_file.name} to use sync_practice_{new_version}.js")
        return True
        
    except Exception as e:
        print(f"ERROR: Failed to update script tag in {html_file}: {e}")
        return False

def rename_html_file(html_file, new_version):
    try:
        new_name = f"sync_practice_{new_version}.html"
        new_path = html_file.parent / new_name
        
        if html_file.name == new_name:
            print(f"✓ HTML file {html_file.name} already has correct name")
            return True
        
        if new_path.exists():
            print(f"WARNING: Target file {new_name} already exists, removing old file...")
            new_path.unlink()
        
        html_file.rename(new_path)
        print(f"✓ Renamed {html_file.name} to {new_name}")
        return True
        
    except Exception as e:
        print(f"ERROR: Failed to rename {html_file}: {e}")
        return False

def main():
    print("DEFCON Sync Practice Client Version Updater")
    print("=" * 50)
    
    new_version = extract_version_from_cmake()
    if not new_version:
        print("FAILED: Could not extract version from CMakeLists.txt")
        sys.exit(1)
    
    if not verify_build_files_exist(new_version):
        print("FAILED: Build output files not found")
        sys.exit(1)
    
    html_files = find_html_files()
    if not html_files:
        print("FAILED: No sync_practice_*.html files found")
        print("Make sure you have at least one HTML file in website/sync_practice/")
        sys.exit(1)
    
    success_count = 0
    for html_file in html_files:
        print(f"\nProcessing {html_file.name}...")
        
        if update_script_tag(html_file, new_version):
            if rename_html_file(html_file, new_version):
                success_count += 1
            else:
                print(f"Failed to rename {html_file.name}")
        else:
            print(f"Failed to update script tag in {html_file.name}")
    
    print("\n" + "=" * 50)
    if success_count > 0:
        print(f"SUCCESS: Updated {success_count} HTML file(s) to version {new_version}")
        print(f"✓ Script tags now point to: sync_practice_{new_version}.js")
        print(f"✓ HTML files renamed to: sync_practice_{new_version}.html")
    else:
        print("FAILED: No files were successfully updated")
        sys.exit(1)

if __name__ == "__main__":
    main() 