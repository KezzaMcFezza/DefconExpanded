#ifndef _included_sprite_atlas_h
#define _included_sprite_atlas_h

// Atlas texture dimensions
constexpr float ATLAS_TEXTURE_WIDTH = 412.0f;
constexpr float ATLAS_TEXTURE_HEIGHT = 1518.0f;

// undefine windows macro, since the atlas texture is defined as ERROR,
// emscripten does not care about this as its not using windows headers
#ifdef ERROR
#undef ERROR
#endif

// Atlas coordinate structure
struct AtlasCoord {
    float u1, v1, u2, v2;  // UV coordinates, normalized 0.0 - 1.0
    int width, height;     // original sprite dimensions
    
    constexpr AtlasCoord() : u1(0), v1(0), u2(0), v2(0), width(0), height(0) {}
    
    constexpr AtlasCoord(int x, int y, int w, int h) 
        : u1((float)x / ATLAS_TEXTURE_WIDTH),
          v1(1.0f - (float)(y + h) / ATLAS_TEXTURE_HEIGHT),  // flip Y for OpenGL
          u2((float)(x + w) / ATLAS_TEXTURE_WIDTH),
          v2(1.0f - (float)y / ATLAS_TEXTURE_HEIGHT),
          width(w),
          height(h)
    {
    }
};

class SpriteAtlas {
public:
    static const AtlasCoord AIRBASE;
    static const AtlasCoord BATTLESHIP;
    static const AtlasCoord BOMBER;
    static const AtlasCoord CARRIER;
    static const AtlasCoord FIGHTER;
    static const AtlasCoord NUKE;
    static const AtlasCoord POPULATION;
    static const AtlasCoord RADAR;
    static const AtlasCoord RADARSTATION;
    static const AtlasCoord SAM;
    static const AtlasCoord SAUCER;
    static const AtlasCoord SILO;
    static const AtlasCoord SUB;
    static const AtlasCoord SUB_SURFACED;
    static const AtlasCoord AIRBASE_BLUR;
    static const AtlasCoord BATTLESHIP_BLUR;
    static const AtlasCoord BOMBER_BLUR;
    static const AtlasCoord CARRIER_BLUR;
    static const AtlasCoord FIGHTER_BLUR;
    static const AtlasCoord NUKE_BLUR;
    static const AtlasCoord RADARSTATION_BLUR;
    static const AtlasCoord SAM_BLUR;
    static const AtlasCoord SILO_BLUR;
    static const AtlasCoord SUB_BLUR;
    static const AtlasCoord SUB_SURFACED_BLUR;
    static const AtlasCoord SMALLBOMBER;
    static const AtlasCoord SMALLFIGHTER;
    static const AtlasCoord SMALLNUKE;
    static const AtlasCoord ARROW;           // 128x128
    static const AtlasCoord EXPLOSION;       // 128x128
    static const AtlasCoord FLEET;           // 128x128
    static const AtlasCoord NUKESYMBOL;      // 128x128
    static const AtlasCoord UNITS;           // 128x128
    static const AtlasCoord CURSOR_SELECTION; // 128x128
    static const AtlasCoord CURSOR_TARGET;   // 128x128
    static const AtlasCoord TORNADO;         // 150x150
    static const AtlasCoord CITY;
    static const AtlasCoord DEPTHCHARGE;
    static const AtlasCoord ERROR;
    static const AtlasCoord LASER;
    static const AtlasCoord TARGETCURSOR;
    static const AtlasCoord BLIP;            // 32x32
    static const AtlasCoord POPBUTTON;       // 16x16
    static const AtlasCoord RADARBUTTON;     // 16x16
    static const AtlasCoord ACTIONLINE;      // 16x8
    
    // check if a filename corresponds to an atlas sprite
    static bool IsAtlasSprite(const char* filename);
    
    // get atlas coordinates for a sprite by filename
    static const AtlasCoord* GetSpriteCoord(const char* filename);
    
    // get the atlas texture filename
    static const char* GetAtlasTexturePath() { return "graphics/defconatlas.bmp"; }
};

#endif // _included_sprite_atlas_h