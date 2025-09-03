#ifndef _included_recordingselection_h
#define _included_recordingselection_h

#include "interface/components/core.h"

class RecordingFileDialog;

// ============================================================================
// Recording Selection Window
// Shows options for starting recording playback from lobby or game start

class RecordingSelectionWindow : public InterfaceWindow
{
public:
    char m_recordingFilename[512];
    
public:
    RecordingSelectionWindow();
    RecordingSelectionWindow(const char *recordingFilename);

    void Create();
    void Render( bool _hasFocus );
};

// ============================================================================
// Buttons for recording playback selection

class PlayFromLobbyButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

class PlayFromGameStartButton : public InterfaceButton
{
public:
    void MouseUp();
    void Render( int realX, int realY, bool highlighted, bool clicked );
};

#endif 