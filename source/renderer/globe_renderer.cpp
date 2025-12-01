#include "lib/universal_include.h"

#include <math.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render2d/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/colour.h"
#include "lib/preferences.h"
#include "lib/render/styletable.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#ifdef TARGET_OS_MACOSX
#include "app/macutil.h"
#endif

#include "interface/interface.h"
#include "interface/toolbar.h"

#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"
#include "renderer/animated_icon.h"

#include "lib/render/renderer_debug_menu.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/worldobject.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/fleet.h"
#include "world/whiteboard.h"

#include "curves.h"

GlobeRenderer::GlobeRenderer()
:   m_3DGlobeMode(false)
{
}

GlobeRenderer::~GlobeRenderer()
{
    Reset();
}

void GlobeRenderer::Init()
{
    Reset();
}


void GlobeRenderer::Reset()
{
    m_animations.EmptyAndDelete();

}

// ******************************************************************************************************************************
//                                              Globe Options Menu Value Conversion Functions
// ******************************************************************************************************************************

//
// these functions convert between menu values and internal values

float GlobeRenderer::ConvertMenuToLandUnitSize(float menuValue) {
    float baseValue = 0.0075f;
    float scaleFactor = 0.002f;
    return baseValue + (menuValue - 1.0f) * scaleFactor;
}

float GlobeRenderer::ConvertLandUnitSizeToMenu(float internalValue) {
    float baseValue = 0.0075f;
    float scaleFactor = 0.002f;
    return 1.0f + (internalValue - baseValue) / scaleFactor;
}

float GlobeRenderer::ConvertMenuToNavalUnitSize(float menuValue) {
    float baseValue = 0.015f;
    float scaleFactor = 0.005f;
    return baseValue + (menuValue - 1.0f) * scaleFactor;
}

float GlobeRenderer::ConvertNavalUnitSizeToMenu(float internalValue) {
    float baseValue = 0.015f;
    float scaleFactor = 0.005f;
    return 1.0f + (internalValue - baseValue) / scaleFactor;
}

float GlobeRenderer::ConvertMenuToFogDensity(float menuValue) {
    float baseValue = 0.03f;
    float scaleFactor = 0.03f;
    return baseValue + (menuValue - 1.0f) * scaleFactor;
}

float GlobeRenderer::ConvertFogDensityToMenu(float internalValue) {
    float baseValue = 0.03f;
    float scaleFactor = 0.03f;
    return 1.0f + (internalValue - baseValue) / scaleFactor;
}

float GlobeRenderer::ConvertMenuToStarSize(float menuValue) {
    float baseValue = 0.032f;
    float scaleFactor = 0.005f;
    return baseValue + (menuValue - 1.0f) * scaleFactor;
}

float GlobeRenderer::ConvertStarSizeToMenu(float internalValue) {
    float baseValue = 0.032f;
    float scaleFactor = 0.005f;
    return 1.0f + (internalValue - baseValue) / scaleFactor;
}

// ******************************************************************************************************************************
//                                              Globe initialisation and camera controls
// ******************************************************************************************************************************

//
// star field data structure for background stars, i think in combination
// with the black globe surface overlay this makes the globe look more realistic

struct Star3D {
    Vector3<float> position;
    float size;
    float brightness;
};

static DArray<Star3D> g_starField3D;
static bool g_starField3DInitialized = false;

// ******************************************************************************************************************************
//                                              3D globe mouse rendering and calculations
// ******************************************************************************************************************************

//
// render mouse cursor and UI elements for 3D globe mode

void GlobeRenderer::RenderGlobeMouse()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    // get mouse screen position
    int mouseScreenX = g_inputManager->m_mouseX;
    int mouseScreenY = g_inputManager->m_mouseY;
    
    // convert screen coordinates to 3D world position on globe surface
    // we need to intersect the mouse ray with the globe surface
    Vector3<float> mouseWorldPos = ScreenToGlobePosition(mouseScreenX, mouseScreenY);
    
    // render move cursor when dragging camera
    if (m_globe3DCamera.m_isDragging) {
        Image *move = g_resource->GetImage("gui/move.bmp");
        if (move) {
            // same as map renderer static size that doesnt use preferences popupscale
            float cursorSize = 48.0f;
            
            g_renderer->StaticSprite(move, 
                            mouseScreenX - cursorSize/2, 
                            mouseScreenY - cursorSize/2, 
                            cursorSize, cursorSize, White);
        }
    }
}

//
// convert screen coordinates to globe surface position
// simplified approach using basic raysphere intersection

Vector3<float> GlobeRenderer::ScreenToGlobePosition(int screenX, int screenY)
{
    if (!Is3DGlobeModeEnabled()) {
        return Vector3<float>(0, 0, 0);
    }
    
    // convert screen coordinates to normalized device coordinates
    float screenW = (float)g_windowManager->WindowW();
    float screenH = (float)g_windowManager->WindowH();
    
    float ndcX = (2.0f * screenX) / screenW - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY) / screenH;
    
    // approximate the ray direction
    // this is less accurate but uses only available math functions
    Vector3<float> rayStart = m_globe3DCamera.m_cameraPos;
    Vector3<float> cameraForward = m_globe3DCamera.m_cameraTarget - m_globe3DCamera.m_cameraPos;
    cameraForward.Normalise();
    
    // create right and up vectors for camera space
    Vector3<float> right = cameraForward ^ Vector3<float>(0, 1, 0);
    right.Normalise();
    Vector3<float> up = right ^ cameraForward;
    up.Normalise();
    
    // calculate approximate ray direction based on mouse offset from center
    float fovRadians = 60.0f * M_PI / 180.0f;
    float aspect = screenW / screenH;
    float halfFovTan = tanf(fovRadians * 0.5f);
    
    Vector3<float> rayDir = cameraForward + 
                           right * (ndcX * halfFovTan * aspect) + 
                           up * (ndcY * halfFovTan);
    rayDir.Normalise();
    
    // yea bro i did not write this
    float a = rayDir * rayDir;
    float b = 2.0f * (rayStart * rayDir);
    float c = (rayStart * rayStart) - 1.0f;
    
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        Vector3<float> forward = m_globe3DCamera.m_cameraTarget - m_globe3DCamera.m_cameraPos;
        forward.Normalise();
        return forward;
    }
    
    // find nearest intersection point
    float t1 = (-b - sqrtf(discriminant)) / (2 * a);
    float t2 = (-b + sqrtf(discriminant)) / (2 * a);
    
    float t = (t1 > 0) ? t1 : t2; 
    
    Vector3<float> intersection = rayStart + rayDir * t;
    intersection.Normalise();
    
    return intersection;
}

// ******************************************************************************************************************************
//                                                    Star field rendering
// ******************************************************************************************************************************

//
// generate random star field around the globe

void GlobeRenderer::Generate3DStarField()
{
    if (g_starField3DInitialized) return;
    
    g_starField3D.Empty();
    
    // generate random stars on a large sphere around the globe
    int numStars = g_preferences->GetInt(PREFS_GLOBE_STAR_DENSITY, 1200); 
    float starSphereRadius = 20.0f;     // far from globe
    
    for (int i = 0; i < numStars; i++) {
        Star3D star;
        
        // generate random position on sphere using spherical coordinates
        float theta = frand(2.0f * M_PI); 
        float phi = frand(M_PI) - M_PI/2.0f; 
        
        // convert to 3D position on star sphere
        star.position.x = starSphereRadius * sin(theta) * cos(phi);
        star.position.y = starSphereRadius * sin(phi);
        star.position.z = starSphereRadius * cos(theta) * cos(phi);
        
        float menuStarSize = g_preferences->GetFloat(PREFS_GLOBE_STAR_SIZE, 1.0f);
        float internalStarSize = GlobeRenderer::ConvertMenuToStarSize(menuStarSize);
        float sizeVariation = internalStarSize * 0.5f;
        star.size = internalStarSize + frand(sizeVariation) - (sizeVariation * 0.5f); 
        
        // random brightness for realism
        float brightnessFactor = frand();
        if (brightnessFactor < 0.3f) {
            star.brightness = 0.4f + brightnessFactor * 0.3f; 
        } else {
            star.brightness = 0.7f + (brightnessFactor - 0.3f) * 0.43f; 
        }
        
        g_starField3D.PutData(star);
    }
    
    g_starField3DInitialized = true;
    AppDebugOut("3D Star Field: Generated %d stars\n", numStars);
}

//
// clean up star field 

void GlobeRenderer::Cleanup3DStarField()
{
    g_starField3D.Empty();
    g_starField3DInitialized = false;
    AppDebugOut("3D Star Field: Cleaned up star field\n");
}

void GlobeRenderer::Regenerate3DStarField()
{
    Cleanup3DStarField();
    Generate3DStarField();
    AppDebugOut("3D Star Field: Regenerated star field\n");
}

//
// render the star field background

void GlobeRenderer::Render3DStarField()
{
    //
    // first lets check if starfield is enabled in preferences
    
    if (!g_preferences->GetInt(PREFS_GLOBE_STARFIELD, 1)) {
        return;
    }
    
    Generate3DStarField();
    
    Image *cityImg = g_resource->GetImage("graphics/city.bmp");
    if (!cityImg) return;
    
    // render stars as white sprites facing the camera
    for (int i = 0; i < g_starField3D.Size(); i++) {
        Star3D& star = g_starField3D[i];
        
        // Calculate star color with variety
        Colour starColor;
        int alpha = (int)(255 * star.brightness);
        
        // create different star types for percieved realism and randomness
        int starType = i % 13; 
        if (starType < 8) {

            //
            // most stars are white/blue-white

            starColor.Set(255, 255, 255, alpha);
        } else if (starType < 10) {

            //
            // some blue stars

            starColor.Set(180, 200, 255, alpha);
        } else if (starType < 12) {

            //
            // some warm/yellow stars

            starColor.Set(255, 230, 180, alpha);
        } else {

            //
            // a few red stars

            starColor.Set(255, 180, 150, alpha);
        }
        
        g_renderer3d->StaticSprite3D(cityImg, 
                                       star.position.x, star.position.y, star.position.z,
                                       star.size, star.size, 
                                       starColor);
    }
}

void GlobeRenderer::AddLineStrip(const DArray<Vector3<float>> &vertices) const
{
    if (vertices.Size() <= 0) {
        return;
    }

    Vector3<float> *vertexArray = new Vector3<float>[vertices.Size()];
    for (int i = 0; i < vertices.Size(); ++i) {
        vertexArray[i] = vertices.GetData(i);
    }
    
    g_renderer3d->AddLineStripToMegaVBO3D(vertexArray, vertices.Size());
    delete[] vertexArray;
}

void GlobeRenderer::GlobeCoastlines()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_GLOBE_COASTLINES) == 1) {
        if (!g_renderer3d->IsMegaVBO3DValid("GlobeCoastlines")) {
#ifndef TARGET_EMSCRIPTEN
            g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_COAST_THICKNESS, 1.0f));
#endif
            g_renderer3d->BeginMegaVBO3D("GlobeCoastlines", g_styleTable->GetPrimaryColour( STYLE_GLOBE_COASTLINES ));

            for( int i = 0; i < g_app->GetEarthData()->m_islands.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_islands[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> coastlineVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];

                    coastlineVertices.PutData(ConvertLongLatTo3DPosition(thePoint->x, thePoint->y));
                }
                AddLineStrip(coastlineVertices);
            }
            
            g_renderer3d->EndMegaVBO3D();

        }
    }

    //
    // Build it

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_COAST_THICKNESS, 1.0f));
#endif

    g_renderer3d->RenderMegaVBO3D("GlobeCoastlines");
}

void GlobeRenderer::GlobeBorders()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_GLOBE_BORDERS) == 1) {
        if (!g_renderer3d->IsMegaVBO3DValid("GlobeBorders")) {
#ifndef TARGET_EMSCRIPTEN
            g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_BORDER_THICKNESS, 1.0f));
#endif
            g_renderer3d->BeginMegaVBO3D("GlobeBorders", g_styleTable->GetPrimaryColour( STYLE_GLOBE_BORDERS ));

            for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_borders[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> borderVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];

                    borderVertices.PutData(ConvertLongLatTo3DPosition(thePoint->x, thePoint->y));
                }
                AddLineStrip(borderVertices);
            }

            g_renderer3d->EndMegaVBO3D();

        }
    }

    //
    // Build it

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_BORDER_THICKNESS, 1.0f));
#endif

    g_renderer3d->RenderMegaVBO3D("GlobeBorders");
}

void GlobeRenderer::GlobeGridlines()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_GLOBE_GRIDLINES) == 1) {
        if (!g_renderer3d->IsMegaVBO3DValid("GlobeGridlines")) {
            g_renderer3d->BeginMegaVBO3D("GlobeGridlines", g_styleTable->GetPrimaryColour( STYLE_GLOBE_GRIDLINES ));
        
        //
        // Longitudinal lines (meridians)

        for( float x = -180; x < 180; x += 10 )
        {
            DArray<Vector3<float>> lineVertices;
            for( float y = -90; y < 90; y += 2.0f )
            {
                lineVertices.PutData(ConvertLongLatTo3DPosition(x, y));
            }
            AddLineStrip(lineVertices);
        }

        //
        // Latitudinal lines (parallels)

        for( float y = -90; y <= 90; y += 10 )
        {
            DArray<Vector3<float>> lineVertices;
            for( float x = -180; x <= 180; x += 2.0f )
            {
                lineVertices.PutData(ConvertLongLatTo3DPosition(x, y));
            }
            AddLineStrip(lineVertices);
        }
        
        g_renderer3d->EndMegaVBO3D();

        }
    }

    //
    // Build it

    g_renderer3d->RenderMegaVBO3D("GlobeGridlines");

}

//
// handle globe initialisation

void GlobeRenderer::Toggle3DGlobeMode()
{
    m_3DGlobeMode = !m_3DGlobeMode;
    
    if (Is3DGlobeModeEnabled()) {

        //
        // only initialize 3D camera position the first time its used
        // this preserves camera position when switching between 2D and 3D modes
        // we save the camera position during runtime just like the 2D map renderer

        if (!m_globe3DCamera.m_initialized) {
            float globeSize = g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f);
            m_globe3DCamera.m_cameraDistance = 3.0f * globeSize;
            m_globe3DCamera.m_cameraTheta = 0.0f;
            m_globe3DCamera.m_cameraPhi = 0.0f;
            
            // default camera position for the camera when we load up
            m_globe3DCamera.m_cameraPos = Vector3<float>(0.0f, 0.5f, m_globe3DCamera.m_cameraDistance);
            m_globe3DCamera.m_cameraTarget = Vector3<float>(0.0f, 0.0f, 0.0f);
            m_globe3DCamera.m_cameraUp = Vector3<float>(0.0f, 1.0f, 0.0f);
            
            m_globe3DCamera.m_initialized = true;
        }
        
        AppDebugOut("3D Globe Mode: ENABLED\n");
    } else {
        Cleanup3DStarField();
        AppDebugOut("3D Globe Mode: DISABLED\n");
    }
    
    //
    // disable the radar and territory buttons in globe mode for now
    // territory button most likely will be permanant but radar
    // might be enabled if i can figure out how to get depth testing
    // working for the radar circles

    EclWindow *toolbar = EclGetWindow("Toolbar");
    if (toolbar) {
        ToolbarButton *radar = (ToolbarButton*)toolbar->GetButton("Radar");
        if (radar) {
            radar->m_disabled = Is3DGlobeModeEnabled();
        }

        ToolbarButton *territory = (ToolbarButton*)toolbar->GetButton("Territory");
        if (territory) {
            territory->m_disabled = Is3DGlobeModeEnabled();
        }
    }
}

