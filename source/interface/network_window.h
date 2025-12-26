
#ifndef _included_networkinterface_h
#define _included_networkinterface_h

#include "lib/eclipse/components/core.h"


class NetworkWindow : public InterfaceWindow
{
public:
    NetworkWindow( const char *name, const char *title = NULL, bool titleIsLanguagePhrase = false );

    void Create();
    void Render( bool hasFocus );
};


#endif
