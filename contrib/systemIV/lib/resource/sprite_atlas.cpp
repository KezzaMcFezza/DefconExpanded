#include "lib/universal_include.h"

#include <string.h>
#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#include "lib/debug_utils.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/resource/bitmap.h"
#include "lib/resource/image.h"
#include "lib/string_utils.h"
#include "lib/tosser/llist.h"
#include "sprite_atlas.h"

SpriteAtlasManager* g_spriteAtlasManager = NULL;

AtlasCoord::AtlasCoord(int x, int y, int w, int h, int atlasWidth, int atlasHeight) {
    width = w;
    height = h;
    pixelX = x;
    pixelY = y;
    
    //
    // convert pixel coordinates to normalized UV coordinates (0.0 - 1.0)

    u1 = (float)x / (float)atlasWidth;
    u2 = (float)(x + w) / (float)atlasWidth;
    
    //
    // standard image coordinates (Y=0 at top)

    v1 = (float)y / (float)atlasHeight;
    v2 = (float)(y + h) / (float)atlasHeight;
}

PackedSprite::PackedSprite() 
    : filename(NULL), sourceBitmap(NULL) {
}

PackedSprite::~PackedSprite() {
    if (filename) {
        delete[] filename;
        filename = NULL;
    }
    if (sourceBitmap) {
        delete sourceBitmap;
        sourceBitmap = NULL;
    }
}

// ============================================================================
// IMPLEMENTATION
// ============================================================================

RuntimeTextureAtlas::RuntimeTextureAtlas(const char* name)
    : m_width(0), m_height(0), m_textureID(0), m_atlasBitmap(NULL), m_name(NULL) {
    m_name = newStr(name);
}

RuntimeTextureAtlas::~RuntimeTextureAtlas() {

    //
    // clean up sprites

    DArray<PackedSprite*>* sprites = m_spriteMap.ConvertToDArray();
    for (int i = 0; i < sprites->Size(); ++i) {
        delete sprites->GetData(i);
    }
    delete sprites;
    m_spriteMap.Empty();
    
    //
    // clean up atlas bitmap

    if (m_atlasBitmap) {
        delete m_atlasBitmap;
        m_atlasBitmap = NULL;
    }

    //
    // clean up OpenGL texture

    if (m_textureID > 0) {
        glDeleteTextures(1, (GLuint*)&m_textureID);
        m_textureID = 0;
    }
    
    if (m_name) {
        delete[] m_name;
        m_name = NULL;
    }
}

//
// load sprites from data folder, or from main.dat archive

void RuntimeTextureAtlas::LoadSprites(
    const char* directory,
    const char** excludeList, int excludeCount,
    PackedSprite*** outSprites, int* outCount) {
    
    //
    // list all BMP files in directory

    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "data/%s", directory);
    searchPath[sizeof(searchPath) - 1] = '\0';
    
    LList<char*>* files = g_fileSystem->ListArchive(searchPath, "*.bmp", false);
    if (!files || files->Size() == 0) {
        AppDebugOut("WARNING: No sprites found in %s\n", directory);
        *outSprites = NULL;
        *outCount = 0;
        if (files) {
            files->EmptyAndDeleteArray();
            delete files;
        }
        return;
    }
    
    int totalFiles = files->Size();
    
    //
    // pre-allocate sprite array to hold results
    
    PackedSprite** sprites = new PackedSprite*[totalFiles];
    for (int i = 0; i < totalFiles; ++i) {
        sprites[i] = NULL; 
    }
    
    //
    // pre-filter excluded files and build work list
    
    bool* shouldProcess = new bool[totalFiles];
    int validFileCount = 0;
    
    for (int i = 0; i < totalFiles; ++i) {
        char* filename = files->GetData(i);
        
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s%s", directory, filename);
        fullPath[sizeof(fullPath) - 1] = '\0';
        
        //
        // check if its an excluded sprite, like data textures

        bool excluded = false;
        for (int e = 0; e < excludeCount; ++e) {
            if (strcmp(fullPath, excludeList[e]) == 0) {
                excluded = true;
                break;
            }
        }
        
        shouldProcess[i] = !excluded;
        if (!excluded) {
            validFileCount++;
        }
    }
    