// ******************************************************************************************************************************
//                                              Main 3D globe rendering
// ******************************************************************************************************************************

//
// main rendering for the 3D globe

void GlobeRenderer::Render(bool inLobbyMode)
{
    //
    // begin draw call tracking

    g_renderer3d->BeginFrame3D();
    
    float aspect = (float)g_windowManager->WindowW() / (float)g_windowManager->WindowH();
    g_renderer3d->SetPerspective(60.0f, aspect, 0.1f, 100.0f);
    
    if (!inLobbyMode) {
        g_renderer3d->SetLookAt(
            m_globe3DCamera.m_cameraPos.x, m_globe3DCamera.m_cameraPos.y, m_globe3DCamera.m_cameraPos.z,
            m_globe3DCamera.m_cameraTarget.x, m_globe3DCamera.m_cameraTarget.y, m_globe3DCamera.m_cameraTarget.z,
            m_globe3DCamera.m_cameraUp.x, m_globe3DCamera.m_cameraUp.y, m_globe3DCamera.m_cameraUp.z
        );
        
        //
        // set camera position for billboard calculations

        g_renderer3d->SetCameraPosition(m_globe3DCamera.m_cameraPos.x, 
                                       m_globe3DCamera.m_cameraPos.y, 
                                       m_globe3DCamera.m_cameraPos.z);
    }
    
    //
    // enable depth testing for 3D sprites

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    //
    // enable fog for the main scene

    if (!inLobbyMode) {
        float fogDistanceSetting = (float)g_preferences->GetInt(PREFS_GLOBE_FOG_DISTANCE, 20);
        float distanceMultiplier = (fogDistanceSetting / 20.0f - 1.0f) * 2.0f + 1.0f;
        
        //
        // scale the camera distance for fog calculations
        // we pass in fake camera values here to make the
        // fog render further or closer to the real camera
        
        Vector3<float> fogCameraPos = m_globe3DCamera.m_cameraPos;
        fogCameraPos = fogCameraPos * distanceMultiplier;
        
        g_renderer3d->EnableOrientationFog(0.03f, 0.03f, 0.03f, 20.0f,
                                          fogCameraPos.x,
                                          fogCameraPos.y,
                                          fogCameraPos.z);
    }

    if(inLobbyMode) {
        GlobeGridlines();
    }
    
    GlobeCoastlines();
    GlobeBorders();

    //
    // disable fog before rendering main scene

    g_renderer3d->DisableFog();

    //
    // begin star field and globe surface scene

    g_renderer3d->BeginStaticSpriteBatch3D();      // Star field batching
    g_renderer3d->BeginTriangleFillBatch3D();   // Globe surface batching
    
    Render3DGlobeCulling();
    Render3DStarField();

    //
    // end the globe surface scene

    g_renderer3d->EndTriangleFillBatch3D();     // Flush globe surface triangles
    g_renderer3d->EndStaticSpriteBatch3D();        // Flush star sprites
    
    //
    // begin scene main scene

    g_renderer3d->BeginLineBatch3D();      // Unit movement trails
    g_renderer3d->BeginNuke3DModelBatch3D();    // 3D nuke models (replaces flat nuke sprites)
    g_renderer3d->BeginStaticSpriteBatch3D();       // Main unit sprites + city icons
    g_renderer3d->BeginRotatingSpriteBatch3D();   // Rotating sprites (aircraft, but not nukes anymore)

    //
    // force additive blending after culling has been applied

    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);

    Render3DGlobeCities();
    Render3DUnits();
    Render3DUnitTrails();
    Render3DGunfire();
    Render3DExplosions();
    Render3DNukeSymbols();
    Render3DWorldObjectTargets();
    Render3DNukeHighlights();
    Render3DWhiteBoard();
    Render3DPopulationDensity();
    Render3DAnimations();
    Render3DSanta();
    Render3DNuke();


    //
    // to prevent z-fighting, dont write to depth buffer and set
    // blend mode to normal for unit trails

    g_renderer3d->EndLineBatch3D();        // Flush all unit trails
    g_renderer3d->BeginLineBatch3D();      // Unit movement trails

    g_renderer->SetBlendMode(Renderer::BlendModeNormal);
    glDepthMask(GL_FALSE);

    Render3DNukeTrajectories();

    g_renderer3d->EndLineBatch3D();        // Flush all unit trails

    glDepthMask(GL_TRUE);
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    
    //
    // now end the main scene and flush
    
    g_renderer3d->EndRotatingSpriteBatch3D();     // Flush all rotating sprites (atlas batching!)
    g_renderer3d->EndStaticSpriteBatch3D();         // Flush all main unit sprites + city icons (atlas batching!)
    g_renderer3d->EndNuke3DModelBatch3D();      // Flush all 3D nuke models


    glDisable(GL_DEPTH_TEST);
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);

    // reset to 2d viewport
    g_renderer->Reset2DViewport();

    g_renderer->BeginStaticSpriteBatch();
    // render mouse cursor
    if (g_app->GetMapRenderer()->IsMouseInMapRenderer()) {
        RenderGlobeMouse();
    }

    g_renderer->EndStaticSpriteBatch();
    
    // end draw call tracking
    g_renderer3d->EndFrame3D();
}

void GlobeRenderer::RenderAnimations()
{
    
    for( int i = 0; i < m_animations.Size(); ++i )
    {
        if( m_animations.ValidIndex(i) )
        {
            AnimatedIcon *anim = m_animations[i];
            if( anim->Render() )
            {
                m_animations.RemoveData(i);
                delete anim;
            }
        }
    }
    
}

int GlobeRenderer::CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude )
{
    AnimatedIcon *anim = NULL;
    switch( animationType )
    {
        case AnimationTypeActionMarker  : anim = new ActionMarker();    break;
        case AnimationTypeSonarPing     : anim = new SonarPing();       break; 
        case AnimationTypeNukePointer   : anim = new NukePointer();     break;

        default: return -1;
    }

    anim->m_longitude = longitude;
    anim->m_latitude = latitude;
    anim->m_fromObjectId = _fromObjectId;
    anim->m_animationType = animationType;
    
    int index = m_animations.PutData( anim );
    return index;
}


//
// camera controls for the 3d globe, to preserve muscle memory
// we use default DEFCON controls for globe mode
// and for the lobby remains unchanged

void GlobeRenderer::SetupCamera3d()
{
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;
    float screenW = g_windowManager->WindowW();
    float screenH = g_windowManager->WindowH();

    // replace deprecated matrix operations with 3D renderer
    g_renderer3d->SetPerspective(fov, screenW / screenH, nearPlane, farPlane);

    static float timeVal = 0.0f;
    timeVal += g_advanceTime * 1;
    float timeNow = timeVal;
    float camDist = 1.7f;       //+sinf(timeNow*0.2f)*0.5f;
    float camHeight = 0.5f + cosf(timeNow*0.2f) * 0.2f;
    Vector3<float> pos(0.0f,camHeight, camDist);
    pos.RotateAroundY( timeNow * -0.1f );
	Vector3<float> requiredFront = pos * -1.0f;

    static Vector3<float> front = Vector3<float>::ZeroVector();
    float timeFactor = g_advanceTime * 0.3f;
    front = requiredFront * timeFactor + front * (1-timeFactor);

	Vector3<float> right = front ^ Vector3<float>::UpVector();
    Vector3<float> up = right ^ front;
    Vector3<float> forwards = pos + front * 100;

    m_camFront = front;
    m_camUp = up;

    m_camFront.Normalise();
    m_camUp.Normalise();

    // replace gluLookAt with 3D renderer
    g_renderer3d->SetLookAt(pos.x, pos.y, pos.z,
              forwards.x, forwards.y, forwards.z,
              up.x, up.y, up.z);

    // OpenGL 3.3 state management
    glDisable( GL_CULL_FACE );
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    glDisable( GL_DEPTH_TEST );

    // changed function name to enableDistanceFog, as now we have two fog modes
    // this function remains unchanged but the name has changed for easier differentiation
    // between fogFactor and fogDistance
    g_renderer3d->EnableDistanceFog(camDist/2.0f, camDist*2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.25f);
}

