#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "lib/render2d/renderer.h"
#include "lib/language_table.h"
#include "lib/hi_res_time.h"
#include "lib/netlib/net_lib.h"
#include "lib/render/styletable.h"
#include "lib/gucci/window_manager.h"  // NEW: For EclGetWindow function

#include "interface/recording_selection.h"
#include "interface/connecting_window.h"
#include "interface/components/message_dialog.h"

#include "app/app.h"
#include "app/globals.h"

#include "network/Server.h"
#include "network/ClientToServer.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

// ============================================================================
// Button implementations

void PlayFromLobbyButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    // Render button background and borders (copied from InterfaceButton::Render but without text)
    char *styleName             = STYLE_BUTTON_BACKGROUND;
    if( highlighted ) styleName = STYLE_BUTTON_HIGHLIGHTED;
    if( clicked ) styleName     = STYLE_BUTTON_CLICKED;

    Colour primaryCol   = g_styleTable->GetPrimaryColour(styleName);
    Colour secondaryCol = g_styleTable->GetSecondaryColour(styleName);
    
    Colour borderPrimary    = g_styleTable->GetPrimaryColour(STYLE_BUTTON_BORDER);
    Colour borderSeconary   = g_styleTable->GetSecondaryColour(STYLE_BUTTON_BORDER);
    
    bool colourAlignment    = g_styleTable->GetStyle(styleName)->m_horizontal;

    g_renderer->RectFill    ( realX, realY, m_w, m_h, primaryCol, secondaryCol, colourAlignment );

    g_renderer->Line        ( realX, realY, realX+m_w, realY, borderPrimary );
    g_renderer->Line        ( realX, realY, realX, realY+m_h, borderPrimary );
    g_renderer->Line        ( realX, realY+m_h, realX+m_w, realY+m_h, borderSeconary );
    g_renderer->Line        ( realX+m_w, realY, realX+m_w, realY+m_h, borderSeconary );
    
    // Render our custom larger text
    Colour fontColour = g_styleTable->GetPrimaryColour(FONTSTYLE_BUTTON);
    float fontSize = 16.0f;  // Larger font size
    
    char caption[256];
    if( m_captionIsLanguagePhrase )
    {
        snprintf( caption, sizeof(caption), "%s", LANGUAGEPHRASE(m_caption) );
    }
    else
    {
        snprintf( caption, sizeof(caption), "%s", m_caption );
    }
    caption[sizeof(caption) - 1] = '\0';
    
    // Center the text in the button
    g_renderer->TextCentre( realX + m_w/2, 
                            realY + m_h/2 - fontSize/2, 
                            fontColour, 
                            fontSize, 
                            caption);
}

void PlayFromLobbyButton::MouseUp()
{
    RecordingSelectionWindow *parent = (RecordingSelectionWindow *)m_parent;
    
    if (strcmp(parent->m_recordingFilename, "NO FILE SPECIFIED") == 0)
    {
        MessageDialog *msg = new MessageDialog("NO FILE SPECIFIED",
                                              "Please select a .dcrec file using the browse button:\n\n", 
                                              false, "dialog_no_file_specified", true);
        EclRegisterWindow(msg);
        return;
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("User selected: Play from Lobby\n");
#endif
    
    if( g_app->GetServer() )
    {
        // Game in progress. Dump the history
        g_app->GetServer()->DebugDumpHistory();
        return;
    }

    g_app->ShutdownCurrentGame();

    //
    // ClientToServer listener opened ok?

    if( !g_app->GetClientToServer()->m_listener )
    {
        char msgtext[2048];
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support") );
        msgtext[sizeof(msgtext) - 1] = '\0';  // Ensure null termination

        MessageDialog *msg = new MessageDialog( "NETWORK ERROR",
                                                msgtext, false, 
                                                "dialog_client_failedtitle", true ); 
        EclRegisterWindow( msg );      
        return;
    }

    char ourIp[256];
    GetLocalHostIP( ourIp, sizeof(ourIp) );

    bool success = g_app->InitServer();

    if( success )
    {
        // Copy filename to avoid corruption when window is destroyed
        char recordingFilename[512];
        strncpy( recordingFilename, parent->m_recordingFilename, sizeof(recordingFilename) - 1 );
        recordingFilename[sizeof(recordingFilename) - 1] = '\0';
        
        // NEW: Close main menu and recording selection window BEFORE doing anything else
        EclRemoveWindow( "Main Menu" );
        EclRemoveWindow( parent->m_name );
        
#if RECORDING_PARSING
        
        // Start recording server using the copied filename
        bool recordingLoaded = g_app->GetServer()->StartRecordingPlaybackServer( recordingFilename );

        if( recordingLoaded )
        {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
            AppDebugOut("Started recording playback from lobby\n");
#endif
            int ourPort = g_app->GetServer()->GetLocalPort();
            g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );

            g_app->InitWorld();
            
            ConnectingWindow *connectingWindow = new ConnectingWindow();
            connectingWindow->m_popupLobbyAtEnd = true;
            EclRegisterWindow( connectingWindow );
        }
        else
        {
            MessageDialog *msg = new MessageDialog( "RECORDING ERROR",
                                                    "Failed to load recording file. Please check that the file exists.", 
                                                    false, "dialog_recording_failed", true );
            EclRegisterWindow( msg );
        }
#else
        MessageDialog *msg = new MessageDialog( "RECORDING NOT SUPPORTED",
                                                "Recording playback is not available in this build.", 
                                                false, "dialog_recording_not_supported", true );
        EclRegisterWindow( msg );
#endif
    }
}

void PlayFromGameStartButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    // Render button background and borders (copied from InterfaceButton::Render but without text)
    char *styleName             = STYLE_BUTTON_BACKGROUND;
    if( highlighted ) styleName = STYLE_BUTTON_HIGHLIGHTED;
    if( clicked ) styleName     = STYLE_BUTTON_CLICKED;

    Colour primaryCol   = g_styleTable->GetPrimaryColour(styleName);
    Colour secondaryCol = g_styleTable->GetSecondaryColour(styleName);
    
    Colour borderPrimary    = g_styleTable->GetPrimaryColour(STYLE_BUTTON_BORDER);
    Colour borderSeconary   = g_styleTable->GetSecondaryColour(STYLE_BUTTON_BORDER);
    
    bool colourAlignment    = g_styleTable->GetStyle(styleName)->m_horizontal;

    g_renderer->RectFill    ( realX, realY, m_w, m_h, primaryCol, secondaryCol, colourAlignment );

    g_renderer->Line        ( realX, realY, realX+m_w, realY, borderPrimary );
    g_renderer->Line        ( realX, realY, realX, realY+m_h, borderPrimary );
    g_renderer->Line        ( realX, realY+m_h, realX+m_w, realY+m_h, borderSeconary );
    g_renderer->Line        ( realX+m_w, realY, realX+m_w, realY+m_h, borderSeconary );
    
    // Render our custom larger text
    Colour fontColour = g_styleTable->GetPrimaryColour(FONTSTYLE_BUTTON);
    float fontSize = 16.0f;  // Larger font size
    
    char caption[256];
    if( m_captionIsLanguagePhrase )
    {
        snprintf( caption, sizeof(caption), "%s", LANGUAGEPHRASE(m_caption) );
    }
    else
    {
        snprintf( caption, sizeof(caption), "%s", m_caption );
    }
    caption[sizeof(caption) - 1] = '\0';
    
    // Center the text in the button
    g_renderer->TextCentre( realX + m_w/2, 
                            realY + m_h/2 - fontSize/2, 
                            fontColour, 
                            fontSize, 
                            caption);
}