#ifdef OPENMP

    //
    // determine if parallelization is worthwhile
    // it always will be unless loading small mods
    
    int numThreads = omp_get_max_threads();
    bool useParallel = (validFileCount >= numThreads * 2);
    
    if (!useParallel) {
        numThreads = 1;
        omp_set_num_threads(1);
    }
    
    AppDebugOut("%s: Loading %d sprites using %d thread%s...\n", 
                m_name, validFileCount, numThreads, numThreads > 1 ? "s" : "");
    
    //
    // parallel bitmap loading, each iteration is completely independent
    
    #pragma omp parallel for schedule(dynamic, 1) if(useParallel)
    for (int i = 0; i < totalFiles; ++i) {
        if (!shouldProcess[i]) {
            continue;
        }
        
        //
        // build paths with thread-local stack variables
        
        char* filename = files->GetData(i);
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s%s", directory, filename);
        fullPath[sizeof(fullPath) - 1] = '\0';
        
        char dataPath[512];
        snprintf(dataPath, sizeof(dataPath), "data/%s", fullPath);
        dataPath[sizeof(dataPath) - 1] = '\0';
        
        //
        // each thread gets its own BinaryReader
        // g_fileSystem is read-only, returns new reader per call
        
        BinaryReader* reader = g_fileSystem->GetBinaryReader(dataPath);
        
        if (reader && reader->IsOpen()) {
            Bitmap* bmp = new Bitmap(reader, "bmp");
            delete reader;
            
            if (bmp->m_width > 0 && bmp->m_height > 0) {

                //
                // each thread allocates its own PackedSprite
                // and writes to a unique array index
                
                PackedSprite* sprite = new PackedSprite();
                sprite->filename = newStr(fullPath);
                sprite->sourceBitmap = bmp;
                sprites[i] = sprite;
            } else {
                #pragma omp critical
                {
                    AppDebugOut("ERROR: Invalid bitmap: %s\n", fullPath);
                }
                delete bmp;
            }
        } else {
            #pragma omp critical
            {
                AppDebugOut("ERROR: Failed to load: %s\n", dataPath);
            }
            if (reader) delete reader;
        }
    }
    
    //
    // restore thread count if we changed it

    if (!useParallel) {
        omp_set_num_threads(omp_get_max_threads());
    }
    
#else

    AppDebugOut("%s: Loading %d sprites...\n", 
                m_name, validFileCount);
    
    for (int i = 0; i < totalFiles; ++i) {
        if (!shouldProcess[i]) {
            continue;
        }
        
        char* filename = files->GetData(i);
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s%s", directory, filename);
        fullPath[sizeof(fullPath) - 1] = '\0';
        
        char dataPath[512];
        snprintf(dataPath, sizeof(dataPath), "data/%s", fullPath);
        dataPath[sizeof(dataPath) - 1] = '\0';
        
        BinaryReader* reader = g_fileSystem->GetBinaryReader(dataPath);
        if (reader && reader->IsOpen()) {
            Bitmap* bmp = new Bitmap(reader, "bmp");
            delete reader;
            
            if (bmp->m_width > 0 && bmp->m_height > 0) {
                PackedSprite* sprite = new PackedSprite();
                sprite->filename = newStr(fullPath);
                sprite->sourceBitmap = bmp;
                sprites[i] = sprite;
            } else {
                AppDebugOut("ERROR: Invalid bitmap: %s\n", fullPath);
                delete bmp;
            }
        } else {
            AppDebugOut("ERROR: Failed to load: %s\n", dataPath);
            if (reader) delete reader;
        }
    }
#endif
     
    PackedSprite** compactedSprites = new PackedSprite*[validFileCount];
    int compactIndex = 0;
    
    for (int i = 0; i < totalFiles; ++i) {
        if (sprites[i] != NULL) {
            compactedSprites[compactIndex++] = sprites[i];
        }
    }
    
    delete[] sprites;
    delete[] shouldProcess;
    
    //
    // clean up file list
    
    files->EmptyAndDeleteArray();
    delete files;
    
    //
    // return results
    
    *outCount = compactIndex;
    *outSprites = compactedSprites;
    
    AppDebugOut("%s Sprites: %d\n", m_name, *outCount);
}

