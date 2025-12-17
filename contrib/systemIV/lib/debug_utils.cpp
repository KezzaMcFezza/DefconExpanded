#include "systemiv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include <crtdbg.h>
#elif TARGET_OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#elif TARGET_OS_LINUX
#ifndef __GNUC__
#error Compiling on linux without GCC?
#endif
#include <execinfo.h>
#endif

#include <string>

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"


static std::string s_debugOutRedirect;

#ifndef WIN32
inline
void OutputDebugString( const char *s )
{
	fputs( s, stderr );
}
#endif


void AppDebugOutRedirect(const char *_filename)
{
	// re-using same log file is a no-op
    if( s_debugOutRedirect == _filename ) return;

    if( !_filename || strcmp( _filename, "" ) == 0 )
    {
        s_debugOutRedirect.clear();
        return;
    }

    // Check we have write access, and clear the file
    s_debugOutRedirect = _filename;
    FILE *file = fopen( _filename, "w" );

    if( !file )
    {
        AppDebugOut( "Failed to open %s for writing\n", s_debugOutRedirect.c_str() );
        s_debugOutRedirect.clear();
    }
    else
    {
        fclose( file );
    }
}


void AppDebugOut(const char *_msg, ...)
{
    char buf[512];
    va_list ap;
    va_start (ap, _msg);
    vsprintf(buf, _msg, ap);

    if( !s_debugOutRedirect.empty() )
    {
        FILE *file = fopen( s_debugOutRedirect.c_str(), "a" );
        if( !file )
        {
            std::string failedPath = s_debugOutRedirect;
            s_debugOutRedirect.clear();
            AppDebugOut( "Failed to open %s for writing\n", failedPath.c_str() );
            OutputDebugString(buf);
        }
        else
        {
            fprintf( file, "%s", buf );
            fclose( file );
        }
    }
    else
    {
        OutputDebugString(buf);
    }
}


void AppDebugOutData(void *_data, int _dataLen)
{
    for( int i = 0; i < _dataLen; ++i )
    {
        if( i % 16 == 0 )
        {
            AppDebugOut( "\n" );
        }
        AppDebugOut( "%02x ", ((unsigned char *)_data)[i] );
    }    
    AppDebugOut( "\n\n" );
}


#ifdef _DEBUG
void AppDebugAssert(bool _condition)
{
    if(!_condition)
    {
#ifdef WIN32
		ShowCursor(true);
		_ASSERT(_condition); 
#else
		abort();
#endif  
    }
}
#endif // _DEBUG


void AppReleaseAssertFailed(char const *_msg, ...)
{
	char buf[512];
	va_list ap;
	va_start (ap, _msg);
	vsprintf(buf, _msg, ap);

#ifdef WIN32
	ShowCursor(true);
	MessageBox(NULL, buf, "Fatal Error", MB_OK);
#else
	fputs(buf, stderr);
#endif

#ifndef _DEBUG
	AppGenerateBlackBox("blackbox.txt",buf);
	throw;
	//exit(-1);
#else
#ifdef WIN32
	_ASSERT(false);
#else
	abort();
#endif
#endif // _DEBUG
}


unsigned *getRetAddress(unsigned *mBP)
{
#ifdef WIN32
#ifdef	_M_X64
	return NULL;
#else
		unsigned *retAddr;

	__asm {
		mov eax, [mBP]
		mov eax, ss:[eax+4];
		mov [retAddr], eax
	}

	return retAddr;
#endif
#else
	unsigned **p = (unsigned **) mBP;
	return p[1];
#endif
}


void AppGenerateBlackBox( const char *_filename, const char *_msg )
{
    FILE *_file = fopen( _filename, "wt" );
    if( _file )
    {
        fprintf( _file, "=========================\n" );
        fprintf( _file, "=   BLACK BOX REPORT    =\n" );
        fprintf( _file, "=========================\n\n" );

        fprintf( _file, "%s %s built %s\n", GetAppName(), GetAppVersion(), __DATE__  );

        time_t timet = time(NULL);
        tm *thetime = localtime(&timet);
        fprintf( _file, "Date %d:%d, %d/%d/%d\n\n", thetime->tm_hour, thetime->tm_min, thetime->tm_mday, thetime->tm_mon+1, thetime->tm_year+1900 );

        if( _msg ) fprintf( _file, "ERROR : '%s'\n", _msg );

		// For MacOSX, suggest Smart Crash Reports: http://smartcrashreports.com/
		
#ifndef TARGET_OS_MACOSX
        //
        // Print stack trace
	    // Get our frame pointer, chain upwards

        fprintf( _file, "\n" );
        fprintf( _file, "=========================\n" );
        fprintf( _file, "=      STACKTRACE       =\n" );
        fprintf( _file, "=========================\n\n" );
        fflush(_file);
#ifndef TARGET_OS_LINUX
	    unsigned *framePtr;
        unsigned *previousFramePtr = NULL;

    
#ifdef WIN32
#ifdef _M_X64
		framePtr = NULL;
#else
	    __asm { 
		    mov [framePtr], ebp
	    }
#endif
#else
	    asm (
	        "movl %%ebp, %0;"
	        :"=r"(framePtr)
	        );
#endif
	    while(framePtr) {
		                    
		    fprintf(_file, "retAddress = %p\n", getRetAddress(framePtr));
		    framePtr = *(unsigned **)framePtr;

	        // Frame pointer must be aligned on a
	        // DWORD boundary.  Bail if not so.
	        if ( (ptrdiff_t) framePtr & 3 )   
		    break;                    

            if ( framePtr <= previousFramePtr )
                break;

            // Can two DWORDs be read from the supposed frame address?          
#ifdef WIN32
            if ( IsBadWritePtr(framePtr, sizeof(PVOID)*2) ||
                 IsBadReadPtr(framePtr, sizeof(PVOID)*2) )
                break;
#endif

            previousFramePtr = framePtr;
    
        }
#else // __LINUX__
// new "GNUC" way of doing things
// big old buffer on the stack, (won't fail)
    void *array[40];
    size_t size;
// fetch the backtrace
    size = backtrace(array, 40);
// Stream it to the file
    backtrace_symbols_fd(array, size, fileno(_file));

#endif // __LINUX__
#endif // TARGET_OS_MACOSX



        fclose( _file );
    }
}
