#include "systemiv.h"

#include "win32library.h"


Win32Library::Win32Library( const char *name )
	: m_hInstance( LoadLibrary( name ) )
{
}


Win32Library::~Win32Library()
{
	if ( m_hInstance )
	{
		FreeLibrary( m_hInstance );
	}
}


HMODULE Win32Library::GetInstance()
{
	return m_hInstance;
}


void *Win32Library::GetProcAddress( const char *name )
{
	if ( !m_hInstance )
		return NULL;
	return ::GetProcAddress( m_hInstance, name );
}
