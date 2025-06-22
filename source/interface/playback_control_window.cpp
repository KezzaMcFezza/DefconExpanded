#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render/styletable.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/hi_res_time.h"
#include "lib/math/math_utils.h"

#include "interface/playback_control_window.h"
#include "interface/components/message_dialog.h"

#include "app/app.h"
#include "world/world.h"
#include "app/globals.h"
#include "network/Server.h"

// ============================================================================
// Playback Control Window

PlaybackControlWindow::PlaybackControlWindow()
:   InterfaceWindow("Playback Controls", "Playback Controls", false),
    m_isPaused(false),
    m_currentSpeed(1.0f),
    m_currentSeqId(0),
    m_totalSeqIds(0),
    m_lastRenderedSeqId(-1),
    m_lastRenderedTotalSeqIds(-1),
    m_updateTimer(0.0f)
{
    // Position at bottom center of screen
    int windowWidth = 400;
    int windowHeight = 95;  // Increased from 80 to 95 for proper spacing
    
    SetSize(windowWidth, windowHeight);
    
    int x = (g_windowManager->WindowW() - windowWidth) / 2;
    int y = g_windowManager->WindowH() - windowHeight - 10; // 10px from bottom
    
    SetPosition(x, y);
    
    // Make it non-movable
    SetMovable(false);
    
    // Initialize cached progress text
    strcpy(m_cachedProgressText, "0.0%");
}

void PlaybackControlWindow::Create()
{
    InterfaceWindow::Create();
    
    // Play/Pause button
    PlayPauseButton *playPause = new PlayPauseButton();
    playPause->SetProperties("PlayPause", 10, 55, 50, 25, "PLAY", "Toggle pause/play", false, true);
    RegisterButton(playPause);
    
    // Speed slider (longer since we removed the speed display)
    SpeedSlider *speedSlider = new SpeedSlider();
    speedSlider->SetProperties("SpeedSlider", 80, 60, 300, 15, "", "Adjust playback speed", false, false);
    RegisterButton(speedSlider);
}

void PlaybackControlWindow::Render(bool _hasFocus)
{
    if (!ShouldRender()) return;
    
    InterfaceWindow::Render(_hasFocus);
    
    // Render progress bar
    float progressX = m_x + 10;
    float progressY = m_y + 25;  // Moved down to 25 for even better spacing
    float progressW = m_w - 20;
    float progressH = 8;
    
    // Background
    g_renderer->RectFill(progressX, progressY, progressW, progressH, Colour(50, 50, 50, 200));
    g_renderer->Rect(progressX, progressY, progressW, progressH, Colour(100, 100, 100, 255));
    
    // Progress fill
    if (m_totalSeqIds > 0) {
        float progress = (float)m_currentSeqId / (float)m_totalSeqIds;
        float fillW = progressW * progress;
        g_renderer->RectFill(progressX, progressY, fillW, progressH, Colour(100, 150, 255, 200));
    }
    
    // Progress text (cached for performance)
    g_renderer->TextCentreSimple(m_x + m_w/2, progressY - 3, Colour(200, 200, 200, 255), 10.0f, m_cachedProgressText);
}

void PlaybackControlWindow::Update()
{
    if (!ShouldRender()) {
        return;
    }
    
    // Throttle updates to reduce CPU usage - only update 30 times per second instead of every frame
    m_updateTimer += g_advanceTime;
    if (m_updateTimer < 0.033f) { // ~30 FPS update rate
        return;
    }
    m_updateTimer = 0.0f;
    
    // Update current state from server
    if (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode()) {
        // Get current progress
        m_currentSeqId = g_app->GetServer()->m_recordingCurrentSeqId;
        m_totalSeqIds = g_app->GetServer()->m_recordingEndSeqId;
        
        // Only update cached progress text if values changed (expensive string formatting)
        if (m_currentSeqId != m_lastRenderedSeqId || m_totalSeqIds != m_lastRenderedTotalSeqIds) {
            if (m_totalSeqIds > 0) {
                float progressPercent = ((float)m_currentSeqId / (float)m_totalSeqIds) * 100.0f;
                snprintf(m_cachedProgressText, sizeof(m_cachedProgressText), "%.1f%%", progressPercent);
            } else {
                strcpy(m_cachedProgressText, "0.0%");
            }
            m_lastRenderedSeqId = m_currentSeqId;
            m_lastRenderedTotalSeqIds = m_totalSeqIds;
        }
        
        // Update pause state
        m_isPaused = g_app->GetServer()->IsRecordingPaused();
        
        // Get current speed - since pause is now handled at main loop level, 
        // we can always use the speed multiplier
        m_currentSpeed = g_app->GetServer()->GetRecordingAdvanceSpeedMultiplier();
        
        // Update play/pause button caption (only when needed)
        EclButton *playPauseBtn = GetButton("PlayPause");
        if (playPauseBtn) {
            const char* expectedCaption = m_isPaused ? "PLAY" : "PAUSE";
            if (strcmp(playPauseBtn->m_caption, expectedCaption) != 0) {
                playPauseBtn->SetCaption(expectedCaption, false);
            }
        }
        
        // Update speed slider
        SpeedSlider *speedSlider = (SpeedSlider*)GetButton("SpeedSlider");
        if (speedSlider) {
            speedSlider->Update(); // Call Update to handle mouse interaction
            if (!speedSlider->m_dragging) {
                // Update slider position when not dragging
                speedSlider->SetValueFromSpeed(m_currentSpeed);
            }
        }
    }
    
    InterfaceWindow::Update();
}

