#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render/styletable.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/hi_res_time.h"
#include "lib/math/math_utils.h"
#include "lib/eclipse/eclipse.h"

#include "interface/playback_control_window.h"
#include "interface/components/message_dialog.h"
#include "interface/connecting_window.h"

#include "app/app.h"
#include "world/world.h"
#include "world/team.h"
#include "app/globals.h"
#include "network/Server.h"
#include "renderer/map_renderer.h"

#if RECORDING_PARSING

class HideUIConfirmCloseButton : public InterfaceButton
{
public:
    void MouseUp()
    {
        // hide the UI when this button is pressed
        g_app->ToggleHideUI();
        
        EclRemoveWindow(m_parent->m_name);
    }
};

class HideUIConfirmDialog : public MessageDialog
{
public:
    HideUIConfirmDialog()
    :   MessageDialog("Hide UI Confirmation", 
                     "You can press H on the keyboard to unhide the UI", 
                     false, 
                     "HidE UI", 
                     false)
    {
    }
    
    void Create()
    {
        MessageDialog::Create();
        
        EclButton *closeBtn = GetButton("Close");
        if (closeBtn) {
            RemoveButton("Close");
        }
        
        HideUIConfirmCloseButton *confirmBtn = new HideUIConfirmCloseButton();
        confirmBtn->SetProperties("Close", (m_w - 80)/2, m_h - 25, 80, 18, "dialog_ok", " ", true, false);
        RegisterButton(confirmBtn);
    }
};

int g_desiredPerspectiveTeamId = -1;        // global variable for team perspective switching
bool g_healthBarsEnabled = false;           // global variable for health bar visibility

// ============================================================================
// Playback Control Window

PlaybackControlWindow::PlaybackControlWindow()
:   FadingWindow("Playback Controls"),
    m_isPaused(false),
    m_currentSpeed(1.0f),
    m_currentSeqId(0),
    m_totalSeqIds(0),
    m_lastRenderedSeqId(-1),
    m_lastRenderedTotalSeqIds(-1),
    m_updateTimer(0.0f),
    m_activePlayerCount(0),
    m_currentPerspective(-1),
    m_playersInitialized(false),
    m_healthBarsEnabled(false),
    m_hideUI(false)
{
    int windowWidth = 520;
    int windowHeight = 190;
    
    SetSize(windowWidth, windowHeight);
    
    int x = (g_windowManager->WindowW() - windowWidth) / 2;
    int y = g_windowManager->WindowH() - windowHeight - 10; // 10px from bottom
    
    SetPosition(x, y);
    
    // Allow the window to be moved - users may want to position it differently
    // Removed SetMovable(false) which was causing weird drag behavior with top-right snapping to cursor
    
    // Prevent resizing by setting min/max size to current size
    m_minW = windowWidth;
    m_minH = windowHeight;
    
    // Initialize cached progress text
    strcpy(m_cachedProgressText, "0.0%");
    
    // initialize player data
    for( int i = 0; i < 6; i++ )
    {
        m_players[i].teamId = -1;
        m_players[i].playerId = -1;
        m_players[i].playerName[0] = '\0';
        m_players[i].truncatedName[0] = '\0';
        m_players[i].teamColour = Colour(100, 100, 100, 255);
        m_players[i].isActive = false;
    }
}

