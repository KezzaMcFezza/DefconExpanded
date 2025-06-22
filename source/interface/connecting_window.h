#ifndef _included_connectingwindow_h
#define _included_connectingwindow_h


#include "interface/components/core.h"



class ConnectingWindow : public InterfaceWindow
{
public:
    int     m_maxUpdatesRemaining;
    int     m_maxLagRemaining;
    bool    m_popupLobbyAtEnd;
    
    int     m_stage;
    float   m_stageStartTime;
    
    // NEW: Fast-forward recording synchronization
    bool    m_fastForwardMode;
    int     m_fastForwardTarget;
    int     m_fastForwardCurrent;

public:
    ConnectingWindow();
    ~ConnectingWindow();

    void Create();
    void Render( bool _hasFocus );
    void RenderTimeRemaining( float _fractionDone );
    
    // NEW: Fast-forward recording methods
    void SetFastForwardMode( bool enabled, int target = 0 );
    void UpdateFastForwardProgress( int current );
};




#endif
