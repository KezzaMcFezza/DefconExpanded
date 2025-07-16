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
        AppDebugOut("3D Globe Mode: DISABLED\n");
    }
}

//
// main rendering for the 3D globe

void MapRenderer::Render3DGlobe()
{
    // begin draw call tracking
    g_renderer3d->BeginFrame3D();
    
    float aspect = (float)g_windowManager->WindowW() / (float)g_windowManager->WindowH();
    g_renderer3d->SetPerspective(60.0f, aspect, 0.1f, 100.0f);
    
    // set up 3D camera, reuse lobby camera logic
    g_renderer3d->SetLookAt(
        m_globe3DCamera.m_cameraPos.x, m_globe3DCamera.m_cameraPos.y, m_globe3DCamera.m_cameraPos.z,
        m_globe3DCamera.m_cameraTarget.x, m_globe3DCamera.m_cameraTarget.y, m_globe3DCamera.m_cameraTarget.z,
        m_globe3DCamera.m_cameraUp.x, m_globe3DCamera.m_cameraUp.y, m_globe3DCamera.m_cameraUp.z
    );
    
    // set camera position for billboard calculations
    g_renderer3d->SetCameraPosition(m_globe3DCamera.m_cameraPos.x, 
                                   m_globe3DCamera.m_cameraPos.y, 
                                   m_globe3DCamera.m_cameraPos.z);
    
    // enable depth testing for 3D sprites
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // use additive blending like 2D mode
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    // new fog mode is being used
    // distance fog is useless for globe mode as when you zoom in and out the coastlines
    // would dissapear and reappear, so we use orientation fog instead
    // this is a much better looking solution
    g_renderer3d->EnableOrientationFog(0.01f, 0.08f, 0.05f, 20.0f, 
                                      m_globe3DCamera.m_cameraPos.x,
                                      m_globe3DCamera.m_cameraPos.y,
                                      m_globe3DCamera.m_cameraPos.z);
    
    // use vbo system
    g_renderer3d->RenderMegaVBO3D("GlobeCoastlines");
    
    if (g_preferences->GetInt(PREFS_GRAPHICS_BORDERS) == 1) {
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

    g_renderer3d->BeginUnitMainBatch3D();       // Main unit sprites + city icons
    g_renderer3d->BeginUnitRotatingBatch3D();   // Rotating sprites (aircraft, but not nukes anymore)
    g_renderer3d->BeginUnitTrailBatch3D();      // Unit movement trails
    g_renderer3d->BeginUnitStateBatch3D();      // Unit state icons (fighters/bombers on units)
    g_renderer3d->BeginUnitNukeBatch3D();       // Small nuke icons on units
    g_renderer3d->BeginNuke3DModelBatch3D();    // 3D nuke models (replaces flat nuke sprites)
    g_renderer3d->BeginEffectsLineBatch3D();    // All line effects (gunfire trails, etc.)
    g_renderer3d->BeginEffectsSpriteBatch3D();  // All sprite effects (explosions, sonar pings, etc.)

    // render all 3d objects inside the scene

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
// we use default DEFCON controls

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
        float rotationSpeed = 2.0f * g_advanceTime;  // smooth rotation speed
        
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
        
        // Q and E for zoom
        if (g_keys[KeyQ]) {
            m_globe3DCamera.m_cameraDistance += 5.0f * g_advanceTime;
        }
        if (g_keys[KeyE]) {
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
        }
        
        float deltaX = g_inputManager->m_mouseX - m_globe3DCamera.m_lastMouseX;
        float deltaY = g_inputManager->m_mouseY - m_globe3DCamera.m_lastMouseY;
        
        m_globe3DCamera.m_cameraTheta += deltaX * 0.01f;
        m_globe3DCamera.m_cameraPhi += deltaY * 0.01f;
        
        m_globe3DCamera.m_cameraPhi = fmax(-M_PI/2.0f + 0.1f, fmin(M_PI/2.0f - 0.1f, m_globe3DCamera.m_cameraPhi));
        m_globe3DCamera.m_cameraPos.x = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.y = m_globe3DCamera.m_cameraDistance * sin(m_globe3DCamera.m_cameraPhi);
        m_globe3DCamera.m_cameraPos.z = m_globe3DCamera.m_cameraDistance * cos(m_globe3DCamera.m_cameraTheta) * cos(m_globe3DCamera.m_cameraPhi);
        
        m_globe3DCamera.m_lastMouseX = g_inputManager->m_mouseX;
        m_globe3DCamera.m_lastMouseY = g_inputManager->m_mouseY;

    } else {

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
            Nuke* nuke = (Nuke*)wobj;
            
            //
            // this is claudes attempt at getting the nuke to face forward along its trajectory
            // the issue that im having is that the nuke only faces the right direction when
            // we hit the half way point in the arc, or the highest point in the arc, i will
            // continue to work on this but for now this is what we get

            Vector3<float> direction(0, 0, 1);  // Default forward direction
            if (nuke->m_vel.x != 0 || nuke->m_vel.y != 0) {
                // Calculate current position
                Vector3<float> currentPos = CalculateNuke3DPosition(nuke);
                
                // Calculate where nuke will be next frame (same as trail logic)
                Fixed predictionTime = Fixed::FromDouble(g_predictionTime + 0.1) * g_app->GetWorld()->GetTimeScaleFactor();
                float nextLongitude = (nuke->m_longitude + nuke->m_vel.x * predictionTime).DoubleValue();
                float nextLatitude = (nuke->m_latitude + nuke->m_vel.y * predictionTime).DoubleValue();
                
                // Calculate next 3D position with proper height
                Vector3<float> nextSurfacePos = ConvertLongLatTo3DPosition(nextLongitude, nextLatitude);
                float nextHeight = CalculateNuke3DHeight(nuke);  // Use same height calculation
                Vector3<float> nextNormal = nextSurfacePos;
                nextNormal.Normalise();
                Vector3<float> nextPos = nextSurfacePos + nextNormal * nextHeight;
                
                // Direction = where we're going minus where we are
                direction = nextPos - currentPos;
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
                sizeCap *= 8;
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
                int enemyTrailLimit = isNuke ? 32 : 16;  // 4*8 for nukes, 4*4 for others
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
                
                // create line segments for nukes with dashed effect
                Vector3<float> prevPos = currentPos;
                
                // track dash position across all segments to avoid gaps
                float dashLength = 0.012f;      
                float gapLength = 0.006f;       
                float totalDashUnit = dashLength + gapLength;
                float currentDashPosition = 0.0f;  
                bool inDash = true;  
                
                for (int j = 0; j < maxSize; ++j) {
                    Vector3<Fixed> *historyPos = history[j];
                    Vector3<float> pos3D = CalculateHistoricalNuke3DPosition(nuke, *historyPos);
                    
                    Colour segmentColour = colour;
                    segmentColour.m_a = 255 - 255 * (float)j / (float)maxSize;
                    
                    // create multiple smaller interpolated segments
                    Vector3<float> segmentDir = pos3D - prevPos;
                    float segmentLength = segmentDir.Mag();
                    
                    if (segmentLength > 0.001f) {  // only process if segment is long enough

                        //
                        // calculate number of interpolation steps based on segment length
                        // more steps for longer segments to keep them smooth

                        int interpolationSteps = (int)(segmentLength * 50.0f) + 1;      // adaptive subdivision
                        interpolationSteps = fmax(1, fmin(interpolationSteps, 20));     // clamp between 1-20 steps
                        
                        Vector3<float> stepVector = segmentDir / (float)interpolationSteps;
                        
                        //
                        // create smooth interpolated line segments

                        for (int step = 0; step < interpolationSteps; ++step) {
                            Vector3<float> interpolatedStart = prevPos + stepVector * (float)step;
                            Vector3<float> interpolatedEnd = prevPos + stepVector * (float)(step + 1);
                            Vector3<float> interpolatedDir = interpolatedEnd - interpolatedStart;
                            float interpolatedLength = interpolatedDir.Mag();
                            
                            if (interpolatedLength > 0.001f) {
                                interpolatedDir.Normalise();
                                
                                Vector3<float> interpolatedSegmentStart = interpolatedStart;
                                float remainingInterpolatedLength = interpolatedLength;
                                
                                while (remainingInterpolatedLength > 0.001f) {
                                    float distanceToNextTransition;
                                    
                                    // Currently in a dash - how far until we need to start a gap?
                                    if (inDash) {
                                        distanceToNextTransition = dashLength - currentDashPosition;
                                    } else {
                                        distanceToNextTransition = gapLength - currentDashPosition;
                                    }
                                    
                                    // Take the minimum of remaining segment length and distance to next transition
                                    float actualStepDistance = fmin(remainingInterpolatedLength, distanceToNextTransition);
                                    Vector3<float> interpolatedSegmentEnd = interpolatedSegmentStart + interpolatedDir * actualStepDistance;
                                    
                                    // only draw if were currently in a dash and not a gap
                                    if (inDash) {
                                        g_renderer3d->EffectsLine3D(interpolatedSegmentStart.x, interpolatedSegmentStart.y, interpolatedSegmentStart.z,
                                                                   interpolatedSegmentEnd.x, interpolatedSegmentEnd.y, interpolatedSegmentEnd.z, segmentColour);
                                    }
                                    
                                    currentDashPosition += actualStepDistance;
                                    remainingInterpolatedLength -= actualStepDistance;
                                    interpolatedSegmentStart = interpolatedSegmentEnd;
                                    
                                    //
                                    // check if we need to transition between dash and gap

                                    if (inDash && currentDashPosition >= dashLength) {
                                        inDash = false;
                                        currentDashPosition = 0.0f;
                                    } else if (!inDash && currentDashPosition >= gapLength) {
                                        inDash = true;
                                        currentDashPosition = 0.0f;
                                    }
                                }
                            }
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
                    
                    g_renderer3d->EffectsLine3D(prevPos.x, prevPos.y, prevPos.z,
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

// we use the same trajectory calculation that nuke already does for the 2d curve
// currently a bit buggy, but it looks great when the nukes dont cross the poles

// wan may please fix this? :)

// ******************************************************************************************************************************

// 
// try to create good looking height curves

float MapRenderer::CalculateNuke3DHeight(Nuke* nuke)
{
    if (!m_3DGlobeMode || !nuke) return 0.0f;
    
    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
    Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
    
    if (nuke->m_totalDistance <= 0) return 0.0f;
    
    Fixed remainingDistance = (target - pos).Mag();
    Fixed fractionDistance = 1 - remainingDistance / nuke->m_totalDistance;
    
    // convert fractionDistance (1->0) to trajectory progress (0->1)
    float progress = 1.0f - fractionDistance.DoubleValue();
    
    // parabolic arc: height = maxHeight * 4 * progress * (1 - progress)
    // this peaks at progress = 0.5 (middle of trajectory)
    float arcFactor = 4.0f * progress * (1.0f - progress);
    float trajectoryDistance = nuke->m_totalDistance.DoubleValue();
    float maxHeight = 0.0f;
    
    // claudes attempt at realistic arc heights
    if (trajectoryDistance > 100.0f) {
        maxHeight = 0.8f;
    } else if (trajectoryDistance > 50.0f) {
        maxHeight = 0.4f;
    } else {
        maxHeight = 0.2f;
    }
    
    return maxHeight * arcFactor;
}

//
// pretty much the same as 2d curve logic, but with 3d height curve

Vector3<float> MapRenderer::CalculateNuke3DPosition(Nuke* nuke)
{
    if (!m_3DGlobeMode || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (nuke->m_longitude + nuke->m_vel.x * predictionTime).DoubleValue();
    float predictedLatitude = (nuke->m_latitude + nuke->m_vel.y * predictionTime).DoubleValue();
    
    // convert to 3D surface position
    Vector3<float> surfacePos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
    
    // calculate height above surface
    float height = CalculateNuke3DHeight(nuke);
    
    // move away from globe center
    Vector3<float> normal = surfacePos;
    normal.Normalise();
    
    return surfacePos + normal * height;
}

//
// same as above, but for the actual trajectory

Vector3<float> MapRenderer::CalculateHistoricalNuke3DPosition(Nuke* nuke, const Vector3<Fixed>& historicalPos)
{
    if (!m_3DGlobeMode || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    Vector3<float> surfacePos = ConvertLongLatTo3DPosition(
        historicalPos.x.DoubleValue(), 
        historicalPos.y.DoubleValue()
    );
    
    // now calculate what the arc height would have been at this point
    if (nuke->m_totalDistance <= 0) return surfacePos;
    
    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
    Fixed distanceFromTarget = (target - historicalPos).Mag();
    Fixed fractionDistance = 1 - distanceFromTarget / nuke->m_totalDistance;
    
    float progress = 1.0f - fractionDistance.DoubleValue();
    float arcFactor = 4.0f * progress * (1.0f - progress);
    float trajectoryDistance = nuke->m_totalDistance.DoubleValue();
    float maxHeight = 0.0f;
    
    if (trajectoryDistance > 100.0f) {
        maxHeight = 0.8f;
    } else if (trajectoryDistance > 50.0f) {
        maxHeight = 0.4f;
    } else {
        maxHeight = 0.2f;
    }
    
    float height = maxHeight * arcFactor;
    
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