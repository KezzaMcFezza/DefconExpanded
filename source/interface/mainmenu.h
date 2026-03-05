#ifndef _included_mainmenu_h
#define _included_mainmenu_h

#include "lib/eclipse/components/core.h"


// ============================================================================


class MainMenu : public InterfaceWindow
{
public:
    MainMenu();

    void Create();
};


// ============================================================================

class OptionsMenuWindow : public InterfaceWindow
{
public:
    OptionsMenuWindow();

    void Create();
};


// ============================================================================

class GraphicsOptionsWindow : public InterfaceWindow
{
public:
#ifndef TARGET_EMSCRIPTEN
    float   m_coastlineThickness;
    float   m_borderThickness;
    float   m_unitTrailThickness;
    float   m_whiteboardThickness;
#endif  
    int     m_coastlines;
    int     m_borders;
    int     m_cityNames;
    int     m_countryNames;
    int     m_water;
    int     m_lowResWorld;
    int     m_trails;
    int     m_outlines;
    int     m_lobbyEffects;
    int     m_cursorCoords;
    int     m_coordSnap;
    int     m_nodes;
    
public:
    GraphicsOptionsWindow();

    void Create();

    void Render( bool _hasFocus );
};


// ============================================================================

class NetworkOptionsWindow : public InterfaceWindow
{
public:
    char    m_clientPort[32];
    char    m_serverPort[32];
    char    m_metaserverPort[32];

    int     m_portForwarding;
    int     m_trackSync;

public:
    NetworkOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
};


// ============================================================================

class RecordingOptionsWindow : public InterfaceWindow
{
public:
    char    m_saveLocation[512];
    int     m_enableChecksum;
    int     m_enableCompression;
    int     m_namingFormat;
    int     m_autoSaveBehaviour;
    int     m_endBehaviour;

public:
    RecordingOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
};


// ============================================================================

class GlobeOptionsWindow : public InterfaceWindow
{
public:
    float   m_globeCoastThickness;
    float   m_globeBorderThickness;
    int     m_globeGridlines;
    float   m_globeGridlineThickness;
    int     m_globeGridlineSpacing;
    int     m_globeFog;
    int     m_globeFogDistance;
    int     m_globeStarfield;
    float   m_globeStarSize;
    int     m_globeStarDensity;
    float   m_globeLandUnitSize;
    float   m_globeNavalUnitSize;
    float   m_globeWhiteboardThickness;
    float   m_globeUnitTrailThickness;
    
public:
    GlobeOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
};


// ============================================================================

class ControlOptionsWindow : public InterfaceWindow
{
public:
    int     m_sideScrolling;
    int     m_zoomSlower;
    int     m_camDragging;
    int     m_tooltips;
    float   m_popupScale;
    int     m_panicKey;
    int     m_keyboardLayout;
    int     m_language;
    int     m_unitInfoToolbar;

public:
    ControlOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
};


void ConfirmExit( const char *_parentWindowName );

#endif