void GlobeRenderer::Update3DGlobeCamera()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    bool ignoreKeys = g_app->GetInterface()->UsingChatWindow();
    
    // handle keyboard layout variations, same as 2d mode
    int keyboardLayout = g_preferences->GetInt(PREFS_INTERFACE_KEYBOARDLAYOUT);
    int KeyQ = KEY_Q, KeyW = KEY_W, KeyE = KEY_E, KeyA = KEY_A, KeyS = KEY_S, KeyD = KEY_D;
    
    switch (keyboardLayout) {
        case 2:  // AZERTY
            KeyQ = KEY_A;
            KeyW = KEY_Z;
            KeyA = KEY_Q;
            break;
        case 3:  // QZERTY
            KeyW = KEY_Z;
            break;
        case 4:  // DVORAK
            KeyQ = KEY_QUOTE;
            KeyW = KEY_COMMA;
            KeyE = KEY_STOP;
            KeyS = KEY_O;
            KeyD = KEY_E;
            break;
        default:  // QWERTY/QWERTZ (1) or invalid pref - do nothing
            break;
    }
    
    // WASD camera rotation
    if (!ignoreKeys) {
        float rotationSpeed = 0.75f * g_advanceTime;  // smooth rotation speed
        
        if (g_keys[KeyA] || g_keys[KEY_LEFT]) {
            m_globe3DCamera.m_cameraTheta -= rotationSpeed;
        }
        if (g_keys[KeyD] || g_keys[KEY_RIGHT]) {
            m_globe3DCamera.m_cameraTheta += rotationSpeed;
        }
        if (g_keys[KeyW] || g_keys[KEY_UP]) {
            m_globe3DCamera.m_cameraPhi += rotationSpeed;
        }
        if (g_keys[KeyS] || g_keys[KEY_DOWN]) {
            m_globe3DCamera.m_cameraPhi -= rotationSpeed;
        }
        
        // Q and E for zoom, dont flip these next time :)
        if (g_keys[KeyE]) {
            m_globe3DCamera.m_cameraDistance += 5.0f * g_advanceTime;
        }
        if (g_keys[KeyQ]) {
            m_globe3DCamera.m_cameraDistance -= 5.0f * g_advanceTime;
        }
        
        //
        // clamp vertical rotation and zoom distance according
        // to the globe size set inside preferences

        m_globe3DCamera.m_cameraPhi = fmaxf(-M_PI/2.0f + 0.1f, fminf(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
        float globeSize = g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f);
        float minDistance = 1.5f * globeSize;
        float maxDistance = 3.0f * globeSize;
        m_globe3DCamera.m_cameraDistance = fmaxf(minDistance, fminf(maxDistance, m_globe3DCamera.m_cameraDistance));
        
        //
        // update camera position
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
    }
    
    // globe dragging with middle mouse button
    if (g_inputManager->m_mmb && g_app->GetMapRenderer()->IsMouseInMapRenderer()) {
        if (!m_globe3DCamera.m_isDragging) {
            m_globe3DCamera.m_isDragging = true;
            g_app->SetMousePointerVisible(false);
        }
        
        // use same approach as 2D map renderer
        // adaptive sensitivity based on zoom level - closer = more sensitive
        float zoomSensitivity = 1.0f / (m_globe3DCamera.m_cameraDistance * 0.8f + 0.2f);
        float baseSensitivity = 0.003f * zoomSensitivity;
        
        m_globe3DCamera.m_cameraTheta -= g_inputManager->m_mouseVelX * baseSensitivity;
        m_globe3DCamera.m_cameraPhi += g_inputManager->m_mouseVelY * baseSensitivity;
        
        // clamp vertical rotation
        m_globe3DCamera.m_cameraPhi = fmaxf(-M_PI/2.0f + 0.1f, fminf(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
        
        // update camera position
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sinf(m_globe3DCamera.m_cameraTheta) * cosf(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sinf(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cosf(m_globe3DCamera.m_cameraTheta) * cosf(m_globe3DCamera.m_cameraPhi);
    } else {
        if (m_globe3DCamera.m_isDragging) {
            // show system mouse pointer again when dragging stops
            if (!g_app->MousePointerIsVisible()) {
                g_app->SetMousePointerVisible(true);
            }
        }
        m_globe3DCamera.m_isDragging = false;
    }
    
    // mouse wheel zoom
    if (g_app->GetMapRenderer()->IsMouseInMapRenderer() && g_inputManager->m_mouseVelZ != 0) {
        m_globe3DCamera.m_cameraDistance += g_inputManager->m_mouseVelZ * -0.1f;
        float globeSize = g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f);
        float minDistance = 1.5f * globeSize;
        float maxDistance = 3.0f * globeSize;
        m_globe3DCamera.m_cameraDistance = fmaxf(minDistance, fminf(maxDistance, m_globe3DCamera.m_cameraDistance));
        
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
    }
}

//
// this is the function that makes it all happen, we convert 2d coordinates to 3d coordinates
// the only issue with this which is pretty unavoidable is that the closer we get to the poles
// the units get squashed, its literally an unsolvable problem.

Vector3<float> GlobeRenderer::ConvertLongLatTo3DPosition(float longitude, float latitude)
{
    
    float lonRad = longitude * M_PI / 180.0f;
    float latRad = latitude * M_PI / 180.0f;
    
    // spherical to cartesian conversion
    float globeRadius = g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f);
    Vector3<float> pos(0, 0, globeRadius);
    pos.RotateAroundY(lonRad);
    Vector3<float> right = pos ^ Vector3<float>(0, 1, 0);
    right.Normalise();
    pos.RotateAround(right * latRad);
    
    return pos;
}

//
// simple culling writes depth values but no color, creating an invisible mask

void GlobeRenderer::Render3DGlobeCulling(bool inLobbyMode)
{
    // disable color writing, we only want to write to depth buffer
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    
    // use a transparent color 
    Colour invisibleColor(0, 0, 0, 0);
    
    // render the same globe geometry as the visible surface
    int segments = 32; 
    float radius = (inLobbyMode ? 1.0f : g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f)) * 0.998f;
    Vector3<float> center(0.0f, 0.0f, 0.0f);
    
    // render filled sphere using 3D renderer triangle system
    for (int lat = 0; lat < segments; ++lat) {
        float theta1 = M_PI * lat / segments - M_PI/2;
        float theta2 = M_PI * (lat + 1) / segments - M_PI/2;
        
        for (int lon = 0; lon < segments; ++lon) {
            float phi1 = 2.0f * M_PI * lon / segments;
            float phi2 = 2.0f * M_PI * (lon + 1) / segments;
            
            // Calculate the 4 vertices of this quad
            float x1 = radius * cosf(theta1) * cosf(phi1);
            float y1 = radius * sinf(theta1);
            float z1 = radius * cosf(theta1) * sinf(phi1);
            
            float x2 = radius * cosf(theta1) * cosf(phi2);
            float y2 = radius * sinf(theta1);
            float z2 = radius * cosf(theta1) * sinf(phi2);
            
            float x3 = radius * cosf(theta2) * cosf(phi2);
            float y3 = radius * sinf(theta2);
            float z3 = radius * cosf(theta2) * sinf(phi2);
            
            float x4 = radius * cosf(theta2) * cosf(phi1);
            float y4 = radius * sinf(theta2);
            float z4 = radius * cosf(theta2) * sinf(phi1);
            
            // First triangle: 1, 2, 3
            g_renderer3d->TriangleFill3D(center.x + x1, center.y + y1, center.z + z1,
                                               center.x + x2, center.y + y2, center.z + z2,
                                               center.x + x3, center.y + y3, center.z + z3, invisibleColor);
            
            // Second triangle: 1, 3, 4
            g_renderer3d->TriangleFill3D(center.x + x1, center.y + y1, center.z + z1,
                                               center.x + x3, center.y + y3, center.z + z3,
                                               center.x + x4, center.y + y4, center.z + z4, invisibleColor);
        }
    }
    
    // re-enable color writing
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

//
// create the same projection matrix as g_renderer3d->SetPerspective
// and the same view matrix as g_renderer3d->SetLookAt

Vector3<float> GlobeRenderer::Project3DToScreen(const Vector3<float>& worldPos)
{
    float aspect = (float)g_windowManager->WindowW() / (float)g_windowManager->WindowH();
    
    Matrix4f3D projection;
    projection.Perspective(60.0f, aspect, 0.1f, 100.0f);
     
    Matrix4f3D modelView;
    modelView.LookAt(
        m_globe3DCamera.m_cameraPos.x, m_globe3DCamera.m_cameraPos.y, m_globe3DCamera.m_cameraPos.z,
        m_globe3DCamera.m_cameraTarget.x, m_globe3DCamera.m_cameraTarget.y, m_globe3DCamera.m_cameraTarget.z,
        m_globe3DCamera.m_cameraUp.x, m_globe3DCamera.m_cameraUp.y, m_globe3DCamera.m_cameraUp.z
    );
    
    // transform by model-view matrix
    Vector3<float> viewPos;
    viewPos.x = modelView.m[0] * worldPos.x + modelView.m[4] * worldPos.y + modelView.m[8] * worldPos.z + modelView.m[12];
    viewPos.y = modelView.m[1] * worldPos.x + modelView.m[5] * worldPos.y + modelView.m[9] * worldPos.z + modelView.m[13];
    viewPos.z = modelView.m[2] * worldPos.x + modelView.m[6] * worldPos.y + modelView.m[10] * worldPos.z + modelView.m[14];
    float w = modelView.m[3] * worldPos.x + modelView.m[7] * worldPos.y + modelView.m[11] * worldPos.z + modelView.m[15];
    
    // transform by projection matrix
    Vector3<float> clipPos;
    float clipW;
    clipPos.x = projection.m[0] * viewPos.x + projection.m[4] * viewPos.y + projection.m[8] * viewPos.z + projection.m[12] * w;
    clipPos.y = projection.m[1] * viewPos.x + projection.m[5] * viewPos.y + projection.m[9] * viewPos.z + projection.m[13] * w;
    clipPos.z = projection.m[2] * viewPos.x + projection.m[6] * viewPos.y + projection.m[10] * viewPos.z + projection.m[14] * w;
    clipW = projection.m[3] * viewPos.x + projection.m[7] * viewPos.y + projection.m[11] * viewPos.z + projection.m[15] * w;
    
    // check if point is behind camera
    if (clipW <= 0.001f) {
        return Vector3<float>(-1000, -1000, -1);
    }
    
    Vector3<float> ndcPos;
    ndcPos.x = clipPos.x / clipW;
    ndcPos.y = clipPos.y / clipW;
    ndcPos.z = clipPos.z / clipW;
     
    if (ndcPos.x < -1.0f || ndcPos.x > 1.0f || ndcPos.y < -1.0f || ndcPos.y > 1.0f) {
        return Vector3<float>(-1000, -1000, -1);
    }
    
    // convert NDC to screen coordinates
    int screenW = g_windowManager->WindowW();
    int screenH = g_windowManager->WindowH();
    
    float screenX = (ndcPos.x + 1.0f) * 0.5f * screenW;
    float screenY = (ndcPos.y + 1.0f) * 0.5f * screenH;
    
    screenY = screenH - screenY;
    
    return Vector3<float>(screenX, screenY, -viewPos.z);
}

// ******************************************************************************************************************************
//                                              Main object rendering functions
// ******************************************************************************************************************************

void GlobeRenderer::Render3DGlobeCities()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    Image *cityImg = g_resource->GetImage("graphics/city.bmp");
    if (!cityImg) return;
    
    // these are added but not actually used, i had alot of issues with city names
    bool showCityNames = g_preferences->GetInt(PREFS_GRAPHICS_CITYNAMES);
    bool showCountryNames = g_preferences->GetInt(PREFS_GRAPHICS_COUNTRYNAMES);
    
    // zoom does not affect city size
    float normalizedDistance = (m_globe3DCamera.m_cameraDistance - 1.5f) / 8.5f;
    float equivalent2DZoom = 1.0f - normalizedDistance;
    equivalent2DZoom = fmaxf(0.05f, fminf(1.0f, equivalent2DZoom));
    
    // fade based on zoom 
    float zoomFactorEquivalent = 1.0f - equivalent2DZoom;
    Colour cityNameColor(120, 180, 255, 255);
    Colour countryNameColor(150, 255, 150, 255);
    cityNameColor.m_a = 200.0f * (1.0f - sqrtf(zoomFactorEquivalent));
    countryNameColor.m_a = 200.0f * (1.0f - sqrtf(zoomFactorEquivalent));
    
    
    struct CityRenderInfo {
        City* city;
        Vector3<float> worldPos;
        float distanceToCamera;
        float size;
        Colour color;
    };
    
    DArray<CityRenderInfo> visibleCities;
    
    for (int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i) {
        City *city = g_app->GetWorld()->m_cities.GetData(i);
        
        if (city->m_capital || city->m_teamId != -1) {
            Vector3<float> cityPos = ConvertLongLatTo3DPosition(
                city->m_longitude.DoubleValue(), 
                city->m_latitude.DoubleValue()
            );
            
            float baseSize = sqrtf(sqrtf((float)city->m_population)) / 25.0f;
            
            float size = baseSize * 0.0075f;  // half of original 0.015f
            size = fmaxf(size, 0.004f);
            size = fminf(size, 0.025f);  
            
            // get city colour
            Colour col(100, 100, 100, 200);
            if (city->m_teamId > -1) {
                col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                float alphaZoomFactor = fminf(0.8f, zoomFactorEquivalent);
                col.m_a = 200.0f * (1.0f - alphaZoomFactor);
            }
            
            Vector3<float> cameraToCity = cityPos - m_globe3DCamera.m_cameraPos;
            float distance = cameraToCity.Mag();
            
            CityRenderInfo info;
            info.city = city;
            info.worldPos = cityPos;
            info.distanceToCamera = distance;
            info.size = size;
            info.color = col;
            
            visibleCities.PutData(info);
        }
    }
    
    for (int i = 0; i < visibleCities.Size() - 1; i++) {
        for (int j = 0; j < visibleCities.Size() - i - 1; j++) {
            if (visibleCities[j].distanceToCamera < visibleCities[j + 1].distanceToCamera) {
                CityRenderInfo temp = visibleCities[j];
                visibleCities[j] = visibleCities[j + 1];
                visibleCities[j + 1] = temp;
            }
        }
    }
    
    for (int i = 0; i < visibleCities.Size(); i++) {
        CityRenderInfo& info = visibleCities[i];
        Vector3<float> cityPos = info.worldPos;
        float size = info.size;
        Colour col = info.color;
        
        Vector3<float> normal = cityPos;
        normal.Normalise();
        
        g_renderer3d->StaticSprite3D(cityImg, cityPos.x, cityPos.y, cityPos.z, size * 2.0f, size * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
    }
    
    // currently not used, i had alot of issues with city names
    if ((showCityNames || showCountryNames) && cityNameColor.m_a > 10.0f) {
        float textSize = 12.0f;
        if (equivalent2DZoom > 0.5f) {
            textSize = 12.0f + (equivalent2DZoom - 0.5f) * 16.0f;
        }
        textSize = fminf(textSize, 24.0f);
        
        float worldTextSize = textSize * 0.0008f;
        
        for (int i = 0; i < visibleCities.Size(); i++) {
            CityRenderInfo& info = visibleCities[i];
            City* city = info.city;
            Vector3<float> cityPos = info.worldPos;
            
            Vector3<float> normal = cityPos;
            normal.Normalise();
            
            Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
            if (fabsf(normal.y) > 0.9f) {
                up = Vector3<float>(1.0f, 0.0f, 0.0f);
            }
            
            Vector3<float> tangent1 = normal ^ up;
            tangent1.Normalise();
            Vector3<float> tangent2 = normal ^ tangent1;
            tangent2.Normalise();
            
            Vector3<float> textBasePos = cityPos + normal * 0.003f - tangent2 * (info.size + worldTextSize);
            
        }
    }
}

//
// function to render units on the globe
// i will refactor this once i have a better understanding of the code that claude generated

void GlobeRenderer::Render3DUnits()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // handle different unit types
    struct UnitRenderInfo {
        WorldObject* unit;
        Vector3<float> worldPos;
        float distanceToCamera;
        float size;
        Colour color;
        Image* image;
        bool isMovingObject;
        bool isAirUnit;
        float rotationAngle;
    };
    
    DArray<UnitRenderInfo> visibleUnits;
    
    // First pass: collect visible units with enhanced categorization (except nukes)
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // skip nukes
            if (wobj->m_type == WorldObject::TypeNuke) continue;
            
            // check what teamid we have set, then we can decide
            // whether to render the unit or not
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            bool renderAsGhost = false;
            if (!shouldRender) {
                // Render as ghost if our team has seen this unit before
                if (myTeamId != -1 &&
                    !g_app->GetMapRenderer()->GetRenderEverything() &&
                    wobj->m_lastSeenTime[myTeamId] > 0)
                {
                    renderAsGhost = true;
                }
                
                if (!renderAsGhost) {
                    continue; // never seen so dont render
                }
            }
            
            Vector3<float> unitPos;
            
            if (renderAsGhost) {
                // ghost prediction logic
                if (wobj->IsMovingObject()) {
                    MovingObject* mobj = (MovingObject*)wobj;
                    
                    // predictive ghost movement for air/sea units only
                    if (mobj->m_movementType == MovingObject::MovementTypeAir ||
                        mobj->m_movementType == MovingObject::MovementTypeSea) {
                        Fixed predictionTime = mobj->m_ghostFadeTime - mobj->m_lastSeenTime[myTeamId];
                        predictionTime += Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                        float predictedLongitude = (mobj->m_lastKnownPosition[myTeamId].x + mobj->m_lastKnownVelocity[myTeamId].x * predictionTime).DoubleValue();
                        float predictedLatitude = (mobj->m_lastKnownPosition[myTeamId].y + mobj->m_lastKnownVelocity[myTeamId].y * predictionTime).DoubleValue();
                        
                        unitPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
                    } else {
                        // land units use last known position without prediction
                        float ghostLongitude = wobj->m_lastKnownPosition[myTeamId].x.DoubleValue();
                        float ghostLatitude = wobj->m_lastKnownPosition[myTeamId].y.DoubleValue();
                        
                        unitPos = ConvertLongLatTo3DPosition(ghostLongitude, ghostLatitude);
                    }
                } else {
                    // use last known position
                    float ghostLongitude = wobj->m_lastKnownPosition[myTeamId].x.DoubleValue();
                    float ghostLatitude = wobj->m_lastKnownPosition[myTeamId].y.DoubleValue();
                    
                    unitPos = ConvertLongLatTo3DPosition(ghostLongitude, ghostLatitude);
                }
            } else {
                // Regular surface positioning for visible units
                Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                Fixed predictedLongitude = wobj->m_longitude + wobj->m_vel.x * predictionTime;
                Fixed predictedLatitude = wobj->m_latitude + wobj->m_vel.y * predictionTime;
                
                unitPos = ConvertLongLatTo3DPosition(
                    predictedLongitude.DoubleValue(), 
                    predictedLatitude.DoubleValue()
                );
            }
            
            //
            // use last known state for ghosts to prevent cheating
            // im so happy i caught this one before releasing to public

            int stateToRender = renderAsGhost ? wobj->m_lastSeenState[myTeamId] : wobj->m_currentState;
            Image *unitImg = wobj->GetBmpImage(stateToRender);
            if (!unitImg) continue;
            
            // pass the image to the size multiplier
            float baseSize = wobj->GetSize3D().DoubleValue();
            float size;
            
            bool isMovingObject = wobj->IsMovingObject();
            bool isAirUnit = false;
            
            if (isMovingObject) {
                MovingObject *mobj = (MovingObject*)wobj;
                isAirUnit = (mobj->m_movementType == MovingObject::MovementTypeAir);
                
                //
                // base size for land and sea units is set inside preferences
                // but we dont change the multiplier. the multiplier is fixed

                if (isAirUnit) {
                    float landUnitMultiplier = ConvertMenuToLandUnitSize(g_preferences->GetFloat(PREFS_GLOBE_LAND_UNIT_SIZE, 1.0f));
                    size = baseSize * landUnitMultiplier * 1.0f;
                } else if (mobj->m_movementType == MovingObject::MovementTypeSea) {
                    float navalUnitMultiplier = ConvertMenuToNavalUnitSize(g_preferences->GetFloat(PREFS_GLOBE_NAVAL_UNIT_SIZE, 1.0f));
                    size = baseSize * navalUnitMultiplier;
                } else {
                    float landUnitMultiplier = ConvertMenuToLandUnitSize(g_preferences->GetFloat(PREFS_GLOBE_LAND_UNIT_SIZE, 1.0f));
                    size = baseSize * landUnitMultiplier;
                }
            } else {
                float landUnitMultiplier = ConvertMenuToLandUnitSize(g_preferences->GetFloat(PREFS_GLOBE_LAND_UNIT_SIZE, 1.0f));
                size = baseSize * landUnitMultiplier * 2.0f; 
            }
            
            size = fmaxf(size, 0.002f);  
            size = fminf(size, 0.1f);
            
            Colour unitColor = White;
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            if (team) {
                unitColor = team->GetTeamColour();
            }
            
            if (renderAsGhost) {
                // calculate ghost transparency based on last seen time
                int transparency = (int)(255 * (wobj->m_lastSeenTime[myTeamId] / wobj->m_ghostFadeTime).DoubleValue());
                unitColor.Set(150, 150, 150, transparency);
            }
            
            // make sure to pass the rotation angle for the unit icons
            float rotationAngle = 0.0f;
            if (isAirUnit) {
                if (renderAsGhost && wobj->IsMovingObject()) {
                    // use last known velocity for ghost rotation
                    MovingObject* mobj = (MovingObject*)wobj;
                    rotationAngle = atan(-mobj->m_lastKnownVelocity[myTeamId].x.DoubleValue() / mobj->m_lastKnownVelocity[myTeamId].y.DoubleValue());
                    if (mobj->m_lastKnownVelocity[myTeamId].y < 0) rotationAngle += M_PI;
                } else {
                    // use current velocity for normal units
                    rotationAngle = atan(-wobj->m_vel.x.DoubleValue() / wobj->m_vel.y.DoubleValue());
                    if (wobj->m_vel.y < 0) rotationAngle += M_PI;
                }
            }
            
            // calculate distance to the camera for sorting
            Vector3<float> cameraToUnit = unitPos - m_globe3DCamera.m_cameraPos;
            float distance = cameraToUnit.Mag();
            
            UnitRenderInfo info;
            info.unit = wobj;
            info.worldPos = unitPos;
            info.distanceToCamera = distance;
            info.size = size;
            info.color = unitColor;
            info.image = unitImg;
            info.isMovingObject = isMovingObject;
            info.isAirUnit = isAirUnit;
            info.rotationAngle = rotationAngle;
            
            visibleUnits.PutData(info);
        }
    }
    
    // Sort units back to front for proper alpha blending
    for (int i = 0; i < visibleUnits.Size() - 1; i++) {
        for (int j = 0; j < visibleUnits.Size() - i - 1; j++) {
            if (visibleUnits[j].distanceToCamera < visibleUnits[j + 1].distanceToCamera) {
                UnitRenderInfo temp = visibleUnits[j];
                visibleUnits[j] = visibleUnits[j + 1];
                visibleUnits[j + 1] = temp;
            }
        }
    }
    
    
    // Second pass: render unit sprites using 3D batching
    for (int i = 0; i < visibleUnits.Size(); i++) {
        UnitRenderInfo& info = visibleUnits[i];
        Vector3<float> unitPos = info.worldPos;
        float size = info.size;
        Colour color = info.color;
        Image *unitImg = info.image;
        WorldObject* wobj = info.unit;
        
        //
        // render on globe surface
        
        Vector3<float> normal = unitPos;
        normal.Normalise();
        
        float elevation = 0.002f;
        
        unitPos += normal * elevation;
        
        if (info.isAirUnit) {
            g_renderer3d->RotatingSprite3D(unitImg, unitPos.x, unitPos.y, unitPos.z, 
                                        size * 2.0f, size * 2.0f, color, info.rotationAngle, BILLBOARD_SURFACE_ALIGNED);
        } else {
            g_renderer3d->StaticSprite3D(unitImg, unitPos.x, unitPos.y, unitPos.z, 
                                          size * 2.0f, size * 2.0f, color, BILLBOARD_SURFACE_ALIGNED);
        }
    }
    
    for (int i = 0; i < visibleUnits.Size(); i++) {
        UnitRenderInfo& info = visibleUnits[i];
        WorldObject* wobj = info.unit;
        Vector3<float> unitPos = info.worldPos;
        float unitSize = info.size;
        Colour teamColor = info.color;
        
        // check teamid to see if we should render the capacity indicators
        int myTeamId = g_app->GetWorld()->m_myTeamId;
        bool shouldRenderCapacity = (myTeamId == -1 || 
                                   wobj->m_teamId == myTeamId || 
                                   g_app->GetGame()->m_winner != -1);
        
        if (!shouldRenderCapacity) continue;
        
        Vector3<float> normal = unitPos;
        normal.Normalise();
        
        float elevation = 0.002f;
        
        unitPos += normal * elevation;
        
        // SILOS: Render small nuke capacity icons
        if (wobj->m_type == WorldObject::TypeSilo) {
            int numNukesInStore = wobj->m_states[0]->m_numTimesPermitted;
            int numNukesInQueue = wobj->m_actionQueue.Size();
            
            if (numNukesInStore > 0) {
                float nukeIconSize = unitSize * 0.35f;
                float spacing = nukeIconSize * 0.5f;
                
                Vector3<float> normal = unitPos;
                normal.Normalise();
                
                Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
                Vector3<float> tangent1;
                if (fabsf(normal.y) > 0.98f) {
                    tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
                } else {
                    tangent1 = globeUp ^ normal;
                    tangent1.Normalise();
                }
                Vector3<float> tangent2 = normal ^ tangent1;
                tangent2.Normalise();
                
                Vector3<float> rightOffset = tangent1 * (unitSize * 0.16f);   
                Vector3<float> downOffset = -tangent2 * (unitSize * 0.15f);    
                
                Vector3<float> startPos = unitPos - tangent2 * (unitSize * 0.9f);
                startPos -= tangent1 * (unitSize * 0.95f);
                startPos += rightOffset + downOffset;  
                
                for (int j = 0; j < numNukesInStore; ++j) {
                    Colour iconColor = teamColor;
                    iconColor.m_a = 150;
                    
                    if (j >= (numNukesInStore - numNukesInQueue)) {
                        iconColor.Set(128, 128, 128, 100);
                    }
                    
                    Vector3<float> iconPos = startPos + tangent1 * (j * spacing);
                    
                    Image *nuke = g_resource->GetImage("graphics/smallnuke.bmp");
                    g_renderer3d->StaticSprite3D(nuke, iconPos.x, iconPos.y, iconPos.z,
                                                nukeIconSize, nukeIconSize, iconColor, BILLBOARD_SURFACE_ALIGNED);
                }
            }
        }
        
        // SUBS: Render small nuke capacity icons
        else if (wobj->m_type == WorldObject::TypeSub) {
            int numNukesInStore = wobj->m_states[2]->m_numTimesPermitted;
            int numNukesInQueue = wobj->m_actionQueue.Size();
            
            if (numNukesInStore > 0) {
                float nukeIconSize = unitSize * 0.35f;
                float spacing = nukeIconSize * 0.5f;
                
                Vector3<float> normal = unitPos;
                normal.Normalise();
                
                Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
                Vector3<float> tangent1;
                if (fabsf(normal.y) > 0.98f) {
                    tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
                } else {
                    tangent1 = globeUp ^ normal;
                    tangent1.Normalise();
                }
                Vector3<float> tangent2 = normal ^ tangent1;
                tangent2.Normalise();
                
                Vector3<float> rightOffset = tangent1 * (unitSize * 0.16f);   
                Vector3<float> downOffset = -tangent2 * (unitSize * 0.15f);    
                
                Vector3<float> startPos = unitPos - tangent1 * (unitSize * 0.2f);
                startPos += rightOffset + downOffset; 
                
                for (int j = 0; j < numNukesInStore; ++j) {
                    Colour iconColor = teamColor;
                    iconColor.m_a = 150;
                    
                    if (j >= (numNukesInStore - numNukesInQueue)) {
                        iconColor.Set(128, 128, 128, 100);
                    }
                    
                    Vector3<float> iconPos = startPos - tangent1 * (j * spacing);
                    
                    Image *nuke = g_resource->GetImage("graphics/smallnuke.bmp");
                    g_renderer3d->StaticSprite3D(nuke, iconPos.x, iconPos.y, iconPos.z,
                                                nukeIconSize, nukeIconSize, iconColor, BILLBOARD_SURFACE_ALIGNED);
                }
            }
        }
        
        // BOMBERS: Render single nuke icon if carrying nuke
        else if (wobj->m_type == WorldObject::TypeBomber && wobj->m_states[1]->m_numTimesPermitted > 0) {
            float nukeIconSize = unitSize * 0.8f;  
            
            Colour iconColor = teamColor;
            iconColor.m_a = 150;
            
            float bomberRotation = 0.0f;
            
            for (int k = 0; k < visibleUnits.Size(); k++) {
                if (visibleUnits[k].unit == wobj) {
                    bomberRotation = visibleUnits[k].rotationAngle;
                    break;
                }
            }
            
            Image *nuke = g_resource->GetImage("graphics/smallnuke.bmp");
            g_renderer3d->RotatingSprite3D(nuke, unitPos.x, unitPos.y, unitPos.z,
                                        nukeIconSize, nukeIconSize, iconColor, bomberRotation, BILLBOARD_SURFACE_ALIGNED);
        }
        
        // AIRBASES & CARRIERS: Render fighter/bomber capacity icons
        else if (wobj->m_type == WorldObject::TypeAirBase || wobj->m_type == WorldObject::TypeCarrier) {
            int numInStore = wobj->m_states[wobj->m_currentState]->m_numTimesPermitted;
            int numInQueue = wobj->m_actionQueue.Size();
            
            if (numInStore > 0) {
                // Choose icon based on current state
                Image *iconImg = nullptr;
                if (wobj->m_currentState == 0) {
                    iconImg = g_resource->GetImage("graphics/smallfighter.bmp");
                } else {
                    iconImg = g_resource->GetImage("graphics/smallbomber.bmp");
                }
                
                if (iconImg) {
                    float iconSize = unitSize * 0.35f;
                    float spacing = iconSize * 0.9f;
                    
                    Vector3<float> normal = unitPos;
                    normal.Normalise();
                    
                    Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
                    Vector3<float> tangent1;
                    if (fabsf(normal.y) > 0.98f) {
                        tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
                    } else {
                        tangent1 = globeUp ^ normal;
                        tangent1.Normalise();
                    }
                    Vector3<float> tangent2 = normal ^ tangent1;
                    tangent2.Normalise();
                    
                    Vector3<float> rightOffset = tangent1 * (unitSize * 0.16f); 
                    Vector3<float> downOffset = -tangent2 * (unitSize * 0.15f);   
                    
                    Vector3<float> startPos = unitPos - tangent2 * (unitSize * 0.3f);
                    startPos += rightOffset + downOffset; 
                    
                    // For carriers, offset for predicted position
                    if (wobj->m_type == WorldObject::TypeCarrier) {
                        startPos -= tangent1 * (unitSize * 0.85f);
                    } else {
                        startPos -= tangent1 * (unitSize * 0.5f);
                    }
                    
                    for (int j = 0; j < numInStore; ++j) {
                        Colour iconColor = teamColor;
                        iconColor.m_a = 150;
                        
                        if (j >= (numInStore - numInQueue)) {
                            iconColor.Set(128, 128, 128, 100);
                        }
                        
                        Vector3<float> iconPos = startPos + tangent1 * (j * spacing);
                        
                        g_renderer3d->StaticSprite3D(iconImg, iconPos.x, iconPos.y, iconPos.z,
                                                     iconSize, iconSize, iconColor, BILLBOARD_SURFACE_ALIGNED);
                    }
                }
            }
        }
    }
}

