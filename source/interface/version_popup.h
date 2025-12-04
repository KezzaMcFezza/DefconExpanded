#ifndef INCLUDED_VERSION_POPUP_H
#define INCLUDED_VERSION_POPUP_H

#include "interface/components/core.h"

class VersionPopupWindow : public InterfaceWindow
{
public:
    VersionPopupWindow();

    void Create();
    void Render( bool _hasFocus );
};


bool CheckMainDatVersion();

#endif
