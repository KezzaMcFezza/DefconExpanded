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
    
    // Fast-forward recording synchronization
    bool    m_fastForwardMode;
    int     m_fastForwardTarget;
    int     m_fastForwardCurrent;
    bool    m_isSeekMode;  // Track if this is a seek operation vs regular fast-forward

public:
    ConnectingWindow();
    ~ConnectingWindow();

    void Create();
    void Render( bool _hasFocus );
    void RenderTimeRemaining( float _fractionDone );
    
    // Fast-forward recording methods
    void UpdateWindowSize();
    void SetFastForwardMode( bool enabled, int target = 0, bool isSeekMode = false );
    void UpdateFastForwardProgress( int current );
};




#endif