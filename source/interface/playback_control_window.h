#ifndef _included_playback_control_window_h
#define _included_playback_control_window_h

#include "interface/components/core.h"
#include "world/world.h"

// global variable for team perspective switching
extern int g_desiredPerspectiveTeamId;

// global variable for health bar visibility
extern bool g_healthBarsEnabled;

class PlaybackControlWindow : public InterfaceWindow
{
private:
    bool m_isPaused;
    float m_currentSpeed;
    int m_currentSeqId;
    int m_totalSeqIds;
    
    // Performance optimization variables
    char m_cachedProgressText[64];      // Cache formatted progress text
    int m_lastRenderedSeqId;            // Track when progress text needs updating
    int m_lastRenderedTotalSeqIds;      // Track when progress text needs updating
    float m_updateTimer;                // Throttle update frequency
    
    // Health bar toggle state
    bool m_healthBarsEnabled;
    
    // Player perspective system
    struct PlayerInfo
    {
        int teamId;
        int playerId;
        char playerName[64];             // Player name for button display
        char truncatedName[32];          // Truncated name for button (with ... if needed)
        Colour teamColour;
        bool isActive;
    };
    
    PlayerInfo m_players[6];             // Max 6 players in DEFCON
    int m_activePlayerCount;             // Number of active players
    int m_currentPerspective;            // -1 = spectator view, 0-5 = player index
    bool m_playersInitialized;           // Whether player data has been scanned
    
public:
    PlaybackControlWindow();
    
    void Create();
    void Render( bool _hasFocus );
    void Update();
    
    // Control methods
    void TogglePause();
    void SetSpeed( float speed );
    void UpdateProgress( int currentSeq, int totalSeq );
    void SeekToPosition( float position );  // Seek to position (0.0 to 1.0)
    
    // Health bar toggle
    void ToggleHealthBars();
    
    // Player perspective methods
    void InitializePlayers();            // Scan teams and detect players
    void SetPlayerPerspective( int playerIndex );  // Switch to player view
    void SetSpectatorPerspective();      // Switch to spectator view (see all)
    void RefreshPlayerButtons();         // Update button states after perspective change
    void RefreshPlayerColors();          // Update player colors when alliances change
    void TruncatePlayerName( const char* fullName, char* truncatedName, int maxWidth );
    
    // Should only show during recording playback
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
    
    float GetSpeedFromValue(); // Convert slider value to actual speed multiplier
    void SetValueFromSpeed( float speed ); // Convert speed to slider position
};

class SeekBar : public InterfaceButton
{
public:
    float m_value;      // 0.0 to 1.0, represents position in recording
    bool m_dragging;
    bool m_seeking;     // True when actively seeking (fast-forwarding)
    
public:
    SeekBar();
    
    void MouseDown();
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void Update();
    
    void SetProgress( float progress );     // Set current progress (0.0 to 1.0)
    float GetSeekPosition();                // Get desired seek position
    bool IsSeeking() const { return m_seeking; }
    void SetSeeking( bool seeking ) { m_seeking = seeking; }
};

// player perspective button for switching radar views
class PlayerPerspectiveButton : public InterfaceButton
{
public:
    int m_playerIndex;       // -1 = spectator view, 0-5 = player index
    int m_teamId;            // actual team ID for this player
    Colour m_teamColour;     // color of this players team
    bool m_isSelected;       // whether this perspective is currently active
    
public:
    PlayerPerspectiveButton();
    
    void SetPlayer( int playerIndex, int teamId, Colour teamColour );
    void SetSelected( bool selected );
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

#endif