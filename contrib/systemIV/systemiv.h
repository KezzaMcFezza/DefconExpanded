#ifndef INCLUDED_SYSTEMIV_H
#define INCLUDED_SYSTEMIV_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <climits>

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <optional>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <map>
#include <deque>
#include <unordered_set>
#include <utility>
#include <fstream>
#include <limits>
#include <filesystem>

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

//Fall back for getaddrinfo on older platform, see end of
// http://msdn.microsoft.com/en-us/library/ms738520.aspx
#include <ws2tcpip.h>
#include <wspiapi.h>

#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <direct.h>
#include <intrin.h>
#include <dbghelp.h>
#pragma comment( lib, "dbghelp.lib" )

#ifdef RENDERER_DIRECTX11
#include <d3dcompiler.h>
#pragma comment( lib, "d3dcompiler.lib" )
#endif

#endif

#if defined ( TARGET_OS_MACOSX ) || defined ( TARGET_OS_LINUX )
#include <unistd.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define Sleep(x) usleep( 1000 * x)

#ifdef TARGET_OS_MACOSX
#define Style Style_Unused
#define StyleTable StyleTable_Unused
#define Fixed Fixed_Unused
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#undef Style
#undef StyleTable
#undef Fixed
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fcntl.h>
#include <dirent.h>
#endif

#ifdef TARGET_OS_LINUX
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#ifndef TARGET_EMSCRIPTEN
#include <sys/ucontext.h>
#include <execinfo.h>
#include <cxxabi.h>
#endif
#include <fcntl.h>
#include <dirent.h>
#endif

//
// String function definitions, handle Emscripten specially

#ifdef TARGET_EMSCRIPTEN
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
#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_thread.h>

#ifndef NO_UNRAR
#include <unrar/unrar.h>
#include "unrar/rartypes.hpp"
#include "unrar/sha1.hpp"
#endif

#include <vorbis/vorbisfile.h>
#include <vorbis/codec.h>

#include <tiny_gltf.h>

#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_sdl3.h"

#ifdef RENDERER_OPENGL
#include "imgui-1.92.5/backends/imgui_impl_opengl3.h"
#endif

#ifdef RENDERER_DIRECTX11
#include "imgui-1.92.5/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#endif

#ifdef USE_CRASHREPORTING
#include "client/crash_report_database.h"
#include "client/settings.h"
#include "client/crashpad_client.h"
#include "base/files/file_path.h"
#include "util/file/file_io.h"
#endif

#ifdef OPENMP
    #include <omp.h>
#endif

const char *GetAppName();
const char *GetAppVersion();
const char *GetAppBuildNumber();
const char *GetRealVersion();
const char *GetAppSystem();
const char *GetDataRoot();
const char *GetPreferencesPath();

void SetAppName         (const char *name);
void SetAppVersion      (const char *version);
void SetAppBuildNumber  (const char *buildNumber);
void SetRealVersion     (const char *realVersion);
void SetAppSystem       (const char *system);
void SetDataRoot        (const char *path);
void SetPreferencesPath (const char *path);

void RelaunchApplication();

enum RendererType {
    RENDERER_TYPE_OPENGL,
    RENDERER_TYPE_DIRECTX11
};

enum TextureFilterMode {
    TEXTURE_FILTER_MODE_LINEAR,
    TEXTURE_FILTER_MODE_NEAREST
};

void         SetRendererType            (RendererType type);
RendererType GetRendererType            ();
bool         IsRendererAvailable        (RendererType type);
const char  *GetRendererTypeName        (RendererType type);

void               SetDefaultTextureFilterMode (TextureFilterMode mode);
TextureFilterMode  GetDefaultTextureFilterMode ();

void               SetTargetFPS                (int fps);
int                GetTargetFPS                ();
void               LimitFrameRate              ();

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
    #define RENDERER_OPENGL
#elif defined(TARGET_MSVC)
    #undef TARGET_OS_LINUX
    #undef TARGET_OS_MACOSX
    #undef TARGET_EMSCRIPTEN
    #ifndef RENDERER_DIRECTX11
    #include <glad/wgl_ext.h>
    #endif
#elif defined(TARGET_EMSCRIPTEN)
    #undef TARGET_OS_MACOSX
    #undef TARGET_OS_WINDOWS
    //#define EMSCRIPTEN_IMGUI
    //#define EMSCRIPTEN_DEBUG_OUTPUT
    //#define EMSCRIPTEN_DEBUG
    //#define EMSCRIPTEN_NETWORK_TESTBED
    //#define DTOGGLE_SOUND_TESTBED
    //#define DEMSCRIPTEN_PLAYBACK_TESTBED
    #include <emscripten.h>
    #include <emscripten/console.h>
    #include <pthread.h>
    #include <sys/time.h>
#endif

#endif
