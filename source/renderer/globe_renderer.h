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

    struct Globe3DCamera {
        float m_cameraDistance;     // Distance from globe center
        float m_cameraTheta;        // Horizontal rotation (longitude)
        float m_cameraPhi;          // Vertical rotation (latitude) 
        
        Vector3<float> m_cameraPos;
        Vector3<float> m_cameraTarget;
        Vector3<float> m_cameraUp;
        
        // Mouse interaction
        bool m_isDragging;
        float m_lastMouseX, m_lastMouseY;
        
        float m_dragVelocityX, m_dragVelocityY;
        
        // track if the camera has been initialized
        bool m_initialized;
        
        Globe3DCamera() : 
            m_cameraDistance(3.0f),
            m_cameraTheta(0.0f),
            m_cameraPhi(0.0f),
            m_isDragging(false),
            m_lastMouseX(0), m_lastMouseY(0),
            m_dragVelocityX(0.0f), m_dragVelocityY(0.0f),
            m_initialized(false) {
            
            m_cameraPos = Vector3<float>(0.0f, 0.5f, m_cameraDistance);
            m_cameraTarget = Vector3<float>(0.0f, 0.0f, 0.0f);
            m_cameraUp = Vector3<float>(0.0f, 1.0f, 0.0f);
        }
    } m_globe3DCamera;

public:

    GlobeRenderer();
    ~GlobeRenderer();
    void    Init();
    void    Reset();
    void    Render(bool inLobbyMode = false);

    void    AddLineStrip(const DArray<Vector3<float>> &vertices) const;
    void    GlobeGridlines();
    void    GlobeCoastlines();
    void    GlobeBorders();

    bool m_3DGlobeMode;

    Vector3  <float> m_camUp;
    Vector3  <float> m_camFront;

    DArray  <AnimatedIcon *>    m_animations;

    enum
    {
        AnimationTypeActionMarker,
        AnimationTypeSonarPing,
        AnimationTypeAttackMarker,
        AnimationTypeNukePointer,
        AnimationTypeNukeMarker,
        NumAnimations
    };

    void    RenderAnimations();
    int     CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude );

    void    Toggle3DGlobeMode();
    bool    Is3DGlobeModeEnabled() const { return m_3DGlobeMode; }
    void    RenderGlobeMouse();
    void    SetupCamera3d();
    void    Update3DGlobeCamera();
    void    Render3DGlobeCities();
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
    Vector3<float> Project3DToScreen                      (const Vector3<float>& worldPos);
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