void PlayFromGameStartButton::MouseUp()
{
    RecordingSelectionWindow *parent = (RecordingSelectionWindow *)m_parent;
    
    // NEW: Check if a file was specified
    if (strcmp(parent->m_recordingFilename, "NO FILE SPECIFIED") == 0)
    {
        MessageDialog *msg = new MessageDialog("NO FILE SPECIFIED",
                                              "Please select a .dcrec file using the browse button:\n\n", 
                                              false, "dialog_no_file_specified", true);
        EclRegisterWindow(msg);
        return;
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("User selected: Play from Game Start\n");
#endif
    
    if( g_app->GetServer() )
    {
        // Game in progress. Dump the history
        g_app->GetServer()->DebugDumpHistory();
        return;
    }

    g_app->ShutdownCurrentGame();

    //
    // ClientToServer listener opened ok?

    if( !g_app->GetClientToServer()->m_listener )
    {
        char msgtext[2048];
#if defined(RETAIL_BRANDING_UK)
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language") );
#elif defined(TARGET_OS_MACOSX)
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_macosx") );
#else
        snprintf( msgtext, sizeof(msgtext), "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support") );
#endif
        msgtext[sizeof(msgtext) - 1] = '\0';  // Ensure null termination

        MessageDialog *msg = new MessageDialog( "NETWORK ERROR",
                                                msgtext, false, 
                                                "dialog_client_failedtitle", true ); 
        EclRegisterWindow( msg );      
        return;
    }

    char ourIp[256];
    GetLocalHostIP( ourIp, sizeof(ourIp) );

    bool success = g_app->InitServer();

    if( success )
    {
        // Copy filename to avoid corruption when window is destroyed
        char recordingFilename[512];
        strncpy( recordingFilename, parent->m_recordingFilename, sizeof(recordingFilename) - 1 );
        recordingFilename[sizeof(recordingFilename) - 1] = '\0';
        
        // NEW: Close main menu and recording selection window BEFORE doing anything else
        EclRemoveWindow( "Main Menu" );
        EclRemoveWindow( parent->m_name );
        
#if RECORDING_PARSING
        // Initialize world FIRST - required for recording system
        g_app->InitWorld();
        
        // Start recording server using the copied filename
        bool recordingLoaded = g_app->GetServer()->StartRecordingPlaybackServer( recordingFilename );

        if( recordingLoaded )
        {
            // NEW: Enable fast-forward mode to skip to game start
            int gameStartSeqId = g_app->GetServer()->ExtractGameStartFromHeader();
            if( gameStartSeqId > 0 )
            {
                g_app->GetServer()->EnableFastForward( gameStartSeqId, 500.0f );
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
                AppDebugOut("Enabled fast-forward to sequence %d for 'Play from Game Start'\n", gameStartSeqId);
#endif
            }
            
            int ourPort = g_app->GetServer()->GetLocalPort();
            g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );
            
            ConnectingWindow *connectingWindow = new ConnectingWindow();
            connectingWindow->m_popupLobbyAtEnd = true;  // Skip lobby and go straight to game
            connectingWindow->SetFastForwardMode( true, gameStartSeqId );  // NEW: Enable fast-forward display
            EclRegisterWindow( connectingWindow );
            
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
            AppDebugOut("Started recording playback from game start\n");
#endif
        }
        else
        {
            MessageDialog *msg = new MessageDialog( "RECORDING ERROR",
                                                    "Failed to load recording file. Please check that the file exists.", 
                                                    false, "dialog_recording_failed", true );
            EclRegisterWindow( msg );
        }
#else
        MessageDialog *msg = new MessageDialog( "RECORDING NOT SUPPORTED",
                                                "Recording playback is not available in this build.", 
                                                false, "dialog_recording_not_supported", true );
        EclRegisterWindow( msg );
#endif
    }
}

#if !(defined(TARGET_EMSCRIPTEN) && (REPLAY_VIEWER))

class BrowseRecordingButton : public InterfaceButton {
public:
    void MouseUp() override {
        RecordingSelectionWindow *parent = (RecordingSelectionWindow *)m_parent;
#ifdef _WIN32
    RECT originalClipRect;
    bool wasClipped = GetClipCursor(&originalClipRect);
    
    int cursorShowCount = ShowCursor(TRUE) - 1; 
    ShowCursor(FALSE); 
    ReleaseCapture();
    ClipCursor(NULL);
    
    while (ShowCursor(TRUE) < 0); 
    
    // open file dialog
    char filename[MAX_PATH] = "";
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow(); 
    ofn.lpstrFilter = "Defcon Recordings (*.dcrec)\0*.dcrec\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Select a Defcon Recording";
    
    BOOL result = GetOpenFileName(&ofn);
    
    while (ShowCursor(FALSE) >= cursorShowCount); 

    if (wasClipped) {
        ClipCursor(&originalClipRect);
    }

    if (result) {
        strncpy(parent->m_recordingFilename, filename, sizeof(parent->m_recordingFilename) - 1);
        parent->m_recordingFilename[sizeof(parent->m_recordingFilename) - 1] = '\0';
        parent->SetTitle("Recording Playback (Selected)");
    }
#endif
    }
};
#endif

// ============================================================================
// Recording Selection Window

RecordingSelectionWindow::RecordingSelectionWindow()
:   InterfaceWindow("Recording Playback", "Recording Playback Options", false)
{
    // NEW: Check for command line replay filename first
    if( g_app && g_app->HasReplayFilename() )
    {
        strncpy(m_recordingFilename, g_app->GetReplayFilename(), sizeof(m_recordingFilename) - 1);
        m_recordingFilename[sizeof(m_recordingFilename) - 1] = '\0';
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingSelectionWindow: Using command line filename '%s'\n", m_recordingFilename);
#endif
        
        // Update window title to show it's from command line
        SetTitle("Recording Playback");
    }
    else
    {
        // NEW: No command line file - show error message instead of default
        strcpy(m_recordingFilename, "NO FILE SPECIFIED");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingSelectionWindow: No command line filename provided\n");
#endif
        
        // Update title to show error state
        SetTitle("Recording Playback (No File)");
    }
    
    SetSize(500, 360);
    Centralise();
}

