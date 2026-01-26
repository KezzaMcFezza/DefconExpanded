#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>


void MacOSXHideWin()
{
	ProcessSerialNumber me;

	GetCurrentProcess( &me );
	ShowHideProcess( &me, false );
}
