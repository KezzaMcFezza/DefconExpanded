#include "lib/universal_include.h"
#include "sprite_atlas.h"
#include <string.h>

const AtlasCoord SpriteAtlas::AIRBASE(153, 1, 128, 128);
const AtlasCoord SpriteAtlas::BATTLESHIP(283, 131, 128, 128);
const AtlasCoord SpriteAtlas::BOMBER(131, 261, 128, 128);
const AtlasCoord SpriteAtlas::CARRIER(261, 261, 128, 128);
const AtlasCoord SpriteAtlas::FIGHTER(1, 543, 128, 128);
const AtlasCoord SpriteAtlas::NUKE(1, 673, 128, 128);
const AtlasCoord SpriteAtlas::POPULATION(1, 803, 128, 128);
const AtlasCoord SpriteAtlas::RADAR(261, 781, 128, 128);
const AtlasCoord SpriteAtlas::RADARSTATION(131, 911, 128, 128);
const AtlasCoord SpriteAtlas::SAM(261, 911, 128, 128);
const AtlasCoord SpriteAtlas::SAUCER(1, 1063, 128, 128);
const AtlasCoord SpriteAtlas::SILO(261, 1041, 128, 128);
const AtlasCoord SpriteAtlas::SUB(1, 1193, 128, 128);
const AtlasCoord SpriteAtlas::SUB_SURFACED(1, 1323, 128, 128);
const AtlasCoord SpriteAtlas::AIRBASE_BLUR(283, 1, 128, 128);
const AtlasCoord SpriteAtlas::BATTLESHIP_BLUR(1, 153, 128, 128);
const AtlasCoord SpriteAtlas::BOMBER_BLUR(1, 283, 128, 128);
const AtlasCoord SpriteAtlas::CARRIER_BLUR(131, 391, 128, 128);
const AtlasCoord SpriteAtlas::FIGHTER_BLUR(261, 521, 128, 128);
const AtlasCoord SpriteAtlas::NUKE_BLUR(261, 651, 128, 128);
const AtlasCoord SpriteAtlas::RADARSTATION_BLUR(1, 933, 128, 128);
const AtlasCoord SpriteAtlas::SAM_BLUR(131, 1041, 128, 128);
const AtlasCoord SpriteAtlas::SILO_BLUR(131, 1171, 128, 128);
const AtlasCoord SpriteAtlas::SUB_BLUR(261, 1171, 128, 128);
const AtlasCoord SpriteAtlas::SUB_SURFACED_BLUR(131, 1301, 128, 128);
const AtlasCoord SpriteAtlas::SMALLBOMBER(131, 189, 16, 16);
const AtlasCoord SpriteAtlas::SMALLFIGHTER(131, 207, 16, 16);
const AtlasCoord SpriteAtlas::SMALLNUKE(131, 225, 16, 16);
const AtlasCoord SpriteAtlas::ARROW(153, 131, 128, 128);
const AtlasCoord SpriteAtlas::EXPLOSION(131, 521, 128, 128);
const AtlasCoord SpriteAtlas::FLEET(131, 651, 128, 128);
const AtlasCoord SpriteAtlas::NUKESYMBOL(131, 781, 128, 128);
const AtlasCoord SpriteAtlas::UNITS(261, 1301, 128, 128);
const AtlasCoord SpriteAtlas::CURSOR_SELECTION(1, 413, 128, 128);
const AtlasCoord SpriteAtlas::CURSOR_TARGET(261, 391, 128, 128);
const AtlasCoord SpriteAtlas::TORNADO(1, 1, 150, 150);
const AtlasCoord SpriteAtlas::CITY(1, 1453, 64, 64);
const AtlasCoord SpriteAtlas::DEPTHCHARGE(67, 1453, 64, 64);
const AtlasCoord SpriteAtlas::ERROR(133, 1431, 64, 64);
const AtlasCoord SpriteAtlas::LASER(199, 1431, 64, 64);
const AtlasCoord SpriteAtlas::TARGETCURSOR(265, 1431, 64, 64);
const AtlasCoord SpriteAtlas::BLIP(331, 1431, 32, 32);           // 32x32
const AtlasCoord SpriteAtlas::POPBUTTON(131, 153, 16, 16);       // 16x16
const AtlasCoord SpriteAtlas::RADARBUTTON(131, 171, 16, 16);     // 16x16
const AtlasCoord SpriteAtlas::ACTIONLINE(131, 243, 16, 8);       // 16x8

struct SpriteMapping {
    const char* filename;
    const AtlasCoord* coord;
};

