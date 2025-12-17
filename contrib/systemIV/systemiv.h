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

// Windows Specific options
#ifdef TARGET_MSVC
#include "../../resources/resource.h"

#pragma warning( disable : 4244 4305 4800 4018 )
// Defines that will enable you to double click on a #pragma message
// in the Visual Studio output window.
#define MESSAGE_LINENUMBERTOSTRING(linenumber)	#linenumber
#define MESSAGE_LINENUMBER(linenumber)			MESSAGE_LINENUMBERTOSTRING(linenumber)
#ifndef MESSAGE
#define MESSAGE(x) message (__FILE__ "(" MESSAGE_LINENUMBER(__LINE__) "): " x)
#endif


#include <crtdbg.h>
#define snprintf _snprintf

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

//Fall back for getaddrinfo on older platform, see end of
// http://msdn.microsoft.com/en-us/library/ms738520.aspx
#include <ws2tcpip.h>
#include <wspiapi.h>

#ifdef TRACK_MEMORY_LEAKS
#include "lib/memory_leak.h"
#endif
#endif // TARGET_MSVC

// Mac OS X Specific Settings
#if defined ( TARGET_OS_MACOSX ) || defined ( TARGET_OS_LINUX )
#undef HAVE_DSOUND

#include <ctype.h>
#include <unistd.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define Sleep(x) usleep( 1000 * x)

// String function definitions - handle Emscripten specially
#ifdef TARGET_EMSCRIPTEN
    // Emscripten provides these functions in its compatibility layer
    // Include string.h to get the declarations
    #include <string.h>
    // Note: strlwr and strupr are declared in Emscripten's compat/string.h
    // but we need to avoid redefinition conflicts
    #ifndef __EMSCRIPTEN_STRLWR_DEFINED__
        extern char* strlwr(char *);
        extern char* strupr(char *);
        #define __EMSCRIPTEN_STRLWR_DEFINED__
    #endif
#else
    // Original inline implementations for macOS and Linux
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

void SetAppName(const char *name);
void SetAppVersion(const char *version);
void SetAppBuildNumber(const char *buildNumber);
void SetRealVersion(const char *realVersion);
void SetAppSystem(const char *system);

// Include library headers
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