void PlaybackControlWindow::Create()
{
    //
    // create a fading window instead of the normal interface window
    // this matches the toolbar and chat window
    
    // buttons on the left side
    int leftButtonsX = 10;
    int buttonWidth = 70;
    int buttonHeight = 25;
    int buttonSpacing = 30; // vertical spacing between buttons
    
    // buttons aligned with progress bar Y level 
    int buttonStartY = 25;
    
    // play/pause button
    PlayPauseButton *playPause = new PlayPauseButton();
    playPause->SetProperties("PlayPause", leftButtonsX, buttonStartY, buttonWidth, buttonHeight, "PLAY", "Toggle pause/play", false, true);
    RegisterButton(playPause);
    
    // Health toggle button
    HealthToggleButton *healthToggle = new HealthToggleButton();
    healthToggle->SetProperties("HealthToggle", leftButtonsX, buttonStartY + buttonSpacing, buttonWidth, buttonHeight, "HEALTH", "Toggle health bars on/off", false, true);
    RegisterButton(healthToggle);
    
    // Hide UI button
    HideUIButton *hideUIButton = new HideUIButton();
    hideUIButton->SetProperties("HideUI", leftButtonsX, buttonStartY + buttonSpacing * 2, buttonWidth, buttonHeight, "Hide UI", "Hide user interface", false, true);
    RegisterButton(hideUIButton);
    
    // Controls area starts after the left buttons
    int controlsStartX = leftButtonsX + buttonWidth + 15; // Start after the left buttons with some padding
    int controlsWidth = m_w - controlsStartX - 10; // Full width minus left buttons and padding
    
    // Seek bar
    SeekBar *seekBar = new SeekBar();
    seekBar->SetProperties("SeekBar", controlsStartX, 50, controlsWidth, 17, "", "Click and drag to seek to different position in recording", false, false);
    RegisterButton(seekBar);
    
    // Speed slider 
    SpeedSlider *speedSlider = new SpeedSlider();
    speedSlider->SetProperties("SpeedSlider", controlsStartX, 85, controlsWidth, 17, "", "Adjust playback speed", false, false);
    RegisterButton(speedSlider);
    
    // initialize it
    InitializePlayers();
    
    // create the buttons below the seek bar and play button
    int spectatorButtonY = 130;      // top row for spectator button
    int playerButtonY = 155;         // second row for player buttons
    int perspectiveButtonWidth = 65;
    int perspectiveButtonHeight = 20;
    int perspectiveButtonSpacing = 70;
    int perspectiveStartX = controlsStartX; 
    
    // make sure to always create the spectator button first
    PlayerPerspectiveButton *spectatorBtn = new PlayerPerspectiveButton();
    spectatorBtn->SetProperties("Player_Spectator", perspectiveStartX, spectatorButtonY, perspectiveButtonWidth, perspectiveButtonHeight, 
                               "Spectator", "View all teams (spectator perspective)", false, true);
    spectatorBtn->SetPlayer(-1, -1, Colour(150, 150, 150, 255)); // grey
    spectatorBtn->SetSelected(m_currentPerspective == -1);
    RegisterButton(spectatorBtn);
    
    // create the buttons for the players in the game
    int buttonIndex = 0; 
    for( int i = 0; i < 6 && buttonIndex < 6; i++ ) // limit to 6 total buttons, in future for eight, ten and sixteen player we can make this higher
    {
        if( m_players[i].isActive )
        {
            char buttonId[32];
            sprintf(buttonId, "Player_%d", i);
            
            PlayerPerspectiveButton *playerBtn = new PlayerPerspectiveButton();
            playerBtn->SetProperties(buttonId, perspectiveStartX + (buttonIndex * perspectiveButtonSpacing), playerButtonY, 
                                   perspectiveButtonWidth, perspectiveButtonHeight, m_players[i].truncatedName, 
                                   m_players[i].playerName, false, true);
            playerBtn->SetPlayer(i, m_players[i].teamId, m_players[i].teamColour);
            playerBtn->SetSelected(m_currentPerspective == i);
            RegisterButton(playerBtn);
            
            buttonIndex++;
        }
    }
}

void PlaybackControlWindow::InitializePlayers()
{
    if( !g_app->GetWorld() || m_playersInitialized )
        return;
    
    // reset player data
    m_activePlayerCount = 0;
    for( int i = 0; i < 6; i++ )
    {
        m_players[i].teamId = -1;
        m_players[i].playerId = -1;
        m_players[i].playerName[0] = '\0';
        m_players[i].truncatedName[0] = '\0';
        m_players[i].isActive = false;
    }
    
    // scan all teams and collect player info
    int playerIndex = 0;
    for( int t = 0; t < g_app->GetWorld()->m_teams.Size() && playerIndex < 6; t++ )
    {
        Team *team = g_app->GetWorld()->m_teams[t];
        if( team )
        {
            m_players[playerIndex].teamId = team->m_teamId;
            m_players[playerIndex].playerId = team->m_teamId; // Use team ID as player ID for now
            strncpy(m_players[playerIndex].playerName, team->m_name, sizeof(m_players[playerIndex].playerName) - 1);
            m_players[playerIndex].playerName[sizeof(m_players[playerIndex].playerName) - 1] = '\0';
            
            // truncate name if its too long for the button
            TruncatePlayerName(m_players[playerIndex].playerName, m_players[playerIndex].truncatedName, 50);
            
            m_players[playerIndex].teamColour = team->GetTeamColour();
            m_players[playerIndex].isActive = true;
            
            playerIndex++;
            m_activePlayerCount++;
        }
    }
    
    m_playersInitialized = true;
}

