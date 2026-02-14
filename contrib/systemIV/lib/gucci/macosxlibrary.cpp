#include "systemiv.h"

void MacOSXHideWin()
{
	ProcessSerialNumber me;

	GetCurrentProcess( &me );
	ShowHideProcess( &me, false );
}
