#include "systemiv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#elif TARGET_OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <execinfo.h>
#elif TARGET_OS_LINUX
#ifndef __GNUC__
#error Compiling on linux without GCC?
#endif
#include <execinfo.h>
#include <cxxabi.h>
#endif

#include <string>
#include <sstream>

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
    vsnprintf(buf, sizeof(buf), _msg, ap);
    va_end(ap);

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

static const char* AppExtractFilename(const char* fullPath)
{
    const char* filename = strrchr(fullPath, '\\');
    if (!filename) filename = strrchr(fullPath, '/');
    if (filename) filename++; // Skip the slash
    else filename = fullPath;
    return filename;
}

#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
static std::string AppExtractFunctionName(const std::string& symbolStr);
static std::string AppDemangleName(const std::string& mangledName);
static std::string AppExtractModuleName(const std::string& symbolStr);
static std::string AppCleanupSymbolString(const std::string& symbolStr);
#endif

static void AppFormatStackFrame(std::ostringstream& oss, int frameIndex
#ifdef WIN32
                               , HANDLE process, SYMBOL_INFO* symbol, IMAGEHLP_LINE64* line, DWORD64 address
#elif defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
                               , const std::string& symbolStr
#endif
                               )
{
    oss << frameIndex << ". ";
    
#ifdef WIN32
    if (SymFromAddr(process, address, 0, symbol))
    {
        oss << symbol->Name;
        
        DWORD displacement = 0;
        if (SymGetLineFromAddr64(process, address, &displacement, line))
        {
            const char* filename = AppExtractFilename(line->FileName);
            oss << " at " << filename << ":" << line->LineNumber;
        }
    }
    else
    {
        oss << "<unknown>";
    }
#elif defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
    std::string funcName = AppExtractFunctionName(symbolStr);
    
    if (!funcName.empty())
    {
        std::string demangled = AppDemangleName(funcName);
        oss << demangled;
    }
    else
    {
        std::string moduleName = AppExtractModuleName(symbolStr);
        if (moduleName != "<unknown>")
        {
            oss << moduleName;
        }
        else
        {
            std::string cleaned = AppCleanupSymbolString(symbolStr);
            oss << cleaned;
        }
    }
#endif
    
    oss << "\n";

    //
    // Format looks like this:
    // 0. FunctionName at filename:linenumber
    // 1. FunctionName at filename:linenumber
    // 2. FunctionName at filename:linenumber
}

static void AppInitStackTraceOutput(std::ostringstream& oss)
{
    oss << "Stack Trace:\n";
    oss << "\n";
}

static std::string AppBuildFullErrorMessage(const char* errorMsg, const std::string& stackTrace)
{
    std::ostringstream fullMessage;
    fullMessage << errorMsg << "\n\n" << stackTrace;
    return fullMessage.str();
}

#ifdef WIN32

static bool AppInitializeSymbolHandler()
{
    static bool symbolsInitialized = false;
    
    if (!symbolsInitialized)
    {
        HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        if (SymInitialize(process, NULL, TRUE))
        {
            symbolsInitialized = true;
        }
        else
        {
            return false;
        }
    }
    
    return true;
}

static std::string AppCaptureStackTrace_Windows(int skipFrames, int maxFrames)
{
    std::ostringstream oss;
    
    if (!AppInitializeSymbolHandler())
    {
        oss << "Failed to initialize symbol handler\n";
        return oss.str();
    }
    
    HANDLE process = GetCurrentProcess();
    
    void* stack[64];
    USHORT frames = CaptureStackBackTrace(skipFrames + 1, maxFrames < 64 ? maxFrames : 64, stack, NULL);
    
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
    symbol->MaxNameLen = MAX_SYM_NAME;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    
    AppInitStackTraceOutput(oss);
    
    USHORT framesToShow = frames < 5 ? frames : 5;
    for (USHORT i = 0; i < framesToShow; i++)
    {
        DWORD64 address = (DWORD64)(stack[i]);
        AppFormatStackFrame(oss, i, process, symbol, &line, address);
    }
    
    return oss.str();
}