//
// render unit trails on the globe

// changed to look different than 2d trails as i think it looks better

void GlobeRenderer::Render3DUnitTrails()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    if (g_preferences->GetInt(PREFS_GRAPHICS_TRAILS) != 1) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render trails for all moving objects
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // not moving, skip
            if (!wobj->IsMovingObject()) continue;
            
            // skip nukes
            if (wobj->m_type == WorldObject::TypeNuke) continue;
            
            MovingObject *mobj = (MovingObject*)wobj;
            
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            if (!shouldRender) continue;
            
            // check if history exists
            LList<Vector3<Fixed> *>& history = mobj->GetHistory();
            if (history.Size() == 0) continue;
            
            Team *team = g_app->GetWorld()->GetTeam(mobj->m_teamId);
            Colour colour;
            if (team) {
                colour = team->GetTeamColour();
            } else {
                colour = COLOUR_SPECIALOBJECTS;
            }
            
            // calculate trail length based on zoom 
            int maxSize = history.Size();
            int sizeCap = (int)(80 * 0.5f); 
            
            // account for increased sampling rate to maintain trail length
            sizeCap *= 4;
            
            sizeCap /= World::GetGameScale().DoubleValue();
            maxSize = (maxSize > sizeCap ? sizeCap : maxSize);
            
            // reduce the trail length for enemy units
            if (mobj->m_teamId != myTeamId &&
                myTeamId != -1 &&
                myTeamId < g_app->GetWorld()->m_teams.Size() &&
                g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned) {
                int enemyTrailLimit = 16;  // 4*4 for others
                maxSize = min(maxSize, enemyTrailLimit);
            }
            
            if (maxSize <= 0) continue;

            
            Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
            float predictedLongitude = (mobj->m_longitude + mobj->m_vel.x * predictionTime).DoubleValue();
            float predictedLatitude = (mobj->m_latitude + mobj->m_vel.y * predictionTime).DoubleValue();
            
            Vector3<float> currentPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
            
            Vector3<float> normal = currentPos;
            normal.Normalise();
            
            // unit trails render slightly below units to prevent z-fighting
            float elevation = 0.001f;
            
            currentPos += normal * elevation;
            Vector3<float> prevPos = currentPos;
            
            for (int j = 0; j < maxSize; ++j) {
                Vector3<Fixed> *historyPos = history[j];
                
                float thisLongitude = historyPos->x.DoubleValue();
                float thisLatitude = historyPos->y.DoubleValue();
                
                if (predictedLongitude < -170 && thisLongitude > 170) {
                    thisLongitude = -180 - (180 - thisLongitude);
                }
                if (predictedLongitude > 170 && thisLongitude < -170) {
                    thisLongitude = 180 + (180 - fabsf(thisLongitude));
                }
                
                Vector3<float> pos3D = ConvertLongLatTo3DPosition(thisLongitude, thisLatitude);
                
                Vector3<float> posNormal = pos3D;
                posNormal.Normalise();
                pos3D += posNormal * elevation;
                
                // apply 2D style alpha fading (newest = opaque, oldest = transparent)
                Colour segmentColour = colour;
                segmentColour.m_a = 255 - 255 * (float)j / (float)maxSize;
                
                g_renderer3d->Line3D(prevPos.x, prevPos.y, prevPos.z,
                                             pos3D.x, pos3D.y, pos3D.z, segmentColour);
                prevPos = pos3D;
            }
        }
    }
}

//
// render 3D nuke models 
//
// dedicated function for nuke 3D model rendering with proper direction calculation
//
void GlobeRenderer::Render3DNuke()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // handle nuke model rendering
    struct NukeRenderInfo {
        WorldObject* unit;
        Vector3<float> worldPos;
        float distanceToCamera;
        float size;
        Colour color;
        float rotationAngle;
    };
    
    DArray<NukeRenderInfo> visibleNukes;
    
    // First pass: collect visible nukes
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // only process nukes
            if (wobj->m_type != WorldObject::TypeNuke) continue;
            
            // check what teamid we have set, then we can decide
            // whether to render the unit or not
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            if (!shouldRender) continue;
            
            Nuke* nuke = (Nuke*)wobj;
            Vector3<float> unitPos = CalculateNuke3DPosition(nuke);
            
            // pass the image to the size multiplier
            float baseSize = wobj->GetSize3D().DoubleValue();
            float size = baseSize * 0.008f;
            
            size = fmaxf(size, 0.002f);  
            size = fminf(size, 0.04f);   
            
            Colour unitColor = White;
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            if (team) {
                unitColor = team->GetTeamColour();
            }
            
            // calculate distance to the camera for sorting
            Vector3<float> cameraToUnit = unitPos - m_globe3DCamera.m_cameraPos;
            float distance = cameraToUnit.Mag();
            
            NukeRenderInfo info;
            info.unit = wobj;
            info.worldPos = unitPos;
            info.distanceToCamera = distance;
            info.size = size;
            info.color = unitColor;
            info.rotationAngle = 0.0f; // not used for nukes, but kept for consistency
            
            visibleNukes.PutData(info);
        }
    }
    
    // Sort nukes back to front for proper alpha blending
    for (int i = 0; i < visibleNukes.Size() - 1; i++) {
        for (int j = 0; j < visibleNukes.Size() - i - 1; j++) {
            if (visibleNukes[j].distanceToCamera < visibleNukes[j + 1].distanceToCamera) {
                NukeRenderInfo temp = visibleNukes[j];
                visibleNukes[j] = visibleNukes[j + 1];
                visibleNukes[j + 1] = temp;
            }
        }
    }
    
    // Second pass: render nuke 3D models using 3D batching
    for (int i = 0; i < visibleNukes.Size(); i++) {
        NukeRenderInfo& info = visibleNukes[i];
        Vector3<float> unitPos = info.worldPos;
        float size = info.size;
        Colour color = info.color;
        WorldObject* wobj = info.unit;
        
        //
        // for nukes in 3D space, use the new 3D nuke model that faces forward
        // now we use the nukes movement trail history for direction calculation
        // this gives us the actual movement tangent, not the target direction
        // we basically made the nuke dumb, it doesnt know where its target is
        
        Vector3<float> direction(0, 0, 1);
        
        // get the nukes movement history (cast to MovingObject since that's where GetHistory() is defined)
        MovingObject* movingObj = (MovingObject*)wobj;
        LList<Vector3<Fixed> *>& history = movingObj->GetHistory();
        
        if (history.Size() > 1) {
            Nuke* nuke = (Nuke*)wobj;
            Vector3<float> currentPos = CalculateNuke3DPosition(nuke);
            
            //
            // Use history[1] instead of history[0] to avoid near-zero distance 
            // when a new trail segment was just created
            // this prevents the nuke from spazzing out when a new trail segment
            // is created. just use the older position instead of the current zero
            // history value
            
            Vector3<Fixed>* olderHistoryPos = history[1];
            Vector3<float> olderPos = CalculateHistoricalNuke3DPosition(nuke, *olderHistoryPos);
            
            // direction = where we are MOVING not going
            direction = currentPos - olderPos;
            direction.Normalise();
        }
        
        float nukeLength = size * 1.2f;  
        float nukeRadius = size * 0.24f;  
        g_renderer3d->Nuke3DModel(unitPos, direction, nukeLength, nukeRadius, color);
    }
}

