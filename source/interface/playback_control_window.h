#ifndef _included_playback_control_window_h
#define _included_playback_control_window_h

#include "interface/components/core.h"
#include "world/world.h"

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
    
public:
    PlaybackControlWindow();
    
    void Create();
    void Render( bool _hasFocus );
    void Update();
    
    // Control methods
    void TogglePause();
    void SetSpeed( float speed );
    void UpdateProgress( int currentSeq, int totalSeq );
    void SeekToPosition( float position );  // NEW: Seek to position (0.0 to 1.0)
    
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

class SpeedSlider : public InterfaceButton
{
public:
    float m_value;  // 0.0 to 1.0, where 0.0 = 1x speed, 1.0 = 500x speed
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

// NEW: Seek bar for navigating through recording
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

#endif 