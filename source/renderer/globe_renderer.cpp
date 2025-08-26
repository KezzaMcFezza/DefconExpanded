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
#include "lib/render/styletable.h"
#include "lib/profiler.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"
#include "lib/sound/soundsystem.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"
#include "app/tutorial.h"
#include "app/game.h"
#ifdef TARGET_OS_MACOSX
#include "app/macutil.h"
#endif

#include "network/ClientToServer.h"

#include "interface/components/core.h"
#include "interface/interface.h"
#include "interface/worldobject_window.h"

#include "renderer/map_renderer.h"
#include "renderer/animated_icon.h"

#include "lib/render/renderer_debug_menu.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/worldobject.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/node.h"
#include "world/blip.h"
#include "world/fleet.h"
#include "world/whiteboard.h"
#include "curves.h"

// i couldnt be arsed figuring out what includes we need and dont need
// so i just copied and pasted all the includes from map renderer :)

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

//
// generate random star field around the globe

void MapRenderer::Generate3DStarField()
{
    if (g_starField3DInitialized) return;
    
    g_starField3D.Empty();
    
    // generate random stars on a large sphere around the globe
    int numStars = 1200;                // adjust for desired star density 
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
        
        star.size = 0.020f + frand(0.025f); 
        
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

void MapRenderer::Cleanup3DStarField()
{
    g_starField3D.Empty();
    g_starField3DInitialized = false;
    AppDebugOut("3D Star Field: Cleaned up star field\n");
}

void MapRenderer::Regenerate3DStarField()
{
    Cleanup3DStarField();
    Generate3DStarField();
    AppDebugOut("3D Star Field: Regenerated star field\n");
}

//
// render the star field background

void MapRenderer::Render3DStarField()
{
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
        
        g_renderer3d->StarFieldSprite3D(cityImg, 
                                       star.position.x, star.position.y, star.position.z,
                                       star.size, star.size, 
                                       starColor);
    }
}

//
// handle globe initialisation

void MapRenderer::Toggle3DGlobeMode()
{
    m_3DGlobeMode = !m_3DGlobeMode;
    
    if (m_3DGlobeMode) {
        // initialize 3D camera position
        m_globe3DCamera.m_cameraDistance = 3.0f;
        m_globe3DCamera.m_cameraTheta = 0.0f;
        m_globe3DCamera.m_cameraPhi = 0.0f;
        
        // default camera position for the camera when we load up
        m_globe3DCamera.m_cameraPos = Vector3<float>(0.0f, 0.5f, m_globe3DCamera.m_cameraDistance);
        m_globe3DCamera.m_cameraTarget = Vector3<float>(0.0f, 0.0f, 0.0f);
        m_globe3DCamera.m_cameraUp = Vector3<float>(0.0f, 1.0f, 0.0f);
        
        AppDebugOut("3D Globe Mode: ENABLED\n");
    } else {
        Cleanup3DStarField();
        AppDebugOut("3D Globe Mode: DISABLED\n");
    }
}

//
// main rendering for the 3D globe