//
// render 3D nuke trajectories and trails

void GlobeRenderer::Render3DNukeTrajectories()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    if (g_preferences->GetInt(PREFS_GRAPHICS_TRAILS) != 1) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render nuke trajectories for all nuke objects
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // only process nukes
            if (wobj->m_type != WorldObject::TypeNuke) continue;
            if (!wobj->IsMovingObject()) continue;
            
            MovingObject *mobj = (MovingObject*)wobj;
            Nuke* nuke = (Nuke*)wobj;
            
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            if (!shouldRender) continue;
            
            // check if history exists
            LList<Vector3<Fixed> *>& history = mobj->GetHistory();
            if (history.Size() == 0) continue;
            
            Team *team = g_app->GetWorld()->GetTeam(mobj->m_teamId);
            Colour colour;
            if (team) {
                colour = team->GetTeamColour();
            } else {
                colour = COLOUR_SPECIALOBJECTS;
            }
            
            // calculate trail length based on zoom 
            int maxSize = history.Size();
            int sizeCap = (int)(80 * 0.5f); 
            
            // account for increased sampling rate to maintain trail length
            sizeCap *= 4;
            
            sizeCap /= World::GetGameScale().DoubleValue();
            maxSize = (maxSize > sizeCap ? sizeCap : maxSize);
            
            // reduce the trail length for enemy units
            if (mobj->m_teamId != myTeamId &&
                myTeamId != -1 &&
                myTeamId < g_app->GetWorld()->m_teams.Size() &&
                g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned) {
                int enemyTrailLimit = 16;  // 4*4 for nukes
                maxSize = min(maxSize, enemyTrailLimit);
            }
            
            if (maxSize <= 0) continue;
            
            //
            // solid nuke trails, it looks alot better than segmented lines

            Vector3<float> currentPos = CalculateNuke3DPosition(nuke);
            
            int maxTrailLength = maxSize;  // Use full detail always
            maxTrailLength = fmaxf(4, fminf(maxTrailLength, 32));
            
            Vector3<float> prevPos = currentPos;
            
            for (int j = 0; j < maxTrailLength; ++j) {
                int actualHistoryIndex = (j * maxSize) / maxTrailLength;
                if (actualHistoryIndex >= history.Size()) break;
                
                Vector3<Fixed> *historyPos = history[actualHistoryIndex];
                
                // Calculate progress based on current nuke progress and history age
                Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
                Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
                Fixed remainingDistance = (target - pos).Mag();
                Fixed fractionDistance = 1 - remainingDistance / nuke->m_totalDistance;
                float currentProgress = fractionDistance.DoubleValue();
                
                // work backwards: j=0 is newest, j=maxTrailLength-1 is oldest
                float progressRange = currentProgress;
                float historicalProgress = currentProgress - (progressRange * (float)j / (float)(maxTrailLength - 1));
                historicalProgress = fmaxf(0.0f, fminf(1.0f, historicalProgress));
                
                Vector3<float> pos3D = CalculateHistoricalNuke3DPositionByAge(nuke, *historyPos, historicalProgress);
                
                Colour segmentColour = colour;
                segmentColour.m_a = 255 - 255 * (float)j / (float)maxTrailLength;
                
                g_renderer3d->Line3D(prevPos.x, prevPos.y, prevPos.z,
                                             pos3D.x, pos3D.y, pos3D.z, segmentColour);
                
                prevPos = pos3D;
            }
        }
    }
}

//
// gunfire trail rendering

// remains the same as 2d gunfire trails but gunfire can now shoot
// at nukes above the globe!

void GlobeRenderer::Render3DGunfire()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render gunfire for all objects
    for (int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i) {
        if (g_app->GetWorld()->m_gunfire.ValidIndex(i)) {
            GunFire *gunfire = g_app->GetWorld()->m_gunfire[i];
            
            bool shouldRender = (myTeamId == -1 ||
                               gunfire->m_teamId == myTeamId ||
                               gunfire->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            if (!shouldRender) continue;
            
            // check for gunfire history
            LList<Vector3<Fixed> *>& history = gunfire->GetHistory();
            if (history.Size() == 0) continue;
            
            // calculate gunfires 3D position 
            Vector3<float> gunfirePos3D = CalculateGunfire3DPosition(gunfire);
            
            Team *team = g_app->GetWorld()->GetTeam(gunfire->m_teamId);
            Colour colour;
            if (team) {
                colour = team->GetTeamColour();
            } else {
                colour = White;
            }
            
            // check if targeting a nuke
            WorldObject* targetObj = g_app->GetWorld()->GetWorldObject(gunfire->m_targetObjectId);
            bool targetingNuke = (targetObj && targetObj->m_type == WorldObject::TypeNuke);
            
            // Gunfire trails are shorter and brighter than unit trails
            int maxSize = min(history.Size(), 10);  // hard limit until i add scene batching
            float maxHistorySize = 10.0f;
            
            for (int j = 1; j < maxSize; ++j) {
                Vector3<Fixed> *lastHistoryPos = history[j-1];
                Vector3<Fixed> *thisHistoryPos = history[j];
                
                Vector3<float> lastPos3D, thisPos3D;
                
                //
                // seperate gunfire trails for nukes

                if (targetingNuke) {
                    
                    WorldObject* launcher = g_app->GetWorld()->GetWorldObject(gunfire->m_origin);
                    Vector3<float> launchPos;
                    if (launcher) {
                        launchPos = ConvertLongLatTo3DPosition(
                            launcher->m_longitude.DoubleValue(), 
                            launcher->m_latitude.DoubleValue()
                        );
                        Vector3<float> launchNormal = launchPos;
                        launchNormal.Normalise();
                        launchPos += launchNormal * 0.001f;
                    }
                    
                    Nuke* targetNuke = (Nuke*)targetObj;
                    Vector3<float> targetPos = CalculateNuke3DPosition(targetNuke);
                    
                    // calculate progress for each historical point
                    // this is an approximation

                    float totalDistance = gunfire->m_distanceToTarget.DoubleValue();
                    
                    Vector3<float> lastSurfacePos = ConvertLongLatTo3DPosition(
                        lastHistoryPos->x.DoubleValue(), lastHistoryPos->y.DoubleValue()
                    );
                    Vector3<float> thisSurfacePos = ConvertLongLatTo3DPosition(
                        thisHistoryPos->x.DoubleValue(), thisHistoryPos->y.DoubleValue()
                    );
                    
                    float lastDistanceToTarget = (targetPos - lastSurfacePos).Mag();
                    float thisDistanceToTarget = (targetPos - thisSurfacePos).Mag();
                    float totalFlightDistance = (targetPos - launchPos).Mag();
                    
                    float lastProgress = 1.0f - (lastDistanceToTarget / totalFlightDistance);
                    float thisProgress = 1.0f - (thisDistanceToTarget / totalFlightDistance);
                    
                    lastProgress = fmaxf(0.0f, fminf(1.0f, lastProgress));
                    thisProgress = fmaxf(0.0f, fminf(1.0f, thisProgress));
                    
                    lastPos3D = launchPos + (targetPos - launchPos) * lastProgress;
                    thisPos3D = launchPos + (targetPos - launchPos) * thisProgress;

                    //
                    // handle the rest

                } else {
                    float lastLongitude = lastHistoryPos->x.DoubleValue();
                    float lastLatitude = lastHistoryPos->y.DoubleValue();
                    float thisLongitude = thisHistoryPos->x.DoubleValue();
                    float thisLatitude = thisHistoryPos->y.DoubleValue();
                    
                    // longitude wrapping, same as 2D logic
                    if (lastLongitude < -170 && thisLongitude > 170) {
                        thisLongitude = -180 - (180 - thisLongitude);
                    }
                    if (lastLongitude > 170 && thisLongitude < -170) {
                        thisLongitude = 180 + (180 - fabsf(thisLongitude));
                    }
                    
                    // convert to 3D positions
                    lastPos3D = ConvertLongLatTo3DPosition(lastLongitude, lastLatitude);
                    thisPos3D = ConvertLongLatTo3DPosition(thisLongitude, thisLatitude);
                    
                    Vector3<float> lastNormal = lastPos3D;
                    lastNormal.Normalise();
                    lastPos3D += lastNormal * 0.001f; 
                    
                    Vector3<float> thisNormal = thisPos3D;
                    thisNormal.Normalise();
                    thisPos3D += thisNormal * 0.001f;
                }
                
                Colour segmentColour = colour;
                segmentColour.m_a = 255 - (255 * (float)j / maxHistorySize);
                
                g_renderer3d->Line3D(lastPos3D.x, lastPos3D.y, lastPos3D.z,
                                           thisPos3D.x, thisPos3D.y, thisPos3D.z, segmentColour);
            }
            
            if (maxSize > 0) {
                Vector3<Fixed> *lastHistoryPos = history[0];
                Vector3<float> lastPos3D;
                
                //
                // calculate 3D position for last historical point for nukes

                if (targetingNuke) {
                    WorldObject* launcher = g_app->GetWorld()->GetWorldObject(gunfire->m_origin);
                    Vector3<float> launchPos;
                    if (launcher) {
                        launchPos = ConvertLongLatTo3DPosition(
                            launcher->m_longitude.DoubleValue(), 
                            launcher->m_latitude.DoubleValue()
                        );
                        Vector3<float> launchNormal = launchPos;
                        launchNormal.Normalise();
                        launchPos += launchNormal * 0.001f;
                    }
                    
                    Nuke* targetNuke = (Nuke*)targetObj;
                    Vector3<float> targetPos = CalculateNuke3DPosition(targetNuke);
                    
                    Vector3<float> lastSurfacePos = ConvertLongLatTo3DPosition(
                        lastHistoryPos->x.DoubleValue(), lastHistoryPos->y.DoubleValue()
                    );
                    
                    float lastDistanceToTarget = (targetPos - lastSurfacePos).Mag();
                    float totalFlightDistance = (targetPos - launchPos).Mag();
                    float lastProgress = 1.0f - (lastDistanceToTarget / totalFlightDistance);
                    lastProgress = fmaxf(0.0f, fminf(1.0f, lastProgress));
                    
                    lastPos3D = launchPos + (targetPos - launchPos) * lastProgress;

                    //
                    // default gunfire handling

                } else {
                    Vector3<float> lastSurfacePos = ConvertLongLatTo3DPosition(
                        lastHistoryPos->x.DoubleValue(), lastHistoryPos->y.DoubleValue()
                    );
                    Vector3<float> lastNormal = lastSurfacePos;
                    lastNormal.Normalise();
                    lastPos3D = lastSurfacePos + lastNormal * 0.001f;
                }
                
                Colour currentColour = colour;
                currentColour.m_a = 255;
                
                g_renderer3d->Line3D(lastPos3D.x, lastPos3D.y, lastPos3D.z,
                                           gunfirePos3D.x, gunfirePos3D.y, gunfirePos3D.z, currentColour);
            }
        }
    }
}

//
// explosion rendering

// we now curve the explosion texture to follow the globe surface
// to prevent the texture from being stretched out in the x direction

void GlobeRenderer::Render3DExplosions()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    for (int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i) {
        if (g_app->GetWorld()->m_explosions.ValidIndex(i)) {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            
            bool shouldRender = (myTeamId == -1 ||
                               explosion->m_visible[myTeamId] ||
                               g_app->GetMapRenderer()->GetRenderEverything());
            
            if (!shouldRender) continue;
            
            float predictedIntensity = explosion->m_intensity.DoubleValue() - 0.2f *
                                     g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
            if (predictedIntensity <= 0) continue;
            
            float size = predictedIntensity / 20.0f;
            size /= g_app->GetWorld()->GetGameScale().DoubleValue();
            
            Vector3<float> explosionPos = ConvertLongLatTo3DPosition(
                explosion->m_longitude.DoubleValue(), 
                explosion->m_latitude.DoubleValue()
            );
            
            Image *explosionImg = g_resource->GetImage("graphics/explosion.bmp");
            if (!explosionImg) continue;
            
            Colour colour;
            if (explosion->m_underWater) {
                colour = Colour(50, 50, 100, 150);
            } else {
                colour = Colour(255, 255, 255, 230);
            }
            
            if (explosion->m_initialIntensity != Fixed::FromDouble(-1.0f)) {
                float fraction = (explosion->m_intensity / explosion->m_initialIntensity).DoubleValue();
                if (fraction < 0.7f) fraction = 0.7f;
                colour.m_a *= fraction;
            }
            
            // convert size to 3D world scale
            float explosionSize = size * 0.015f;  
            explosionSize = fmaxf(explosionSize, 0.01f); 
            explosionSize = fminf(explosionSize, 0.2f);    
            
            //
            // for large explosions, create curved geometry to follow globe surface

            if (explosionSize > 0.05f) {
                
                
                Vector3<float> normal = explosionPos;
                normal.Normalise();
                
                // Offset slightly above surface
                explosionPos += normal * 0.002f;

                g_renderer3d->StaticSprite3D(explosionImg, explosionPos.x, explosionPos.y, explosionPos.z,
                                             explosionSize * 2.0f, explosionSize * 2.0f, colour, BILLBOARD_SURFACE_ALIGNED);
                
            } else {

                //
                // smaller explosions such as gunfire hits dont need to be curved

                Vector3<float> normal = explosionPos;
                normal.Normalise();
                
                explosionPos += normal * 0.002f;
                
                g_renderer3d->StaticSprite3D(explosionImg, explosionPos.x, explosionPos.y, explosionPos.z,
                                             explosionSize * 2.0f, explosionSize * 2.0f, colour, BILLBOARD_SURFACE_ALIGNED);
            }
        }
    }
}

//
// nuke symbol rendering
//
// render nukesymbol.bmp for world objects and cities that have nukes in flight or queue

void GlobeRenderer::Render3DNukeSymbols()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    Image *nukeSymbolImg = g_resource->GetImage("graphics/nukesymbol.bmp");
    if (!nukeSymbolImg) return;
    
    //
    // render nuke symbols on world objects

    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
        // skip objects without nukes
        if (!wobj->m_numNukesInFlight && !wobj->m_numNukesInQueue) continue;
                        
            // convert object position to 3D globe coordinates
            Vector3<float> objectPos = ConvertLongLatTo3DPosition(
                wobj->m_longitude.DoubleValue(), 
                wobj->m_latitude.DoubleValue()
            );
            
            //
            // red with alpha based on nuke status

            Colour col(255, 0, 0, 255);
            if (!wobj->m_numNukesInFlight) col.m_a = 100; // dimmed for queued nukes
            

            float iconSize = 6.0f;
            if (wobj->m_numNukesInFlight) {
                iconSize += sinf(g_gameTime * 10) * 0.2f;
            }
            
            // convert 2D size to 3D world scale
            float nukeSymbolSize = iconSize * 0.0075f;
            nukeSymbolSize = fmaxf(nukeSymbolSize, 0.005f);
            nukeSymbolSize = fminf(nukeSymbolSize, 0.065f);
            
            Vector3<float> normal = objectPos;
            normal.Normalise();
            
            g_renderer3d->StaticSprite3D(nukeSymbolImg, objectPos.x, objectPos.y, objectPos.z,
                                         nukeSymbolSize * 2.0f, nukeSymbolSize * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
        }
    }
    
    //
    // render nuke symbols on cities

    for (int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i) {
        City *city = g_app->GetWorld()->m_cities.GetData(i);
        
        // skip cities without nukes
        if (!city->m_numNukesInFlight && !city->m_numNukesInQueue) continue;
        
        // convert city position to 3D globe coordinates
        Vector3<float> cityPos = ConvertLongLatTo3DPosition(
            city->m_longitude.DoubleValue(), 
            city->m_latitude.DoubleValue()
        );
        
        Colour col(255, 0, 0, 255);
        if (!city->m_numNukesInFlight) col.m_a = 100; 
        
        float iconSize = 3.0f;
        if (city->m_numNukesInFlight) {
            iconSize += sinf(g_gameTime * 10) * 0.2f; 
        }
        
        float nukeSymbolSize = iconSize * 0.0075f;  
        nukeSymbolSize = fmaxf(nukeSymbolSize, 0.005f);
        nukeSymbolSize = fminf(nukeSymbolSize, 0.03f);
        
        Vector3<float> normal = cityPos;
        normal.Normalise();
        
        g_renderer3d->StaticSprite3D(nukeSymbolImg, cityPos.x, cityPos.y, cityPos.z,
                                     nukeSymbolSize * 2.0f, nukeSymbolSize * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
    }
}

