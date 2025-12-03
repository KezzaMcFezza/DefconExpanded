#ifndef _included_worldrenderer_h
#define _included_worldrenderer_h

#include "world/world.h"

class AnimatedIcon;

#define    PREFS_INTERFACE_TOOLTIPS                   "InterfaceTooltips"
#define    PREFS_INTERFACE_POPUPSCALE                 "InterfacePopupScale"
#define    PREFS_INTERFACE_SIDESCROLL                 "InterfaceSideScrolling"
#define    PREFS_INTERFACE_CAMDRAGGING                "InterfaceCameraDragging"
#define    PREFS_INTERFACE_PANICKEY                   "InterfacePanicKey"
#define    PREFS_INTERFACE_STYLE                      "InterfaceStyle"
#define    PREFS_INTERFACE_KEYBOARDLAYOUT             "InterfaceKeyboard"
#define    PREFS_INTERFACE_ZOOM_SPEED                 "InterfaceZoomSpeed"

#define    STYLE_POPUP_BACKGROUND                     "PopupBackground"
#define    STYLE_POPUP_TITLEBAR                       "PopupTitleBar"
#define    STYLE_POPUP_BORDER                         "PopupBorder"
#define    STYLE_POPUP_HIGHLIGHT                      "PopupHighlight"
#define    STYLE_POPUP_SELECTION                      "PopupSelection"
#define    FONTSTYLE_POPUP_TITLE                      "FontPopupTitle"
#define    FONTSTYLE_POPUP                            "FontPopup"

class WorldRenderer
{
protected:

public:
    WorldRenderer();
    ~WorldRenderer();

    void    Init();
    void    Reset();

    DArray  <AnimatedIcon *>    m_animations;
    DArray  <AnimatedIcon *>   &GetAnimations() { return m_animations; }

    bool m_renderEverything;
    bool CanRenderEverything() { return m_renderEverything; }
    void SetRenderEverything( bool _renderEverything );

    enum
    {
        AnimationTypeActionMarker,
        AnimationTypeSonarPing,
        AnimationTypeAttackMarker,
        AnimationTypeNukePointer,
        AnimationTypeNukeMarker,
        NumAnimations
    };
    
    int     CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude );
};

#endif