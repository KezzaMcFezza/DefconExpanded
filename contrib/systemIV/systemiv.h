#ifndef INCLUDED_SYSTEMIV_H
#define INCLUDED_SYSTEMIV_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WAN_PLAY_ENABLED
#define LAN_PLAY_ENABLED
//#define FIXED64_NUMERICS
#define FLOAT_NUMERICS
#define TRACK_LANGUAGEPHRASE_ERRORS
#define TRACK_GL_ERRORS

#ifdef TARGET_MSVC

#include <crtdbg.h>
#define snprintf _snprintf

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

//Fall back for getaddrinfo on older platform, see end of
// http://msdn.microsoft.com/en-us/library/ms738520.aspx
#include <ws2tcpip.h>
#include <wspiapi.h>

#endif

#if defined ( TARGET_OS_MACOSX ) || defined ( TARGET_OS_LINUX )
#include <ctype.h>
#include <unistd.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define Sleep(x) usleep( 1000 * x)

//
// String function definitions, handle Emscripten specially

#ifdef TARGET_EMSCRIPTEN
    #include <string.h>

    //
    // Note: strlwr and strupr are declared in Emscripten's compat/string.h
    // but we need to avoid redefinition conflicts

    #ifndef __EMSCRIPTEN_STRLWR_DEFINED__
        extern char* strlwr(char *);
        extern char* strupr(char *);
        #define __EMSCRIPTEN_STRLWR_DEFINED__
    #endif
#else
    inline char * strlwr(char *s) {
        for (char *p = s; *p; p++)
            *p = tolower(*p);
        return s;
    }
    inline char * strupr(char *s) {
        for (char *p = s; *p; p++)
            *p = toupper(*p);
        return s;
    }
#endif

#if __GNUC__ == 3
#define sinf sin
#define cosf cos
#define sqrtf sqrt
#define expf exp
#define powf pow
#define logf log
#endif

#endif

#ifdef Round
    #undef Round
#endif

#include <glad/glad.h>
#include <hwy/highway.h>
#include <SDL2/SDL.h>

#ifdef OPENMP
    #include <omp.h>
#endif

const char *GetAppName();
const char *GetAppVersion();
const char *GetAppBuildNumber();
const char *GetRealVersion();
const char *GetAppSystem();
const char *GetDataRoot();

void SetAppName       (const char *name);
void SetAppVersion    (const char *version);
void SetAppBuildNumber(const char *buildNumber);
void SetRealVersion   (const char *realVersion);
void SetAppSystem     (const char *system);
void SetDataRoot      (const char *path);

const char *GetResourceDir                  (int index);
const char * const *GetResourceDirExclusions(int index, int *count);

void        AddResourceDir                  (const char *path,
                                             const char * const *excludeList = NULL,
                                             int excludeCount = 0);
int         GetResourceDirCount();

#ifdef TARGET_OS_MACOSX
    #undef TARGET_OS_LINUX
    #undef TARGET_OS_WINDOWS
    #undef TARGET_EMSCRIPTEN
#elif defined(TARGET_MSVC)
    #undef TARGET_OS_LINUX
    #undef TARGET_OS_MACOSX
    #undef TARGET_EMSCRIPTEN
    #include <glad/wgl_ext.h>
#elif defined(TARGET_EMSCRIPTEN)
    #undef TARGET_OS_MACOSX
    #undef TARGET_OS_WINDOWS
    #include <emscripten.h>
    #include <pthread.h>
    #include <sys/time.h>
#endif

#endif
