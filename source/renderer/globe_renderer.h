#ifndef _included_globerenderer_h
#define _included_globerenderer_h

#include "world/worldobject.h"
#include "world/world.h"
#include "world/nuke.h"
#include "world/blip.h"

class Image;
class WorldObject;
class AnimatedIcon;
class Model3D;
class Blip;

#define    GLOBE_RADIUS                                1.0f     // default globe radius
#define    GLOBE_CULL_RADIUS                           0.995f   // culling sphere radius (inside the globe)
#define    GLOBE_ELEVATION                             0.0f     // no elevation, but here incase we do need elevation 
#define    GLOBE_NUKE_MODEL_SIZE                       0.75f    // nuke model scale
#define    GLOBE_ANIMATED_ICON_SIZE                    0.06f    // base size of animated icons
#define    GLOBE_OBJECT_SIZE                           0.0075f  // base size for objects on the globe
#define    GLOBE_UNIT_ICON_SIZE                        0.35f    // base size for unit icons (smallfighter.bmp etc)

#define    STYLE_GLOBE_COASTLINES                     "GlobeCoastlines"
#define    STYLE_GLOBE_BORDERS                        "GlobeBorders"
#define    STYLE_GLOBE_GRIDLINES                      "GlobeGridlines"

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
public:

    Vector3<float> ConvertLongLatTo3DPosition(float longitude, float latitude);
    Vector3<float> GetNormalizedFromLongLat (float longitude, float latitude);
    Vector3<float> GetElevatedPosition       (const Vector3<float>& position);
    Vector3<float> SlerpNormal               (const Vector3<float>& fromNormal, const Vector3<float>
                                              & toNormal, float t);
    void           GetSurfaceTangents        (const Vector3<float>& normal, Vector3<float>
                                              & tangent1, Vector3<float>& tangent2);

    struct Star3D {
        Vector3<float> position;
        float size;
        float brightness;
    };

    DArray<Star3D> g_starField3D;
    bool g_starField3DInitialized = false;

    Model3D *m_nukeModel;

    float   m_zoomFactor;

    float	m_drawScale;

    int     m_currentHighlightId;
    int     m_currentSelectionId;
    int     m_currentStateId;

    bool    m_draggingCamera;

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

protected:
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

    void    GenerateStarField();
    void    CleanupStarField();
    void    RegenerateStarField();
    void    BuildStarfieldVBO();
    void    GenerateStarSkybox();
    void    RenderStarField();
    
    void    BuildCullSphereVBO();
    void    RenderCullSphere();

    void    GlobeGridlines();
    void    GlobeCoastlines();
    void    GlobeBorders();

    void    AddLineStrip(const DArray<Vector3<float>> &vertices) const;

    void    RenderObjects();
    void    RenderGunfire();
    void    RenderExplosions();
    void    RenderAnimations();
    void    RenderPopulationDensity();
    void    RenderNukeUnits();
    void    RenderUnitHighlight( int _objectId );
    void    RenderWhiteBoard();
    void    RenderCities();
    void    RenderSanta();

    bool    IsPointVisible   (const Vector3<float>& globePoint, const Vector3<float>& cameraPos, float globeRadius);
    float   CullingThreshold (float cameraDistance, float globeRadius);
    void    GetCameraPosition( float &longitude, float &latitude, float &distance );
    void    SetCameraPosition( float longitude, float latitude, float distance );
    void    IsCameraIdle     (float oldLongitude, float oldLatitude);
    
    float   GetZoomFactor();
    float   GetDrawScale();

    void    RenderWorldObjectTargets( WorldObject *wobj, bool maxRanges );
    void    RenderActionLine           ( float fromLong, float fromLat, float toLong, float toLat,
                                        Colour col, float width, bool animate=true );
    
    Vector3<float> GetCameraPosition();

    static  float ConvertMenuToFogDensity                 (float menuValue);
    static  float ConvertFogDensityToMenu                 (float internalValue);
    static  float ConvertMenuToStarSize                   (float menuValue);
    static  float ConvertStarSizeToMenu                   (float internalValue);
};

#endif