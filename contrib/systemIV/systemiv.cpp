#include "systemiv.h"
#include "lib/tosser/llist.h"
#include "lib/debug/debug_utils.h"

// ============================================================================

static const char *s_appName        = "SystemIV";
static const char *s_appVersion     = "1.37";
static const char *s_appBuildNumber = "1.64";
static const char *s_realVersion    = "1.64_systemiv";
static const char *s_appSystem = 
#ifdef TARGET_MSVC
    "Windows"
#elif defined(TARGET_OS_MACOSX)
    #ifdef __ppc__
        "Mac (PPC)"
    #else
        "Mac (Intel)"
    #endif
#elif defined(TARGET_OS_LINUX)
    "Linux"
#elif defined(TARGET_EMSCRIPTEN)
    "Emscripten"
#else
    "Unknown"
#endif
;

const char *GetAppName()
{
    return s_appName;
}

const char *GetAppVersion()
{
    return s_appVersion;
}

const char *GetAppBuildNumber()
{
    return s_appBuildNumber;
}

const char *GetRealVersion()
{
    return s_realVersion;
}

const char *GetAppSystem()
{
    return s_appSystem;
}

void SetAppName(const char *name)
{
    s_appName = name;
}

void SetAppVersion(const char *version)
{
    s_appVersion = version;
}

void SetAppBuildNumber(const char *buildNumber)
{
    s_appBuildNumber = buildNumber;
}

void SetRealVersion(const char *realVersion)
{
    s_realVersion = realVersion;
}

void SetAppSystem(const char *system)
{
    s_appSystem = system;
}


// ============================================================================


static RendererType s_rendererType = RENDERER_TYPE_OPENGL;
static bool s_rendererTypeSet = false;

void SetRendererType(RendererType type)
{
    //
    // Validate that the requested renderer is available on this platform
    
    if (!IsRendererAvailable(type))
    {
        const char *typeName = GetRendererTypeName(type);
        AppDebugOut("ERROR: Renderer type '%s' is not available on this platform\n", typeName);
        
#if !defined(RENDERER_OPENGL) && !defined(RENDERER_DIRECTX11)
        AppDebugOut("ERROR: No renderer backends compiled into this build!\n");
        AppDebugOut("Please define RENDERER_OPENGL or RENDERER_DIRECTX11 in your build system.\n");
#endif
        
        exit(1);
    }
    
    s_rendererType = type;
    s_rendererTypeSet = true;
}

RendererType GetRendererType()
{
    //
    // SetRendererType() must be called before GetRendererType()
    // If not set, we exit immediately
    
    if (!s_rendererTypeSet)
    {
        AppDebugOut("ERROR: Renderer type not set! Call SetRendererType() before creating WindowManager.\n");
        exit(1);
    }
    
    return s_rendererType;
}

bool IsRendererAvailable(RendererType type)
{
    switch (type)
    {
        case RENDERER_TYPE_OPENGL:
#ifdef RENDERER_OPENGL
            return true;
#else
            return false;
#endif
            
        case RENDERER_TYPE_DIRECTX11:
#ifdef RENDERER_DIRECTX11
            return true;
#else
            return false;
#endif
            
        default:
            return false;
    }
}

const char *GetRendererTypeName(RendererType type)
{
    switch (type)
    {
        case RENDERER_TYPE_OPENGL:    return "OpenGL";
        case RENDERER_TYPE_DIRECTX11: return "DirectX11";
        default:                      return "Unknown";
    }
}


// ============================================================================


static const char *s_dataRoot    = NULL;

struct ResourceDirConfig
{
    const char *path;
    const char * const *excludeList;
    int excludeCount;
};

static LList<ResourceDirConfig> s_resourceDirs;

const char *GetDataRoot()
{
    return s_dataRoot;
}

void SetDataRoot(const char *path)
{
    if( path ) s_dataRoot = path;
}

void AddResourceDir(const char *path,
                    const char * const *excludeList,
                    int excludeCount)
{
    if( !path || !path[0] ) return;

    ResourceDirConfig cfg;
    cfg.path        = path;
    cfg.excludeList = excludeList;
    cfg.excludeCount= excludeCount;
    
    s_resourceDirs.PutData(cfg);
}

int GetResourceDirCount()
{
    return s_resourceDirs.Size();
}

const char *GetResourceDir(int index)
{
    if( index < 0 || index >= s_resourceDirs.Size() ) return NULL;
    ResourceDirConfig *cfg = s_resourceDirs.GetPointer(index);
    return cfg ? cfg->path : NULL;
}

const char * const *GetResourceDirExclusions(int index, int *count)
{
    if( index < 0 || index >= s_resourceDirs.Size() )
    {
        if( count ) *count = 0;
        return NULL;
    }

    ResourceDirConfig *cfg = s_resourceDirs.GetPointer(index);
    if( !cfg )
    {
        if( count ) *count = 0;
        return NULL;
    }
    
    if( count ) *count = cfg->excludeCount;
    return cfg->excludeList;
}