bool RuntimeTextureAtlas::PackSprites(PackedSprite** sprites, int spriteCount,
                                       int maxWidth, int maxHeight) {
    
    //
    // allocate stb_rect_pack structures

    stbrp_rect* rects = new stbrp_rect[spriteCount];
    
    //
    // initialize rectangles with sprite dimensions with 2px padding

    for (int i = 0; i < spriteCount; ++i) {
        rects[i].id = i;
        rects[i].w = sprites[i]->sourceBitmap->m_width + 2;  // 1px padding each side
        rects[i].h = sprites[i]->sourceBitmap->m_height + 2;
        rects[i].was_packed = 0;
    }
    
    //
    // try packing with progressively larger atlas sizes

    int atlasW = 512;
    int atlasH = 512;
    bool allPacked = false;
    stbrp_context context;
    stbrp_node* nodes = NULL;
    
    while (!allPacked && atlasW <= maxWidth) {

        //
        // allocate nodes for this attempt

        if (nodes) delete[] nodes;
        nodes = new stbrp_node[atlasW];
        
        //
        // initialize packing context

        stbrp_init_target(&context, atlasW, atlasH, nodes, atlasW);
        
        //
        // attempt to pack all rectangles

        stbrp_pack_rects(&context, rects, spriteCount);
        
        //
        // check if all rectangles were packed

        allPacked = true;
        for (int i = 0; i < spriteCount; ++i) {
            if (!rects[i].was_packed) {
                allPacked = false;
                break;
            }
        }
        
        if (!allPacked) {
            if (atlasH < atlasW) {
                atlasH = atlasW;
            } else {
                atlasW *= 2;
                atlasH = atlasW / 2;
            }
        }
    }
    
    delete[] nodes;
    
    if (!allPacked) {
        delete[] rects;
        return false;
    }
    
    m_width = atlasW;
    m_height = atlasH;
    
    //
    // store packed coordinates in sprites

    for (int i = 0; i < spriteCount; ++i) {
        PackedSprite* sprite = sprites[rects[i].id];
        
        //
        // account for 1px padding offset

        int x = rects[i].x + 1;
        int y = rects[i].y + 1;
        int w = sprite->sourceBitmap->m_width;
        int h = sprite->sourceBitmap->m_height;
        
        sprite->coord = AtlasCoord(x, y, w, h, m_width, m_height);
        
    }
    
    delete[] rects;
    return true;
}

void RuntimeTextureAtlas::BlitSpritesToAtlas(PackedSprite** sprites, int spriteCount) {
    
    //
    // now create the atlas bitmaps

    m_atlasBitmap = new Bitmap(m_width, m_height);
    m_atlasBitmap->Clear(Colour(0, 0, 0, 0));  // transparent black
    
    //
    // blit each sprite to its position in the atlas

    for (int i = 0; i < spriteCount; ++i) {
        PackedSprite* sprite = sprites[i];
        Bitmap* src = sprite->sourceBitmap;
        
        int x = sprite->coord.pixelX;
        int y = sprite->coord.pixelY;
        
        m_atlasBitmap->Blit(
            0, 0, src->m_width, src->m_height, src,
            x, y, src->m_width, src->m_height,
            false);
        
    }
}

void RuntimeTextureAtlas::CreateGLTexture() {
    m_atlasBitmap->ConvertPinkToTransparent();
    m_textureID = m_atlasBitmap->ConvertToTexture(true);
}

bool RuntimeTextureAtlas::BuildFromDirectory(const char* directory,
                                              const char** excludeList,
                                              int excludeCount) {
    
    //
    // load all sprites from directory

    PackedSprite** sprites = NULL;
    int spriteCount = 0;
    LoadSprites(directory, excludeList, excludeCount, &sprites, &spriteCount);
    
    if (spriteCount == 0) {
        AppDebugOut("No sprites loaded for atlas %s\n\n", m_name);
        return false;
    }
    
    //
    // pack sprites into atlas

    if (!PackSprites(sprites, spriteCount)) {
        for (int i = 0; i < spriteCount; ++i) {
            delete sprites[i];
        }
        delete[] sprites;
        return false;
    }
    
    BlitSpritesToAtlas(sprites, spriteCount);
    CreateGLTexture();
    
    //
    // store sprites in map for lookup
    
    for (int i = 0; i < spriteCount; ++i) {
        m_spriteMap.PutData(sprites[i]->filename, sprites[i]);
        
        //
        // delete source bitmap to save memory
        
        delete sprites[i]->sourceBitmap;
        sprites[i]->sourceBitmap = NULL;
    }
    
    delete[] sprites;
    return true;
}

const PackedSprite* RuntimeTextureAtlas::GetSprite(const char* filename) const {
    return const_cast<BTree<PackedSprite*>*>(&m_spriteMap)->GetData(filename);
}

bool RuntimeTextureAtlas::HasSprite(const char* filename) const {
    return const_cast<BTree<PackedSprite*>*>(&m_spriteMap)->GetData(filename) != NULL;
}

