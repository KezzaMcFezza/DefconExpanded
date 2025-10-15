#ifndef _included_mapeditorwindow_h
#define _included_mapeditorwindow_h


#include "interface/components/core.h"

class MapEditorWindow: public InterfaceWindow
{

public:

    MapEditorWindow();
    ~MapEditorWindow();
    void Create();
    void Render( bool _hasFocus );

};




#endif
