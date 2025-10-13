#include <glad/glad.h>
#include <glad/wgl_ext.h>
#include <windows.h>

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;

int gladLoadWGL(HDC hdc) {
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    
    if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB) {
        return 0; 
    }
    
    return 1; // Success
}