void MapRenderer::Render3DGlobe(bool inLobbyMode)
{
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
        
        // set camera position for billboard calculations
        g_renderer3d->SetCameraPosition(m_globe3DCamera.m_cameraPos.x, 
                                       m_globe3DCamera.m_cameraPos.y, 
                                       m_globe3DCamera.m_cameraPos.z);
    }
    
    // enable depth testing for 3D sprites
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // use additive blending like 2D mode
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    g_renderer3d->EnableOrientationFog(0.01f, 0.08f, 0.05f, 20.0f, 
                                      m_globe3DCamera.m_cameraPos.x,
                                      m_globe3DCamera.m_cameraPos.y,
                                      m_globe3DCamera.m_cameraPos.z);

    //
    // Check validity of coastlines and borders vbo

    if (inLobbyMode) {
    
    // Build globe geometry if not cached
    if (!g_renderer3d->IsMegaVBO3DValid("GlobeGridlines")) {
        // Build gridlines mega-VBO
        g_renderer3d->BeginMegaVBO3D("GlobeGridlines", Colour(77, 255, 77, 51)); // 0.15f, 0.25f, 0.15f, 0.2f
        
        // Longitudinal lines (meridians)
        for( float x = -180; x < 180; x += 10 )
        {
            DArray<Vector3<float>> lineVertices;
            for( float y = -90; y < 90; y += 10 )
            {
                Vector3<float> thisPoint(0,0,1);
                thisPoint.RotateAroundY( x/180.0f * M_PI );
                Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();
                right.Normalise();
                thisPoint.RotateAround( right * y/180.0f * M_PI );

                lineVertices.PutData(thisPoint);
            }
            if (lineVertices.Size() > 0) {
                Vector3<float>* vertexArray = new Vector3<float>[lineVertices.Size()];
                for (int i = 0; i < lineVertices.Size(); i++) {
                    vertexArray[i] = lineVertices.GetData(i);
            }
                g_renderer3d->AddLineStripToMegaVBO3D(vertexArray, lineVertices.Size());
                delete[] vertexArray;
        }
        }

        // Latitudinal lines (parallels)
        for( float y = -90; y < 90; y += 10 )
        {
            DArray<Vector3<float>> lineVertices;
            for( float x = -180; x <= 180; x += 10 )
            {
                Vector3<float> thisPoint(0,0,1);
                thisPoint.RotateAroundY( x/180.0f * M_PI );
                Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();
                right.Normalise();
                thisPoint.RotateAround( right * y/180.0f * M_PI );

                lineVertices.PutData(thisPoint);
            }
            if (lineVertices.Size() > 0) {
                Vector3<float>* vertexArray = new Vector3<float>[lineVertices.Size()];
                for (int i = 0; i < lineVertices.Size(); i++) {
                    vertexArray[i] = lineVertices.GetData(i);
                }
                g_renderer3d->AddLineStripToMegaVBO3D(vertexArray, lineVertices.Size());
                delete[] vertexArray;
            }
        }
        
        g_renderer3d->EndMegaVBO3D();
        }
    }

    //
    // Coastlines VBO

    if (g_preferences->GetInt(PREFS_GRAPHICS_COASTLINES) == 1) {
        if (!g_renderer3d->IsMegaVBO3DValid("GlobeCoastlines")) {
            g_renderer3d->BeginMegaVBO3D("GlobeCoastlines", Colour(0, 255, 0, 255));

            for( int i = 0; i < g_app->GetEarthData()->m_islands.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_islands[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> coastlineVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];

                    Vector3<float> thisPoint(0,0,1);
                    thisPoint.RotateAroundY( thePoint->x/180.0f * M_PI );
                    Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();
                    right.Normalise();
                    thisPoint.RotateAround( right * thePoint->y/180.0f * M_PI );

                    coastlineVertices.PutData(thisPoint);
                }
                if (coastlineVertices.Size() > 0) {
                    Vector3<float>* vertexArray = new Vector3<float>[coastlineVertices.Size()];
                    for (int k = 0; k < coastlineVertices.Size(); k++) {
                        vertexArray[k] = coastlineVertices.GetData(k);
                    }
                    g_renderer3d->AddLineStripToMegaVBO3D(vertexArray, coastlineVertices.Size());
                    delete[] vertexArray;
                }
            }
            
            g_renderer3d->EndMegaVBO3D();
            AppDebugOut("Rebuilt globe coastlines VBO during gameplay with %d islands\n", g_app->GetEarthData()->m_islands.Size());
        }
    }

    // Build it
    g_renderer3d->RenderMegaVBO3D("GlobeCoastlines");
    
    //
    // Borders VBO

    if (g_preferences->GetInt(PREFS_GRAPHICS_BORDERS) == 1) {
        if (!g_renderer3d->IsMegaVBO3DValid("GlobeBorders")) {
            g_renderer3d->BeginMegaVBO3D("GlobeBorders", Colour(0, 255, 0, 71));

            for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_borders[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> borderVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];

                    Vector3<float> thisPoint(0,0,1);
                    thisPoint.RotateAroundY( thePoint->x/180.0f * M_PI );
                    Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();
                    right.Normalise();
                    thisPoint.RotateAround( right * thePoint->y/180.0f * M_PI );

                    borderVertices.PutData(thisPoint);
                }
                if (borderVertices.Size() > 0) {
                    Vector3<float>* vertexArray = new Vector3<float>[borderVertices.Size()];
                    for (int k = 0; k < borderVertices.Size(); k++) {
                        vertexArray[k] = borderVertices.GetData(k);
                    }
                    g_renderer3d->AddLineStripToMegaVBO3D(vertexArray, borderVertices.Size());
                    delete[] vertexArray;
                }
            }

            g_renderer3d->EndMegaVBO3D();
            AppDebugOut("Rebuilt globe borders VBO during gameplay with %d border segments\n", g_app->GetEarthData()->m_borders.Size());
        }
        
        // Build it
        g_renderer3d->RenderMegaVBO3D("GlobeBorders");
    }
    
    // disable fog after rendering coastlines and borders
    g_renderer3d->DisableFog();
  
    //
    // master scene batching, pretty much identical to map renderer
    // if it aint broke dont fix it, since im good at breaking shit
    //
    
    //
    // begin scene

    g_renderer3d->BeginStarFieldBatch3D();      // Star field batching
    g_renderer3d->BeginGlobeSurfaceBatch3D();   // Globe surface batching
    g_renderer3d->BeginUnitMainBatch3D();       // Main unit sprites + city icons
    g_renderer3d->BeginUnitRotatingBatch3D();   // Rotating sprites (aircraft, but not nukes anymore)
    g_renderer3d->BeginUnitTrailBatch3D();      // Unit movement trails
    g_renderer3d->BeginUnitStateBatch3D();      // Unit state icons (fighters/bombers on units)
    g_renderer3d->BeginUnitNukeBatch3D();       // Small nuke icons on units
    g_renderer3d->BeginNuke3DModelBatch3D();    // 3D nuke models (replaces flat nuke sprites)
    g_renderer3d->BeginEffectsLineBatch3D();    // All line effects (gunfire trails, etc.)
    g_renderer3d->BeginEffectsSpriteBatch3D();  // All sprite effects (explosions, sonar pings, etc.)

    // render all 3d objects inside the scene

    Render3DStarField();
    Render3DGlobeSurface(); 
    Render3DGlobeCities();
    Render3DUnits();
    Render3DUnitTrails();
    Render3DGunfire();
    Render3DExplosions();
    Render3DSonarPing();
    
    //
    // now end the scene and flush
    
    g_renderer3d->EndUnitTrailBatch3D();        // Flush all unit trails
    g_renderer3d->EndEffectsSpriteBatch3D();    // Flush all sprite effects (explosions, sonar pings)
    g_renderer3d->EndEffectsLineBatch3D();      // Flush all line effects (gunfire trails)
    g_renderer3d->EndNuke3DModelBatch3D();      // Flush all 3D nuke models
    g_renderer3d->EndUnitNukeBatch3D();         // Flush all small nuke icons
    g_renderer3d->EndUnitStateBatch3D();        // Flush all unit state icons
    g_renderer3d->EndUnitRotatingBatch3D();     // Flush all rotating sprites (atlas batching!)
    g_renderer3d->EndUnitMainBatch3D();         // Flush all main unit sprites + city icons (atlas batching!)
    g_renderer3d->EndGlobeSurfaceBatch3D();     // Flush globe surface triangles
    g_renderer3d->EndStarFieldBatch3D();        // Flush star sprites

    glDisable(GL_DEPTH_TEST);
    
    // restore blending state to 2D default
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // reset to 2d viewport
    g_renderer->Reset2DViewport();
    
    // end draw call tracking
    g_renderer3d->EndFrame3D();
}

//
// camera controls for the 3d globe, to preserve muscle memory
// we use default DEFCON controls for globe mode
// and for the lobby remains unchanged

void MapRenderer::SetupCamera3d()
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
    glEnable( GL_BLEND );
    g_renderer->SetBlendFunc( GL_SRC_ALPHA, GL_ONE );
    glDisable( GL_DEPTH_TEST );
}

