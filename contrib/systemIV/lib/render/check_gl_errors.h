#ifndef CHECK_GL_ERRORS_H
#define CHECK_GL_ERRORS_H

#include "systemiv.h"
#include "lib/debug_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*

  Very simple and effective OpenGL error checking API. The API usage is meant 
  to be as simple as possible to prevent messy code.

  USAGE:

  Basic error checking and will provide good results
    CHECK_GL_ERRORS("operation name");
    Example: CHECK_GL_ERRORS("SetBlendMode");

    
  Debug error checking, more verbose and breaks on critical errors
    CHECK_GL_ERRORS_DEBUG("operation name");
    Example: CHECK_GL_ERRORS_DEBUG("DrawElements");


  Break into debugger on any error:
    CHECK_GL_ERRORS_BREAK("operation name");


  For when you just want to check for errors without logging the operation name
  More useful for performance, avoids string lookups
    CHECK_GL_ERRORS_SILENT();


  BUILD CONFIGURATION:
  
  To enable, define TRACK_GL_ERRORS:
  #define TRACK_GL_ERRORS

  And if you want to track the number of errors too:
  #define TRACK_GL_ERROR_COUNT
    
  (CHECK_GL_ERRORS_DEBUG, CHECK_GL_ERRORS_BREAK) are only
  available when _DEBUG is defined


  OUTPUT FORMAT:
  
  Regular errors:
    OpenGL Error in ClearScreen (renderer.cpp:699): GL_INVALID_ENUM (0x0500)

  Silent errors:
    OpenGL Error in unknown operation (renderer.cpp:789): GL_INVALID_ENUM (0x0500)
    
  Debug errors:
    DEBUG OpenGL Error in DrawElements (renderer.cpp:1337): GL_INVALID_OPERATION (0x0502)
    Warning: No shader program currently bound
    Current VAO: 1
    Current VBO: 2
    Current FBO: 0

*/

//
// platform specific debugger break

#ifdef TARGET_MSVC
    #define _DEBUG_BREAK() __debugbreak()
#elif defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
    #define _DEBUG_BREAK() __builtin_trap()
#else
    #define _DEBUG_BREAK() ((void)0)
#endif

#ifdef TRACK_GL_ERROR_COUNT
static int g_glErrorCount = 0;
#endif

//
// convert OpenGL error code to string

static inline const char* glErrorToString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:                      return "GL_NO_ERROR";
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    
        #ifndef TARGET_EMSCRIPTEN
        case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
        #endif
        
        #ifdef GL_CONTEXT_LOST
        case GL_CONTEXT_LOST:                  return "GL_CONTEXT_LOST";
        #endif
        
        default:                               return "UNKNOWN_ERROR";
    }
}

//
// truncate filepath, just show filename

static inline const char* getFilename(const char* filepath) {
    if (!filepath) return "unknown file";
    
    const char* filename = filepath;
    const char* lastSlash = strrchr(filepath, '/');
    const char* lastBackslash = strrchr(filepath, '\\');
    
    if (lastSlash && lastSlash > filename) {
        filename = lastSlash + 1;
    }
    if (lastBackslash && lastBackslash > filename) {
        filename = lastBackslash + 1;
    }
    
    return filename;
}

//
// lightweight OpenGL error checking
// can be used in release with minimal overhead

static inline void checkGLErrors(const char* operation, const char* file, int line) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        #ifdef TRACK_GL_ERROR_COUNT
        g_glErrorCount++;
        AppDebugOut("OpenGL Error #%d in %s (%s:%d): %s (0x%04X)\n",
                    g_glErrorCount,
                    operation ? operation : "unknown operation",
                    getFilename(file),
                    line,
                    glErrorToString(error), 
                    error);
        #else
        AppDebugOut("OpenGL Error in %s (%s:%d): %s (0x%04X)\n", 
                    operation ? operation : "unknown operation",
                    getFilename(file),
                    line,
                    glErrorToString(error), 
                    error);
        #endif

        while ((error = glGetError()) != GL_NO_ERROR) {
            AppDebugOut("OpenGL Error: %s (0x%04X)\n", 
                        glErrorToString(error), 
                        error);
        }
    }
}

