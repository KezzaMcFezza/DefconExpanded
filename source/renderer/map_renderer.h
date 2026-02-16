#ifndef _included_maprenderer_h
#define _included_maprenderer_h

#include "world/worldobject.h"
#include "world/world.h"
#include "world/nuke.h"

class Image;
class WorldObject;
class AnimatedIcon;

#define    CLEARQUEUE_STATEID  255

#define    PREFS_GRAPHICS_COASTLINE_THICKNESS         "RenderCoastlineThickness"
#define    PREFS_GRAPHICS_BORDER_THICKNESS            "RenderBorderThickness"
#define    PREFS_GRAPHICS_COASTLINES                  "RenderCoastlines"
#define    PREFS_GRAPHICS_BORDERS                     "RenderBorders"
#define    PREFS_GRAPHICS_CITYNAMES                   "RenderCityNames"
#define    PREFS_GRAPHICS_COUNTRYNAMES                "RenderCountryNames"
#define    PREFS_GRAPHICS_WATER                       "RenderWater"
#define    PREFS_GRAPHICS_TRAILS                      "RenderObjectTrails"

#define    STYLE_WORLD_COASTLINES                     "WorldCoastlines"
#define    STYLE_WORLD_BORDERS                        "WorldBorders"
#define    STYLE_WORLD_WATER                          "WorldWater"
#define    STYLE_WORLD_LAND                           "WorldLand"

class MapRenderer
{
protected:
    int     m_pixelX;               // Exact pixel positions
    int     m_pixelY;               // of the 'drawable'
    int     m_pixelW;               // part of the window
    int     m_pixelH;

    float   m_totalZoom;

    int     m_currentStateId;
    float   m_highlightTime;

    float   m_selectTime;
    float   m_deselectTime;
    int     m_previousSelectionId;

    float   m_stateRenderTime;
    int     m_stateObjectId;
    bool    m_stateApplyToSelectionSameType;

	float	m_drawScale;
    
    float   m_lastClickTime;
    float   m_lastClickX;
    float   m_lastClickY;
    
    Image   *bmpRadar;    
    Image   *bmpBlur;    
    Image   *bmpWater;
    Image   *bmpExplosion;
    Image   *bmpPopulation;

    float   m_oldMouseX;  // Used for mouse idle time
    float   m_oldMouseY;

    float   m_cameraLongitude;
    float   m_cameraLatitude;
    float   m_speedRatioX;
    float   m_speedRatioY;
    float   m_cameraIdleTime;
    int     m_cameraZoom;
    int     m_camSpeed;

    bool    m_autoCam;
    bool    m_camAtTarget;
    int     m_autoCamCounter;
    float   m_autoCamTimer;

    bool    m_lockCamControl;
    bool    m_lockCommands;
    bool    m_draggingCamera;

    bool    m_isDraggingSelect;
    int     m_dragSelectMode;      // 0=replace, 1=add, 2=remove
    Fixed   m_dragStartLong;
    Fixed   m_dragStartLat;
    Fixed   m_dragEndLong;
    Fixed   m_dragEndLat;

    float   m_tooltipTimer;

	float   m_longitudePlanningOld;
	float   m_latitudePlanningOld;

public:
    
    bool    m_showTeam[MAX_TEAMS];
    bool    m_showAllTeams;
    bool    m_showNodes;
    bool    m_hideMouse;

    int     m_highlightUnit;
    
    float   m_middleX;
    float   m_middleY;
    float   m_zoomFactor;
    int     m_seamIteration;

    Vector3  <float> m_camUp;
    Vector3  <float> m_camFront;

    LList   <const char *>     *m_tooltip;

    float   m_mouseIdleTime;
    
    bool    m_nukeTravelTimeEnabled;
    bool    m_siloRankingsDirty;
    Fixed   m_nukeTravelTargetLongitude;
    Fixed   m_nukeTravelTargetLatitude;
    
    int     m_wrongSiloId;
    float   m_wrongSiloTime;
    bool    m_wrongSiloWasFirst;
    
    LList<int> m_flightTimeCacheIds;
    LList<int> m_siloRankings;    
    LList<Fixed> m_flightTimeCacheTimes;
    LList<Fixed> m_siloLaunchTimes;
    LList<bool> m_siloLaunchCorrect;

public:
    MapRenderer();
	~MapRenderer();