void PlaybackControlWindow::TruncatePlayerName( const char* fullName, char* truncatedName, int maxWidth )
{
    if( !fullName || !truncatedName )
        return;
    
    // simple truncation, not the best way to do it
    int maxChars = 8; // approximate max chars that fit in button width
    int nameLen = strlen(fullName);
    
    if( nameLen <= maxChars )
    {
        strcpy(truncatedName, fullName);
    }
    else
    {
        strncpy(truncatedName, fullName, maxChars - 3);
        truncatedName[maxChars - 3] = '\0';
        strcat(truncatedName, "...");
    }
}

void PlaybackControlWindow::SetPlayerPerspective( int playerIndex )
{
    if( playerIndex >= 0 && playerIndex < 6 && m_players[playerIndex].isActive )
    {
        m_currentPerspective = playerIndex;
        
        g_desiredPerspectiveTeamId = m_players[playerIndex].teamId;
        g_app->GetWorld()->m_myTeamId = m_players[playerIndex].teamId;
        g_app->GetWorld()->UpdateRadar(); // refresh
        
        RefreshPlayerButtons();
    }
}

void PlaybackControlWindow::SetSpectatorPerspective()
{
    m_currentPerspective = -1;
    g_desiredPerspectiveTeamId = -1;
    g_app->GetWorld()->m_myTeamId = -1;
    g_app->GetWorld()->UpdateRadar(); // refresh
    RefreshPlayerButtons();
}

void PlaybackControlWindow::RefreshPlayerButtons()
{
    // update the spectator button selected state
    PlayerPerspectiveButton *spectatorBtn = (PlayerPerspectiveButton*)GetButton("Player_Spectator");
    if( spectatorBtn )
    {
        spectatorBtn->SetSelected(m_currentPerspective == -1);
    }
    
    // now update the player buttons selected state
    for( int i = 0; i < 6; i++ )
    {
        if( m_players[i].isActive )
        {
            char buttonId[32];
            sprintf(buttonId, "Player_%d", i);
            PlayerPerspectiveButton *playerBtn = (PlayerPerspectiveButton*)GetButton(buttonId);
            if( playerBtn )
            {
                playerBtn->SetSelected(m_currentPerspective == i);
            }
        }
    }
}

void PlaybackControlWindow::RefreshPlayerColors()
{
    // only refresh if players are initialized and we have world data!
    if( !m_playersInitialized || !g_app->GetWorld() )
        return;
    
    // update colors for all active players
    for( int i = 0; i < 6; i++ )
    {
        if( m_players[i].isActive )
        {
            // find the team and get its current color
            Team *team = g_app->GetWorld()->GetTeam(m_players[i].teamId);
            if( team )
            {
                Colour newColour = team->GetTeamColour();
                
                // only update if color has changed
                if( m_players[i].teamColour.m_r != newColour.m_r || 
                    m_players[i].teamColour.m_g != newColour.m_g || 
                    m_players[i].teamColour.m_b != newColour.m_b )
                {
                    m_players[i].teamColour = newColour;
                    
                    // now update the buttons color
                    char buttonId[32];
                    sprintf(buttonId, "Player_%d", i);
                    PlayerPerspectiveButton *playerBtn = (PlayerPerspectiveButton*)GetButton(buttonId);
                    if( playerBtn )
                    {
                        playerBtn->SetPlayer(i, m_players[i].teamId, newColour);
                    }
                }
            }
        }
    }
}