//
// more verbose OpenGL error checking
// more useful for integrated graphics, especially Intel Xe and HD graphics

static inline void checkGLErrorsDebug(const char* operation, const char* file, int line) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        GLenum firstError = error;
        
        #ifdef TRACK_GL_ERROR_COUNT
        g_glErrorCount++;
        AppDebugOut("DEBUG OpenGL Error #%d in %s (%s:%d): %s (0x%04X)\n",
                    g_glErrorCount,
                    operation ? operation : "unknown operation",
                    getFilename(file),
                    line,
                    glErrorToString(error), 
                    error);
        #else
        AppDebugOut("DEBUG OpenGL Error in %s (%s:%d): %s (0x%04X)\n", 
                    operation ? operation : "unknown operation",
                    getFilename(file),
                    line,
                    glErrorToString(error), 
                    error);
        #endif

        GLint currentProgram;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
        if (currentProgram == 0) {
            AppDebugOut("Warning: No shader program currently bound\n");
        } else {
            AppDebugOut("Current Program: %d\n", currentProgram);
        }
        
        //
        // check VAO binding

        GLint currentVAO;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
        AppDebugOut("  Current VAO: %d\n", currentVAO);
        
        //
        // check VBO binding

        GLint currentVBO;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &currentVBO);
        AppDebugOut("  Current VBO: %d\n", currentVBO);
        
        //
        // check framebuffer binding
        
        GLint currentFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
        AppDebugOut("  Current FBO: %d\n", currentFBO);
        
        if (currentFBO != 0) {
            GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
                AppDebugOut("Warning: Framebuffer is incomplete (0x%04X)\n", fbStatus);
            }
        }
        
        while ((error = glGetError()) != GL_NO_ERROR) {
            AppDebugOut("DEBUG OpenGL Error: %s (0x%04X)\n", 
                        glErrorToString(error), 
                        error);
        }
        
        //
        // break on critical errors in debug builds

        #ifdef _DEBUG
        if (firstError == GL_OUT_OF_MEMORY || 
            firstError == GL_INVALID_OPERATION ||
            firstError == GL_INVALID_FRAMEBUFFER_OPERATION) {
            _DEBUG_BREAK();
        }
        #endif
        
        (void)firstError;
    }
}

//
// return nothing if error tracking is disabled

#ifndef TRACK_GL_ERRORS
    #define CHECK_GL_ERRORS(operation) ((void)0)
    #define CHECK_GL_ERRORS_DEBUG(operation) ((void)0)
    #define CHECK_GL_ERRORS_SILENT() ((void)0)
    #define CHECK_GL_ERRORS_BREAK(operation) ((void)0)
#endif

//
// define macros when tracking is enabled

#ifdef TRACK_GL_ERRORS
    #define CHECK_GL_ERRORS(operation) checkGLErrors(operation, __FILE__, __LINE__)
    #define CHECK_GL_ERRORS_SILENT() checkGLErrors(NULL, __FILE__, __LINE__)

    #ifdef _DEBUG
        #define CHECK_GL_ERRORS_DEBUG(operation) checkGLErrorsDebug(operation, __FILE__, __LINE__)
        
        #define CHECK_GL_ERRORS_BREAK(operation) do { \
            GLenum error = glGetError(); \
            if (error != GL_NO_ERROR) { \
                checkGLErrors(operation, __FILE__, __LINE__); \
                _DEBUG_BREAK(); \
            } \
        } while(0)
    #else
        #define CHECK_GL_ERRORS_DEBUG(operation) ((void)0)
        #define CHECK_GL_ERRORS_BREAK(operation) ((void)0)
    #endif
#endif
#endif