//
// orders overlay rendering
//
// render cursor_target.bmp at order destinations and curved order lines along globe surface
// follows the same visibility and animation logic as 2D orders overlay

void GlobeRenderer::Render3DWorldObjectTargets()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    // check if orders overlay is enabled
    if (!g_app->GetMapRenderer()->m_showOrders) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    Image *targetImg = g_resource->GetImage("graphics/cursor_target.bmp");
    if (!targetImg) return;
    
    //
    // render targets and order lines for all world objects

    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // visibility check
            if (!(wobj->m_teamId == myTeamId || myTeamId == -1)) continue;
            
            // calculate predicted unit position
            Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
            Fixed predictedLongitude = wobj->m_longitude + wobj->m_vel.x * predictionTime;
            Fixed predictedLatitude = wobj->m_latitude + wobj->m_vel.y * predictionTime;
            
            Vector3<float> unitPos = ConvertLongLatTo3DPosition(
                predictedLongitude.DoubleValue(), 
                predictedLatitude.DoubleValue()
            );
            
            // add small elevation for visibility
            Vector3<float> unitNormal = unitPos;
            unitNormal.Normalise();
            unitPos += unitNormal * 0.002f;
            
            //
            // render combat target (red)

            int targetObjectId = wobj->GetTargetObjectId();
            WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(targetObjectId);
            if (targetObject) {
                
                // calculate predicted target position
                Fixed targetPredictedLong = targetObject->m_longitude + targetObject->m_vel.x * predictionTime;
                Fixed targetPredictedLat = targetObject->m_latitude + targetObject->m_vel.y * predictionTime;
                
                Vector3<float> targetPos = ConvertLongLatTo3DPosition(
                    targetPredictedLong.DoubleValue(), 
                    targetPredictedLat.DoubleValue()
                );
                
                // position target icon at the correct elevation
                Vector3<float> targetNormal = targetPos;
                targetNormal.Normalise();
                float targetElevation = CalculateUnitElevation(targetObject);
                targetPos += targetNormal * targetElevation;
                
                // render animated target cursor (red)
                Colour actionCursorCol(255, 0, 0, 150);
                float actionCursorSize = 4.0f * 0.0075f; 
                float actionCursorAngle = g_gameTime * -1.0f; 
                
                g_renderer3d->RotatingSprite3D(targetImg, targetPos.x, targetPos.y, targetPos.z,
                                             actionCursorSize * 2.0f, actionCursorSize * 2.0f, actionCursorCol, BILLBOARD_SURFACE_ALIGNED);
                
                // render curved order line 
                Render3DActionLine(unitPos, targetPos, actionCursorCol, true);
            }
            
            //
            // render movement target (blue)

            if (!targetObject && wobj->IsMovingObject()) {
                MovingObject *mobj = (MovingObject*)wobj;
                float actionCursorLongitude = mobj->m_targetLongitude.DoubleValue();
                float actionCursorLatitude = mobj->m_targetLatitude.DoubleValue();
                
                // handle fleet targets
                if (mobj->m_fleetId != -1) {
                    Fleet *fleet = g_app->GetWorld()->GetTeam(mobj->m_teamId)->GetFleet(mobj->m_fleetId);
                    if (fleet && 
                        fleet->m_targetLongitude != 0 && 
                        fleet->m_targetLatitude != 0) {
                        actionCursorLongitude = fleet->m_targetLongitude.DoubleValue();
                        actionCursorLatitude = fleet->m_targetLatitude.DoubleValue();                    
                    }
                }
                
                if (actionCursorLongitude != 0.0f || actionCursorLatitude != 0.0f) {
                    Vector3<float> targetPos = ConvertLongLatTo3DPosition(actionCursorLongitude, actionCursorLatitude);
                    
                    // position target icon on the globe surface 
                    Vector3<float> targetNormal = targetPos;
                    targetNormal.Normalise();
                    targetPos += targetNormal * 0.002f;
                    
                    // determine color
                    Colour actionCursorCol(0, 0, 255, 150);
                    if (mobj->m_type == WorldObject::TypeNuke) {
                        actionCursorCol.Set(255, 0, 0, 150);
                    }
                    if (mobj->m_type == WorldObject::TypeBomber && mobj->m_currentState == 1) {
                        actionCursorCol.Set(255, 0, 0, 150);
                    }
                    
                    float actionCursorSize = 4.0f * 0.0075f; 
                    
                    g_renderer3d->RotatingSprite3D(targetImg, targetPos.x, targetPos.y, targetPos.z,
                                                 actionCursorSize * 2.0f, actionCursorSize * 2.0f, actionCursorCol, BILLBOARD_SURFACE_ALIGNED);
                    
                    // render curved order line, but not for nukes in flight as they look odd in 3D
                    if (mobj->m_type != WorldObject::TypeNuke) {
                        Render3DActionLine(unitPos, targetPos, actionCursorCol, true);
                    }
                }
            }
        }
    }
}

//
// render curved action line along globe surface with animation
// uses same great circle math as nuke trajectories to avoid coordinate wrapping/pole issues

void GlobeRenderer::Render3DActionLine(const Vector3<float>& fromPos, const Vector3<float>& toPos, const Colour& col, bool animate)
{
    if (!Is3DGlobeModeEnabled()) return;
    
    // convert 3D positions back to longitude/latitude for proper great circle calculation
    Vector3<float> fromNorm = fromPos;
    fromNorm.Normalise();
    Vector3<float> toNorm = toPos;  
    toNorm.Normalise();
    
    // convert to longitude/latitude, reverse of ConvertLongLatTo3DPosition()
    float fromLat = asinf(fromNorm.y) * 180.0f / M_PI;
    float fromLon = atan2f(fromNorm.x, fromNorm.z) * 180.0f / M_PI;
    float toLat = asinf(toNorm.y) * 180.0f / M_PI;
    float toLon = atan2f(toNorm.x, toNorm.z) * 180.0f / M_PI;
    
    // calculate great circle distance for segment count
    float latDiff = (toLat - fromLat) * M_PI / 180.0f;
    float lonDiff = (toLon - fromLon) * M_PI / 180.0f;
    
    // handle longitude wrapping for shortest path
    if (lonDiff > M_PI) {
        toLon -= 360.0f;
        lonDiff = (toLon - fromLon) * M_PI / 180.0f;
    } else if (lonDiff < -M_PI) {
        toLon += 360.0f;
        lonDiff = (toLon - fromLon) * M_PI / 180.0f;
    }
    
    float fromLatRad = fromLat * M_PI / 180.0f;
    float toLatRad = toLat * M_PI / 180.0f;
    float a = sinf(fromLatRad) * sinf(toLatRad) + cosf(fromLatRad) * cosf(toLatRad) * cosf(lonDiff);
    a = fmaxf(-1.0f, fminf(1.0f, a));
    float distance = acosf(a);
    
    if (distance < 0.001f) return; 
    
    // create segments using great circle interpolation
    int numSegments = (int)(distance * 50.0f); 
    numSegments = fmaxf(8, fminf(numSegments, 64)); 
    
    Vector3<float> prevPos = fromPos;
    
    for (int i = 1; i <= numSegments; ++i) {
        float progress = (float)i / (float)numSegments;
        
        Vector3<float> currentPos = CalculateGreatCirclePosition(fromLon, fromLat, toLon, toLat, progress);
        
        // elevate below units to prevent z-fighting
        Vector3<float> normal = currentPos;
        normal.Normalise();
        currentPos = normal * 1.001f;
        
        // render line segment
        g_renderer3d->Line3D(prevPos.x, prevPos.y, prevPos.z,
                                   currentPos.x, currentPos.y, currentPos.z, col);
        
        prevPos = currentPos;
    }
    
    // render animated segments using same great circle math
    if (animate) {
        float factor1 = fmodf(GetHighResTime(), 1.0f);
        float factor2 = fmodf(GetHighResTime(), 1.0f) + 0.2f;
        factor1 = fmaxf(0.0f, fminf(1.0f, factor1));
        factor2 = fmaxf(0.0f, fminf(1.0f, factor2));
        
        // calculate animated positions using great circle math
        Vector3<float> animPos1 = CalculateGreatCirclePosition(fromLon, fromLat, toLon, toLat, factor1);
        Vector3<float> animPos2 = CalculateGreatCirclePosition(fromLon, fromLat, toLon, toLat, factor2);
        
        Vector3<float> animNorm1 = animPos1;
        animNorm1.Normalise();
        animPos1 = animNorm1 * 1.001f;
        
        Vector3<float> animNorm2 = animPos2;
        animNorm2.Normalise();
        animPos2 = animNorm2 * 1.001f;
        
        // render bright animated segment
        Colour brightCol = col;
        brightCol.m_a = 255;
        g_renderer3d->Line3D(animPos1.x, animPos1.y, animPos1.z,
                                   animPos2.x, animPos2.y, animPos2.z, brightCol);
    }
}

//
// render whiteboard drawings on the 3D globe surface
// follows same logic as 2D whiteboard but converts coordinates to 3D positions

void GlobeRenderer::Render3DWhiteBoard()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    if (!g_app->GetMapRenderer()->GetShowWhiteBoard() && !g_app->GetMapRenderer()->GetEditWhiteBoard()) {
        return;
    }

    // get team for whiteboard viewing, use perspective team if set. otherwise use myTeam
    Team *effectiveTeam = g_app->GetMapRenderer()->GetEffectiveWhiteBoardTeam();
    if (!effectiveTeam) {
        return;
    }

    int sizeteams = g_app->GetWorld()->m_teams.Size();
    for (int i = 0; i < sizeteams; ++i) {
        Team *team = g_app->GetWorld()->m_teams[i];

        if ((g_app->GetMapRenderer()->ShowAllWhiteBoards() && g_app->GetWorld()->IsFriend(effectiveTeam->m_teamId, team->m_teamId)) || 
            effectiveTeam->m_teamId == team->m_teamId) {
            
            WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[team->m_teamId];
            
            Colour colourBoard = team->GetTeamColour();
            
            // create line strips
            const LList<WhiteBoardPoint *> *points = whiteBoard->GetListPoints();
            int sizePoints = points->Size();

            if (sizePoints > 0) {
                bool lineStripActive = false;
                WhiteBoardPoint *prevPt = nullptr;
                
                for (int j = 0; j < sizePoints; j++) {
                    WhiteBoardPoint *pt = points->GetData(j);

                    if (j == 0 || pt->m_startPoint) {
                        if (lineStripActive) {
                            g_renderer3d->EndLineStrip3D();
                        }
                        
                        // start new line strip
                        g_renderer3d->BeginLineStrip3D(colourBoard);
                        lineStripActive = true;
                        
                        // add the first point of the new line
                        Vector3<float> pos3D = ConvertLongLatTo3DPosition(pt->m_longitude, pt->m_latitude);
                        Vector3<float> normal = pos3D;
                        normal.Normalise();
                        pos3D += normal * 0.003f;  // raise whiteboard above globe surface
                        
                        g_renderer3d->LineStripVertex3D(pos3D.x, pos3D.y, pos3D.z);
                        
                    } else if (prevPt && lineStripActive) {
                        // create smooth line segment between previous and current point
                        float fromLon = prevPt->m_longitude;
                        float fromLat = prevPt->m_latitude;
                        float toLon = pt->m_longitude;
                        float toLat = pt->m_latitude;
                        
                        // calculate distance and determine segment count
                        float latDiff = (toLat - fromLat) * M_PI / 180.0f;
                        float lonDiff = (toLon - fromLon) * M_PI / 180.0f;
                        
                        // handle longitude wrapping
                        if (lonDiff > M_PI) {
                            toLon -= 360.0f;
                            lonDiff = (toLon - fromLon) * M_PI / 180.0f;
                        } else if (lonDiff < -M_PI) {
                            toLon += 360.0f;
                            lonDiff = (toLon - fromLon) * M_PI / 180.0f;
                        }
                        
                        float fromLatRad = fromLat * M_PI / 180.0f;
                        float toLatRad = toLat * M_PI / 180.0f;
                        float a = sinf(fromLatRad) * sinf(toLatRad) + cosf(fromLatRad) * cosf(toLatRad) * cosf(lonDiff);
                        a = fmaxf(-1.0f, fminf(1.0f, a));
                        float distance = acosf(a);
                        
                        // create segments for smooth lines
                        int numSegments = (int)(distance * 40.0f);
                        numSegments = fmaxf(2, fminf(numSegments, 32));  
                        
                        // add intermediate points using great circle interpolation
                        for (int seg = 1; seg <= numSegments; ++seg) {
                            float progress = (float)seg / (float)numSegments;
                            Vector3<float> interpPos = CalculateGreatCirclePosition(fromLon, fromLat, toLon, toLat, progress);
                            
                            Vector3<float> normal = interpPos;
                            normal.Normalise();
                            interpPos += normal * 0.003f;
                            
                            g_renderer3d->LineStripVertex3D(interpPos.x, interpPos.y, interpPos.z);
                        }
                    }
                    
                    prevPt = pt;
                }
                
                if (lineStripActive) {
                    g_renderer3d->EndLineStrip3D();
                }
            }
        }
    }
}

//
// render population density on the 3D globe surface
// follows same logic as 2D population density but converts coordinates to 3D positions

void GlobeRenderer::Render3DPopulationDensity()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    if (!g_app->GetMapRenderer()->m_showPopulation) return;
    
    Image *populationImg = g_resource->GetImage("graphics/population.bmp");
    if (!populationImg) return;
    
    // render population density for all cities
    for (int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i) {
        if (g_app->GetWorld()->m_cities.ValidIndex(i)) {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if (city->m_teamId != -1) {
                // do the deed first
                Vector3<float> cityPos = ConvertLongLatTo3DPosition(
                    city->m_longitude.DoubleValue(), 
                    city->m_latitude.DoubleValue()
                );
                
                float size2D = sqrtf(sqrtf((float)city->m_population)) / 2.0f;
                
                // convert 2D world units (degrees) to 3D world units on globe surface
                // 1 degree ≈ π/180 radians on a sphere with radius 1.0
                float populationSize = size2D * (M_PI / 180.0f);
                populationSize = fmaxf(populationSize, 0.008f);
                populationSize = fminf(populationSize, 0.2f);
                
                Colour col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                col.m_a = 255.0f * fminf(1.0f, city->m_population / 10000000.0f);
                
                Vector3<float> normal = cityPos;
                normal.Normalise();
                cityPos += normal * 0.003f;
                
                g_renderer3d->StaticSprite3D(populationImg, cityPos.x, cityPos.y, cityPos.z,
                                            populationSize * 2.0f, populationSize * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
            }
        }
    }
}

