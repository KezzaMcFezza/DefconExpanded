#ifndef _included_worldrenderer_h
#define _included_worldrenderer_h

#include "world/world.h"
#include "world/team.h"

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
    bool m_renderEverything;

    bool    m_showWhiteBoard;			// Used to show the white board(s) without editing the player white board
	bool	m_editWhiteBoard;			// Used to show the white board panel
	bool    m_showPlanning;				// Used to edit the player white board
	bool	m_showAllWhiteBoards;		// Used to toggle between showing only the player whiteboard or all whiteboard in the alliance
	bool    m_drawingPlanning;			// Used to indicate the player is currently drawing
	bool    m_erasingPlanning;			// Used to indicate the player is currently erasing
	double  m_drawingPlanningTime;      // Used to track the time the player has been drawing

    DArray  <AnimatedIcon *>    m_animations;

public:
    WorldRenderer();
    ~WorldRenderer();

    void    Init();
    void    Reset();

    DArray  <AnimatedIcon *>   &GetAnimations() { return m_animations; }

    bool    CanRenderEverything() { return m_renderEverything; }
    void    SetRenderEverything( bool _renderEverything );

    void    LockRadarRenderer();
    void    UnlockRadarRenderer();

    bool    m_showRadar;
    bool    m_radarLocked;
    bool    m_showPopulation;
    bool    m_showOrders;
    bool    m_showNukeUnits;

    bool	GetShowWhiteBoard()     const;
    bool    GetShowPlanning()       const;
    bool	GetEditWhiteBoard()     const;
    bool	GetShowAllWhiteBoards() const;
    bool    GetShowWhiteBoard()     { return m_showWhiteBoard; }
    bool    GetEditWhiteBoard()     { return m_editWhiteBoard; }
    bool    GetDrawingPlanning()    { return m_drawingPlanning; }
    bool    GetErasingPlanning()    { return m_erasingPlanning; }
    double  GetDrawingPlanningTime(){ return m_drawingPlanningTime; }

	void	SetShowWhiteBoard     ( bool showWhiteBoard );
	void	SetEditWhiteBoard     ( bool showPlanning );
	void    SetShowPlanning       ( bool showPlanning );
	void	SetShowAllWhiteBoards ( bool showAllWhiteBoards );
    void    SetDrawingPlanningTime( double drawingPlanningTime );
    void    SetDrawingPlanning    ( bool drawingPlanning );
    void    SetErasingPlanning    ( bool erasingPlanning );
    bool    ShowAllWhiteBoards()  { return m_showAllWhiteBoards; }

    Team*   GetEffectiveWhiteBoardTeam();  // getter for perspective based whiteboard viewing

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