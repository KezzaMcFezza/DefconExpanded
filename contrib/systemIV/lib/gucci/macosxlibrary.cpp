#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>


void GetMainDisplayResolution( int &width, int &height )
{
    CGRect rect = CGDisplayBounds( CGMainDisplayID() );
    width = (int)rect.size.width;
    height = (int)rect.size.height;
}


void MacOSXHideWin()
{
    ProcessSerialNumber me;
    
    GetCurrentProcess(&me);
    ShowHideProcess(&me, false);
}
