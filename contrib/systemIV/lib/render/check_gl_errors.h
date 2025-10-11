#ifndef CHECK_GL_ERRORS_H
#define CHECK_GL_ERRORS_H

#include "lib/universal_include.h"
#include "lib/debug_utils.h"

#include <stdio.h>
#include <stdlib.h>

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
        case GL_TABLE_TOO_LARGE:               return "GL_TABLE_TOO_LARGE";
        #endif
        
        #ifdef GL_CONTEXT_LOST
        case GL_CONTEXT_LOST:                  return "GL_CONTEXT_LOST";
        #endif
        
        default:                               return "UNKNOWN_ERROR";
    }
}

static inline void checkGLErrors(const char* operation, const char* file, int line) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        AppDebugOut("OpenGL Error in %s (%s:%d): %s (0x%04X)\n", 
                    operation ? operation : "unknown operation",
                    file ? file : "unknown file",
                    line,
                    glErrorToString(error), 
                    error);
        
        while ((error = glGetError()) != GL_NO_ERROR) {
            AppDebugOut("OpenGL Error: %s (0x%04X)\n", 
                        glErrorToString(error), 
                        error);
        }
    }
}

#ifndef TRACK_GL_ERRORS
#define CHECK_GL_ERRORS(operation) ((void)0)
#define CHECK_GL_ERRORS_DEBUG(operation) ((void)0)
#define CHECK_GL_ERRORS_SILENT() ((void)0)
#define CHECK_GL_ERRORS_BREAK(operation) ((void)0)
#endif

#ifdef TRACK_GL_ERRORS
#define CHECK_GL_ERRORS(operation) checkGLErrors(operation, __FILE__, __LINE__)
#define CHECK_GL_ERRORS_SILENT() checkGLErrors(NULL, __FILE__, __LINE__)

#ifdef _DEBUG
    #define CHECK_GL_ERRORS_DEBUG(operation) checkGLErrors(operation, __FILE__, __LINE__)
#endif

#ifdef _DEBUG
    #define CHECK_GL_ERRORS_BREAK(operation) do { \
        GLenum error = glGetError(); \
        if (error != GL_NO_ERROR) { \
            checkGLErrors(operation, __FILE__, __LINE__); \
            __debugbreak(); \
        } \
    } while(0)
#endif
#endif

#endif
