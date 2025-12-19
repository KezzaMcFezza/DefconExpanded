#include "systemiv.h"
#include "lib/tosser/llist.h"

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