#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <string>
#include <emscripten/console.h>
#include "lib/debug_utils.h"

// WebAssembly debug utilities stubs
// These provide basic functionality without execinfo.h dependencies

static std::string s_debugOutRedirect;

void AppDebugOutRedirect(const char *_filename)
{
#ifdef EMSCRIPTEN_DEBUG_OUTPUT
    // Simple implementation for WASM - just store the filename
    if( !_filename || strcmp( _filename, "" ) == 0 )
    {
        s_debugOutRedirect.clear();
        return;
    }
    s_debugOutRedirect = _filename;
#endif
}

void AppDebugOut(const char *_msg, ...)
{
#ifdef EMSCRIPTEN_DEBUG_OUTPUT
    char buf[512];
    va_list ap;
    va_start (ap, _msg);
    vsprintf(buf, _msg, ap);
    
    // For WebAssembly, just output to console
    emscripten_console_log( buf );
#endif
}

void AppDebugOutData(void *_data, int _dataLen)
{
#ifdef EMSCRIPTEN_DEBUG_OUTPUT
    for( int i = 0; i < _dataLen; ++i )
    {
        if( i % 16 == 0 )
        {
            AppDebugOut( "\n" );
        }
        AppDebugOut( "%02x ", ((unsigned char *)_data)[i] );
    }    
    AppDebugOut( "\n\n" );
#endif
}

#ifdef _DEBUG
void AppDebugAssert(bool _condition)
{
    if(!_condition)
    {
        // For WebAssembly, just abort
        abort();
    }
}
#endif // _DEBUG

void AppReleaseAssertFailed(char const *_msg, ...)
{
    char buf[512];
    va_list ap;
    va_start (ap, _msg);
    vsprintf(buf, _msg, ap);

    // Output error message
    emscripten_console_log( buf );

#ifndef _DEBUG
    AppGenerateBlackBox("blackbox.txt", buf);
    // For WebAssembly, just abort instead of throw
    abort();
#else
    abort();
#endif // _DEBUG
}

unsigned *getRetAddress(unsigned *mBP)
{
    // Not supported in WebAssembly
    return NULL;
}

void AppGenerateBlackBox( const char *_filename, const char *_msg )
{
    // Simplified version for WebAssembly
    printf("=========================\n");
    printf("=   BLACK BOX REPORT    =\n");
    printf("=========================\n\n");
    
    printf("%s %s built %s\n", APP_NAME, APP_VERSION, __DATE__);
    
    time_t timet = time(NULL);
    tm *thetime = localtime(&timet);
    printf("Date %d:%d, %d/%d/%d\n\n", thetime->tm_hour, thetime->tm_min, 
           thetime->tm_mday, thetime->tm_mon+1, thetime->tm_year+1900);
    
    if( _msg ) printf("ERROR : '%s'\n", _msg);
    
    printf("Stack trace not available in WebAssembly\n");
}

void SetupPathToProgram(const char *program)
{
    // No-op for WebAssembly - path setup not needed in browser environment
    (void)program; // Suppress unused parameter warning
}

void SetupMemoryAccessHandlers()
{
    // No-op for WebAssembly - memory access handlers not supported
}

#endif // TARGET_EMSCRIPTEN 