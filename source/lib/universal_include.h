#ifndef INCLUDED_UNIVERSAL_INCLUDE_H
#define INCLUDED_UNIVERSAL_INCLUDE_H

//
// It's worth noting that using this version string alongside the gunfire and
// retarget patch violates the license agreement! What ever you do, do not try
// to connect to a steam lobby when building the desktop client!

//
// To make version control easier, we now have a "real version" string, which
// is used to match crashes or bugs to an internal string. Because APP_VERSION
// is never changed so cannot be used here.

#define APP_NAME        "Defcon"
#define	APP_VERSION		"1.64 STEAM"
#define APP_BUILD_NUMBER "1.30"

#if defined(REPLAY_VIEWER) || defined(REPLAY_VIEWER_DESKTOP)
    #define REAL_VERSION    APP_BUILD_NUMBER"_replay_viewer"
#elif defined(SYNC_PRACTICE)
    #define REAL_VERSION    "1.16.1_sync_practice"
#else
    #define REAL_VERSION    APP_BUILD_NUMBER"_defcon_full"
#endif

#define ENABLE_SANTA_EASTEREGG

//#define APP_VERSION     "1.5 fr rtl" // Defcon 1.5 Codemasters french build, use TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
//#define APP_VERSION     "1.5 de rtl" // Defcon 1.5 german build, use TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
//#define APP_VERSION     "1.5 it rtl" // Defcon 1.5 italian build, use TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
//#define APP_VERSION     "1.5 pl rtl" // Defcon 1.5 polish build, use TARGET_RETAIL_MULTI_LANGUAGE_POLISH
//#define APP_VERSION     "1.51 ru rtl" // Defcon 1.51 russian build, use TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN

//
// Choose one of these targets ONLY
//

//#define     TARGET_DEBUG
#define     TARGET_FINAL
//#define     TARGET_TESTBED
//#define     TARGET_RETAIL_UK
//#define     TARGET_RETAIL_UK_DEMO
//#define     TARGET_RETAIL
//#define     TARGET_RETAIL_DEMO
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ALL_LANGUAGES
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ENGLISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
//#define     TARGET_RETAIL_MULTI_LANGUAGE_SPANISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_POLISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN


// ============================================================================
// Dont edit anything below this line
// ============================================================================

#ifdef TARGET_RETAIL_UK
    #define RETAIL
    #define RETAIL_BRANDING_UK
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_UK_DEMO
    #define RETAIL
    #define RETAIL_DEMO
    #define RETAIL_BRANDING_UK
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL
    #define RETAIL
    #define RETAIL_BRANDING
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_DEMO
    #define RETAIL
    #define RETAIL_DEMO
    #define RETAIL_BRANDING
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ALL_LANGUAGES
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ENGLISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "english"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "french"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "german"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "italian"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_SPANISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "spanish"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_POLISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "polish"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "russian"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_FINAL
    #define DUMP_DEBUG_LOG
    #define TOOLS_ENABLED
    #define AUTHENTICATION_LEVEL 1
    //#define TRACK_SYNC_RAND
    //#define TRACK_SYNC_RAND_EXTRA_VALIDATION     // Warning: Changes sync value calculation.
#endif

#ifdef TARGET_DEBUG
    //#define TRACK_SYNC_RAND
    //#define TRACK_SYNC_RAND_EXTRA_VALIDATION     // Warning: Changes sync value calculation.
    #define TOOLS_ENABLED
    #define SOUND_EDITOR_ENABLED
    //#define DUMP_DEBUG_LOG
    //#define NON_PLAYABLE
    //#define SHOW_TEST_HOURS
    #define AUTHENTICATION_LEVEL 1
#endif

#ifdef TARGET_TESTBED
    #define TRACK_SYNC_RAND
    #define TRACK_SYNC_RAND_EXTRA_VALIDATION     // Warning: Changes sync value calculation.
    #define TOOLS_ENABLED
    #define NON_PLAYABLE
    #define DUMP_DEBUG_LOG
    #define TESTBED
    #define AUTHENTICATION_LEVEL 3
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "systemiv.h"

#ifdef TARGET_MSVC
    #define APP_SYSTEM "Windows"
#endif
#ifdef TARGET_OS_MACOSX
	#ifdef __ppc__
		#define APP_SYSTEM "Mac (PPC)"
	#else
		#define APP_SYSTEM "Mac (Intel)"
	#endif
    // XCode unhelpfully defines TARGET_OS_LINUX
    #undef TARGET_OS_LINUX
#endif
#ifdef TARGET_OS_LINUX
    #define APP_SYSTEM "Linux"
#endif

// Windows Specific options
#ifdef TARGET_MSVC
#include "../../resources/resource.h"

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
#include <ctype.h>
#include <unistd.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define Sleep(x) usleep( 1000 * x)


#if __GNUC__ == 3
#define sinf sin
#define cosf cos
#define sqrtf sqrt
#define expf exp
#define powf pow
#define logf log
#endif

#endif

#endif