#endif // WIN32

#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)

//
// Extract and demangle function name from symbol string

static std::string AppExtractFunctionName(const std::string& symbolStr)
{
    size_t funcStart = symbolStr.find('(');
    size_t funcEnd = symbolStr.find('+', funcStart);
    
    if (funcStart == std::string::npos || funcEnd == std::string::npos)
    {
        return "";
    }
    
    return symbolStr.substr(funcStart + 1, funcEnd - funcStart - 1);
}

//
// Demangle C++ function name

static std::string AppDemangleName(const std::string& mangledName)
{
#ifdef TARGET_OS_LINUX
    if (mangledName.empty())
        return mangledName;
    
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangledName.c_str(), nullptr, nullptr, &status);
    
    if (status == 0 && demangled)
    {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    
    return mangledName;
#else
    return mangledName;
#endif
}

//
// Extract module name from symbol string

static std::string AppExtractModuleName(const std::string& symbolStr)
{
    size_t moduleEnd = symbolStr.find('(');
    if (moduleEnd == std::string::npos)
        return "<unknown>";
    
    std::string module = symbolStr.substr(0, moduleEnd);
    size_t lastSlash = module.find_last_of('/');
    if (lastSlash != std::string::npos)
        module = module.substr(lastSlash + 1);
    
    return module;
}

//
// Clean up symbol string by removing address

static std::string AppCleanupSymbolString(const std::string& symbolStr)
{
    std::string result = symbolStr;
    size_t addrStart = result.find_last_of('[');
    if (addrStart != std::string::npos)
        result = result.substr(0, addrStart);
    
    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    
    return result;
}

//
// Capture and format stack trace

static std::string AppCaptureStackTrace_Unix(int skipFrames, int maxFrames)
{
    std::ostringstream oss;
    
    void* callstack[128];
    int frames = backtrace(callstack, maxFrames < 128 ? maxFrames : 128);
    char** symbols = backtrace_symbols(callstack, frames);
    
    if (!symbols)
    {
        oss << "Failed to capture stack trace\n";
        return oss.str();
    }
    
    AppInitStackTraceOutput(oss);
    
    int maxFramesToShow = skipFrames + 1 + 5;
    for (int i = skipFrames + 1; i < frames && i < maxFramesToShow; i++)
    {
        std::string symbolStr(symbols[i]);
        AppFormatStackFrame(oss, i - skipFrames - 1, symbolStr);
    }
    
    free(symbols);
    return oss.str();
}

#endif // TARGET_OS_LINUX || TARGET_OS_MACOSX


std::string AppCaptureStackTrace(int skipFrames, int maxFrames)
{
    std::ostringstream oss;
    
#ifdef WIN32
    return AppCaptureStackTrace_Windows(skipFrames, maxFrames);
#elif defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
    return AppCaptureStackTrace_Unix(skipFrames, maxFrames);
#else
    oss << "Stack trace not available on this platform\n";
    return oss.str();
#endif
}


void AppReleaseAssertFailed(char const *_msg, ...)
{
	char buf[512];
	va_list ap;
	va_start (ap, _msg);
	vsnprintf(buf, sizeof(buf), _msg, ap);
    va_end(ap);

    std::string stackTrace = AppCaptureStackTrace(1, 32);  
    std::string fullMsg = AppBuildFullErrorMessage(buf, stackTrace);
    
    AppGenerateBlackBox("blackbox.txt", fullMsg.c_str());

#ifdef WIN32
	ShowCursor(true);
	MessageBoxA(NULL, fullMsg.c_str(), "Fatal Error", MB_OK | MB_ICONERROR);
#else
	fputs(fullMsg.c_str(), stderr);
	fputs("\n", stderr);
#endif

#ifndef _DEBUG
	throw;
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
	// For 64-bit, use the new stack trace API
    // 32-Bit applications are deprecated by SystemIV
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

        if( _msg ) fprintf( _file, "%s\n", _msg );

        fclose( _file );
    }
}