static const SpriteMapping s_spriteMap[] = {
    // main unit sprites
    { "graphics/airbase.bmp", &SpriteAtlas::AIRBASE },
    { "graphics/battleship.bmp", &SpriteAtlas::BATTLESHIP },
    { "graphics/bomber.bmp", &SpriteAtlas::BOMBER },
    { "graphics/carrier.bmp", &SpriteAtlas::CARRIER },
    { "graphics/fighter.bmp", &SpriteAtlas::FIGHTER },
    { "graphics/nuke.bmp", &SpriteAtlas::NUKE },
    { "graphics/population.bmp", &SpriteAtlas::POPULATION },
    { "graphics/radar.bmp", &SpriteAtlas::RADAR },
    { "graphics/radarstation.bmp", &SpriteAtlas::RADARSTATION },
    { "graphics/sam.bmp", &SpriteAtlas::SAM },
    { "graphics/saucer.bmp", &SpriteAtlas::SAUCER },
    { "graphics/silo.bmp", &SpriteAtlas::SILO },
    { "graphics/sub.bmp", &SpriteAtlas::SUB },
    { "graphics/sub_surfaced.bmp", &SpriteAtlas::SUB_SURFACED },
    
    // blur variants
    { "graphics/airbase_blur.bmp", &SpriteAtlas::AIRBASE_BLUR },
    { "graphics/battleship_blur.bmp", &SpriteAtlas::BATTLESHIP_BLUR },
    { "graphics/bomber_blur.bmp", &SpriteAtlas::BOMBER_BLUR },
    { "graphics/carrier_blur.bmp", &SpriteAtlas::CARRIER_BLUR },
    { "graphics/fighter_blur.bmp", &SpriteAtlas::FIGHTER_BLUR },
    { "graphics/nuke_blur.bmp", &SpriteAtlas::NUKE_BLUR },
    { "graphics/radarstation_blur.bmp", &SpriteAtlas::RADARSTATION_BLUR },
    { "graphics/sam_blur.bmp", &SpriteAtlas::SAM_BLUR },
    { "graphics/silo_blur.bmp", &SpriteAtlas::SILO_BLUR },
    { "graphics/sub_blur.bmp", &SpriteAtlas::SUB_BLUR },
    { "graphics/sub_surfaced_blur.bmp", &SpriteAtlas::SUB_SURFACED_BLUR },
    
    // small unit indicators
    { "graphics/smallbomber.bmp", &SpriteAtlas::SMALLBOMBER },
    { "graphics/smallfighter.bmp", &SpriteAtlas::SMALLFIGHTER },
    { "graphics/smallnuke.bmp", &SpriteAtlas::SMALLNUKE },
    
    // UI and effect sprites
    { "graphics/arrow.bmp", &SpriteAtlas::ARROW },
    { "graphics/explosion.bmp", &SpriteAtlas::EXPLOSION },
    { "graphics/fleet.bmp", &SpriteAtlas::FLEET },
    { "graphics/nukesymbol.bmp", &SpriteAtlas::NUKESYMBOL },
    { "graphics/units.bmp", &SpriteAtlas::UNITS },
    { "graphics/cursor_selection.bmp", &SpriteAtlas::CURSOR_SELECTION },
    { "graphics/cursor_target.bmp", &SpriteAtlas::CURSOR_TARGET },
    { "graphics/tornado.bmp", &SpriteAtlas::TORNADO },
    { "graphics/city.bmp", &SpriteAtlas::CITY },
    { "graphics/depthcharge.bmp", &SpriteAtlas::DEPTHCHARGE },
    { "graphics/error.bmp", &SpriteAtlas::ERROR },
    { "graphics/laser.bmp", &SpriteAtlas::LASER },
    { "graphics/targetcursor.bmp", &SpriteAtlas::TARGETCURSOR },
    { "graphics/blip.bmp", &SpriteAtlas::BLIP },
    { "graphics/popbutton.bmp", &SpriteAtlas::POPBUTTON },
    { "graphics/radarbutton.bmp", &SpriteAtlas::RADARBUTTON },
    { "graphics/actionline.bmp", &SpriteAtlas::ACTIONLINE },
};

static const int s_spriteMapSize = sizeof(s_spriteMap) / sizeof(SpriteMapping);

bool SpriteAtlas::IsAtlasSprite(const char* filename) {
    if (!filename) return false;
    
    for (int i = 0; i < s_spriteMapSize; i++) {
        if (strcmp(filename, s_spriteMap[i].filename) == 0) {
            return true;
        }
    }
    return false;
}

const AtlasCoord* SpriteAtlas::GetSpriteCoord(const char* filename) {
    if (!filename) return NULL;
    
    for (int i = 0; i < s_spriteMapSize; i++) {
        if (strcmp(filename, s_spriteMap[i].filename) == 0) {
            return s_spriteMap[i].coord;
        }
    }
    return NULL;
}