void PlaybackControlWindow::TogglePause()
{
    if (!g_app->GetServer() || !g_app->GetServer()->IsRecordingPlaybackMode()) return;
    
    // Simply toggle pause state - the main loop will handle the rest
    g_app->GetServer()->SetRecordingPaused(!m_isPaused);
}

void PlaybackControlWindow::SetSpeed(float speed)
{
    if (!g_app->GetServer() || !g_app->GetServer()->IsRecordingPlaybackMode()) return;
    
    g_app->GetServer()->SetRecordingSpeed(speed);
}

void PlaybackControlWindow::UpdateProgress(int currentSeq, int totalSeq)
{
    m_currentSeqId = currentSeq;
    m_totalSeqIds = totalSeq;
}

bool PlaybackControlWindow::ShouldRender()
{
    // Only show during recording playback
    return g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode();
}

// ============================================================================
// Play/Pause Button

void PlayPauseButton::MouseUp()
{
    PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
    if (parent) {
        parent->TogglePause();
    }
}

void PlayPauseButton::Render(int realX, int realY, bool highlighted, bool clicked)
{
    // Use standard button rendering
    InterfaceButton::Render(realX, realY, highlighted, clicked);
}

// ============================================================================
// Speed Slider

SpeedSlider::SpeedSlider()
:   m_value(0.0f),  // Start at 1x speed (left side)
    m_dragging(false)
{
}

void SpeedSlider::MouseDown()
{
    m_dragging = true;
    
    // Immediately update the value when clicked
    float relativeX = g_inputManager->m_mouseX - (m_x + m_parent->m_x);
    m_value = relativeX / (float)m_w;
    m_value = max(0.0f, min(1.0f, m_value)); // Clamp to 0-1
    
    // Update speed immediately
    PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
    if (parent) {
        float speed = GetSpeedFromValue();
        parent->SetSpeed(speed);
    }
}

void SpeedSlider::MouseUp()
{
    m_dragging = false;
}

void SpeedSlider::Render(int realX, int realY, bool highlighted, bool clicked)
{
    // Slider track
    Colour trackCol = Colour(80, 80, 80, 255);
    Colour thumbCol = highlighted ? Colour(150, 150, 255, 255) : Colour(120, 120, 200, 255);
    
    g_renderer->RectFill(realX, realY + m_h/2 - 2, m_w, 4, trackCol);
    g_renderer->Rect(realX, realY + m_h/2 - 2, m_w, 4, Colour(120, 120, 120, 255));
    
    // Slider thumb
    float thumbX = realX + (m_value * (m_w - 10));
    g_renderer->RectFill(thumbX, realY, 10, m_h, thumbCol);
    g_renderer->Rect(thumbX, realY, 10, m_h, Colour(200, 200, 200, 255));
    
    // Static speed markers (rendered every frame but these are simple)
    g_renderer->TextSimple(realX, realY - 12, Colour(180, 180, 180, 255), 8.0f, "1x");
    g_renderer->TextSimple(realX + m_w - 20, realY - 12, Colour(180, 180, 180, 255), 8.0f, "10x");
}

void SpeedSlider::Update()
{
    if (m_dragging) {
        // Update value based on mouse position
        float relativeX = g_inputManager->m_mouseX - (m_x + m_parent->m_x);
        m_value = relativeX / (float)m_w;
        m_value = max(0.0f, min(1.0f, m_value)); // Clamp to 0-1
        
        // Update speed
        PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
        if (parent) {
            float speed = GetSpeedFromValue();
            parent->SetSpeed(speed);
        }
    }
    
    // Check for mouse release to stop dragging
    if (m_dragging && !g_inputManager->m_lmb) {
        m_dragging = false;
    }
}

float SpeedSlider::GetSpeedFromValue()
{
    if (m_value == 0.0f) {
        return 1.0f; // Left side = 1x speed
    }
    
    // Exponential curve for better control
    // 0.0 -> 1x, 0.5 -> ~25x, 1.0 -> 10x
    float normalizedValue = m_value;
    return 1.0f + (99.0f * normalizedValue * normalizedValue);
}

void SpeedSlider::SetValueFromSpeed(float speed)
{
    if (speed <= 1.0f) {
        m_value = 0.0f;
    } else {
        // Inverse of GetSpeedFromValue calculation
        float normalizedSpeed = (speed - 1.0f) / 99.0f;
        m_value = sqrt(normalizedSpeed);
        m_value = max(0.0f, min(1.0f, m_value));
    }
} 