RecordingSelectionWindow::RecordingSelectionWindow(const char *recordingFilename)
:   InterfaceWindow("Recording Playback", "Recording Playback Options", false)
{
    if(recordingFilename)
    {
        strncpy(m_recordingFilename, recordingFilename, sizeof(m_recordingFilename) - 1);
        m_recordingFilename[sizeof(m_recordingFilename) - 1] = '\0';
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingSelectionWindow: Using provided filename '%s'\n", m_recordingFilename);
#endif
        
        // Update window title to show it's explicitly provided
        SetTitle("Recording Playback (Provided)");
    }
    else if( g_app && g_app->HasReplayFilename() )
    {
        // NEW: Check for command line replay filename if no explicit filename provided
        strncpy(m_recordingFilename, g_app->GetReplayFilename(), sizeof(m_recordingFilename) - 1);
        m_recordingFilename[sizeof(m_recordingFilename) - 1] = '\0';
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingSelectionWindow: Using command line filename '%s'\n", m_recordingFilename);
#endif
        
        // Update window title to show it's from command line
        SetTitle("Recording Playback");
    }
    else
    {
        // NEW: No command line file - show error message instead of default
        strcpy(m_recordingFilename, "NO FILE SPECIFIED");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingSelectionWindow: No command line filename provided\n");
#endif
        
        // Update title to show error state
        SetTitle("Recording Playback (No File)");
    }
    
    SetSize(500, 360);
    Centralise();
}

void RecordingSelectionWindow::Create()
{
    InterfaceWindow::Create();

    // Title area box
    InvertedBox *titleBox = new InvertedBox();
    titleBox->SetProperties("titlebox", 20, 40, m_w-40, 60, " ", " ", false, false);
    RegisterButton(titleBox);

    // Main selection buttons
    PlayFromLobbyButton *lobbyBtn = new PlayFromLobbyButton();
    lobbyBtn->SetProperties("PlayFromLobby", 50, 140, 170, 40, "START FROM LOBBY", 
                           "Watch the entire recording from the lobby phase", false, true);
    RegisterButton(lobbyBtn);

    PlayFromGameStartButton *gameBtn = new PlayFromGameStartButton();
    gameBtn->SetProperties("PlayFromGameStart", 280, 140, 170, 40, "START FROM GAME", 
                          "Skip lobby and start from when the game begins", false, true);
    RegisterButton(gameBtn);

#if !(defined(TARGET_EMSCRIPTEN) && (REPLAY_VIEWER))
    BrowseRecordingButton *browseBtn = new BrowseRecordingButton();
    browseBtn->SetProperties("BrowseRecording", 50, 240, 100, 20, "Browse...", "Select a .dcrec file from your computer", false, true);
    RegisterButton(browseBtn);

    // Close button - only show in normal mode, not in replay viewer mode
    CloseButton *close = new CloseButton();
    close->SetProperties("Close", m_w-120, m_h-40, 80, 20, "Cancel", "Close this window", false, true);
    RegisterButton(close);
#endif
}

void RecordingSelectionWindow::Render(bool _hasFocus)
{
    InterfaceWindow::Render(_hasFocus);

    // Title text
    g_renderer->TextCentreSimple(m_x + m_w/2, m_y + 55, White, 18.0f, "Where would you like to start watching?");
    g_renderer->TextCentreSimple(m_x + m_w/2, m_y + 75, Colour(200,200,200,255), 14.0f, "Choose your preferred starting point for playback");

    // Current file display with source indicator
    g_renderer->TextSimple(m_x + 55, m_y + 195, Colour(180,180,180,255), 12.0f, "Current file:");
    
    // NEW: Use different color for error state
    Colour filenameColor = (strcmp(m_recordingFilename, "NO FILE SPECIFIED") == 0) ? 
                           Colour(255,100,100,255) : Colour(255,255,150,255);
    g_renderer->TextSimple(m_x + 55, m_y + 210, filenameColor, 12.0f, m_recordingFilename);

    // Help text
    g_renderer->TextCentreSimple(m_x + m_w/2, m_y + m_h-70, Colour(150,150,150,255), 11.0f, 
                                "Lobby: Watch the pre-game setup");
    g_renderer->TextCentreSimple(m_x + m_w/2, m_y + m_h-55, Colour(150,150,150,255), 11.0f, 
                                "Game: Skip directly to when the game begins");
} 