//
// render nuke units overlay on the 3D globe
// again the functionality is idential to 2d but converted to 3d coordinates

void GlobeRenderer::Render3DNukeHighlights()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    if (!g_app->GetMapRenderer()->m_showNukeUnits) return;
    
    Team *team = g_app->GetWorld()->GetMyTeam();
    if (!team) return;
    
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if (obj->m_teamId == team->m_teamId) {
                int nukeCount = 0;

                switch (obj->m_type) {
                    case WorldObject::TypeSilo:
                        nukeCount = obj->m_states[0]->m_numTimesPermitted;
                        break;

                    case WorldObject::TypeSub:
                        nukeCount = obj->m_states[2]->m_numTimesPermitted;
                        break;

                    case WorldObject::TypeBomber:
                        nukeCount = obj->m_states[1]->m_numTimesPermitted;
                        break;

                    case WorldObject::TypeAirBase:
                    case WorldObject::TypeCarrier:
                        if (obj->m_nukeSupply > 0) {
                            nukeCount = obj->m_states[1]->m_numTimesPermitted;
                        }
                        break;
                }

                if (obj->UsingNukes()) {
                    nukeCount -= obj->m_actionQueue.Size();
                }

                if (nukeCount > 0) {
                    Render3DUnitHighlight(obj->m_objectId);
                }
            }
        }
    }
}

//
// render 3D unit highlight
// equivalent of the 2D RenderUnitHighlight but adapted for 3D globe

void GlobeRenderer::Render3DUnitHighlight(int objectId)
{
    if (!Is3DGlobeModeEnabled()) return;
    
    WorldObject *obj = g_app->GetWorld()->GetWorldObject(objectId);
    if (!obj) return;
    
    // calculate predicted position, same as 2D version
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    Fixed predictedLongitude = obj->m_longitude + obj->m_vel.x * predictionTime;
    Fixed predictedLatitude = obj->m_latitude + obj->m_vel.y * predictionTime;
    
    // perform the magic
    Vector3<float> unitPos = ConvertLongLatTo3DPosition(
        predictedLongitude.DoubleValue(), 
        predictedLatitude.DoubleValue()
    );
    
    // position above globe surface
    Vector3<float> normal = unitPos;
    normal.Normalise();
    unitPos += normal * 0.001f;
    
    float size2D = 5.0f;
    float size = size2D * (M_PI / 180.0f);
    size = fmaxf(size, 0.01f); 
    size = fminf(size, 0.2f); 
    
    // same color and animation as 2D version
    Colour col = g_app->GetWorld()->GetTeam(obj->m_teamId)->GetTeamColour();
    col.m_a = 255;
    if (fmodf(g_gameTime * 2, 2.0f) < 1.0f) col.m_a = 100;
    
    Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
    Vector3<float> tangent1;
    if (fabsf(normal.y) > 0.98f) {
        tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
    } else {
        tangent1 = globeUp ^ normal;
        tangent1.Normalise();
    }
    Vector3<float> tangent2 = normal ^ tangent1;
    tangent2.Normalise();
    
    float halfSize = size * 0.5f;
    float extend = size * 0.5f;
    
    //
    // render crosshair lines extending outward from center square

    // left line
    Vector3<float> leftStart = unitPos - tangent1 * (halfSize + extend);
    Vector3<float> leftEnd = unitPos - tangent1 * halfSize;
    g_renderer3d->Line3D(leftStart.x, leftStart.y, leftStart.z,
                               leftEnd.x, leftEnd.y, leftEnd.z, col);
    
    // right line  
    Vector3<float> rightStart = unitPos + tangent1 * (halfSize + extend);
    Vector3<float> rightEnd = unitPos + tangent1 * halfSize;
    g_renderer3d->Line3D(rightStart.x, rightStart.y, rightStart.z,
                               rightEnd.x, rightEnd.y, rightEnd.z, col);
    
    // top line
    Vector3<float> topStart = unitPos + tangent2 * (halfSize + extend);
    Vector3<float> topEnd = unitPos + tangent2 * halfSize;
    g_renderer3d->Line3D(topStart.x, topStart.y, topStart.z,
                               topEnd.x, topEnd.y, topEnd.z, col);
    
    // bottom line
    Vector3<float> bottomStart = unitPos - tangent2 * (halfSize + extend);
    Vector3<float> bottomEnd = unitPos - tangent2 * halfSize;
    g_renderer3d->Line3D(bottomStart.x, bottomStart.y, bottomStart.z,
                               bottomEnd.x, bottomEnd.y, bottomEnd.z, col);
    
    // render central square using 4 line segments
    Vector3<float> topLeft = unitPos - tangent1 * halfSize + tangent2 * halfSize;
    Vector3<float> topRight = unitPos + tangent1 * halfSize + tangent2 * halfSize;
    Vector3<float> bottomRight = unitPos + tangent1 * halfSize - tangent2 * halfSize;
    Vector3<float> bottomLeft = unitPos - tangent1 * halfSize - tangent2 * halfSize;

    // render it
    
    g_renderer3d->Line3D(topLeft.x, topLeft.y, topLeft.z,
                               topRight.x, topRight.y, topRight.z, col);
    g_renderer3d->Line3D(topRight.x, topRight.y, topRight.z,
                               bottomRight.x, bottomRight.y, bottomRight.z, col);
    g_renderer3d->Line3D(bottomRight.x, bottomRight.y, bottomRight.z,
                               bottomLeft.x, bottomLeft.y, bottomLeft.z, col);
    g_renderer3d->Line3D(bottomLeft.x, bottomLeft.y, bottomLeft.z,
                               topLeft.x, topLeft.y, topLeft.z, col);
}

//
// animated texture rendering

// curve the textures to follow the globe surface
// to prevent the texture from being stretched out in the x direction

