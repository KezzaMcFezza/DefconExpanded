# DEFCON Texture Atlas Implementation Guide

## 📋 Table of Contents

1. [Overview & Goals](#overview--goals)
2. [Atlas Creation](#atlas-creation)
3. [Data Structures](#data-structures)
4. [Renderer Integration](#renderer-integration)
5. [Unit Rendering Updates](#unit-rendering-updates)
6. [Effects & UI Integration](#effects--ui-integration)
7. [Testing & Validation](#testing--validation)
8. [Performance Monitoring](#performance-monitoring)
9. [Migration Strategy](#migration-strategy)

## 🎯 Overview & Goals

### Current State
- **700-1000 draw calls** per frame due to texture switching
- Separate texture files for each unit type
- Texture binding on every sprite change

### Target State  
- **<100 draw calls** per frame
- **4 texture atlases** replacing 40+ individual textures
- **Texture-based batching** with minimal binding changes

### Expected Performance Gain
```
Unit Sprites:     16 calls → 1 call  (94% reduction)
Effects:          20 calls → 1 call  (95% reduction) 
Blur/Highlights:  16 calls → 1 call  (94% reduction)
Small Icons:      30 calls → 1 call  (97% reduction)
TOTAL:           ~80% overall draw call reduction
```

---

## 📐 Atlas Creation

### Step 1: Atlas Layout Planning

Based on your real sprite sizes, create **4 specialized atlases**:

```cpp
// Atlas specifications
Atlas 1: atlas_main_units.png    (1024x1024) - Main unit sprites
Atlas 2: atlas_blur_effects.png  (1024x1024) - Blur/highlight versions  
Atlas 3: atlas_small_icons.png   (512x512)   - 16x16 icons + 64x64 effects
Atlas 4: atlas_explosions.png    (1024x1024) - Animation frames + particles
```

### Step 2: Atlas Layout Template

**Atlas 1 - Main Units (1024x1024):**
```
Grid: 4x4 cells, each 256x256 pixels
Rotation padding: 64px around rotating units

┌─────────┬─────────┬─────────┬─────────┐
│ Fighter │ Bomber  │  Nuke   │ Arrow   │ 0-256
│ (rot)   │ (rot)   │ (rot)   │         │
├─────────┼─────────┼─────────┼─────────┤
│Battleshp│ Carrier │   Sub   │ Fleet   │ 256-512
│         │         │         │         │
├─────────┼─────────┼─────────┼─────────┤
│  Silo   │ Airbase │  Radar  │   SAM   │ 512-768
│         │         │         │         │
├─────────┼─────────┼─────────┼─────────┤
│Population│ Units  │ Spare1  │ Spare2  │ 768-1024
│         │         │         │         │
└─────────┴─────────┴─────────┴─────────┘
```

### Step 3: Atlas Creation Tools

**Option A: Manual Creation (Photoshop/GIMP)**
1. Create 1024x1024 black canvas
2. Draw 256x256 grid guidelines  
3. Center each 128x128 sprite in its cell
4. Add 64px black padding around rotating sprites
5. Export as PNG with alpha

**Option B: Automated Script (Python + PIL)**

```python
from PIL import Image
import os

def create_main_units_atlas():
    # Create 1024x1024 black canvas
    atlas = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
    
    # Unit layout mapping
    units = {
        'fighter.bmp':    (0, 0, True),    # (x, y, needs_rotation_padding)
        'bomber.bmp':     (1, 0, True),
        'nuke.bmp':       (2, 0, True), 
        'arrow.bmp':      (3, 0, False),
        'battleship.bmp': (0, 1, False),
        'carrier.bmp':    (1, 1, False),
        'sub.bmp':        (2, 1, False),
        'fleet.bmp':      (3, 1, False),
        # ... etc
    }
    
    for filename, (grid_x, grid_y, needs_padding) in units.items():
        sprite_path = f"graphics/{filename}"
        if os.path.exists(sprite_path):
            sprite = Image.open(sprite_path)
            
            # Calculate position in atlas
            cell_x = grid_x * 256
            cell_y = grid_y * 256
            
            if needs_padding:
                # Center in 256x256 cell with 64px padding
                sprite_x = cell_x + 64  # 64px padding
                sprite_y = cell_y + 64
            else:
                # Center in 256x256 cell
                sprite_x = cell_x + (256 - sprite.width) // 2
                sprite_y = cell_y + (256 - sprite.height) // 2
            
            atlas.paste(sprite, (sprite_x, sprite_y))
    
    atlas.save("atlas_main_units.png")

# Run atlas creation
create_main_units_atlas()
```

---

## 🗂️ Data Structures

### Atlas Region Definitions

Create a new header file: `atlas_definitions.h`

```cpp
#ifndef _included_atlas_definitions_h
#define _included_atlas_definitions_h

enum AtlasType {
    ATLAS_MAIN_UNITS = 0,
    ATLAS_BLUR_EFFECTS,
    ATLAS_SMALL_ICONS, 
    ATLAS_EXPLOSIONS,
    NUM_ATLAS_TYPES
};

enum UnitAtlasIndex {
    UNIT_ATLAS_FIGHTER = 0,
    UNIT_ATLAS_BOMBER,
    UNIT_ATLAS_NUKE,
    UNIT_ATLAS_ARROW,
    UNIT_ATLAS_BATTLESHIP,
    UNIT_ATLAS_CARRIER,
    UNIT_ATLAS_SUB,
    UNIT_ATLAS_FLEET,
    UNIT_ATLAS_SILO,
    UNIT_ATLAS_AIRBASE,
    UNIT_ATLAS_RADAR,
    UNIT_ATLAS_SAM,
    UNIT_ATLAS_POPULATION,
    UNIT_ATLAS_UNITS,
    NUM_UNIT_ATLAS_ENTRIES
};

struct AtlasRegion {
    float u1, v1, u2, v2;           // Full cell UV coordinates
    float sprite_u1, sprite_v1;     // Sprite UV coordinates (excluding padding)
    float sprite_u2, sprite_v2;
    float center_u, center_v;       // Center point for rotation
    bool needs_rotation_padding;    // Whether this sprite rotates
    int original_width, original_height;  // Original sprite dimensions
};

struct AtlasData {
    unsigned int texture_id;
    int width, height;
    AtlasRegion regions[64];  // Max 64 sprites per atlas
    int region_count;
    char filename[256];
};

// Global atlas data
extern AtlasData g_atlases[NUM_ATLAS_TYPES];

// Unit type to atlas mapping
extern UnitAtlasIndex g_unitTypeToAtlas[WorldObject::NumObjectTypes];

#endif
```

### Atlas Manager Class

Create `atlas_manager.h`:

```cpp
#ifndef _included_atlas_manager_h
#define _included_atlas_manager_h

#include "atlas_definitions.h"
#include "lib/resource/image.h"

class AtlasManager {
private:
    bool m_initialized;
    
public:
    AtlasManager();
    ~AtlasManager();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Atlas loading
    bool LoadAtlas(AtlasType type, const char* filename);
    void SetupAtlasRegions();
    
    // Atlas access
    AtlasData& GetAtlas(AtlasType type);
    AtlasRegion& GetUnitRegion(int unitType);
    AtlasRegion& GetEffectRegion(const char* effectName);
    
    // Texture binding optimization
    void BindAtlas(AtlasType type);
    AtlasType GetCurrentBoundAtlas() const { return m_currentAtlas; }
    
    // Utility functions
    bool IsUnitRotating(int unitType);
    void GetSpriteUVCoords(UnitAtlasIndex index, float& u1, float& v1, float& u2, float& v2);
    void GetRotationCenter(UnitAtlasIndex index, float& center_u, float& center_v);
    
private:
    AtlasType m_currentAtlas;
    void CalculateUVCoordinates();
};

extern AtlasManager* g_atlasManager;

#endif
```

### Implementation: `atlas_manager.cpp`

```cpp
#include "lib/universal_include.h"
#include "atlas_manager.h"
#include "lib/resource/resource.h"
#include "lib/render/renderer.h"

AtlasManager* g_atlasManager = NULL;
AtlasData g_atlases[NUM_ATLAS_TYPES];

// Unit type mapping to atlas indices
UnitAtlasIndex g_unitTypeToAtlas[WorldObject::NumObjectTypes] = {
    UNIT_ATLAS_FIGHTER,     // WorldObject::TypeFighter
    UNIT_ATLAS_BOMBER,      // WorldObject::TypeBomber  
    UNIT_ATLAS_BATTLESHIP,  // WorldObject::TypeBattleship
    UNIT_ATLAS_CARRIER,     // WorldObject::TypeCarrier
    UNIT_ATLAS_SUB,         // WorldObject::TypeSub
    UNIT_ATLAS_NUKE,        // WorldObject::TypeNuke
    UNIT_ATLAS_SILO,        // WorldObject::TypeSilo
    UNIT_ATLAS_RADAR,       // WorldObject::TypeRadarStation
    UNIT_ATLAS_AIRBASE,     // WorldObject::TypeAirBase
    // ... etc for all unit types
};

AtlasManager::AtlasManager()
:   m_initialized(false),
    m_currentAtlas(NUM_ATLAS_TYPES)  // Invalid atlas
{
}

bool AtlasManager::Initialize() {
    if (m_initialized) return true;
    
    // Load all atlases
    bool success = true;
    success &= LoadAtlas(ATLAS_MAIN_UNITS, "atlas_main_units.png");
    success &= LoadAtlas(ATLAS_BLUR_EFFECTS, "atlas_blur_effects.png");
    success &= LoadAtlas(ATLAS_SMALL_ICONS, "atlas_small_icons.png");
    success &= LoadAtlas(ATLAS_EXPLOSIONS, "atlas_explosions.png");
    
    if (success) {
        SetupAtlasRegions();
        m_initialized = true;
    }
    
    return success;
}

bool AtlasManager::LoadAtlas(AtlasType type, const char* filename) {
    AtlasData& atlas = g_atlases[type];
    
    // Load texture using existing resource system
    Image* image = g_resource->GetImage(filename);
    if (!image) {
        AppDebugOut("AtlasManager: Failed to load atlas %s\n", filename);
        return false;
    }
    
    atlas.texture_id = image->m_textureID;
    atlas.width = image->Width();
    atlas.height = image->Height();
    strcpy(atlas.filename, filename);
    
    AppDebugOut("AtlasManager: Loaded atlas %s (%dx%d)\n", filename, atlas.width, atlas.height);
    return true;
}

void AtlasManager::SetupAtlasRegions() {
    // Setup main units atlas (4x4 grid, 256x256 cells)
    AtlasData& mainAtlas = g_atlases[ATLAS_MAIN_UNITS];
    mainAtlas.region_count = NUM_UNIT_ATLAS_ENTRIES;
    
    for (int i = 0; i < NUM_UNIT_ATLAS_ENTRIES; i++) {
        AtlasRegion& region = mainAtlas.regions[i];
        
        // Calculate grid position
        int grid_x = i % 4;
        int grid_y = i / 4;
        
        // Calculate UV coordinates for 256x256 cell
        region.u1 = (float)grid_x / 4.0f;
        region.v1 = (float)grid_y / 4.0f;
        region.u2 = (float)(grid_x + 1) / 4.0f;
        region.v2 = (float)(grid_y + 1) / 4.0f;
        
        // Check if this unit needs rotation padding
        region.needs_rotation_padding = (i == UNIT_ATLAS_FIGHTER || 
                                       i == UNIT_ATLAS_BOMBER || 
                                       i == UNIT_ATLAS_NUKE);
        
        if (region.needs_rotation_padding) {
            // 64px padding = 64/1024 = 0.0625 in UV space
            float padding = 64.0f / 1024.0f;
            region.sprite_u1 = region.u1 + padding;
            region.sprite_v1 = region.v1 + padding;
            region.sprite_u2 = region.u2 - padding;
            region.sprite_v2 = region.v2 - padding;
        } else {
            // Use full cell for non-rotating sprites
            region.sprite_u1 = region.u1;
            region.sprite_v1 = region.v1;
            region.sprite_u2 = region.u2;
            region.sprite_v2 = region.v2;
        }
        
        // Calculate center point for rotation
        region.center_u = (region.u1 + region.u2) * 0.5f;
        region.center_v = (region.v1 + region.v2) * 0.5f;
        
        // Store original sprite dimensions (128x128 for most)
        region.original_width = 128;
        region.original_height = 128;
    }
}

void AtlasManager::BindAtlas(AtlasType type) {
    if (m_currentAtlas != type) {
        AtlasData& atlas = g_atlases[type];
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas.texture_id);
        m_currentAtlas = type;
    }
}

AtlasRegion& AtlasManager::GetUnitRegion(int unitType) {
    UnitAtlasIndex atlasIndex = g_unitTypeToAtlas[unitType];
    return g_atlases[ATLAS_MAIN_UNITS].regions[atlasIndex];
}

void AtlasManager::GetSpriteUVCoords(UnitAtlasIndex index, float& u1, float& v1, float& u2, float& v2) {
    AtlasRegion& region = g_atlases[ATLAS_MAIN_UNITS].regions[index];
    u1 = region.sprite_u1;
    v1 = region.sprite_v1;
    u2 = region.sprite_u2;
    v2 = region.sprite_v2;
}
```

---

## 🔧 Renderer Integration

### Modify Renderer Header

Add to `renderer.h`:

```cpp
// Add after existing includes
#include "atlas_manager.h"

class Renderer {
    // ... existing members ...
    
    // Atlas rendering support
    bool m_atlasSystemEnabled;
    AtlasType m_currentBoundAtlas;
    
public:
    // Atlas rendering methods
    void EnableAtlasSystem(bool enabled) { m_atlasSystemEnabled = enabled; }
    bool IsAtlasSystemEnabled() const { return m_atlasSystemEnabled; }
    
    // Atlas-based rendering functions
    void UnitMainSpriteAtlas(int unitType, float x, float y, float w, float h, Colour const &col);
    void UnitRotatingAtlas(int unitType, float x, float y, float w, float h, Colour const &col, float angle);
    void EffectsSpriteAtlas(const char* effectName, float x, float y, float w, float h, Colour const &col);
    
    // Atlas management
    void BindAtlasIfNeeded(AtlasType atlasType);
    void FlushAtlasBuffer(AtlasType atlasType);
};
```

### Renderer Implementation Updates

Add to `renderer.cpp`:

```cpp
// In Renderer constructor, add:
m_atlasSystemEnabled(false),
m_currentBoundAtlas(NUM_ATLAS_TYPES)

// Atlas-based unit rendering
void Renderer::UnitMainSpriteAtlas(int unitType, float x, float y, float w, float h, Colour const &col) {
    if (!m_atlasSystemEnabled || !g_atlasManager) {
        // Fallback to original rendering
        return;
    }
    
    // Get atlas region for this unit type
    AtlasRegion& region = g_atlasManager->GetUnitRegion(unitType);
    
    // Check if we need to flush due to buffer overflow
    FlushUnitMainSpritesIfFull(6);
    
    // Bind main units atlas
    BindAtlasIfNeeded(ATLAS_MAIN_UNITS);
    
    // Check if we need to flush due to atlas change
    if (m_unitMainVertexCount > 0 && m_currentBoundAtlas != ATLAS_MAIN_UNITS) {
        FlushUnitMainSprites();
    }
    
    // Update current atlas state
    m_currentUnitMainTexture = g_atlases[ATLAS_MAIN_UNITS].texture_id;
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Use sprite UV coordinates (excludes padding)
    float u1 = region.sprite_u1;
    float v1 = region.sprite_v1;
    float u2 = region.sprite_u2;
    float v2 = region.sprite_v2;
    
    // Create two triangles for the quad using atlas UV coordinates
    // First triangle: TL, TR, BR
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u1; 
    m_unitMainVertices[m_unitMainVertexCount].v = v2;  // Flipped V
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u2; 
    m_unitMainVertices[m_unitMainVertexCount].v = v2;  // Flipped V
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u2; 
    m_unitMainVertices[m_unitMainVertexCount].v = v1;  // Flipped V
    m_unitMainVertexCount++;
    
    // Second triangle: TL, BR, BL
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u1; 
    m_unitMainVertices[m_unitMainVertexCount].v = v2;  // Flipped V
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x + w;  
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u2; 
    m_unitMainVertices[m_unitMainVertexCount].v = v1;  // Flipped V
    m_unitMainVertexCount++;
    
    m_unitMainVertices[m_unitMainVertexCount].x = x;      
    m_unitMainVertices[m_unitMainVertexCount].y = y + h;
    m_unitMainVertices[m_unitMainVertexCount].r = r;      
    m_unitMainVertices[m_unitMainVertexCount].g = g;
    m_unitMainVertices[m_unitMainVertexCount].b = b;      
    m_unitMainVertices[m_unitMainVertexCount].a = a;
    m_unitMainVertices[m_unitMainVertexCount].u = u1; 
    m_unitMainVertices[m_unitMainVertexCount].v = v1;  // Flipped V
    m_unitMainVertexCount++;
}

void Renderer::UnitRotatingAtlas(int unitType, float x, float y, float w, float h, Colour const &col, float angle) {
    if (!m_atlasSystemEnabled || !g_atlasManager) {
        return;
    }
    
    // Get atlas region for this unit type
    AtlasRegion& region = g_atlasManager->GetUnitRegion(unitType);
    
    // Only rotating units should use this function
    if (!region.needs_rotation_padding) {
        // Fall back to non-rotating atlas rendering
        UnitMainSpriteAtlas(unitType, x, y, w, h, col);
        return;
    }
    
    // Check if we need to flush due to buffer overflow
    FlushUnitRotatingIfFull(6);
    
    // Bind main units atlas
    BindAtlasIfNeeded(ATLAS_MAIN_UNITS);
    
    // Check if we need to flush due to atlas change
    if (m_unitRotatingVertexCount > 0 && m_currentBoundAtlas != ATLAS_MAIN_UNITS) {
        FlushUnitRotating();
    }
    
    // Update current atlas state
    m_currentUnitRotatingTexture = g_atlases[ATLAS_MAIN_UNITS].texture_id;
    
    // ROTATION MATH: Calculate rotated vertices (same as original)
    Vector3<float> vert1( -w/2, +h/2, 0 );
    Vector3<float> vert2( +w/2, +h/2, 0 );
    Vector3<float> vert3( +w/2, -h/2, 0 );
    Vector3<float> vert4( -w/2, -h/2, 0 );

    vert1.RotateAroundZ(angle);
    vert2.RotateAroundZ(angle);
    vert3.RotateAroundZ(angle);
    vert4.RotateAroundZ(angle);

    vert1 += Vector3<float>( x + w/2, y + h/2, 0 );
    vert2 += Vector3<float>( x + w/2, y + h/2, 0 );
    vert3 += Vector3<float>( x + w/2, y + h/2, 0 );
    vert4 += Vector3<float>( x + w/2, y + h/2, 0 );
    
    // Convert color to float
    float r = col.m_r / 255.0f, g = col.m_g / 255.0f, b = col.m_b / 255.0f, a = col.m_a / 255.0f;
    
    // Use sprite UV coordinates (includes rotation padding)
    float u1 = region.sprite_u1;
    float v1 = region.sprite_v1;
    float u2 = region.sprite_u2;
    float v2 = region.sprite_v2;
    
    // Create two triangles for the rotated quad
    // First triangle: vert1, vert2, vert3
    m_unitRotatingVertices[m_unitRotatingVertexCount].x = vert1.x;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].y = vert1.y;
    m_unitRotatingVertices[m_unitRotatingVertexCount].r = r;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].g = g;
    m_unitRotatingVertices[m_unitRotatingVertexCount].b = b;      
    m_unitRotatingVertices[m_unitRotatingVertexCount].a = a;
    m_unitRotatingVertices[m_unitRotatingVertexCount].u = u1; 
    m_unitRotatingVertices[m_unitRotatingVertexCount].v = v2;  // Flipped V
    m_unitRotatingVertexCount++;
    
    // ... complete triangle setup similar to above ...
}

void Renderer::BindAtlasIfNeeded(AtlasType atlasType) {
    if (m_currentBoundAtlas != atlasType) {
        g_atlasManager->BindAtlas(atlasType);
        m_currentBoundAtlas = atlasType;
    }
}
```

---

## 🎮 Unit Rendering Updates

### Modify WorldObject Rendering

Update `worldobject.cpp` or your unit rendering code:

```cpp
// In WorldObject::Render() or MovingObject::Render()
void WorldObject::Render() {
    if (!g_atlasManager || !g_renderer->IsAtlasSystemEnabled()) {
        // Fallback to original rendering
        RenderOriginal();
        return;
    }
    
    // Get predicted position
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue();
    float size = GetSize().DoubleValue();
    
    // Get team color
    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();
    colour.m_a = 255;
    
    if (m_movementType == MovementTypeAir) {
        // Calculate rotation angle for aircraft
        float angle = atan2(-m_vel.x.DoubleValue(), m_vel.y.DoubleValue());
        if (m_vel.y < 0) angle += M_PI;
        
        // Use atlas-based rotating rendering
        g_renderer->UnitRotatingAtlas(m_type, 
                                     predictedLongitude - size/2, 
                                     predictedLatitude - size/2,
                                     size, size, colour, angle);
    } else {
        // Use atlas-based static rendering
        g_renderer->UnitMainSpriteAtlas(m_type,
                                       predictedLongitude - size/2,
                                       predictedLatitude - size/2, 
                                       size, size, colour);
    }
    
    // Render selection highlight if needed
    if (m_selected) {
        RenderSelectionHighlight();
    }
    
    // Render state icons, counters, etc.
    RenderStateIcons();
}

void WorldObject::RenderSelectionHighlight() {
    // Use blur atlas for highlights
    if (g_atlasManager && g_renderer->IsAtlasSystemEnabled()) {
        // Implementation for atlas-based blur rendering
        // This would use ATLAS_BLUR_EFFECTS
    }
}
```

### Batch Management in MapRenderer

Update `map_renderer.cpp`:

```cpp
void MapRenderer::RenderObjects() {
    START_PROFILE("Objects");
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    if (g_renderer->IsAtlasSystemEnabled()) {
        // ATLAS-BASED BATCHING: Single batch for all units
        g_renderer->BeginUnitMainBatch();
        g_renderer->BeginUnitRotatingBatch();
        
        for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
            if (g_app->GetWorld()->m_objects.ValidIndex(i)) {            
                WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                
                bool onScreen = IsOnScreen(wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue());
                if (onScreen || wobj->m_type == WorldObject::TypeNuke) {
                    if (myTeamId == -1 || wobj->m_teamId == myTeamId || 
                        wobj->m_visible[myTeamId] || m_renderEverything) {
                        wobj->Render();  // Uses atlas rendering internally
                    } else {
                        wobj->RenderGhost(myTeamId);
                    }
                }
            }
        }
        
        // ATLAS BATCHING: End batches - this creates only 1-2 draw calls total!
        g_renderer->EndUnitRotatingBatch();
        g_renderer->EndUnitMainBatch();
        
    } else {
        // Original texture-per-unit rendering (fallback)
        RenderObjectsOriginal();
    }
    
    END_PROFILE("Objects");
}
```

---

## 💥 Effects & UI Integration

### Explosion Atlas Integration

Update `explosion.cpp`:

```cpp
void Explosion::Render() {
    if (!g_atlasManager || !g_renderer->IsAtlasSystemEnabled()) {
        RenderOriginal();
        return;
    }
    
    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour;
    if (m_underWater) {
        colour = Colour(50, 50, 100, 150);
    } else {           
        colour = Colour(255, 255, 255, 230);
    }

    if (m_initialIntensity != Fixed::FromDouble(-1.0f)) {
        float fraction = (m_intensity / m_initialIntensity).DoubleValue();
        if (fraction < 0.7f) fraction = 0.7f;
        colour.m_a *= fraction;
    }

    float predictedIntensity = m_intensity.DoubleValue() - 0.2f *
                               g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
    float size = predictedIntensity / 20.0f;
    size /= g_app->GetWorld()->GetGameScale().DoubleValue();

    float predictedLongitude = m_longitude.DoubleValue();
    float predictedLatitude = m_latitude.DoubleValue();

    // Use atlas-based effects rendering
    g_renderer->EffectsSpriteAtlas("explosion", 
                                   predictedLongitude - size, 
                                   predictedLatitude - size,
                                   size * 2, size * 2, colour);
}
```

### UI Icons Integration

For small icons, create a separate atlas system:

```cpp
// In UI rendering code
void RenderUIIcon(const char* iconName, float x, float y, float size) {
    if (g_atlasManager && g_renderer->IsAtlasSystemEnabled()) {
        // Get small icon from atlas
        AtlasRegion& region = g_atlasManager->GetSmallIconRegion(iconName);
        g_renderer->UIIconAtlas(region, x, y, size, size, White);
    } else {
        // Fallback to individual texture
        Image* icon = g_resource->GetImage(iconName);
        g_renderer->Blit(icon, x, y, size, size, White);
    }
}
```

---

## 🧪 Testing & Validation

### Step 1: Atlas Loading Test

Create a test function to verify atlas loading:

```cpp
void TestAtlasSystem() {
    AppDebugOut("=== ATLAS SYSTEM TEST ===\n");
    
    // Initialize atlas manager
    g_atlasManager = new AtlasManager();
    if (!g_atlasManager->Initialize()) {
        AppDebugOut("ERROR: Atlas system failed to initialize\n");
        return;
    }
    
    // Test atlas loading
    for (int i = 0; i < NUM_ATLAS_TYPES; i++) {
        AtlasData& atlas = g_atlasManager->GetAtlas((AtlasType)i);
        AppDebugOut("Atlas %d: %s (%dx%d) TextureID: %u\n", 
                   i, atlas.filename, atlas.width, atlas.height, atlas.texture_id);
    }
    
    // Test UV coordinate calculation
    for (int i = 0; i < NUM_UNIT_ATLAS_ENTRIES; i++) {
        float u1, v1, u2, v2;
        g_atlasManager->GetSpriteUVCoords((UnitAtlasIndex)i, u1, v1, u2, v2);
        AppDebugOut("Unit %d UV: (%.3f,%.3f) to (%.3f,%.3f)\n", i, u1, v1, u2, v2);
    }
    
    AppDebugOut("=== ATLAS TEST COMPLETE ===\n");
}
```

### Step 2: Rendering Comparison Test

Create a side-by-side comparison:

```cpp
void TestAtlasRendering() {
    // Render scene with original system
    g_renderer->EnableAtlasSystem(false);
    RenderScene();
    int originalDrawCalls = g_renderer->GetTotalDrawCalls();
    
    // Render scene with atlas system  
    g_renderer->EnableAtlasSystem(true);
    RenderScene();
    int atlasDrawCalls = g_renderer->GetTotalDrawCalls();
    
    AppDebugOut("Draw calls - Original: %d, Atlas: %d, Improvement: %.1f%%\n",
               originalDrawCalls, atlasDrawCalls, 
               100.0f * (originalDrawCalls - atlasDrawCalls) / originalDrawCalls);
}
```

### Step 3: Rotation Bleeding Test

Test rotation with corner sprites:

```cpp
void TestRotationBleeding() {
    // Render rotating fighter at various angles
    for (float angle = 0; angle < 360; angle += 45) {
        g_renderer->UnitRotatingAtlas(UNIT_ATLAS_FIGHTER, 100, 100, 64, 64, 
                                     White, angle * M_PI / 180.0f);
        
        // Check for visual artifacts at sprite boundaries
        // This requires manual visual inspection
    }
}
```

### Step 4: Performance Benchmarking

```cpp
void BenchmarkAtlasPerformance() {
    const int NUM_FRAMES = 1000;
    
    // Benchmark original system
    double originalTime = GetHighResTime();
    g_renderer->EnableAtlasSystem(false);
    for (int i = 0; i < NUM_FRAMES; i++) {
        RenderComplexScene();
    }
    double originalDuration = GetHighResTime() - originalTime;
    
    // Benchmark atlas system
    double atlasTime = GetHighResTime();
    g_renderer->EnableAtlasSystem(true);
    for (int i = 0; i < NUM_FRAMES; i++) {
        RenderComplexScene();
    }
    double atlasDuration = GetHighResTime() - atlasTime;
    
    AppDebugOut("Performance - Original: %.2fms/frame, Atlas: %.2fms/frame, Speedup: %.1fx\n",
               originalDuration * 1000 / NUM_FRAMES,
               atlasDuration * 1000 / NUM_FRAMES,
               originalDuration / atlasDuration);
}
```

---

## 📊 Performance Monitoring

### Enhanced Debug Statistics

Update `renderer_debug_menu.cpp` to show atlas statistics:

```cpp
void RendererDebugMenu::RenderAtlasPerformanceStats(float& yPos) {
    char statsBuffer[512];
    float lineHeight = 18.0f;
    
    // Header
    m_renderer->TextSimple(10, yPos, Colour(255, 255, 255, 255), 13.0f, "ATLAS PERFORMANCE STATISTICS");
    yPos += lineHeight;
    
    if (!g_atlasManager || !m_renderer->IsAtlasSystemEnabled()) {
        m_renderer->TextSimple(10, yPos, Colour(255, 100, 100, 255), 11.0f, "Atlas system: DISABLED");
        yPos += lineHeight;
        return;
    }
    
    // Atlas binding statistics
    m_renderer->TextSimple(10, yPos, Colour(100, 255, 100, 255), 11.0f, "Atlas system: ENABLED");
    yPos += lineHeight;
    
    // Draw call comparison
    int totalCalls = m_renderer->GetTotalDrawCalls();
    int unitCalls = m_renderer->GetUnitMainSpriteCalls() + m_renderer->GetUnitRotatingCalls();
    snprintf(statsBuffer, sizeof(statsBuffer), "Total Draw Calls: %d (Unit Calls: %d)", totalCalls, unitCalls);
    m_renderer->TextSimple(10, yPos, Colour(255, 255, 255, 255), 12.0f, statsBuffer);
    yPos += lineHeight;
    
    // Atlas usage breakdown
    m_renderer->TextSimple(10, yPos, Colour(100, 255, 255, 255), 11.0f, "Atlas Usage:");
    yPos += lineHeight;
    
    for (int i = 0; i < NUM_ATLAS_TYPES; i++) {
        AtlasData& atlas = g_atlasManager->GetAtlas((AtlasType)i);
        snprintf(statsBuffer, sizeof(statsBuffer), "  %s: %dx%d (%d regions)", 
                atlas.filename, atlas.width, atlas.height, atlas.region_count);
        m_renderer->TextSimple(20, yPos, Colour(200, 200, 200, 255), 11.0f, statsBuffer);
        yPos += lineHeight;
    }
    
    // Expected vs actual performance
    int expectedCalls = CalculateExpectedAtlasCalls();
    Colour performanceColor = (totalCalls <= expectedCalls * 1.2f) ? 
        Colour(100, 255, 100, 255) : Colour(255, 100, 100, 255);
    snprintf(statsBuffer, sizeof(statsBuffer), "Expected calls: %d, Actual: %d", expectedCalls, totalCalls);
    m_renderer->TextSimple(10, yPos, performanceColor, 11.0f, statsBuffer);
    yPos += lineHeight;
}

int RendererDebugMenu::CalculateExpectedAtlasCalls() {
    // With atlas system, we should have roughly:
    return 4 +      // Atlas binding calls (4 atlases)
           10 +     // UI rendering calls
           5 +      // Text rendering calls
           3 +      // Effect rendering calls
           2;       // Miscellaneous calls
    // Total: ~24 calls (vs 700+ without atlas)
}
```

### Real-time Performance Tracking

```cpp
class AtlasPerformanceTracker {
private:
    int m_atlasBindings[NUM_ATLAS_TYPES];
    int m_textureBindings;
    double m_frameStartTime;
    
public:
    void BeginFrame() {
        m_frameStartTime = GetHighResTime();
        memset(m_atlasBindings, 0, sizeof(m_atlasBindings));
        m_textureBindings = 0;
    }
    
    void RecordAtlasBinding(AtlasType type) {
        m_atlasBindings[type]++;
    }
    
    void RecordTextureBinding() {
        m_textureBindings++;
    }
    
    void EndFrame() {
        double frameDuration = GetHighResTime() - m_frameStartTime;
        
        // Log performance metrics
        AppDebugOut("Frame: %.2fms, Texture bindings: %d, Atlas bindings: %d,%d,%d,%d\n",
                   frameDuration * 1000,
                   m_textureBindings,
                   m_atlasBindings[0], m_atlasBindings[1], 
                   m_atlasBindings[2], m_atlasBindings[3]);
    }
};
```

---

## 🚀 Migration Strategy

### Phase 1: Setup & Basic Testing (1-2 days)

1. **Create atlas images** using provided layout guidelines
2. **Implement data structures** (atlas_definitions.h, atlas_manager.h)
3. **Basic atlas loading** and UV coordinate calculation
4. **Test atlas loading** without rendering changes

```cpp
// Phase 1 test code
void Phase1Test() {
    g_atlasManager = new AtlasManager();
    bool success = g_atlasManager->Initialize();
    
    if (success) {
        AppDebugOut("Phase 1 SUCCESS: Atlas system loaded\n");
        TestAtlasSystem();  // Verify UV coordinates
    } else {
        AppDebugOut("Phase 1 FAILED: Atlas loading failed\n");
    }
}
```

### Phase 2: Core Unit Rendering (2-3 days)

1. **Implement atlas rendering functions** (UnitMainSpriteAtlas, UnitRotatingAtlas)
2. **Add atlas system toggle** for A/B testing
3. **Update WorldObject::Render()** to use atlas system
4. **Test with single unit type** first (e.g., Fighter)

```cpp
// Phase 2 test - single unit type
void Phase2Test() {
    g_renderer->EnableAtlasSystem(true);
    
    // Render single fighter with atlas
    g_renderer->BeginUnitMainBatch();
    g_renderer->UnitMainSpriteAtlas(WorldObject::TypeFighter, 100, 100, 64, 64, White);
    g_renderer->EndUnitMainBatch();
    
    // Compare draw calls: should be 1 instead of multiple
    int drawCalls = g_renderer->GetTotalDrawCalls();
    AppDebugOut("Phase 2: Single unit draw calls: %d\n", drawCalls);
}
```

### Phase 3: Full Unit Integration (2-3 days)

1. **Enable all unit types** in atlas rendering
2. **Implement rotation testing** for aircraft/nukes
3. **Add effects atlas** support (explosions)
4. **Full scene performance testing**

### Phase 4: Optimization & Polish (1-2 days)

1. **Performance monitoring** and bottleneck identification
2. **Memory usage optimization**
3. **Error handling** and fallback systems
4. **Documentation** and code cleanup

### Rollback Plan

Always maintain the ability to disable atlas system:

```cpp
// In main game initialization
bool useAtlasSystem = g_preferences->GetInt("UseAtlasSystem", 1);
g_renderer->EnableAtlasSystem(useAtlasSystem);

// If problems occur, can disable via preferences file:
// UseAtlasSystem=0
```

---

## 🎯 Expected Results

### Performance Targets

| Metric | Before Atlas | After Atlas | Improvement |
|--------|-------------|-------------|-------------|
| **Draw Calls/Frame** | 700-1000 | <100 | 85-90% |
| **Texture Bindings** | 40+ | 4 | 90% |
| **Frame Time** | 16ms | 8-12ms | 25-50% |
| **Memory Usage** | 50MB+ textures | 20MB atlases | 60% |

### Validation Checklist

- [ ] Atlas images load correctly
- [ ] UV coordinates map to correct sprites  
- [ ] Rotation works without bleeding
- [ ] All unit types render correctly
- [ ] Team colors work with atlas sprites
- [ ] Performance targets achieved
- [ ] No visual artifacts or corruption
- [ ] Fallback system works when disabled

---

## 🔧 Troubleshooting Guide

### Common Issues & Solutions

**Issue: Sprites appear corrupted or wrong**
- **Cause**: Incorrect UV coordinate calculation
- **Fix**: Verify atlas layout matches UV coordinates, check for off-by-one errors

**Issue: Rotation causes sprite bleeding**  
- **Cause**: Insufficient padding around rotating sprites
- **Fix**: Increase padding to 64+ pixels, verify padding in UV calculations

**Issue: Performance doesn't improve**
- **Cause**: Still binding individual textures instead of atlas
- **Fix**: Verify BindAtlasIfNeeded() is being called, check texture binding logic

**Issue: Atlas fails to load**
- **Cause**: File path or format issues
- **Fix**: Verify atlas files are in correct directory, check file format (PNG recommended)

**Issue: Team colors don't work**
- **Cause**: Atlas sprites aren't white/colorizable
- **Fix**: Ensure atlas sprites are white on black background for proper color multiplication

This comprehensive guide provides everything needed to implement a high-performance texture atlas system that should reduce your draw calls from 700+ to under 100, achieving the 80-90% performance improvement you're targeting.