void MapRenderer::Update3DGlobeCamera()
{
    if (!m_3DGlobeMode) return;
    
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
        
        // clamp vertical rotation and zoom distance
        m_globe3DCamera.m_cameraPhi = fmax(-M_PI/2.0f + 0.1f, fmin(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
        m_globe3DCamera.m_cameraDistance = fmax(1.5f, fmin(10.0f, m_globe3DCamera.m_cameraDistance));
        
        // update camera position
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
    }
    
    // globe dragging with middle mouse button
    if (g_inputManager->m_mmb && IsMouseInMapRenderer()) {
        if (!m_globe3DCamera.m_isDragging) {
            m_globe3DCamera.m_isDragging = true;
            m_globe3DCamera.m_lastMouseX = g_inputManager->m_mouseX;
            m_globe3DCamera.m_lastMouseY = g_inputManager->m_mouseY;
            m_globe3DCamera.m_dragVelocityX = 0.0f;
            m_globe3DCamera.m_dragVelocityY = 0.0f;
        }
        
        float deltaX = g_inputManager->m_mouseX - m_globe3DCamera.m_lastMouseX;
        float deltaY = g_inputManager->m_mouseY - m_globe3DCamera.m_lastMouseY;
        
        // apply frame time compensation for consistent movement speed
        float frameTimeCompensation = g_advanceTime * 60.0f; // normalize to 60fps
        frameTimeCompensation = fmax(0.1f, fmin(3.0f, frameTimeCompensation)); // clamp for extreme cases
        
        // adaptive sensitivity based on zoom level - closer = more sensitive
        float zoomSensitivity = 1.0f / (m_globe3DCamera.m_cameraDistance * 0.5f + 0.5f);
        zoomSensitivity = fmax(0.3f, fmin(2.0f, zoomSensitivity)); // reasonable range
        
        // base sensitivity
        float baseSensitivity = 0.016f * frameTimeCompensation * zoomSensitivity;
        float targetVelX = deltaX * baseSensitivity;
        float targetVelY = deltaY * baseSensitivity;
        
        // smooth velocity interpolation for less jittery movement
        float smoothing = 0.3f; 
        m_globe3DCamera.m_dragVelocityX = m_globe3DCamera.m_dragVelocityX * (1.0f - smoothing) + targetVelX * smoothing;
        m_globe3DCamera.m_dragVelocityY = m_globe3DCamera.m_dragVelocityY * (1.0f - smoothing) + targetVelY * smoothing;
        m_globe3DCamera.m_cameraTheta -= m_globe3DCamera.m_dragVelocityX;
        m_globe3DCamera.m_cameraPhi += m_globe3DCamera.m_dragVelocityY;
        m_globe3DCamera.m_cameraPhi = fmax(-M_PI/2.0f + 0.1f, fmin(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        
        m_globe3DCamera.m_lastMouseX = g_inputManager->m_mouseX;
        m_globe3DCamera.m_lastMouseY = g_inputManager->m_mouseY;

    //
    // Add slight momentum when stopping drag for smoother experience

    } else {
        if (m_globe3DCamera.m_isDragging) {
            float momentum = 0.92f; // how much momentum to preserve
            m_globe3DCamera.m_dragVelocityX *= momentum;
            m_globe3DCamera.m_dragVelocityY *= momentum;
            
            if (fabsf(m_globe3DCamera.m_dragVelocityX) > 0.001f || fabsf(m_globe3DCamera.m_dragVelocityY) > 0.001f) {
                m_globe3DCamera.m_cameraTheta -= m_globe3DCamera.m_dragVelocityX;
                m_globe3DCamera.m_cameraPhi += m_globe3DCamera.m_dragVelocityY;
                m_globe3DCamera.m_cameraPhi = fmax(-M_PI/2.0f + 0.1f, fmin(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
                m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
                m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
                m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
            } else {
                m_globe3DCamera.m_dragVelocityX = 0.0f;
                m_globe3DCamera.m_dragVelocityY = 0.0f;
            }
        }
        
        m_globe3DCamera.m_isDragging = false;
    }
    
    // mouse wheel zoom
    if (IsMouseInMapRenderer() && g_inputManager->m_mouseVelZ != 0) {
        m_globe3DCamera.m_cameraDistance += g_inputManager->m_mouseVelZ * -0.1f;
        m_globe3DCamera.m_cameraDistance = fmax(1.5f, fmin(10.0f, m_globe3DCamera.m_cameraDistance));
        
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
    }
}

//
// this is the function that makes it all happen, we convert 2d coordinates to 3d coordinates
// the only issue with this which is pretty unavoidable is that the closer we get to the poles
// the units get squashed, its literally an unsolvable problem.

Vector3<float> MapRenderer::ConvertLongLatTo3DPosition(float longitude, float latitude)
{
    
    float lonRad = longitude * M_PI / 180.0f;
    float latRad = latitude * M_PI / 180.0f;
    
    // spherical to cartesian conversion
    Vector3<float> pos(0, 0, 1);
    pos.RotateAroundY(lonRad);
    Vector3<float> right = pos ^ Vector3<float>(0, 1, 0);
    right.Normalise();
    pos.RotateAround(right * latRad);
    
    return pos;
}

//
// render a filled semi transparent globe surface to occlude stars behind it

void MapRenderer::Render3DGlobeSurface()
{

    //
    // This will occlude stars behind the globe while maintaining coastline visibility

    Colour globeColor(0, 0, 0, 30);
    
    int segments = 64; 
    float radius = 1.0f; 
    Vector3<float> center(0.0f, 0.0f, 0.0f);
    
    //
    // render filled sphere using triangle approach
    // create triangular faces for the sphere

    for (int lat = 0; lat < segments; ++lat) {
        float theta1 = M_PI * lat / segments - M_PI/2; // -90 to +90 degrees
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
            g_renderer3d->GlobeSurfaceTriangle3D(center.x + x1, center.y + y1, center.z + z1,
                                               center.x + x2, center.y + y2, center.z + z2,
                                               center.x + x3, center.y + y3, center.z + z3, globeColor);
            
            // Second triangle: 1, 3, 4
            g_renderer3d->GlobeSurfaceTriangle3D(center.x + x1, center.y + y1, center.z + z1,
                                               center.x + x3, center.y + y3, center.z + z3,
                                               center.x + x4, center.y + y4, center.z + z4, globeColor);
        }
    }
}

//
// create a simple sphere using triangles

void MapRenderer::Render3DSphere(const Vector3<float>& center, float radius, const Colour& color, int segments)
{
    if (!m_3DGlobeMode) return;
    
    float r = color.m_r / 255.0f;
    float g = color.m_g / 255.0f;
    float b = color.m_b / 255.0f;
    float a = color.m_a / 255.0f;
    
    g_renderer3d->BeginLineStrip3D(color);
      
    for (int lat = 0; lat < segments; ++lat) {
        float theta1 = M_PI * lat / segments - M_PI/2; // -90 to +90 degrees
        float theta2 = M_PI * (lat + 1) / segments - M_PI/2;
        
        float y1 = sinf(theta1) * radius;
        float y2 = sinf(theta2) * radius;
        float r1 = cosf(theta1) * radius;
        float r2 = cosf(theta2) * radius;
        
        for (int lon = 0; lon <= segments; ++lon) {
            float phi = 2.0f * M_PI * lon / segments;
            
            float x1 = r1 * cosf(phi);
            float z1 = r1 * sinf(phi);
            
            g_renderer3d->LineStripVertex3D(center.x + x1, center.y + y1, center.z + z1);
        }
    }
    
    g_renderer3d->EndLineStrip3D();
    
    for (int lon = 0; lon < segments/2; ++lon) {
        float phi = 2.0f * M_PI * lon / segments;
        
        g_renderer3d->BeginLineStrip3D(color);
        
        for (int lat = 0; lat <= segments; ++lat) {
            float theta = M_PI * lat / segments - M_PI/2;
            
            float x = cosf(theta) * cosf(phi) * radius;
            float y = sinf(theta) * radius;
            float z = cosf(theta) * sinf(phi) * radius;
            
            g_renderer3d->LineStripVertex3D(center.x + x, center.y + y, center.z + z);
        }
        
        g_renderer3d->EndLineStrip3D();
    }
}

//
// create the same projection matrix as g_renderer3d->SetPerspective
// and the same view matrix as g_renderer3d->SetLookAt

Vector3<float> MapRenderer::Project3DToScreen(const Vector3<float>& worldPos)
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

void MapRenderer::Render3DGlobeCities()
{
    if (!m_3DGlobeMode) return;
    
    Image *cityImg = g_resource->GetImage("graphics/city.bmp");
    if (!cityImg) return;
    
    // these are added but not actually used, i had alot of issues with city names
    bool showCityNames = g_preferences->GetInt(PREFS_GRAPHICS_CITYNAMES);
    bool showCountryNames = g_preferences->GetInt(PREFS_GRAPHICS_COUNTRYNAMES);
    
    // zoom does not affect city size
    float normalizedDistance = (m_globe3DCamera.m_cameraDistance - 1.5f) / 8.5f;
    float equivalent2DZoom = 1.0f - normalizedDistance;
    equivalent2DZoom = fmax(0.05f, fmin(1.0f, equivalent2DZoom));
    
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
            
            // buggy viewport culling, it works good enough for now
            Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
            globeToCamera.Normalise();
            float dotProduct = cityPos * globeToCamera;
            if (dotProduct < 0.0f) continue; 
            
            float baseSize = sqrtf(sqrtf((float)city->m_population)) / 25.0f;
            
            float size = baseSize * 0.0075f;  // half of original 0.015f
            size = fmax(size, 0.004f);
            size = fmin(size, 0.025f);  
            
            // get city colour
            Colour col(100, 100, 100, 200);
            if (city->m_teamId > -1) {
                col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                float alphaZoomFactor = fmin(0.8f, zoomFactorEquivalent);
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
        cityPos += normal * 0.002f;
        
        g_renderer3d->UnitMainSprite3D(cityImg, cityPos.x, cityPos.y, cityPos.z, size * 2.0f, size * 2.0f, col, BILLBOARD_SURFACE_ALIGNED);
    }
    
    // currently not used, i had alot of issues with city names
    if ((showCityNames || showCountryNames) && cityNameColor.m_a > 10.0f) {
        float textSize = 12.0f;
        if (equivalent2DZoom > 0.5f) {
            textSize = 12.0f + (equivalent2DZoom - 0.5f) * 16.0f;
        }
        textSize = fmin(textSize, 24.0f);
        
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

void MapRenderer::Render3DUnits()
{
    if (!m_3DGlobeMode) return;
    
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
        bool isNuke;
        float rotationAngle;
    };
    
    DArray<UnitRenderInfo> visibleUnits;
    
    // First pass: collect visible units with enhanced categorization
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // check what teamid we have set, then we can decide
            // whether to render the unit or not
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               m_renderEverything);
            
            if (!shouldRender) continue;
            
            Vector3<float> unitPos;
            bool isNuke = (wobj->m_type == WorldObject::TypeNuke);
            
            if (isNuke) {
                Nuke* nuke = (Nuke*)wobj;
                unitPos = CalculateNuke3DPosition(nuke);
                
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                Vector3<float> nukeToCameraDir = unitPos;
                nukeToCameraDir.Normalise();
                float dotProduct = nukeToCameraDir * globeToCamera;
                if (dotProduct < -0.3f) continue; // More lenient culling for high-altitude nukes
            } else {
                // Regular surface positioning for other units
                Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                Fixed predictedLongitude = wobj->m_longitude + wobj->m_vel.x * predictionTime;
                Fixed predictedLatitude = wobj->m_latitude + wobj->m_vel.y * predictionTime;
                
                unitPos = ConvertLongLatTo3DPosition(
                    predictedLongitude.DoubleValue(), 
                    predictedLatitude.DoubleValue()
                );
                
                // viewport culling,only render visible hemisphere however the issue is that
                // the logic is pretty flawed as it does not take into account the zoom level
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                float dotProduct = unitPos * globeToCamera;
                if (dotProduct < 0.0f) continue;
            }
            
            Image *unitImg = wobj->GetBmpImage(wobj->m_currentState);
            if (!unitImg) continue;
            
            // pass the image to the size multiplier
            float baseSize = wobj->GetSize3D().DoubleValue();
            float size;
            
            bool isMovingObject = wobj->IsMovingObject();
            bool isAirUnit = false;
            
            if (isMovingObject) {
                MovingObject *mobj = (MovingObject*)wobj;
                isAirUnit = (mobj->m_movementType == MovingObject::MovementTypeAir);
                
                if (isAirUnit) {
                    size = baseSize * 0.0075f * 0.75f;
                } else if (mobj->m_movementType == MovingObject::MovementTypeSea) {
                    size = baseSize * 0.0075f * 1.25f;
                } else {
                    size = baseSize * 0.0075f;
                }
            } else {
                size = baseSize * 0.0075f * 2.0f;
            }
            
            if (isNuke) {
                size = baseSize * 0.008f; 
            }
            
            size = fmax(size, 0.002f);  
            size = fmin(size, 0.04f);   
            
            Colour unitColor = White;
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            if (team) {
                unitColor = team->GetTeamColour();
            }
            
            // make sure to pass the rotation angle for the unit icons
            float rotationAngle = 0.0f;
            if (isAirUnit || isNuke) {
                rotationAngle = atan(-wobj->m_vel.x.DoubleValue() / wobj->m_vel.y.DoubleValue());
                if (wobj->m_vel.y < 0) rotationAngle += M_PI;
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
            info.isNuke = isNuke;
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
        // elevation system, now air units render higher than other units
        // and every other unit lays flat on the globe
        
        Vector3<float> normal = unitPos;
        normal.Normalise();
        
        float elevation = 0.0f; 
        
        if (info.isNuke) {
            elevation = 0.0f; // nukes are already positioned with proper height by CalculateNuke3DPosition
        } else if (info.isAirUnit) {
            elevation = 0.015f;  // 2.5x higher 
        } else {
            bool isSurfaceUnit = false;
            
            if (info.isMovingObject) {
                MovingObject* mobj = (MovingObject*)wobj;
                if (mobj->m_movementType == MovingObject::MovementTypeSea) {
                    isSurfaceUnit = true;
                }
            } else {
                if (wobj->m_type == WorldObject::TypeAirBase ||
                    wobj->m_type == WorldObject::TypeSilo ||
                    wobj->m_type == WorldObject::TypeRadarStation) {
                    isSurfaceUnit = true;
                }
            }
            
            if (isSurfaceUnit) {
                elevation = 0.001f;  
            } else {
                elevation = 0.006f;  
            }
        }
        
        unitPos += normal * elevation;
        
        //
        // for nukes in 3D space, use the new 3D nuke model that faces forward

        if (info.isNuke) {
            
            //
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
        } else if (info.isAirUnit) {
            g_renderer3d->UnitRotating3D(unitImg, unitPos.x, unitPos.y, unitPos.z, 
                                        size * 2.0f, size * 2.0f, color, info.rotationAngle, BILLBOARD_SURFACE_ALIGNED);
        } else {
            g_renderer3d->UnitMainSprite3D(unitImg, unitPos.x, unitPos.y, unitPos.z, 
                                          size * 2.0f, size * 2.0f, color, BILLBOARD_SURFACE_ALIGNED);
        }
    }
    
    for (int i = 0; i < visibleUnits.Size(); i++) {
        UnitRenderInfo& info = visibleUnits[i];
        WorldObject* wobj = info.unit;
        Vector3<float> unitPos = info.worldPos;
        float unitSize = info.size;
        Colour teamColor = info.color;
        
        // nukes dont cary nukes
        if (info.isNuke) continue;
        
        // check teamid to see if we should render the capacity indicators
        int myTeamId = g_app->GetWorld()->m_myTeamId;
        bool shouldRenderCapacity = (myTeamId == -1 || 
                                   wobj->m_teamId == myTeamId || 
                                   g_app->GetGame()->m_winner != -1);
        
        if (!shouldRenderCapacity) continue;
        
        Vector3<float> normal = unitPos;
        normal.Normalise();
        
        float elevation = 0.0f; 
        
        if (info.isAirUnit) {
            elevation = 0.015f; 
        } else {
            bool isSurfaceUnit = false;
            
            if (info.isMovingObject) {
                MovingObject* mobj = (MovingObject*)wobj;
                if (mobj->m_movementType == MovingObject::MovementTypeSea) {
                    isSurfaceUnit = true;
                }
            } else {
                if (wobj->m_type == WorldObject::TypeAirBase ||
                    wobj->m_type == WorldObject::TypeSilo ||
                    wobj->m_type == WorldObject::TypeRadarStation) {
                    isSurfaceUnit = true;
                }
            }
            
            if (isSurfaceUnit) {
                elevation = 0.001f;  
            } else {
                elevation = 0.006f;  
            }
        }
        
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
                    
                    g_renderer3d->UnitNukeIcon3D(iconPos.x, iconPos.y, iconPos.z,
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
                    
                    g_renderer3d->UnitNukeIcon3D(iconPos.x, iconPos.y, iconPos.z,
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
            
            g_renderer3d->UnitNukeIcon3D(unitPos.x, unitPos.y, unitPos.z,
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
                        
                        g_renderer3d->UnitStateIcon3D(iconImg, iconPos.x, iconPos.y, iconPos.z,
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

void MapRenderer::Render3DUnitTrails()
{
    if (!m_3DGlobeMode) return;
    
    if (g_preferences->GetInt(PREFS_GRAPHICS_TRAILS) != 1) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render trails for all moving objects
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            // not moving, skip
            if (!wobj->IsMovingObject()) continue;
            
            MovingObject *mobj = (MovingObject*)wobj;
            bool isNuke = (wobj->m_type == WorldObject::TypeNuke);
            
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               m_renderEverything);
            
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
            if (isNuke) {
                sizeCap *= 4;
            } else {
                sizeCap *= 4;
            }
            
            sizeCap /= World::GetGameScale().DoubleValue();
            maxSize = (maxSize > sizeCap ? sizeCap : maxSize);
            
            // reduce the trail length for enemy units
            if (mobj->m_teamId != myTeamId &&
                myTeamId != -1 &&
                myTeamId < g_app->GetWorld()->m_teams.Size() &&
                g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned) {
                int enemyTrailLimit = isNuke ? 16 : 16;  // 4*4 for nukes, 4*4 for others
                maxSize = min(maxSize, enemyTrailLimit);
            }
            
            if (maxSize <= 0) continue;
            
            //
            // nuke trails, added segmented lines instead of one big line
            // it looks okay but might need some fine tuning

            if (isNuke) {
                
                Nuke* nuke = (Nuke*)wobj;
                Vector3<float> currentPos = CalculateNuke3DPosition(nuke);
                
                // viewport culling
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                Vector3<float> nukeToCameraDir = currentPos;
                nukeToCameraDir.Normalise();
                float dotProduct = nukeToCameraDir * globeToCamera;
                if (dotProduct < -0.3f) continue;
                
                //
                // create line segments for nukes with dashed effect
                // reduce trail detail based on distance to camera
                // this almost doubles the fps when in a 3v3
                
                float distanceToCamera = (currentPos - m_globe3DCamera.m_cameraPos).Mag();
                int lodLevel = (distanceToCamera > 3.0f) ? 4 : 2;  // lower detail when far away
                int reducedMaxSize = maxSize / lodLevel;  // reduce number of trail segments
                reducedMaxSize = fmax(4, fmin(reducedMaxSize, 32));  // clamp between 4-32 segments
                
                Vector3<float> prevPos = currentPos;
                
                for (int j = 0; j < reducedMaxSize; ++j) {
                    int actualHistoryIndex = (j * maxSize) / reducedMaxSize;
                    if (actualHistoryIndex >= history.Size()) break;
                    
                    Vector3<Fixed> *historyPos = history[actualHistoryIndex];
                    
                    // Calculate progress based on current nuke progress and history age
                    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
                    Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
                    Fixed remainingDistance = (target - pos).Mag();
                    Fixed fractionDistance = 1 - remainingDistance / nuke->m_totalDistance;
                    float currentProgress = fractionDistance.DoubleValue();
                    
                    // work backwards: j=0 is newest, j=reducedMaxSize-1 is oldest
                    float progressRange = currentProgress;
                    float historicalProgress = currentProgress - (progressRange * (float)j / (float)(reducedMaxSize - 1));
                    historicalProgress = fmaxf(0.0f, fminf(1.0f, historicalProgress));
                    
                    Vector3<float> pos3D = CalculateHistoricalNuke3DPositionByAge(nuke, *historyPos, historicalProgress);
                    
                    Colour segmentColour = colour;
                    segmentColour.m_a = 255 - 255 * (float)j / (float)reducedMaxSize;
                    
                    // removed interpolation for now as it was a huge cpu bottleneck
                    Vector3<float> segmentDir = pos3D - prevPos;
                    float segmentLength = segmentDir.Mag();
                    
                    if (segmentLength > 0.001f) {
                        // just create 2-3 dash segments per trail segment
                        int numDashes = (segmentLength > 0.05f) ? 3 : 2;
                        float dashSize = segmentLength / (numDashes * 2.0f);
                        
                        for (int dash = 0; dash < numDashes; dash++) {
                            float startT = (dash * 2.0f) / (numDashes * 2.0f);
                            float endT = (dash * 2.0f + 1.0f) / (numDashes * 2.0f);
                            
                            Vector3<float> dashStart = prevPos + segmentDir * startT;
                            Vector3<float> dashEnd = prevPos + segmentDir * endT;
                            
                            g_renderer3d->UnitTrailLine3D(dashStart.x, dashStart.y, dashStart.z,
                                                         dashEnd.x, dashEnd.y, dashEnd.z, segmentColour);
                        }
                    }
                    
                    prevPos = pos3D;
                }
            } else {

                //
                // everything else uses normal 2d unit trails that are similiar
                // to the 2d trails, i need to add the proper segmentation
                
                Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                float predictedLongitude = (mobj->m_longitude + mobj->m_vel.x * predictionTime).DoubleValue();
                float predictedLatitude = (mobj->m_latitude + mobj->m_vel.y * predictionTime).DoubleValue();
                
                Vector3<float> currentPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
                
                // only render trails on visible hemisphere
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                float dotProduct = currentPos * globeToCamera;
                if (dotProduct < 0.0f) continue;
                
                Vector3<float> normal = currentPos;
                normal.Normalise();
                
                float elevation = 0.0f; 
                
                bool isMovingObject = wobj->IsMovingObject();
                bool isAirUnit = false;
                
                if (isMovingObject) {
                    MovingObject* movingObj = (MovingObject*)wobj;
                    isAirUnit = (movingObj->m_movementType == MovingObject::MovementTypeAir);
                    
                    if (isAirUnit) {
                        elevation = 0.015f;
                    } else if (movingObj->m_movementType == MovingObject::MovementTypeSea) {
                        elevation = 0.001f;
                    } else {
                        elevation = 0.006f;
                    }
                } else {
                    elevation = 0.006f;
                }
                
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
                    
                    g_renderer3d->UnitTrailLine3D(prevPos.x, prevPos.y, prevPos.z,
                                                 pos3D.x, pos3D.y, pos3D.z, segmentColour);
                    prevPos = pos3D;
                }
            }
        }
    }
}

//
// gunfire trail rendering

// remains the same as 2d gunfire trails but gunfire can now shoot
// at nukes above the globe!

void MapRenderer::Render3DGunfire()
{
    if (!m_3DGlobeMode) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render gunfire for all objects
    for (int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i) {
        if (g_app->GetWorld()->m_gunfire.ValidIndex(i)) {
            GunFire *gunfire = g_app->GetWorld()->m_gunfire[i];
            
            bool shouldRender = (myTeamId == -1 ||
                               gunfire->m_teamId == myTeamId ||
                               gunfire->m_visible[myTeamId] ||
                               m_renderEverything);
            
            if (!shouldRender) continue;
            
            // check for gunfire history
            LList<Vector3<Fixed> *>& history = gunfire->GetHistory();
            if (history.Size() == 0) continue;
            
            // calculate gunfires 3D position 
            Vector3<float> gunfirePos3D = CalculateGunfire3DPosition(gunfire);
            
            // viewport culling for gunfire
            Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
            globeToCamera.Normalise();
            Vector3<float> gunfireToCameraDir = gunfirePos3D;
            gunfireToCameraDir.Normalise();
            float dotProduct = gunfireToCameraDir * globeToCamera;
            if (dotProduct < -0.3f) continue; //
            
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
                        launchPos += launchNormal * 0.01f;
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
                    
                    lastProgress = fmax(0.0f, fmin(1.0f, lastProgress));
                    thisProgress = fmax(0.0f, fmin(1.0f, thisProgress));
                    
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
                    lastPos3D += lastNormal * 0.01f; 
                    
                    Vector3<float> thisNormal = thisPos3D;
                    thisNormal.Normalise();
                    thisPos3D += thisNormal * 0.01f;
                }
                
                Colour segmentColour = colour;
                segmentColour.m_a = 255 - (255 * (float)j / maxHistorySize);
                
                g_renderer3d->EffectsLine3D(lastPos3D.x, lastPos3D.y, lastPos3D.z,
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
                        launchPos += launchNormal * 0.01f;
                    }
                    
                    Nuke* targetNuke = (Nuke*)targetObj;
                    Vector3<float> targetPos = CalculateNuke3DPosition(targetNuke);
                    
                    Vector3<float> lastSurfacePos = ConvertLongLatTo3DPosition(
                        lastHistoryPos->x.DoubleValue(), lastHistoryPos->y.DoubleValue()
                    );
                    
                    float lastDistanceToTarget = (targetPos - lastSurfacePos).Mag();
                    float totalFlightDistance = (targetPos - launchPos).Mag();
                    float lastProgress = 1.0f - (lastDistanceToTarget / totalFlightDistance);
                    lastProgress = fmax(0.0f, fmin(1.0f, lastProgress));
                    
                    lastPos3D = launchPos + (targetPos - launchPos) * lastProgress;

                    //
                    // default gunfire handling

                } else {
                    Vector3<float> lastSurfacePos = ConvertLongLatTo3DPosition(
                        lastHistoryPos->x.DoubleValue(), lastHistoryPos->y.DoubleValue()
                    );
                    Vector3<float> lastNormal = lastSurfacePos;
                    lastNormal.Normalise();
                    lastPos3D = lastSurfacePos + lastNormal * 0.01f;
                }
                
                Colour currentColour = colour;
                currentColour.m_a = 255;
                
                g_renderer3d->EffectsLine3D(lastPos3D.x, lastPos3D.y, lastPos3D.z,
                                           gunfirePos3D.x, gunfirePos3D.y, gunfirePos3D.z, currentColour);
            }
        }
    }
}

//
// explosion rendering

// we now curve the explosion texture to follow the globe surface
// to prevent the texture from being stretched out in the x direction

void MapRenderer::Render3DExplosions()
{
    if (!m_3DGlobeMode) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    for (int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i) {
        if (g_app->GetWorld()->m_explosions.ValidIndex(i)) {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            
            bool shouldRender = (myTeamId == -1 ||
                               explosion->m_visible[myTeamId] ||
                               m_renderEverything);
            
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
            
            // viewport culling
            Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
            globeToCamera.Normalise();
            float dotProduct = explosionPos * globeToCamera;
            if (dotProduct < 0.0f) continue;
            
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
            explosionSize = fmax(explosionSize, 0.01f); 
            explosionSize = fmin(explosionSize, 0.2f);    
            
            //
            // for large explosions, create curved geometry to follow globe surface

            if (explosionSize > 0.05f) {
                
                
                Vector3<float> normal = explosionPos;
                normal.Normalise();
                
                // Offset slightly above surface
                explosionPos += normal * 0.001f;

                g_renderer3d->EffectsSprite3D(explosionImg, explosionPos.x, explosionPos.y, explosionPos.z,
                                             explosionSize * 2.0f, explosionSize * 2.0f, colour, BILLBOARD_SURFACE_ALIGNED);
                
            } else {

                //
                // smaller explosions such as gunfire hits dont need to be curved

                Vector3<float> normal = explosionPos;
                normal.Normalise();
                
                explosionPos += normal * 0.001f;
                
                g_renderer3d->EffectsSprite3D(explosionImg, explosionPos.x, explosionPos.y, explosionPos.z,
                                             explosionSize * 2.0f, explosionSize * 2.0f, colour, BILLBOARD_SURFACE_ALIGNED);
            }
        }
    }
}

//
// sonar ping rendering

// just like explosions, we curve the sonar ping texture to follow the globe surface
// to prevent the texture from being stretched out in the x direction

void MapRenderer::Render3DSonarPing()
{
    if (!m_3DGlobeMode) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // render all sonar pings
    for (int i = 0; i < m_animations.Size(); ++i) {
        if (m_animations.ValidIndex(i)) {
            AnimatedIcon *anim = m_animations[i];
            
            bool shouldRender = true;
            if (anim->m_animationType == AnimationTypeSonarPing) {
                SonarPing *ping = (SonarPing*)anim;
                shouldRender = (myTeamId == -1 || ping->m_visible[myTeamId]);
            }
            
            if (!shouldRender) continue;
            
            if (anim->m_animationType == AnimationTypeSonarPing) {
                
                SonarPing *ping = (SonarPing*)anim;
                
                // calculate ping parameters, same as 2D logic
                float timePassed = GetHighResTime() - ping->m_startTime;
                float pingSize = timePassed * 2.5f;
                if (pingSize > 5.0f) continue;  // skip expired pings
                
                Vector3<float> pingPos = ConvertLongLatTo3DPosition(
                    ping->m_longitude, 
                    ping->m_latitude
                );
                
                // viewport culling
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                float dotProduct = pingPos * globeToCamera;
                if (dotProduct < 0.0f) continue;
                
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
                    
                // create inner and outer circles for thickness
                float innerRadius = ping3DSize * 0.95f;
                float outerRadius = ping3DSize * 1.05f;
                    
                //
                // render the two circles using 3D batching system
                
                // create inner circle using line segments
                Vector3<float> prevInner;
                for (int seg = 0; seg <= numSegments; ++seg) {
                    float angle = 2.0f * M_PI * seg / numSegments;
                    Vector3<float> offset = tangent1 * sinf(angle) + tangent2 * cosf(angle);
                    Vector3<float> inner = pingPos + offset * innerRadius;
                    inner.Normalise();
                    inner = inner * 1.001f;
                    
                    if (seg > 0) {
                        g_renderer3d->EffectsLine3D(prevInner.x, prevInner.y, prevInner.z,
                                                   inner.x, inner.y, inner.z, colour);
                    }
                    prevInner = inner;
                }
                    
                // create outer circle using line segments
                Vector3<float> prevOuter;
                for (int seg = 0; seg <= numSegments; ++seg) {
                    float angle = 2.0f * M_PI * seg / numSegments;
                    Vector3<float> offset = tangent1 * sinf(angle) + tangent2 * cosf(angle);
                    Vector3<float> outer = pingPos + offset * outerRadius;
                    outer.Normalise();
                    outer = outer * 1.001f;
                    
                    if (seg > 0) {
                        g_renderer3d->EffectsLine3D(prevOuter.x, prevOuter.y, prevOuter.z,
                                                   outer.x, outer.y, outer.z, colour);
                    }
                    prevOuter = outer;
                }                             
            }
        }
    }
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

Vector3<float> MapRenderer::CalculateGreatCirclePosition(float startLon, float startLat, 
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

float MapRenderer::CalculateBallisticHeight(float totalDistanceRadians, float progress)
{
    // ballistic trajectory: height follows a parabolic arc
    // height = maxHeight * 4 * progress * (1 - progress)
    // this peaks at progress = 0.5 (midpoint)
    
    float arcFactor = 4.0f * progress * (1.0f - progress);
    
    // scale max height based on total distance, longer shots go higher
    float maxHeight = 0.0f;
    float totalDistanceDegrees = totalDistanceRadians * 180.0f / M_PI;
    
    //
    // adjustable nuke height arcs, right now they are pretty intense
    // as it looks good. at some point i might make them more subtle

        if (totalDistanceDegrees > 90.0f) {
            maxHeight = 0.2f;
        }
        else if (totalDistanceDegrees > 80.0f) {
            maxHeight = 0.19f;
        }
        else if (totalDistanceDegrees > 75.0f) {
            maxHeight = 0.18f;
        }
        else if (totalDistanceDegrees > 70.0f) {
            maxHeight = 0.16f;
        }
        else if (totalDistanceDegrees > 65.0f) {
            maxHeight = 0.15f;
        }
        else if (totalDistanceDegrees > 60.0f) {
            maxHeight = 0.14f;
        }
        else if (totalDistanceDegrees > 55.0f) {
            maxHeight = 0.13f;
        }
        else if (totalDistanceDegrees > 50.0f) {
            maxHeight = 0.12f;
        }
        else if (totalDistanceDegrees > 45.0f) {
            maxHeight = 0.11f;
        }
        else if (totalDistanceDegrees > 40.0f) {
            maxHeight = 0.09f;
        }
        else if (totalDistanceDegrees > 35.0f) {
            maxHeight = 0.08f;
        }
        else if (totalDistanceDegrees > 30.0f) {
            maxHeight = 0.07f;
        }
        else if (totalDistanceDegrees > 25.0f) {
            maxHeight = 0.06f;
        }
        else if (totalDistanceDegrees > 20.0f) {
            maxHeight = 0.05f;
        }
        else if (totalDistanceDegrees > 15.0f) {
            maxHeight = 0.045f;
        }
        else if (totalDistanceDegrees > 10.0f) {
            maxHeight = 0.04f;
        }
        else if (totalDistanceDegrees > 5.0f) {
            maxHeight = 0.035f;
        }
        else {
            maxHeight = 0.03f;
        }
    
    return maxHeight * arcFactor;
}

//
// calculate 3D nuke position using great circle + ballistic arc

Vector3<float> MapRenderer::CalculateNuke3DPosition(Nuke* nuke)
{
    if (!m_3DGlobeMode || !nuke) {
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

Vector3<float> MapRenderer::CalculateHistoricalNuke3DPosition(Nuke* nuke, const Vector3<Fixed>& historicalPos)
{
    if (!m_3DGlobeMode || !nuke) {
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

Vector3<float> MapRenderer::CalculateGunfire3DPosition(GunFire* gunfire)
{
    if (!m_3DGlobeMode || !gunfire) {
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
        return surfacePos + normal * 0.01f;
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
        launchPos += launchNormal * 0.01f; // Slightly above surface
    } else {
        // Fallback to surface position
        Vector3<float> surfacePos = ConvertLongLatTo3DPosition(gunfireLongitude, gunfireLatitude);
        Vector3<float> normal = surfacePos;
        normal.Normalise();
        return surfacePos + normal * 0.01f;
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
    progress = fmax(0.0f, fmin(1.0f, progress)); // Clamp to [0,1]
    
    // Interpolate along 3D trajectory from launch to target
    Vector3<float> gunfirePos = launchPos + (targetPos - launchPos) * progress;
    
    return gunfirePos;
}

//
// calculate historical nuke position using age-based progress
// this solves the issue of the nuke trajectory becoming corrupted
// and resetting to a vertical line half way through big arcs

Vector3<float> MapRenderer::CalculateHistoricalNuke3DPositionByAge(Nuke* nuke, const Vector3<Fixed>& historicalPos, float historicalProgress)
{
    if (!m_3DGlobeMode || !nuke) {
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