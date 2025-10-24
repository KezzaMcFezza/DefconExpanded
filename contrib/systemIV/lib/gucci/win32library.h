#ifndef INCLUDED_WIN32_LIBRARY_H
#define INCLUDED_WIN32_LIBRARY_H

#include <windows.h>


class Win32Library
{
public:
    Win32Library( const char *name );
    ~Win32Library();
    HMODULE GetInstance();
    void *GetProcAddress( const char *name );

private:
    HMODULE m_hInstance;
};

#endif
