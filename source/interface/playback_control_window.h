#ifndef _included_playback_control_window_h
#define _included_playback_control_window_h

#include "interface/components/core.h"
#include "interface/fading_window.h"
#include "world/world.h"

extern int g_desiredPerspectiveTeamId;
extern bool g_healthBarsEnabled;

class PlaybackControlWindow : public FadingWindow
{
private:
    bool m_isPaused;
    float m_currentSpeed;
    int m_currentSeqId;
    int m_totalSeqIds;
    char m_cachedProgressText[64];
    int m_lastRenderedSeqId;
    int m_lastRenderedTotalSeqIds;
    float m_updateTimer;
    bool m_healthBarsEnabled;
    bool m_hideUI;

    struct PlayerInfo
    {
        int teamId;
        int playerId;
        char playerName[64];
        char truncatedName[32];
        Colour teamColour;
        bool isActive;
    };
    
    PlayerInfo m_players[6];
    int m_activePlayerCount;
    int m_currentPerspective;
    bool m_playersInitialized;
    
public:
    PlaybackControlWindow();
    
    void Create();
    void Render( bool _hasFocus );
    void Update();
    
    void TogglePause();
    void SetSpeed( float speed );
    void UpdateProgress( int currentSeq, int totalSeq );
    void SeekToPosition( float position );  // Seek to position (0.0 to 1.0)
    void ToggleHealthBars();
    void ToggleHideUI();
    void InitializePlayers();
    void SetPlayerPerspective( int playerIndex );
    void SetSpectatorPerspective();
    void RefreshPlayerButtons();
    void RefreshPlayerColors();
    void TruncatePlayerName( const char* fullName, char* truncatedName, int maxWidth );
    bool ShouldRender();
};

// ============================================================================
// Control buttons

class PlayPauseButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class HealthToggleButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class GlobeToggleButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class HideUIButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class SpeedSlider : public InterfaceButton
{
public:
    float m_value;  
    bool m_dragging;
    
public:
    SpeedSlider();
    
    void MouseDown();
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void Update();
    
    float GetSpeedFromValue();
    void SetValueFromSpeed( float speed ); 
};

class SeekBar : public InterfaceButton
{
public:
    float m_value;
    bool m_dragging;
    bool m_seeking;
    
public:
    SeekBar();
    
    void MouseDown();
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void Update();
    
    void SetProgress( float progress );
    float GetSeekPosition();
    bool IsSeeking() const { return m_seeking; }
    void SetSeeking( bool seeking ) { m_seeking = seeking; }
};

class PlayerPerspectiveButton : public InterfaceButton
{
public:
    int m_playerIndex;
    int m_teamId;
    Colour m_teamColour;
    bool m_isSelected;
    
public:
    PlayerPerspectiveButton();
    
    void SetPlayer( int playerIndex, int teamId, Colour teamColour );
    void SetSelected( bool selected );
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

#endif