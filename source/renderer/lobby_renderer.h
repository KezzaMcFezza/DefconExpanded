#ifndef _included_lobbyrenderer_h
#define _included_lobbyrenderer_h

class LobbyOverlay;
class MapRenderer;

#define PREFS_GRAPHICS_LOBBYEFFECTS         "RenderLobbyEffects"


class LobbyRenderer
{
protected:
    int     m_maxOverlays;
    float   m_overlayTimer;
    float   m_finishTimer;
    
    int     m_currentOverlay;
    int     m_previousOverlay;
    int     m_maxStrings;
            
    int     m_currentCity;

    char   *m_lastLanguage;
    
protected:
    void Render3DScene();     
    void RenderBorder();
    void RenderTestHours();
    void RenderVersionInfo();
    void RenderAuthStatus();
    void RenderMotd();
    void RenderDemoLimits();
    void RenderOverlay();
    void RenderCityDetails();
    void RenderTitle();
    void RenderNetworkIdentity();

public:
    LobbyRenderer();
    ~LobbyRenderer();
    
    void Initialise();
    void InitialiseLanguage();

    void Render();

};



#endif
