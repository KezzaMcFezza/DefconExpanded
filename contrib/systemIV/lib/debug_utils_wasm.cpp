#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <string>
#include <sstream>
#include <emscripten/console.h>
#include <emscripten.h>
#include "lib/debug_utils.h"

static std::string s_debugOutRedirect;

void AppDebugOutRedirect(const char *_filename)
{
#ifdef EMSCRIPTEN_DEBUG_OUTPUT
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
    vsnprintf(buf, sizeof(buf), _msg, ap);
    va_end(ap);
    
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
        abort();
    }
}
#endif // _DEBUG

void AppReleaseAssertFailed(char const *_msg, ...)
{
    char buf[512];
    va_list ap;
    va_start (ap, _msg);
    vsnprintf(buf, sizeof(buf), _msg, ap);
    va_end(ap);
    
    std::ostringstream fullMessage;
    fullMessage << buf;
    std::string fullMsg = fullMessage.str();

    emscripten_console_error(fullMsg.c_str());
    
    //
    // Show alert dialog in browser

    EM_ASM({
        alert(UTF8ToString($0));
    }, fullMsg.c_str());

#ifndef _DEBUG
    AppGenerateBlackBox("blackbox.txt", fullMsg.c_str());
    abort();
#else
    abort();
#endif // _DEBUG
}

unsigned *getRetAddress(unsigned *mBP)
{
    //
    // Not supported in WebAssembly

    return NULL;
}

void AppGenerateBlackBox( const char *_filename, const char *_msg )
{
    printf("=========================\n");
    printf("=   BLACK BOX REPORT    =\n");
    printf("=========================\n\n");
    
    printf("%s %s built %s\n", GetAppName(), GetAppVersion(), __DATE__);
    
    time_t timet = time(NULL);
    tm *thetime = localtime(&timet);
    printf("Date %d:%d, %d/%d/%d\n\n", thetime->tm_hour, thetime->tm_min, 
           thetime->tm_mday, thetime->tm_mon+1, thetime->tm_year+1900);
    
    if( _msg ) printf("%s\n", _msg);
}

void SetupPathToProgram(const char *program)
{
    //
    // Path setup not needed in browser environment

    (void)program; 
}

void SetupMemoryAccessHandlers()
{
    //
    // Memory access handlers not supported
}

#endif // TARGET_EMSCRIPTEN