void GlobeRenderer::Render3DAnimations()
{
    if (!Is3DGlobeModeEnabled()) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render all animations
    for (int i = 0; i < m_animations.Size(); ++i) {
        if (m_animations.ValidIndex(i)) {
            AnimatedIcon *anim = m_animations[i];
            
            // check if animation should be removed
            bool shouldRemove = false;
            
            if (anim->m_animationType == g_app->GetMapRenderer()->AnimationTypeNukePointer) {
                NukePointer *nukePtr = (NukePointer*)anim;
                WorldObject *targetObj = g_app->GetWorld()->GetWorldObject(nukePtr->m_targetId);
                
                if (!targetObj) {
                    shouldRemove = true;
                } else {
                    // decrease lifetime when seen
                    if (nukePtr->m_seen) {
                        nukePtr->m_lifeTime -= g_advanceTime;
                        nukePtr->m_lifeTime = max(nukePtr->m_lifeTime, 0.0f);
                        
                        if (nukePtr->m_lifeTime <= 0.0f) {
                            shouldRemove = true;
                        }
                    }
                    
                    // mark as seen if on screen
                    bool onScreen = g_app->GetMapRenderer()->IsOnScreen(targetObj->m_longitude.DoubleValue(), targetObj->m_latitude.DoubleValue());
                    if (onScreen && !g_app->m_hidden) {
                        nukePtr->m_seen = true;
                    }
                }
            }
            
            // remove expired animations
            if (shouldRemove) {
                m_animations.RemoveData(i);
                delete anim;
                continue;
            }
            
            bool shouldRender = true;
            if (anim->m_animationType == g_app->GetMapRenderer()->AnimationTypeSonarPing) {
                SonarPing *ping = (SonarPing*)anim;
                shouldRender = (myTeamId == -1 || ping->m_visible[myTeamId]);
            } else if (anim->m_animationType == g_app->GetMapRenderer()->AnimationTypeNukePointer) {
                NukePointer *nukePtr = (NukePointer*)anim;
                WorldObject *targetObj = g_app->GetWorld()->GetWorldObject(nukePtr->m_targetId);
                if (targetObj) {
                    shouldRender = true;
                } else {
                    shouldRender = false;
                }
            }
            
            if (!shouldRender) continue;
            
            if (anim->m_animationType == g_app->GetMapRenderer()->AnimationTypeSonarPing) {
                
                SonarPing *ping = (SonarPing*)anim;
                
                // calculate ping parameters, same as 2D logic
                float timePassed = GetHighResTime() - ping->m_startTime;
                float pingSize = timePassed * 2.5f;
                if (pingSize > 5.0f) continue;  // skip expired pings
                
                Vector3<float> pingPos = ConvertLongLatTo3DPosition(
                    ping->m_longitude, 
                    ping->m_latitude
                );
                
                int alpha = 255 - 255 * (pingSize / 5.0f);
                alpha *= 0.5f;
                Colour colour(255, 255, 255, alpha);
                
                Vector3<float> normal = pingPos;
                normal.Normalise();
                
                Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
                Vector3<float> tangent1;
                if (fabsf(normal.y) > 0.98f) {
                    tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
                } else {
                    tangent1 = globeUp ^ normal;
                    tangent1.Normalise();
                }
                Vector3<float> tangent2 = normal ^ tangent1;
                tangent2.Normalise();
                
                // custom ping size on the 3d globe
                float ping3DSize = pingSize * 0.015f; 
                
                //
                // curve the sonar ping to globe surface
                    
                int numSegments = 32; 
                    
                //
                // render single circle instead of inner and outer
                // it looked better with two circles when the fog
                // was broken :)

                Vector3<float> prevPoint;
                for (int seg = 0; seg <= numSegments; ++seg) {
                    float angle = 2.0f * M_PI * seg / numSegments;
                    Vector3<float> offset = tangent1 * sinf(angle) + tangent2 * cosf(angle);
                    Vector3<float> point = pingPos + offset * ping3DSize;
                    point.Normalise();
                    point = point * 1.001f;
                    
                    if (seg > 0) {
                        g_renderer3d->Line3D(prevPoint.x, prevPoint.y, prevPoint.z,
                                                   point.x, point.y, point.z, colour);
                    }
                    prevPoint = point;
                }                             
            } else if (anim->m_animationType == g_app->GetMapRenderer()->AnimationTypeNukePointer) {
                
                NukePointer *nukePtr = (NukePointer*)anim;
                WorldObject *targetObj = g_app->GetWorld()->GetWorldObject(nukePtr->m_targetId);
                
                if (targetObj) {
                    // render yellow static nukesymbol.bmp at target location
                    Vector3<float> targetPos = ConvertLongLatTo3DPosition(
                        targetObj->m_longitude.DoubleValue(), 
                        targetObj->m_latitude.DoubleValue()
                    );
                    
                    // position above surface like other effects
                    Vector3<float> normal = targetPos;
                    normal.Normalise();
                    targetPos += normal * 0.002f; // same as other surface elements
                    
                    Image *nukeSymbolImg = g_resource->GetImage("graphics/nukesymbol.bmp");
                    if (nukeSymbolImg) {
                        int transparency = 255;
                        
                        // yellow color for launch detection
                        Colour col(255, 255, 0, transparency);
                        
                        float iconSize = 6.0f;
                        
                        // convert 2D size to 3D world scale
                        float nukeSymbolSize = iconSize * 0.0075f;  
                        nukeSymbolSize = fmaxf(nukeSymbolSize, 0.005f);
                        nukeSymbolSize = fminf(nukeSymbolSize, 0.065f);
                        
                        g_renderer3d->StaticSprite3D(nukeSymbolImg, targetPos.x, targetPos.y, targetPos.z,
                                                     nukeSymbolSize * 2.0f, nukeSymbolSize * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
                    }
                }
            }
        }
    }
}

//
// Render Santa on the 3D globe
// identical logic to map renderer but adapted for 3D coordinates

void GlobeRenderer::Render3DSanta()
{
#ifdef ENABLE_SANTA_EASTEREGG
	if ( !Is3DGlobeModeEnabled() ) return;
	
	if ( g_app->GetWorld()->m_santaAlive )
	{
        
		Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
		Fixed currentTime = g_app->GetWorld()->m_theDate.m_theDate + predictionTime;
		if ( currentTime > g_app->GetWorld()->m_santaRouteTotal )
		{
			while ( currentTime > g_app->GetWorld()->m_santaRouteTotal )
			{
				currentTime -= g_app->GetWorld()->m_santaRouteTotal;
			}
		}
        
        while ( g_app->GetWorld()->m_santaRouteLength.ValidIndex( g_app->GetWorld()->m_santaCurrent ) &&
                g_app->GetWorld()->m_santaRouteLength[ g_app->GetWorld()->m_santaCurrent ] < currentTime )
        {
            ++g_app->GetWorld()->m_santaCurrent;
		}

        float size = 3.0f;
		float x;
		float y;
		float thisSize = size*2;

        if( g_app->GetWorld()->m_santaCurrent >= g_app->GetWorld()->m_santaRoute.Size() ) 
        {
            g_app->GetWorld()->m_santaAlive = false;
            return;
        }
        
		//if index is odd then we are inside a city so don't render santa
		if ( g_app->GetWorld()->m_santaCurrent % 2 == 0 )
		{
			City *cityStart;
			City *cityEnd;

			int index = g_app->GetWorld()->m_santaCurrent / 2;
			cityStart = g_app->GetWorld()->m_santaRoute.GetData( index );
			if ( index == g_app->GetWorld()->m_santaRoute.Size() - 1 )
			{
				cityEnd =  g_app->GetWorld()->m_santaRoute.GetData( 0 );
			}
			else
			{
				cityEnd =  g_app->GetWorld()->m_santaRoute.GetData( index + 1 );
			}

			Fixed prevTime;
			if ( g_app->GetWorld()->m_santaCurrent == 0 )
			{
				prevTime = 0;
			}
			else
			{
				prevTime = g_app->GetWorld()->m_santaRouteLength.GetData( g_app->GetWorld()->m_santaCurrent - 1 );
			}
			Fixed endTime = g_app->GetWorld()->m_santaRouteLength.GetData( g_app->GetWorld()->m_santaCurrent );

			Fixed dist = currentTime - prevTime;
			Fixed totalDist = endTime - prevTime;
			
			x = BezierCurve( ( dist / totalDist ).DoubleValue(), cityEnd->m_longitude.DoubleValue(), cityEnd->m_longitude.DoubleValue(), 
									cityStart->m_longitude.DoubleValue(), cityStart->m_longitude.DoubleValue() );
			y = BezierCurve( ( dist / totalDist ).DoubleValue(), cityEnd->m_latitude.DoubleValue(), cityEnd->m_latitude.DoubleValue(), 
									cityStart->m_latitude.DoubleValue(), cityStart->m_latitude.DoubleValue() );

			g_app->GetWorld()->m_santaLongitude = Fixed::FromDouble( x );
			g_app->GetWorld()->m_santaLatitude = Fixed::FromDouble( y );

            if( cityStart->m_longitude <= cityEnd->m_longitude )
            {
				g_app->GetWorld()->m_santaPrevFlipped = false;
            }
            else
            {
				g_app->GetWorld()->m_santaPrevFlipped = true;
            }

			Colour colour = White;
			colour.m_a = 255;

			Image *bmpImage = g_resource->GetImage( "graphics/santa.bmp" );
			if( bmpImage )
			{
				// convert 2D coordinates to 3D globe position
				Vector3<float> santaPos = ConvertLongLatTo3DPosition(x, y);
				
				// position above globe surface like other units
				Vector3<float> normal = santaPos;
				normal.Normalise();
				float elevation = 0.002f;
				santaPos += normal * elevation;
				
				// convert 2D size to 3D world scale
				float santaSize = thisSize * 0.0075f;
				santaSize = fmaxf(santaSize, 0.005f);
				santaSize = fminf(santaSize, 0.1f);
				
				g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, -santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				else
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				g_renderer->SetBlendMode( Renderer::BlendModeNormal );				
			}
		}
		else
		{
			City *city;

			int index = ( g_app->GetWorld()->m_santaCurrent + 1 )/ 2;
			city = g_app->GetWorld()->m_santaRoute.GetData( index );

			Fixed testx = city->m_longitude;
			Fixed testy = city->m_latitude;

			float x = testx.DoubleValue();
			float y = testy.DoubleValue();

			Colour colour = White;
			colour.m_a = 255;

			Image *bmpImage = g_resource->GetImage( "graphics/santa.bmp" );
			if( bmpImage )
			{
				// convert 2D coordinates to 3D globe position
				Vector3<float> santaPos = ConvertLongLatTo3DPosition(x, y);
				
				// position above globe surface like other units
				Vector3<float> normal = santaPos;
				normal.Normalise();
				float elevation = 0.002f;
				santaPos += normal * elevation;
				
				// convert 2D size to 3D world scale
				float santaSize = thisSize * 0.0075f;
				santaSize = fmaxf(santaSize, 0.005f);
				santaSize = fminf(santaSize, 0.1f);
				
				g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, -santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				else
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				g_renderer->SetBlendMode( Renderer::BlendModeNormal );				
			}		
		}
		
	}

    //Nuke Santa and thereby end Christmas for everyone, for ever more
#endif
}

// ******************************************************************************************************************************
//                                             3D nuke trajectory calculations
// ******************************************************************************************************************************

// we now use great circle routes with ballistic arcs for realistic 3D trajectories
// while maintaining the 2D timing for determinism, this also removed the longitude
// flipping that was a huge issue with the 2d trajectory system

// ******************************************************************************************************************************

// 
// calculate great circle route between two points on the sphere

Vector3<float> GlobeRenderer::CalculateGreatCirclePosition(float startLon, float startLat, 
                                                         float endLon, float endLat, 
                                                         float progress)
{
    // convert to radians
    float lat1 = startLat * M_PI / 180.0f;
    float lon1 = startLon * M_PI / 180.0f;
    float lat2 = endLat * M_PI / 180.0f;
    float lon2 = endLon * M_PI / 180.0f;
    
    // handle longitude wrapping for shortest path
    float lonDiff = lon2 - lon1;
    if (lonDiff > M_PI) {
        lon2 -= 2.0f * M_PI;
    } else if (lonDiff < -M_PI) {
        lon2 += 2.0f * M_PI;
    }
    
    // calculate great circle distance
    float deltaLon = lon2 - lon1;
    float a = sinf(lat1) * sinf(lat2) + cosf(lat1) * cosf(lat2) * cosf(deltaLon);
    a = fmaxf(-1.0f, fminf(1.0f, a));
    float distance = acosf(a);
    
    if (distance < 0.001f) {
        // points are very close, just interpolate directly
        float lat = lat1 + (lat2 - lat1) * progress;
        float lon = lon1 + (lon2 - lon1) * progress;
        return ConvertLongLatTo3DPosition(lon * 180.0f / M_PI, lat * 180.0f / M_PI);
    }
    
    // interpolate along great circle using spherical interpolation
    float A = sinf((1.0f - progress) * distance) / sinf(distance);
    float B = sinf(progress * distance) / sinf(distance);
    float x1 = cosf(lat1) * cosf(lon1);
    float y1 = cosf(lat1) * sinf(lon1);
    float z1 = sinf(lat1);
    float x2 = cosf(lat2) * cosf(lon2);
    float y2 = cosf(lat2) * sinf(lon2);
    float z2 = sinf(lat2);
    
    // interpolated point
    float x = A * x1 + B * x2;
    float y = A * y1 + B * y2;
    float z = A * z1 + B * z2;
    
    // now convert back to lat/lon
    float lat = asinf(z);
    float lon = atan2f(y, x);
    
    return ConvertLongLatTo3DPosition(lon * 180.0f / M_PI, lat * 180.0f / M_PI);
}

//
// calculate realistic ballistic arc height based on distance and progress

float GlobeRenderer::CalculateBallisticHeight(float totalDistanceRadians, float progress)
{
    float arcFactor = 4.0f * progress * (1.0f - progress);
    
    //
    // calculate max height dynamically based on distance
    // uses square root scaling for smooth transitions
    
    float minHeight = 0.03f;      
    float maxHeight = 0.22f;      
    float maxDistance = M_PI;     
    
    //
    // square root scaling gives nice visual progression
    
    float normalizedDistance = totalDistanceRadians / maxDistance;
    float scaledDistance = sqrtf(normalizedDistance);
    float trajectoryHeight = minHeight + (maxHeight - minHeight) * scaledDistance;
    
    return trajectoryHeight * arcFactor;
}

//
// calculate the elevation offset for different unit types in 3D globe mode
// right now it just returns the default but down the line i might add different
// elevations for different units

float GlobeRenderer::CalculateUnitElevation(WorldObject* wobj)
{
    if (!Is3DGlobeModeEnabled() || !wobj) return 0.0f;
    
    return 0.002f;
}

//
// calculate 3D nuke position using great circle + ballistic arc

Vector3<float> GlobeRenderer::CalculateNuke3DPosition(Nuke* nuke)
{
    if (!Is3DGlobeModeEnabled() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    // use the 2D progress calculation for determinism
    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
    Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
    
    if (nuke->m_totalDistance <= 0) {
        return ConvertLongLatTo3DPosition(pos.x.DoubleValue(), pos.y.DoubleValue());
    }
    
    Fixed remainingDistance = (target - pos).Mag();
    Fixed fractionDistance = 1 - remainingDistance / nuke->m_totalDistance;
    
    // fractionDistance is already 0 (launch) to 1 (impact)
    float progress = fractionDistance.DoubleValue();
    progress = fmaxf(0.0f, fminf(1.0f, progress));
    
    // get target position
    float targetLon = nuke->m_targetLongitude.DoubleValue();
    float targetLat = nuke->m_targetLatitude.DoubleValue();
    float launchLon, launchLat;
    float currentLon = pos.x.DoubleValue();
    float currentLat = pos.y.DoubleValue();
    
    //
    // a key insight: nuke history is unlimited (-1), so the oldest point IS the launch position
    // the rendering truncation doesn't affect the actual stored history

    if (nuke->GetHistory().Size() > 0) {
        int lastIndex = nuke->GetHistory().Size() - 1;
        Vector3<Fixed> *launchPos = nuke->GetHistory()[lastIndex];
        launchLon = launchPos->x.DoubleValue();
        launchLat = launchPos->y.DoubleValue();
    } else {
        // when no history exists yet
        launchLon = currentLon;
        launchLat = currentLat;
    }
    
    //
    // calculate great circle position

    Vector3<float> surfacePos = CalculateGreatCirclePosition(launchLon, launchLat, targetLon, targetLat, progress);
    
    // calculate total distance in radians for height calculation
    float lat1 = launchLat * M_PI / 180.0f;
    float lon1 = launchLon * M_PI / 180.0f;
    float lat2 = targetLat * M_PI / 180.0f;
    float lon2 = targetLon * M_PI / 180.0f;
    
    float lonDiff = lon2 - lon1;
    if (lonDiff > M_PI) lon2 -= 2.0f * M_PI;
    else if (lonDiff < -M_PI) lon2 += 2.0f * M_PI;
    
    float deltaLon = lon2 - lon1;
    float a = sinf(lat1) * sinf(lat2) + cosf(lat1) * cosf(lat2) * cosf(deltaLon);
    a = fmaxf(-1.0f, fminf(1.0f, a));
    float totalDistanceRadians = acosf(a);
    
    // calculate ballistic height
    float height = CalculateBallisticHeight(totalDistanceRadians, progress);
    
    Vector3<float> normal = surfacePos;
    normal.Normalise();
    
    return surfacePos + normal * height;
}

//
// calculate historical nuke position for trail rendering

Vector3<float> GlobeRenderer::CalculateHistoricalNuke3DPosition(Nuke* nuke, const Vector3<Fixed>& historicalPos)
{
    if (!Is3DGlobeModeEnabled() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    if (nuke->m_totalDistance <= 0) {
        return ConvertLongLatTo3DPosition(historicalPos.x.DoubleValue(), historicalPos.y.DoubleValue());
    }
    
    // calculate what the progress would have been at this historical point
    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
    Fixed distanceFromTarget = (target - historicalPos).Mag();
    Fixed fractionDistance = 1 - distanceFromTarget / nuke->m_totalDistance;
    
    float progress = fractionDistance.DoubleValue();
    progress = fmaxf(0.0f, fminf(1.0f, progress));

    float targetLon = nuke->m_targetLongitude.DoubleValue();
    float targetLat = nuke->m_targetLatitude.DoubleValue();
    
    //
    // for historical positions, we need to find the launch position
    // use the oldest available history point, or estimate

    float launchLon, launchLat;
    if (nuke->GetHistory().Size() > 0) {
        int lastIndex = nuke->GetHistory().Size() - 1;
        Vector3<Fixed> *oldestPos = nuke->GetHistory()[lastIndex];
        launchLon = oldestPos->x.DoubleValue();
        launchLat = oldestPos->y.DoubleValue();
    } else {
        launchLon = historicalPos.x.DoubleValue();
        launchLat = historicalPos.y.DoubleValue();
    }
    
    Vector3<float> surfacePos = CalculateGreatCirclePosition(launchLon, launchLat, targetLon, targetLat, progress);
    
    float lat1 = launchLat * M_PI / 180.0f;
    float lon1 = launchLon * M_PI / 180.0f;
    float lat2 = targetLat * M_PI / 180.0f;
    float lon2 = targetLon * M_PI / 180.0f;
    
    float lonDiff = lon2 - lon1;
    if (lonDiff > M_PI) lon2 -= 2.0f * M_PI;
    else if (lonDiff < -M_PI) lon2 += 2.0f * M_PI;
    
    float deltaLon = lon2 - lon1;
    float a = sinf(lat1) * sinf(lat2) + cosf(lat1) * cosf(lat2) * cosf(deltaLon);
    a = fmaxf(-1.0f, fminf(1.0f, a));
    float totalDistanceRadians = acosf(a);
    
    float height = CalculateBallisticHeight(totalDistanceRadians, progress);
    
    Vector3<float> normal = surfacePos;
    normal.Normalise();
    
    return surfacePos + normal * height;
}

//
// work in progress, claudes attempt was pretty lame

// the gunfire shots curve and warp towards the nuke
// but their actual trajectory doesnt change

Vector3<float> GlobeRenderer::CalculateGunfire3DPosition(GunFire* gunfire)
{
    if (!Is3DGlobeModeEnabled() || !gunfire) {
        return Vector3<float>(0, 0, 0);
    }
    
    // Get gunfire's current 2D position
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float gunfireLongitude = (gunfire->m_longitude + gunfire->m_vel.x * predictionTime).DoubleValue();
    float gunfireLatitude = (gunfire->m_latitude + gunfire->m_vel.y * predictionTime).DoubleValue();
    
    // Check if targeting a nuke
    WorldObject* targetObj = g_app->GetWorld()->GetWorldObject(gunfire->m_targetObjectId);
    if (!targetObj || targetObj->m_type != WorldObject::TypeNuke) {
        // Regular surface positioning for non-nuke targets
        Vector3<float> surfacePos = ConvertLongLatTo3DPosition(gunfireLongitude, gunfireLatitude);
        Vector3<float> normal = surfacePos;
        normal.Normalise();
        return surfacePos + normal * 0.001f;
    }
    
    // SPECIAL: Calculate 3D trajectory when targeting a nuke
    Nuke* targetNuke = (Nuke*)targetObj;
    
    // Get launch position (where gunfire started)
    WorldObject* launcher = g_app->GetWorld()->GetWorldObject(gunfire->m_origin);
    Vector3<float> launchPos;
    if (launcher) {
        launchPos = ConvertLongLatTo3DPosition(
            launcher->m_longitude.DoubleValue(), 
            launcher->m_latitude.DoubleValue()
        );
        Vector3<float> launchNormal = launchPos;
        launchNormal.Normalise();
        launchPos += launchNormal * 0.001f; 
    } else {
        // Fallback to surface position
        Vector3<float> surfacePos = ConvertLongLatTo3DPosition(gunfireLongitude, gunfireLatitude);
        Vector3<float> normal = surfacePos;
        normal.Normalise();
        return surfacePos + normal * 0.001f;
    }
    
    // Get target's current 3D position
    Vector3<float> targetPos = CalculateNuke3DPosition(targetNuke);
    
    // Calculate how far along the trajectory the gunfire is
    Fixed totalDistance = gunfire->m_distanceToTarget;
    Vector3<float> currentSurfacePos = ConvertLongLatTo3DPosition(gunfireLongitude, gunfireLatitude);
    
    // Calculate progress (0 = at launch, 1 = at target)
    float distanceToTarget = (targetPos - currentSurfacePos).Mag();
    float totalFlightDistance = (targetPos - launchPos).Mag();
    float progress = 1.0f - (distanceToTarget / totalFlightDistance);
    progress = fmaxf(0.0f, fminf(1.0f, progress)); // Clamp to [0,1]
    
    // Interpolate along 3D trajectory from launch to target
    Vector3<float> gunfirePos = launchPos + (targetPos - launchPos) * progress;
    
    return gunfirePos;
}

//
// calculate historical nuke position using age-based progress
// this solves the issue of the nuke trajectory becoming corrupted
// and resetting to a vertical line half way through big arcs

Vector3<float> GlobeRenderer::CalculateHistoricalNuke3DPositionByAge(Nuke* nuke, const Vector3<Fixed>& historicalPos, float historicalProgress)
{
    if (!Is3DGlobeModeEnabled() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    if (nuke->m_totalDistance <= 0) {
        return ConvertLongLatTo3DPosition(historicalPos.x.DoubleValue(), historicalPos.y.DoubleValue());
    }
    
    // get target position
    float targetLon = nuke->m_targetLongitude.DoubleValue();
    float targetLat = nuke->m_targetLatitude.DoubleValue();
    
    // get launch position from the oldest history point
    float launchLon, launchLat;
    if (nuke->GetHistory().Size() > 0) {
        int lastIndex = nuke->GetHistory().Size() - 1;
        Vector3<Fixed> *launchPos = nuke->GetHistory()[lastIndex];
        launchLon = launchPos->x.DoubleValue();
        launchLat = launchPos->y.DoubleValue();
    } else {
        launchLon = historicalPos.x.DoubleValue();
        launchLat = historicalPos.y.DoubleValue();
    }
    
    Vector3<float> surfacePos = CalculateGreatCirclePosition(launchLon, launchLat, targetLon, targetLat, historicalProgress);
    
    float lat1 = launchLat * M_PI / 180.0f;
    float lon1 = launchLon * M_PI / 180.0f;
    float lat2 = targetLat * M_PI / 180.0f;
    float lon2 = targetLon * M_PI / 180.0f;
    
    float lonDiff = lon2 - lon1;
    if (lonDiff > M_PI) lon2 -= 2.0f * M_PI;
    else if (lonDiff < -M_PI) lon2 += 2.0f * M_PI;
    
    float deltaLon = lon2 - lon1;
    float a = sinf(lat1) * sinf(lat2) + cosf(lat1) * cosf(lat2) * cosf(deltaLon);
    a = fmaxf(-1.0f, fminf(1.0f, a));
    float totalDistanceRadians = acosf(a);
    
    float height = CalculateBallisticHeight(totalDistanceRadians, historicalProgress);
    
    Vector3<float> normal = surfacePos;
    normal.Normalise();
    
    return surfacePos + normal * height;
}