    void    Init();                   // Call me after the window is open
    void    Reset();

    void    Update();
    void    UpdateCameraControl     ( float _mouseX, float _mouseY );
    bool    UpdatePlanning          ( float _mouseX, float _mouseY );
    int     GetNearestObjectToMouse ( float _mouseX, float _mouseY );
    void    HandleObjectAction      ( float _mouseX, float _mouseY, int _underMouseId );
    void    HandleSetWaypoint       ( float _mouseX, float _mouseY );
    void    HandleSelectObject      ( int _underMouseId );
    void    HandleClickStateMenu    ();

    void    Render();
    void    RenderMouse();
    void    RenderNukeSyncTarget();
    void    RenderSyncOverlay();
    Colour  GetLaunchGradientColour(float timeOverdue);
    void    RenderCoastlines();
    void    RenderLowDetailCoastlines();
    void    RenderBorders();
    void    RenderObjects();
    void    RenderRadar();
    void    RenderRadar( bool _allies, bool _outlined );
    void    RenderCities();
    void    RenderCountryControl();
    void    RenderPopulationDensity();
    void    RenderWorldMessages();
    void    RenderNodes();
    void    RenderGunfire();
    void    RenderExplosions();
    void    RenderAnimations();
    void    RenderRadiation();
    void    RenderBlips();
    void    RenderUnitHighlight( int _objectId );
    void    RenderDragSelectionMarquee();
    void    RenderNukeUnits();
	
	void	RenderSanta();

    void    RenderWorldObjectTargets   ( WorldObject *wobj, bool maxRanges=true );
    void    RenderWorldObjectDetails   ( WorldObject *wobj );
    void    GetWorldObjectStatePosition( WorldObject *wobj, int state,
                                         float *screenX, float *screenY, float *screenW, float *screenH );
    void    GetStatePositionSizes     ( float *titleSize, float *textSize, float *gapSize );
	
    void    RenderFriendlyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderFriendlyObjectMenu   ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
	void    RenderEnemyObjectDetails   ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderCityObjectDetails    ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderEmptySpaceDetails    ( float mouseX, float mouseY );
    void    RenderPlacementDetails     ();

    void    RenderTooltip              ( char *_tooltip );          // old, remove me eventually
    void    RenderTooltip              ( float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep, float alpha );
    void    RenderActionLine           ( float fromLong, float fromLat, float toLong, float toLat,
                                         Colour col, float width, bool animate=true );

    void    GetWindowBounds             ( float *left, float *right, float *top, float *bottom );               // Returns angles
    void    ConvertPixelsToAngle        ( float pixelX, float pixelY, float *longitude, float *latitude,  bool absoluteLongitude = false );
    void    ConvertAngleToPixels        ( float longitude, float latitude, float *pixelX, float *pixelY );

    float   GetZoomFactor();
    float   GetDrawScale();

    void    GetPosition( float &_middleX, float &_middleY );

    int     GetLongitudeMod();
    
    Fixed   GetCachedFlightTime(int objectId);
    Fixed   GetLaunchCountdown(int siloId);
    
    void    SetCachedFlightTime(int objectId, Fixed flightTime);
    void    RecordSiloLaunch(int siloId);
    void    RankSilosForSync();

    int     GetSiloRank(int siloId);
    const char* GetNukeDirection(int siloId);

    void    CenterViewport( float longitude, float latitude, int zoom = 20, int camSpeed = 200 );
    void    CenterViewport( int objectId, int zoom = 20, int camSpeed = 200 );
    void    CameraCut     ( float longitude, float latitude, int zoom = 10 );

    void    FindScreenEdge( Vector3<float> const &_line, float *_posX, float *_posY );

    void    UpdateMouseIdleTime();
    bool    IsMouseInMapRenderer();

    void    UpdateSelectionAnimation( int id );

    void    AutoCam();
    void    ToggleAutoCam();
    bool    GetAutoCam();

    void    DragCamera();
    void    MoveCam();
    bool    IsDraggingCamera() const { return m_draggingCamera; }

    bool    IsOnScreen( float _longitude, float _latitude, float _expandScreen = 2.0f );
    void    RenderWhiteBoard();
};


#endif