void PlaybackControlWindow::Render(bool _hasFocus)
{
    if (!ShouldRender()) return;
    
    FadingWindow::Render(_hasFocus, false);
    
    EclButton *playPauseBtn = GetButton("PlayPause");
    if (playPauseBtn) {
        playPauseBtn->Render(m_x + playPauseBtn->m_x, m_y + playPauseBtn->m_y, false, false);
    }
    
    EclButton *healthToggleBtn = GetButton("HealthToggle");
    if (healthToggleBtn) {
        healthToggleBtn->Render(m_x + healthToggleBtn->m_x, m_y + healthToggleBtn->m_y, false, false);
    }
    
    EclButton *hideUIBtn = GetButton("HideUI");
    if (hideUIBtn) {
        hideUIBtn->Render(m_x + hideUIBtn->m_x, m_y + hideUIBtn->m_y, false, false);
    }
    
    EclButton *speedSlider = GetButton("SpeedSlider");
    if (speedSlider) {
        speedSlider->Render(m_x + speedSlider->m_x, m_y + speedSlider->m_y, false, false);
    }
    
    EclButton *seekBar = GetButton("SeekBar");
    if (seekBar) {
        seekBar->Render(m_x + seekBar->m_x, m_y + seekBar->m_y, false, false);
    }
    
    EclButton *spectatorBtn = GetButton("Player_Spectator");
    if (spectatorBtn) {
        spectatorBtn->Render(m_x + spectatorBtn->m_x, m_y + spectatorBtn->m_y, false, false);
    }
    
    for (int i = 0; i < 6; i++) {
        if (m_players[i].isActive) {
            char buttonId[32];
            sprintf(buttonId, "Player_%d", i);
            EclButton *playerBtn = GetButton(buttonId);
            if (playerBtn) {
                playerBtn->Render(m_x + playerBtn->m_x, m_y + playerBtn->m_y, false, false);
            }
        }
    }
    
    // reinitialize players if not done yet, in case teams loaded after window creation
    if( !m_playersInitialized && g_app->GetWorld() && g_app->GetWorld()->m_teams.Size() > 0 )
    {
        InitializePlayers();
        // recreate window to add player buttons
        Remove();
        PlaybackControlWindow *newWindow = new PlaybackControlWindow();
        EclRegisterWindow(newWindow);
        return;
    }
    
    float progressX = m_x + 95;
    float progressY = m_y + 25;
    float progressW = m_w - 105;
    float progressH = 10;  
    
    // Background
    g_renderer2d->RectFill(progressX, progressY, progressW, progressH, Colour(50, 50, 50, 200));
    
    // Progress fill
    if (m_totalSeqIds > 0) {
        float progress = (float)m_currentSeqId / (float)m_totalSeqIds;
        float fillW = progressW * progress;
        g_renderer2d->RectFill(progressX, progressY, fillW, progressH, Colour(100, 150, 255, 200));
    }
    
    // Progress text centered on the progress bar
    float progressCenterX = progressX + progressW / 2;
    g_renderer2d->TextCentreSimple(progressCenterX, progressY, Colour(200, 200, 200, 255), 11.0f, m_cachedProgressText);
    
    
    // radar perspective label
    char perspectiveText[64];
    if( m_currentPerspective == -1 )
    {
        sprintf(perspectiveText, "Player Perspective: Spectator View");
    }
    else if( m_currentPerspective >= 0 && m_currentPerspective < 6 && m_players[m_currentPerspective].isActive )
    {
        sprintf(perspectiveText, "Player Perspective: %s", m_players[m_currentPerspective].playerName);
    }
    else
    {
        sprintf(perspectiveText, "Player Perspective: Unknown");
    }
    g_renderer2d->TextSimple(m_x + 95, m_y + 110, Colour(180, 180, 180, 255), 10.0f, perspectiveText);
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
        
        RefreshPlayerColors();
        
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
        
        // Update health toggle button caption (only when needed)
        EclButton *healthToggleBtn = GetButton("HealthToggle");
        if (healthToggleBtn) {
            const char* expectedCaption = m_healthBarsEnabled ? "HEALTH" : "HEALTH";
            if (strcmp(healthToggleBtn->m_caption, expectedCaption) != 0) {
                healthToggleBtn->SetCaption(expectedCaption, false);
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
        
        SeekBar *seekBar = (SeekBar*)GetButton("SeekBar");
        if (seekBar) {
            seekBar->Update(); // Call Update to handle mouse interaction
            if (!seekBar->m_dragging && !seekBar->IsSeeking()) {
                // Update seek position when not dragging and not currently seeking
                if (m_totalSeqIds > 0) {
                    float progress = (float)m_currentSeqId / (float)m_totalSeqIds;
                    seekBar->SetProgress(progress);
                }
            }
        }
    }
    
    FadingWindow::Update();
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

void PlaybackControlWindow::ToggleHealthBars()
{
    m_healthBarsEnabled = !m_healthBarsEnabled;
    g_healthBarsEnabled = m_healthBarsEnabled;
    
    // Update health toggle button caption
    EclButton *healthToggleBtn = GetButton("HealthToggle");
    if (healthToggleBtn) {
        const char* expectedCaption = m_healthBarsEnabled ? "HEALTH*" : "HEALTH";
        healthToggleBtn->SetCaption(expectedCaption, false);
    }
}

void PlaybackControlWindow::ToggleHideUI()
{
    g_app->ToggleHideUI();
}

void PlaybackControlWindow::UpdateProgress(int currentSeq, int totalSeq)
{
    m_currentSeqId = currentSeq;
    m_totalSeqIds = totalSeq;
}

void PlaybackControlWindow::SeekToPosition(float position)
{
    if (!g_app->GetServer() || !g_app->GetServer()->IsRecordingPlaybackMode()) return;
    
    // Clamp position to valid range
    position = max(0.0f, min(1.0f, position));
    
    // Calculate target sequence ID
    int targetSeqId = (int)(position * m_totalSeqIds);
    targetSeqId = max(0, min(targetSeqId, m_totalSeqIds));
    
    int currentSeqId = g_app->GetServer()->m_recordingCurrentSeqId;
    
    // If seeking backwards or to current position, we need to restart the recording
    // since we can't go backwards in time
    if (targetSeqId <= currentSeqId + 10) { // Small tolerance for "current" position
        if (targetSeqId < currentSeqId - 10) { // Only restart if significantly backwards
            // TODO: Implement recording restart + fast-forward
            // For now, just show a message
            MessageDialog *msg = new MessageDialog("SEEK NOT SUPPORTED", 
                                                  "Rewinding is coming soon!", 
                                                  false, "dialog_seek_backwards", true);
            EclRegisterWindow(msg);
            return;
        }
        else {
            // Close enough to current position, don't seek
            return;
        }
    }
    
    // Seeking forward - use fast-forward mechanism
    if (targetSeqId > currentSeqId) {
        // Mark seek bar as seeking
        SeekBar *seekBar = (SeekBar*)GetButton("SeekBar");
        if (seekBar) {
            seekBar->SetSeeking(true);
        }
        
        // Enable fast-forward to target position at maximum speed for fastest seeking
        g_app->GetServer()->EnableFastForward(targetSeqId, 1000.0f);
        
        // Show connecting window for progress feedback
        if (!EclGetWindow("Connection Status")) {
            ConnectingWindow *connectingWindow = new ConnectingWindow();
            connectingWindow->m_popupLobbyAtEnd = false; // Don't popup lobby when done
            connectingWindow->SetFastForwardMode(true, targetSeqId, true); // true = isSeekMode
            EclRegisterWindow(connectingWindow);
        }

#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Seeking from sequence %d to %d (%.1f%% to %.1f%%)\n", 
                   currentSeqId, targetSeqId, 
                   (float)currentSeqId / m_totalSeqIds * 100.0f,
                   (float)targetSeqId / m_totalSeqIds * 100.0f);
#endif
    }
}

bool PlaybackControlWindow::ShouldRender()
{
    // dont render if UI is hidden
    if( g_hideUI ) return false;
    
    // Show during recording playback - both lobby and game phases
    // This allows seeking and speed control even during lobby phase of recordings
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
// Health Toggle Button

void HealthToggleButton::MouseUp()
{
    PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
    if (parent) {
        parent->ToggleHealthBars();
    }
}

void HealthToggleButton::Render(int realX, int realY, bool highlighted, bool clicked)
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
    
    g_renderer2d->RectFill(realX, realY + m_h/2 - 3, m_w, 6, trackCol);
    
    // Slider thumb
    float thumbX = realX + (m_value * (m_w - 10));
    g_renderer2d->RectFill(thumbX, realY, 10, m_h, thumbCol);

    g_renderer2d->TextSimple(realX, realY - 12, Colour(180, 180, 180, 255), 8.0f, "1x");
    g_renderer2d->TextSimple(realX + m_w - 25, realY - 12, Colour(180, 180, 180, 255), 10.0f, "100x");
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
    
    float normalizedValue = m_value;
    return 1.0f + (99.0f * normalizedValue * normalizedValue);
}

void SpeedSlider::SetValueFromSpeed(float speed)
{
    if (speed <= 1.0f) {
        m_value = 0.0f;
    } else {
        float normalizedSpeed = (speed - 1.0f) / 99.0f;
        m_value = sqrt(normalizedSpeed);
        m_value = max(0.0f, min(1.0f, m_value));
    }
}

// ============================================================================
// Seek Bar

SeekBar::SeekBar()
:   m_value(0.0f),
    m_dragging(false),
    m_seeking(false)
{
}

void SeekBar::MouseDown()
{
    m_dragging = true;
    
    // Immediately update the value when clicked (adjusted for thinner thumb)
    float relativeX = g_inputManager->m_mouseX - (m_x + m_parent->m_x);
    m_value = relativeX / (float)m_w;
    m_value = max(0.0f, min(1.0f, m_value)); // Clamp to 0-1
}

void SeekBar::MouseUp()
{
    if (m_dragging) {
        m_dragging = false;
        
        // Trigger seeking when mouse is released
        PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
        if (parent) {
            parent->SeekToPosition(m_value);
        }
    }
}

void SeekBar::Render(int realX, int realY, bool highlighted, bool clicked)
{
    Colour trackCol = Colour(60, 60, 60, 255);
    Colour thumbCol;
    
    if (m_seeking) {
        // Different color when seeking
        thumbCol = Colour(255, 150, 0, 255); // Orange for seeking
    } else if (highlighted || m_dragging) {
        thumbCol = Colour(150, 255, 150, 255); // Green when highlighted
    } else {
        thumbCol = Colour(120, 200, 120, 255); // Normal green
    }
    
    g_renderer2d->RectFill(realX, realY + m_h/2 - 3, m_w, 6, trackCol);
    
    // Seek thumb (thinner for precise clicking)
    float thumbX = realX + (m_value * (m_w - 6));
    g_renderer2d->RectFill(thumbX, realY, 6, m_h, thumbCol);
}

void SeekBar::Update()
{
    if (m_dragging) {
        // Update value based on mouse position (for thinner thumb)
        float relativeX = g_inputManager->m_mouseX - (m_x + m_parent->m_x);
        m_value = relativeX / (float)m_w;
        m_value = max(0.0f, min(1.0f, m_value)); // Clamp to 0-1
    }
    
    // Check for mouse release to stop dragging
    if (m_dragging && !g_inputManager->m_lmb) {
        MouseUp(); // This will trigger the seek
    }
    
    // Check if seeking is complete
    if (m_seeking) {
        // Check if fast-forward mode is disabled (meaning seek completed)
        if (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode()) {
            if (!g_app->GetServer()->IsRecordingFastForwardMode()) {
                m_seeking = false;
            }
        }
    }
}

void SeekBar::SetProgress(float progress)
{
    m_value = max(0.0f, min(1.0f, progress));
}

float SeekBar::GetSeekPosition()
{
    return m_value;
}

// ============================================================================
// Player Perspective Button

PlayerPerspectiveButton::PlayerPerspectiveButton()
:   m_playerIndex(-1),
    m_teamId(-1),
    m_teamColour(Colour(150, 150, 150, 255)),
    m_isSelected(false)
{
}

void PlayerPerspectiveButton::SetPlayer( int playerIndex, int teamId, Colour teamColour )
{
    m_playerIndex = playerIndex;
    m_teamId = teamId;
    m_teamColour = teamColour;
}

void PlayerPerspectiveButton::SetSelected( bool selected )
{
    m_isSelected = selected;
}

void PlayerPerspectiveButton::MouseUp()
{
    PlaybackControlWindow *parent = (PlaybackControlWindow*)m_parent;
    if( parent )
    {
        if( m_playerIndex == -1 )
        {
            parent->SetSpectatorPerspective();
        }
        else
        {
            parent->SetPlayerPerspective(m_playerIndex);
        }
    }
}

void PlayerPerspectiveButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    Colour buttonCol = m_teamColour;
    
    if( m_isSelected )
    {
        // selected button is brighter
        buttonCol.m_a = 255;
        g_renderer2d->RectFill(realX, realY, m_w, m_h, buttonCol);
    }
    else if( highlighted || clicked )
    {
        // highlighted button
        buttonCol.m_a = 200;
        g_renderer2d->RectFill(realX, realY, m_w, m_h, buttonCol);
    }
    else
    {
        // normal button
        buttonCol.m_a = 150;
        g_renderer2d->RectFill(realX, realY, m_w, m_h, buttonCol);
    }
    
    // button text
    Colour textCol = m_isSelected ? Colour(255, 255, 255, 255) : Colour(220, 220, 220, 255);
    g_renderer2d->TextCentreSimple(realX + m_w/2, realY + m_h/2 - 6, textCol, 9.0f, m_caption);
}

// ============================================================================
// Hide UI Button

void HideUIButton::MouseUp()
{
    // show confirmation dialog instead of directly hiding UI
    HideUIConfirmDialog *dialog = new HideUIConfirmDialog();
    EclRegisterWindow(dialog);
}

void HideUIButton::Render(int realX, int realY, bool highlighted, bool clicked)
{
    InterfaceButton::Render(realX, realY, highlighted, clicked);
}

#endif