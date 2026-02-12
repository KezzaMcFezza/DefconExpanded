#ifndef _included_radar_perspective_window_h
#define _included_radar_perspective_window_h

#include "lib/eclipse/components/core.h"

#include "world/world.h"


class RadarPerspectiveWindow : public InterfaceWindow
{
public:
    int     m_teamOrder [MAX_TEAMS];
    int     m_selectionTeamId;

public:
    RadarPerspectiveWindow();

    void Create();
    void Update();
    void Render( bool _hasFocus );
};

#endif