AtlasImage::AtlasImage(PackedSprite* sprite, RuntimeTextureAtlas* atlas)
    : Image((Bitmap*)NULL),  // dont load individual bitmap
      m_packedSprite(sprite),
      m_atlas(atlas) {

    //
    // copy texture ID from atlas
    
    m_textureID = atlas->GetTextureID();
    m_mipmapping = true;
    m_bitmap = NULL;      
}

AtlasImage::~AtlasImage() {
    m_textureID = -1;
}

int AtlasImage::Width() {
    return m_packedSprite ? m_packedSprite->coord.width : 0;
}

int AtlasImage::Height() {
    return m_packedSprite ? m_packedSprite->coord.height : 0;
}

const AtlasCoord* AtlasImage::GetAtlasCoord() const {
    return m_packedSprite ? &m_packedSprite->coord : NULL;
}

unsigned int AtlasImage::GetAtlasTextureID() const {
    return m_atlas ? m_atlas->GetTextureID() : 0;
}

const char* AtlasImage::GetFilename() const {
    return m_packedSprite ? m_packedSprite->filename : NULL;
}

SpriteAtlasManager::SpriteAtlasManager()
    : m_graphicsAtlas(NULL), m_guiAtlas(NULL), m_initialized(false) {
}

SpriteAtlasManager::~SpriteAtlasManager() {
    Shutdown();
}

bool SpriteAtlasManager::Initialize() {
    if (m_initialized) {
        return true;
    }
    
    //
    // define excluded data textures that cannot be atlased
    
        const char* excludedGraphics[] = {
        "graphics/blur.bmp",
        "graphics/map.bmp",
        "graphics/water.bmp",
        "graphics/water_shaded.bmp",
        "graphics/territory.bmp",
        "graphics/defconatlas.bmp",
        "gui/defconuiatlas.bmp"
    };

    int excludeCount = sizeof(excludedGraphics) / sizeof(char*);
    

    //
    // build graphics atlas
    
    m_graphicsAtlas = new RuntimeTextureAtlas("Graphics");
    if (!m_graphicsAtlas->BuildFromDirectory("graphics/", excludedGraphics, excludeCount)) {
        delete m_graphicsAtlas;
        m_graphicsAtlas = NULL;
        return false;
    }
    
    //
    // build interface atlas
    
    m_guiAtlas = new RuntimeTextureAtlas("Interface");
    if (!m_guiAtlas->BuildFromDirectory("gui/", NULL, 0)) {
        delete m_guiAtlas;
        m_guiAtlas = NULL;
        return false;
    }
    
    //
    // calculate and display total sprites loaded
    
    int totalSprites = 0;
    if (m_graphicsAtlas) {
        totalSprites += m_graphicsAtlas->GetSpriteCount();
    }
    if (m_guiAtlas) {
        totalSprites += m_guiAtlas->GetSpriteCount();
    }
    
    AppDebugOut("Total sprites loaded: %d\n", totalSprites);
    
    m_initialized = true; 
    return true;
}

void SpriteAtlasManager::Rebuild() {
    Shutdown();
    Initialize();
}

void SpriteAtlasManager::Shutdown() {
    if (m_graphicsAtlas) {
        delete m_graphicsAtlas;
        m_graphicsAtlas = NULL;
    }
    
    if (m_guiAtlas) {
        delete m_guiAtlas;
        m_guiAtlas = NULL;
    }
    
    m_initialized = false;
}

bool SpriteAtlasManager::IsAtlasSprite(const char* filename) const {
    if (!m_initialized) return false;
    
    if (m_graphicsAtlas && m_graphicsAtlas->HasSprite(filename)) {
        return true;
    }
    
    if (m_guiAtlas && m_guiAtlas->HasSprite(filename)) {
        return true;
    }
    
    return false;
}

const PackedSprite* SpriteAtlasManager::GetSpriteFromAnyAtlas(
    const char* filename, RuntimeTextureAtlas** outAtlas) const {
    
    if (!m_initialized) {
        if (outAtlas) *outAtlas = NULL;
        return NULL;
    }
    
    //
    // check graphics atlas first
    
    if (m_graphicsAtlas) {
        const PackedSprite* sprite = m_graphicsAtlas->GetSprite(filename);
        if (sprite) {
            if (outAtlas) *outAtlas = m_graphicsAtlas;
            return sprite;
        }
    }
    
    //
    // now check interface atlas
    
    if (m_guiAtlas) {
        const PackedSprite* sprite = m_guiAtlas->GetSprite(filename);
        if (sprite) {
            if (outAtlas) *outAtlas = m_guiAtlas;
            return sprite;
        }
    }
    
    if (outAtlas) *outAtlas = NULL;
    return NULL;
}