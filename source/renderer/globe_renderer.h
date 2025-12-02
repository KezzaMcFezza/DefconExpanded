#ifndef _included_globerenderer_h
#define _included_globerenderer_h

#include "world/worldobject.h"
#include "world/world.h"
#include "world/nuke.h"

class Image;
class WorldObject;
class AnimatedIcon;

#define    STYLE_GLOBE_COASTLINES                     "GlobeCoastlines"
#define    STYLE_GLOBE_BORDERS                        "GlobeBorders"
#define    STYLE_GLOBE_GRIDLINES                      "GlobeGridlines"

#define    PREFS_GRAPHICS_GLOBE_COASTLINES            "RenderGlobeCoastlines"
#define    PREFS_GRAPHICS_GLOBE_BORDERS               "RenderGlobeBorders"
#define    PREFS_GRAPHICS_GLOBE_GRIDLINES             "RenderGlobeGridlines"

#define    PREFS_GLOBE_SIZE                           "GlobeSize"
#define    PREFS_GLOBE_COAST_THICKNESS                "GlobeCoastThickness"
#define    PREFS_GLOBE_BORDER_THICKNESS               "GlobeBorderThickness"
#define    PREFS_GLOBE_FOG_DISTANCE                   "GlobeFogDistance"
#define    PREFS_GLOBE_STARFIELD                      "GlobeStarfield"
#define    PREFS_GLOBE_STAR_SIZE                      "GlobeStarSize"
#define    PREFS_GLOBE_STAR_DENSITY                   "GlobeStarDensity"
#define    PREFS_GLOBE_LAND_UNIT_SIZE                 "GlobeLandUnitSize"
#define    PREFS_GLOBE_NAVAL_UNIT_SIZE                "GlobeNavalUnitSize"

class GlobeRenderer
{
protected:

    float   m_zoomFactor;
    float   m_middleX;
    float   m_middleY;

    float	m_drawScale;

    bool    m_lockCamControl;
    bool    m_lockCommands;
    bool    m_draggingCamera;

    void DragCamera();
    void UpdateCameraControl();
    void RenderDragIcon();

    void GameCamera();
    void LobbyCamera();

public:
    GlobeRenderer();
    ~GlobeRenderer();

    void    Init();
    void    Reset();
    void    Update();
    void    Render();

    void    AddLineStrip(const DArray<Vector3<float>> &vertices) const;
    void    GlobeGridlines();
    void    GlobeCoastlines();
    void    GlobeBorders();

    void    RenderCities();

    void    GetWindowBounds  ( float *left, float *right, float *top, float *bottom );
    void    GetCameraPosition( float &longitude, float &latitude, float &distance );
    void    SetCameraPosition( float longitude, float latitude, float distance );
    void    IsCameraIdle     (float oldLongitude, float oldLatitude);

    bool m_renderEverything;

    float   m_maxCameraDistance;
    float   m_minCameraDistance;
    float   m_maxZoomFactor;
    float   m_minZoomFactor;
    float   m_cameraLongitude;
    float   m_cameraLatitude;
    float   m_cameraDistance;
    float   m_targetCameraDistance;
    float   m_cameraIdleTime;

    Vector3  <float> m_camUp;
    Vector3  <float> m_camFront;

    void    Render3DUnits();          
    void    Render3DUnitTrails();
    void    Render3DNukeTrajectories();
    void    Render3DNuke();
    void    Render3DGunfire();       
    void    Render3DExplosions();     
    void    Render3DNukeSymbols();
    void    Render3DWorldObjectTargets();
    void    Render3DNukeHighlights();
    void    Render3DAnimations();
    void    Render3DSanta();
    void    Render3DGlobeCulling(bool inLobbyMode = false);
    void    Render3DWhiteBoard();
    void    Render3DPopulationDensity();
    void    Render3DUnitHighlight                         (int objectId);
    void    Render3DActionLine                            (const Vector3<float>& fromPos, const Vector3<float>& toPos, const Colour& col, bool animate);
    float   CalculateUnitElevation                        (WorldObject* wobj);
    float   CalculateBallisticHeight                      (float totalDistanceRadians, float progress);
    Vector3<float> ConvertLongLatTo3DPosition             (float longitude, float latitude);
    Vector3<float> ScreenToGlobePosition                  (int screenX, int screenY);
    Vector3<float> CalculateGreatCirclePosition           (float startLon, float startLat, float endLon, float endLat, float progress);
    Vector3<float> CalculateNuke3DPosition                (Nuke* nuke);
    Vector3<float> CalculateHistoricalNuke3DPosition      (Nuke* nuke, const Vector3<Fixed>& historicalPos);
    Vector3<float> CalculateHistoricalNuke3DPositionByAge (Nuke* nuke, const Vector3<Fixed>& historicalPos, float historicalProgress);
    Vector3<float> CalculateGunfire3DPosition             (GunFire* gunfire);
    static  float ConvertMenuToLandUnitSize               (float menuValue);
    static  float ConvertLandUnitSizeToMenu               (float internalValue);
    static  float ConvertMenuToNavalUnitSize              (float menuValue);
    static  float ConvertNavalUnitSizeToMenu              (float internalValue);
    static  float ConvertMenuToFogDensity                 (float menuValue);
    static  float ConvertFogDensityToMenu                 (float internalValue);
    static  float ConvertMenuToStarSize                   (float menuValue);
    static  float ConvertStarSizeToMenu                   (float internalValue);
    
    // 3D star field functionality
    void    Generate3DStarField    ();
    void    Cleanup3DStarField     ();
    void    Regenerate3DStarField  ();
    void    Render3DStarField      ();
};

#endif