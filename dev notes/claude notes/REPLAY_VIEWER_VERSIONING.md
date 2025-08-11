# DEFCON Replay Viewer Dynamic Versioning System

## Overview
The DEFCON WebAssembly replay viewer uses a dynamic versioning system that automatically updates cache when new versions are built. This ensures users always get the latest version without manual cache clearing.

## How It Works

### Version-Based File Naming
Files are named with version prefixes that match the `REPLAY_VIEWER_VERSION` in CMakeLists.txt:
- `replay_viewer_A1.05.html` 
- `replay_viewer_A1.05.js`
- `replay_viewer_A1.05.wasm`
- `replay_viewer_A1.05.data` (if exists)

### Build System Architecture
The build system now uses a template-based approach:

1. **HTML Template Management**: Build scripts manage the HTML file directly instead of using Emscripten's `--shell-file` option
2. **Automatic Version Extraction**: Build scripts read `REPLAY_VIEWER_VERSION` from CMakeLists.txt
3. **Selective File Copying**: Only `.js`, `.wasm`, and `.data` files are copied from build output
4. **Dynamic HTML Updates**: The existing HTML file is renamed to match the new version and its script tag is updated

### Current Version
```cmake
set(REPLAY_VIEWER_VERSION "A1.05" CACHE STRING "Version prefix for replay viewer")
```

## Build Process

### What Happens During Build

1. **CMake Configuration**: Emscripten generates versioned output files (`replay_viewer_A1.05.*`)
2. **Version Extraction**: Build script extracts version from CMakeLists.txt
3. **File Management**:
   - Cleans up old `.js`, `.wasm`, `.data` files (preserves HTML)
   - Copies new `.js`, `.wasm`, `.data` files from build output
   - Renames existing HTML file to match new version
   - Updates script tag in HTML to reference correct JS file

### Build Commands

**Release Build:**
```bash
build-wasm-replay.bat
```

**Debug Build:**
```bash
build-wasm-replay-debug.bat
```

## Cache Management

### Service Worker Caching
The replay viewer includes a service worker that:
- Automatically caches files with version prefixes
- Supports dynamic versioning (no hardcoded file names)
- Uses cache patterns like `/replay_viewer_.*\.js$/`
- Automatically updates when version changes

### Browser Cache Utilities
Available in browser console:
```javascript
// Check cache status
await DefconReplayCache.status()

// Clear all caches
await DefconReplayCache.clearCache()

// Force cache update
await DefconReplayCache.forceUpdate()
```

## Version Management

### Changing Versions
1. Edit `REPLAY_VIEWER_VERSION` in CMakeLists.txt:
   ```cmake
   set(REPLAY_VIEWER_VERSION "A1.06" CACHE STRING "Version prefix for replay viewer")
   ```

2. Run build script - it will automatically:
   - Rename HTML file to new version
   - Update script tag to point to new JS file
   - Copy new WebAssembly files with correct version

### Version Naming Convention
- Format: `[Letter][Major].[Minor]` 
- Examples: `A1.04`, `B2.1`, `R1.0`
- Letter prefixes can indicate: Alpha (A), Beta (B), Release (R), etc.

## Server Integration

### Dynamic File Discovery
The Node.js server automatically discovers current replay viewer files:

```javascript
function findReplayViewerFiles() {
    // Searches for replay_viewer_*.* files
    // Returns version and file mappings
    // Supports fallback to replay-viewer.html
}
```

### URL Generation
Replay viewer URLs are generated dynamically:
- Pattern: `/replay-viewer/{demo-name}`
- Server injects file loading logic for the specific demo
- Automatically uses current version files

## File Structure

```
website/demo_recordings/
├── replay_viewer_A1.05.html    # Main HTML file (managed by build scripts)
├── replay_viewer_A1.05.js      # WebAssembly JavaScript (from Emscripten)
├── replay_viewer_A1.05.wasm    # WebAssembly binary (from Emscripten)
├── replay_viewer_A1.05.data    # Asset data (from Emscripten, optional)
├── replay-viewer-service-worker.js  # Service worker for caching
└── *.dcrec files...            # Demo recordings
```

## Troubleshooting

### Build Issues
1. **Version extraction fails**: Check CMakeLists.txt format
2. **HTML file not found**: Ensure `replay_viewer_*.html` exists before building
3. **Script tag not updated**: PowerShell may be restricted - check execution policy

### Cache Issues
1. **Old version loading**: Clear browser cache or use `DefconReplayCache.clearCache()`
2. **Service worker not updating**: Hard refresh (Ctrl+F5) or force update
3. **Files not caching**: Check browser developer tools Network tab

### Server Issues
1. **Demo not found**: Check file exists in demo_recordings folder
2. **Version mismatch**: Rebuild with correct version
3. **CORS errors**: Ensure SharedArrayBuffer headers are set

## Development Notes

### Template System
- No longer uses Emscripten's `--shell-file` option
- HTML template is maintained independently 
- Build scripts handle version synchronization
- Allows for more flexible HTML customization

### Performance Optimizations
- Versioned caching reduces repeated downloads
- Service worker enables offline functionality
- SharedArrayBuffer support for WebAssembly threading
- Optimized build flags for production vs debug

### Future Enhancements
- Automatic version bumping
- Build timestamp integration
- Enhanced error handling
- Multiple version support 