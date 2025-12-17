#include "systemiv.h"

static const char *s_appName = "SystemIV";
static const char *s_appVersion = "1.37";
static const char *s_appBuildNumber = "1.64";
static const char *s_realVersion = "1.64_systemiv";
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

