#include "lib/universal_include.h"

#include <math.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render/renderer_3d.h"
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

// TODO: add batching for the 3d world objects, i need to preserve gl_blend as
//       i have had issues in the past with blending and batching in map renderer

void MapRenderer::Render3DGlobeView()
{
    float aspect = (float)g_windowManager->WindowW() / (float)g_windowManager->WindowH();
    g_renderer3d->SetPerspective(60.0f, aspect, 0.1f, 100.0f);
    
    // set up 3D camera, reuse lobby camera logic
    g_renderer3d->SetLookAt(
        m_globe3DCamera.m_cameraPos.x, m_globe3DCamera.m_cameraPos.y, m_globe3DCamera.m_cameraPos.z,
        m_globe3DCamera.m_cameraTarget.x, m_globe3DCamera.m_cameraTarget.y, m_globe3DCamera.m_cameraTarget.z,
        m_globe3DCamera.m_cameraUp.x, m_globe3DCamera.m_cameraUp.y, m_globe3DCamera.m_cameraUp.z
    );
    
    // Enable depth testing for 3D
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // new fog mode is being used
    // distance fog is useless for globe mode as when you zoom in and out the coastlines
    // would dissapear and reappear, so we use orientation fog instead
    // this is a much better looking solution
    g_renderer3d->EnableOrientationFog(0.02f, 0.02f, 0.05f, 1.0f, 
                                      m_globe3DCamera.m_cameraPos.x,
                                      m_globe3DCamera.m_cameraPos.y,
                                      m_globe3DCamera.m_cameraPos.z);
    
    // use vbo system
    g_renderer3d->RenderMegaVBO3D("GlobeCoastlines");
    
    if (g_preferences->GetInt(PREFS_GRAPHICS_BORDERS) == 1) {
        g_renderer3d->RenderMegaVBO3D("GlobeBorders");
    }
    
    // Disable fog after rendering coastlines and borders
    g_renderer3d->DisableFog();

    Render3DGlobeCities();
    Render3DUnits();
    Render3DUnitTrails();
    Render3DGunfire();
    Render3DExplosions();
    Render3DSonarPing();
    glDisable(GL_DEPTH_TEST);

    // reset to 2d viewport
    g_renderer->Reset2DViewport();
}

//
// camera controls for the 3d globe, to preserve muscle memory
// we use default defcon controls

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
    
    // render city BMPs as 3D quads
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    
    g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    float uv_u1, uv_v1, uv_u2, uv_v2;
    g_renderer->GetImageUVCoords(cityImg, uv_u1, uv_v1, uv_u2, uv_v2);
    unsigned int textureID = g_renderer->GetEffectiveTextureID(cityImg);
    
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
        
        Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
        if (fabsf(normal.y) > 0.9f) {
            up = Vector3<float>(1.0f, 0.0f, 0.0f);
        }
        
        Vector3<float> tangent1 = normal ^ up;
        tangent1.Normalise();
        Vector3<float> tangent2 = normal ^ tangent1;
        tangent2.Normalise();
        
        // prevent z fighting with the globe vertices
        cityPos += normal * 0.002f;
        
        Vector3<float> v1 = cityPos - tangent1 * size - tangent2 * size;
        Vector3<float> v2 = cityPos + tangent1 * size - tangent2 * size;
        Vector3<float> v3 = cityPos + tangent1 * size + tangent2 * size;
        Vector3<float> v4 = cityPos - tangent1 * size + tangent2 * size;
        
        g_renderer3d->BeginTexturedQuad3D(textureID, col);
        g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v1);
        g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v1);
        g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v2);
        g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v2);
        g_renderer3d->EndTexturedQuad3D();
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
    
    g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//
// function to render units on the globe
// i will refactor this once i have a better understanding of the code that claude generated

void MapRenderer::Render3DUnits()
{
    if (!m_3DGlobeMode) return;
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    // Enhanced structure to handle different unit types
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
            
            // Check if unit should be rendered
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               m_renderEverything);
            
            if (!shouldRender) continue;
            
            Vector3<float> unitPos;
            bool isNuke = (wobj->m_type == WorldObject::TypeNuke);
            
            // SPECIAL HANDLING FOR NUKES: Use 3D arc position
            if (isNuke) {
                Nuke* nuke = (Nuke*)wobj;
                unitPos = CalculateNuke3DPosition(nuke);
                
                // More lenient viewport culling for nukes (they can be high above surface)
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
                
                // Enhanced viewport culling for surface units - only render visible hemisphere
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                float dotProduct = unitPos * globeToCamera;
                if (dotProduct < 0.0f) continue;
            }
            
            // Get unit image (use GetBmpImage with current state for state-dependent textures like silos)
            Image *unitImg = wobj->GetBmpImage(wobj->m_currentState);
            if (!unitImg) continue;
            
            // Enhanced size calculation based on unit type
            float baseSize = wobj->GetSize3D().DoubleValue();
            float size;
            
            // FIXED: Different size multipliers for different unit categories
            bool isMovingObject = wobj->IsMovingObject();
            bool isAirUnit = false;
            
            if (isMovingObject) {
                MovingObject *mobj = (MovingObject*)wobj;
                isAirUnit = (mobj->m_movementType == MovingObject::MovementTypeAir);
                
                if (isAirUnit) {
                    // Air units: 25% smaller
                    size = baseSize * 0.0075f * 0.75f;
                } else if (mobj->m_movementType == MovingObject::MovementTypeSea) {
                    // Sea units: 25% bigger
                    size = baseSize * 0.0075f * 1.25f;
                } else {
                    // Moving land units: normal size
                    size = baseSize * 0.0075f;
                }
            } else {
                // Static land units (silos, airbases, radar): DOUBLE size
                size = baseSize * 0.0075f * 2.0f;
            }
            
            // Special size scaling for nukes in 3D space
            if (isNuke) {
                size = baseSize * 0.008f; // Slightly larger for visibility at high altitude
            }
            
            size = fmax(size, 0.002f);  // Minimum size
            size = fmin(size, 0.04f);   // Maximum size
            
            // Get unit color (team color or default)
            Colour unitColor = White;
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            if (team) {
                unitColor = team->GetTeamColour();
            }
            unitColor.m_a = 255; // Full opacity for units
            
            // Calculate rotation angle for air units (same as 2D system)
            float rotationAngle = 0.0f;
            if (isAirUnit || isNuke) {
                // Use same angle calculation as 2D system
                rotationAngle = atan(-wobj->m_vel.x.DoubleValue() / wobj->m_vel.y.DoubleValue());
                if (wobj->m_vel.y < 0) rotationAngle += M_PI;
            }
            
            // Calculate distance to camera for sorting
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
    
    // Set up OpenGL state for rendering
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending like cities
    
    // Second pass: render unit sprites
    for (int i = 0; i < visibleUnits.Size(); i++) {
        UnitRenderInfo& info = visibleUnits[i];
        Vector3<float> unitPos = info.worldPos;
        float size = info.size;
        Colour color = info.color;
        Image *unitImg = info.image;
        
        // Get UV coordinates and texture ID for sprite atlas
        float uv_u1, uv_v1, uv_u2, uv_v2;
        g_renderer->GetImageUVCoords(unitImg, uv_u1, uv_v1, uv_u2, uv_v2);
        unsigned int textureID = g_renderer->GetEffectiveTextureID(unitImg);
        
        // Create proper "upward-facing" billboard that doesn't flip based on hemisphere
        Vector3<float> normal;
        
        if (info.isNuke) {
            // For nukes in 3D space, create billboard facing camera
            Vector3<float> cameraDir = m_globe3DCamera.m_cameraPos - unitPos;
            cameraDir.Normalise();
            
            // Create billboard facing camera
            Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
            Vector3<float> right = up ^ cameraDir;
            right.Normalise();
            up = cameraDir ^ right;
            up.Normalise();
            
            // Apply rotation for nuke orientation
            if (info.rotationAngle != 0.0f) {
                float cosRot = cosf(info.rotationAngle);
                float sinRot = sinf(info.rotationAngle);
                Vector3<float> rotatedRight = right * cosRot + up * sinRot;
                Vector3<float> rotatedUp = -right * sinRot + up * cosRot;
                right = rotatedRight;
                up = rotatedUp;
            }
            
            // Create quad vertices for camera-facing billboard
            Vector3<float> v1 = unitPos - right * size + up * size;
            Vector3<float> v2 = unitPos + right * size + up * size;
            Vector3<float> v3 = unitPos + right * size - up * size;
            Vector3<float> v4 = unitPos - right * size - up * size;
            
            // Render nuke sprite
            g_renderer3d->BeginTexturedQuad3D(textureID, color);
            g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
            g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
            g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
            g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
            g_renderer3d->EndTexturedQuad3D();
        } else {
            // Surface units use globe-surface billboards
            normal = unitPos;
            normal.Normalise();
            
            // Create consistent "up" direction relative to globe north pole
            Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
            
            // For units right at poles, use a stable tangent
            Vector3<float> tangent1;
            if (fabsf(normal.y) > 0.98f) {
                // At poles, use east direction
                tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
            } else {
                // Create tangent pointing "east" (perpendicular to north and surface normal)
                tangent1 = globeUp ^ normal;
                tangent1.Normalise();
            }
            
            // Create tangent pointing "north" (always toward globe north pole)
            Vector3<float> tangent2 = normal ^ tangent1;
            tangent2.Normalise();
            
            // For air units, apply rotation to the tangent vectors
            if (info.isAirUnit) {
                // Rotate tangent vectors by the unit's heading angle
                Vector3<float> rotatedTangent1 = tangent1 * cosf(info.rotationAngle) + tangent2 * sinf(info.rotationAngle);
                Vector3<float> rotatedTangent2 = tangent2 * cosf(info.rotationAngle) - tangent1 * sinf(info.rotationAngle);
                tangent1 = rotatedTangent1;
                tangent2 = rotatedTangent2;
            }
            
            // Offset slightly above surface (higher than cities to avoid z-fighting)
            unitPos += normal * 0.006f;
            
            // Create quad vertices with FIXED orientation (always upward-facing)
            Vector3<float> v1 = unitPos - tangent1 * size + tangent2 * size;  // Top-left
            Vector3<float> v2 = unitPos + tangent1 * size + tangent2 * size;  // Top-right  
            Vector3<float> v3 = unitPos + tangent1 * size - tangent2 * size;  // Bottom-right
            Vector3<float> v4 = unitPos - tangent1 * size - tangent2 * size;  // Bottom-left
            
            // Render unit sprite with proper UV mapping (no flipping)
            g_renderer3d->BeginTexturedQuad3D(textureID, color);
            g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);  // Top-left
            g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);  // Top-right
            g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);  // Bottom-right
            g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);  // Bottom-left
            g_renderer3d->EndTexturedQuad3D();
        }
    }
    
    // PART 3: Render small unit capacity icons (fighters, bombers, nukes on units)
    for (int i = 0; i < visibleUnits.Size(); i++) {
        UnitRenderInfo& info = visibleUnits[i];
        WorldObject* wobj = info.unit;
        Vector3<float> unitPos = info.worldPos;
        float unitSize = info.size;
        Colour teamColor = info.color;
        
        // Skip capacity icons for nukes (they don't carry anything)
        if (info.isNuke) continue;
        
        // Check if we should render capacity indicators for this team
        int myTeamId = g_app->GetWorld()->m_myTeamId;
        bool shouldRenderCapacity = (myTeamId == -1 || 
                                   wobj->m_teamId == myTeamId || 
                                   g_app->GetGame()->m_winner != -1);
        
        if (!shouldRenderCapacity) continue;
        
        Vector3<float> normal = unitPos;
        normal.Normalise();
        
        // FIX: Use the same reference frame as main unit sprites
        unitPos += normal * 0.006f;
        
        // Create consistent tangent vectors for icon positioning
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
        
        // Manual positioning offsets to fix visual alignment
        Vector3<float> rightOffset = tangent1 * (unitSize * 0.16f);   // Move slightly right
        Vector3<float> downOffset = -tangent2 * (unitSize * 0.11f);    // Move slightly down
        
        // SILOS: Render small nuke capacity icons
        if (wobj->m_type == WorldObject::TypeSilo) {
            int numNukesInStore = wobj->m_states[0]->m_numTimesPermitted;
            int numNukesInQueue = wobj->m_actionQueue.Size();
            
            if (numNukesInStore > 0) {
                Image *smallNukeImg = g_resource->GetImage("graphics/smallnuke.bmp");
                if (smallNukeImg) {
                    float nukeIconSize = unitSize * 0.35f;
                    float spacing = nukeIconSize * 0.5f;
                    
                    // Position icons below the unit with manual alignment offsets
                    Vector3<float> startPos = unitPos - tangent2 * (unitSize * 0.9f);
                    startPos -= tangent1 * (unitSize * 0.95f);
                    startPos += rightOffset + downOffset;  // Apply manual offsets
                    
                    // Get UV coordinates and texture ID
                    float uv_u1, uv_v1, uv_u2, uv_v2;
                    g_renderer->GetImageUVCoords(smallNukeImg, uv_u1, uv_v1, uv_u2, uv_v2);
                    unsigned int textureID = g_renderer->GetEffectiveTextureID(smallNukeImg);
                    
                    for (int j = 0; j < numNukesInStore; ++j) {
                        Colour iconColor = teamColor;
                        iconColor.m_a = 150;
                        
                        // Dim queued nukes
                        if (j >= (numNukesInStore - numNukesInQueue)) {
                            iconColor.Set(128, 128, 128, 100);
                        }
                        
                        Vector3<float> iconPos = startPos + tangent1 * (j * spacing);
                        
                        // Create small quad for nuke icon
                        Vector3<float> v1 = iconPos - tangent1 * nukeIconSize/2 + tangent2 * nukeIconSize/2;
                        Vector3<float> v2 = iconPos + tangent1 * nukeIconSize/2 + tangent2 * nukeIconSize/2;
                        Vector3<float> v3 = iconPos + tangent1 * nukeIconSize/2 - tangent2 * nukeIconSize/2;
                        Vector3<float> v4 = iconPos - tangent1 * nukeIconSize/2 - tangent2 * nukeIconSize/2;
                        
                        g_renderer3d->BeginTexturedQuad3D(textureID, iconColor);
                        g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
                        g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
                        g_renderer3d->EndTexturedQuad3D();
                    }
                }
            }
        }
        
        // SUBS: Render small nuke capacity icons
        else if (wobj->m_type == WorldObject::TypeSub) {
            int numNukesInStore = wobj->m_states[2]->m_numTimesPermitted;
            int numNukesInQueue = wobj->m_actionQueue.Size();
            
            if (numNukesInStore > 0) {
                Image *smallNukeImg = g_resource->GetImage("graphics/smallnuke.bmp");
                if (smallNukeImg) {
                    float nukeIconSize = unitSize * 0.35f;
                    float spacing = nukeIconSize * 0.5f;
                    
                    // Position icons next to the unit with manual alignment offsets
                    Vector3<float> startPos = unitPos - tangent1 * (unitSize * 0.2f);
                    startPos += rightOffset + downOffset;  // Apply manual offsets
                    
                    // Get UV coordinates and texture ID
                    float uv_u1, uv_v1, uv_u2, uv_v2;
                    g_renderer->GetImageUVCoords(smallNukeImg, uv_u1, uv_v1, uv_u2, uv_v2);
                    unsigned int textureID = g_renderer->GetEffectiveTextureID(smallNukeImg);
                    
                    for (int j = 0; j < numNukesInStore; ++j) {
                        Colour iconColor = teamColor;
                        iconColor.m_a = 150;
                        
                        // Dim queued nukes
                        if (j >= (numNukesInStore - numNukesInQueue)) {
                            iconColor.Set(128, 128, 128, 100);
                        }
                        
                        Vector3<float> iconPos = startPos - tangent1 * (j * spacing);
                        
                        // Create small quad for nuke icon
                        Vector3<float> v1 = iconPos - tangent1 * nukeIconSize/2 + tangent2 * nukeIconSize/2;
                        Vector3<float> v2 = iconPos + tangent1 * nukeIconSize/2 + tangent2 * nukeIconSize/2;
                        Vector3<float> v3 = iconPos + tangent1 * nukeIconSize/2 - tangent2 * nukeIconSize/2;
                        Vector3<float> v4 = iconPos - tangent1 * nukeIconSize/2 - tangent2 * nukeIconSize/2;
                        
                        g_renderer3d->BeginTexturedQuad3D(textureID, iconColor);
                        g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
                        g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
                        g_renderer3d->EndTexturedQuad3D();
                    }
                }
            }
        }
        
        // BOMBERS: Render single nuke icon if carrying nuke
        else if (wobj->m_type == WorldObject::TypeBomber && wobj->m_states[1]->m_numTimesPermitted > 0) {
            Image *smallNukeImg = g_resource->GetImage("graphics/smallnuke.bmp");
            if (smallNukeImg) {
                float nukeIconSize = unitSize * 0.8f;  // Make bomber nuke icon bigger
                
                // Position icon directly at bomber center (perfectly centered)
                Vector3<float> iconPos = unitPos;  // Perfectly centered
                
                // Apply rotation to match bomber orientation
                float rotationAngle = info.rotationAngle;
                Vector3<float> rotatedTangent1 = tangent1;
                Vector3<float> rotatedTangent2 = tangent2;
                
                if (rotationAngle != 0.0f) {
                    // Rotate tangent vectors around the normal axis
                    float cosRot = cosf(rotationAngle);
                    float sinRot = sinf(rotationAngle);
                    
                    rotatedTangent1 = tangent1 * cosRot + tangent2 * sinRot;
                    rotatedTangent2 = -tangent1 * sinRot + tangent2 * cosRot;
                    
                    // Keep icon centered even when rotated
                    iconPos = unitPos;
                }
                
                Colour iconColor = teamColor;
                iconColor.m_a = 150;
                
                // Get UV coordinates and texture ID
                float uv_u1, uv_v1, uv_u2, uv_v2;
                g_renderer->GetImageUVCoords(smallNukeImg, uv_u1, uv_v1, uv_u2, uv_v2);
                unsigned int textureID = g_renderer->GetEffectiveTextureID(smallNukeImg);
                
                // Create quad vertices using rotated tangent vectors (same size as other units)
                Vector3<float> v1 = iconPos - rotatedTangent1 * nukeIconSize/2 + rotatedTangent2 * nukeIconSize/2;
                Vector3<float> v2 = iconPos + rotatedTangent1 * nukeIconSize/2 + rotatedTangent2 * nukeIconSize/2;
                Vector3<float> v3 = iconPos + rotatedTangent1 * nukeIconSize/2 - rotatedTangent2 * nukeIconSize/2;
                Vector3<float> v4 = iconPos - rotatedTangent1 * nukeIconSize/2 - rotatedTangent2 * nukeIconSize/2;
                
                // Render textured quad (same method as other units)
                g_renderer3d->BeginTexturedQuad3D(textureID, iconColor);
                g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
                g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
                g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
                g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
                g_renderer3d->EndTexturedQuad3D();
            }
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
                    
                    // Position icons next to the unit with manual alignment offsets
                    Vector3<float> startPos = unitPos - tangent2 * (unitSize * 0.3f);
                    startPos += rightOffset + downOffset;  // Apply manual offsets
                    
                    // For carriers, offset for predicted position
                    if (wobj->m_type == WorldObject::TypeCarrier) {
                        startPos -= tangent1 * (unitSize * 0.85f);
                    } else {
                        startPos -= tangent1 * (unitSize * 0.5f);
                    }
                    
                    // Get UV coordinates and texture ID
                    float uv_u1, uv_v1, uv_u2, uv_v2;
                    g_renderer->GetImageUVCoords(iconImg, uv_u1, uv_v1, uv_u2, uv_v2);
                    unsigned int textureID = g_renderer->GetEffectiveTextureID(iconImg);
                    
                    for (int j = 0; j < numInStore; ++j) {
                        Colour iconColor = teamColor;
                        iconColor.m_a = 150;
                        
                        // Dim queued units
                        if (j >= (numInStore - numInQueue)) {
                            iconColor.Set(128, 128, 128, 100);
                        }
                        
                        Vector3<float> iconPos = startPos + tangent1 * (j * spacing);
                        
                        // Create small quad for unit icon
                        Vector3<float> v1 = iconPos - tangent1 * iconSize/2 + tangent2 * iconSize/2;
                        Vector3<float> v2 = iconPos + tangent1 * iconSize/2 + tangent2 * iconSize/2;
                        Vector3<float> v3 = iconPos + tangent1 * iconSize/2 - tangent2 * iconSize/2;
                        Vector3<float> v4 = iconPos - tangent1 * iconSize/2 - tangent2 * iconSize/2;
                        
                        g_renderer3d->BeginTexturedQuad3D(textureID, iconColor);
                        g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
                        g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
                        g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
                        g_renderer3d->EndTexturedQuad3D();
                    }
                }
            }
        }
    }
    
    // Reset OpenGL state
    g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
            
            Vector3<float> currentPos;
            
            //
            // special handling for nuke trails

            if (isNuke) {
                Nuke* nuke = (Nuke*)wobj;
                currentPos = CalculateNuke3DPosition(nuke);
                
                // viewport culling
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                Vector3<float> nukeToCameraDir = currentPos;
                nukeToCameraDir.Normalise();
                float dotProduct = nukeToCameraDir * globeToCamera;
                if (dotProduct < -0.3f) continue; // needs to be fine tuned
            } else {

                //
                // regular surface trails for the rest of the units

                Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                float predictedLongitude = (mobj->m_longitude + mobj->m_vel.x * predictionTime).DoubleValue();
                float predictedLatitude = (mobj->m_latitude + mobj->m_vel.y * predictionTime).DoubleValue();
                
                currentPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
                
                // only render trails on visible hemisphere
                Vector3<float> globeToCamera = m_globe3DCamera.m_cameraPos;
                globeToCamera.Normalise();
                float dotProduct = currentPos * globeToCamera;
                if (dotProduct < 0.0f) continue; 
            }
            
            // calculate trail length based on zoom 
            int maxSize = history.Size();
            int sizeCap = (int)(80 * 0.5f); 
            sizeCap /= World::GetGameScale().DoubleValue();
            
            maxSize = (maxSize > sizeCap ? sizeCap : maxSize);
            
            Team *team = g_app->GetWorld()->GetTeam(mobj->m_teamId);
            Colour colour;
            if (team) {
                colour = team->GetTeamColour();
            } else {
                colour = COLOUR_SPECIALOBJECTS;
            }
            
            // reduce the trail length for enemy units
            if (mobj->m_teamId != myTeamId &&
                myTeamId != -1 &&
                myTeamId < g_app->GetWorld()->m_teams.Size() &&
                g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned) {
                maxSize = min(maxSize, 4);
            }
            
            if (maxSize <= 0) continue;
            
            g_renderer3d->BeginLineStrip3D(colour);
            g_renderer3d->LineStripVertex3D(currentPos.x, currentPos.y, currentPos.z);
            
            // save history positions
            for (int j = 0; j < maxSize; ++j) {
                Vector3<Fixed> *historyPos = history[j];
                
                Vector3<float> pos3D;

                //
                // save nuke history positions
                
                if (isNuke) {
                    Nuke* nuke = (Nuke*)wobj;
                    pos3D = CalculateHistoricalNuke3DPosition(nuke, *historyPos);
                } else {

                    //
                    // save surface history positions

                    float thisLongitude = historyPos->x.DoubleValue();
                    float thisLatitude = historyPos->y.DoubleValue();
                    
                    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
                    float predictedLongitude = (mobj->m_longitude + mobj->m_vel.x * predictionTime).DoubleValue();
                    
                    if (predictedLongitude < -170 && thisLongitude > 170) {
                        thisLongitude = -180 - (180 - thisLongitude);
                    }
                    if (predictedLongitude > 170 && thisLongitude < -170) {
                        thisLongitude = 180 + (180 - fabsf(thisLongitude));
                    }
                    
                    pos3D = ConvertLongLatTo3DPosition(thisLongitude, thisLatitude);
                    
                    // offset slightly above surface to avoid z fighting 
                    Vector3<float> normal = pos3D;
                    normal.Normalise();
                    pos3D += normal * 0.008f; 
                }
                
                g_renderer3d->LineStripVertex3D(pos3D.x, pos3D.y, pos3D.z);
            }
            
            g_renderer3d->EndLineStrip3D();
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
                
                g_renderer3d->BeginLineStrip3D(segmentColour);
                g_renderer3d->LineStripVertex3D(lastPos3D.x, lastPos3D.y, lastPos3D.z);
                g_renderer3d->LineStripVertex3D(thisPos3D.x, thisPos3D.y, thisPos3D.z);
                g_renderer3d->EndLineStrip3D();
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
                
                g_renderer3d->BeginLineStrip3D(currentColour);
                g_renderer3d->LineStripVertex3D(lastPos3D.x, lastPos3D.y, lastPos3D.z);
                g_renderer3d->LineStripVertex3D(gunfirePos3D.x, gunfirePos3D.y, gunfirePos3D.z);
                g_renderer3d->EndLineStrip3D();
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
                
                // calculate how many segments we need based on explosion size
                int segments = (int)(explosionSize * 20.0f) + 4; // bigger? more segments
                segments = fmax(segments, 4);
                segments = fmin(segments, 16);
                
                Vector3<float> normal = explosionPos;
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
                
                float uv_u1, uv_v1, uv_u2, uv_v2;
                g_renderer->GetImageUVCoords(explosionImg, uv_u1, uv_v1, uv_u2, uv_v2);
                unsigned int textureID = g_renderer->GetEffectiveTextureID(explosionImg);
                
                // set up OpenGL state for curved rendering
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE);  
                
                //
                // create curved quads

                for (int row = 0; row < segments - 1; ++row) {
                    for (int col = 0; col < segments - 1; ++col) {
                        float u1 = (float)col / (float)(segments - 1);
                        float v1 = (float)row / (float)(segments - 1);
                        float u2 = (float)(col + 1) / (float)(segments - 1);
                        float v2 = (float)(row + 1) / (float)(segments - 1);
                        
                        Vector3<float> p1, p2, p3, p4;
                        
                        float offset1X = (u1 - 0.5f) * explosionSize;
                        float offset1Y = (v1 - 0.5f) * explosionSize;
                        float offset2X = (u2 - 0.5f) * explosionSize;
                        float offset2Y = (v2 - 0.5f) * explosionSize;
                        
                        // now project onto curved globe surface
                        p1 = explosionPos + tangent1 * offset1X + tangent2 * offset1Y;
                        p1.Normalise();
                        p1 = p1 * 1.001f;
                        
                        p2 = explosionPos + tangent1 * offset2X + tangent2 * offset1Y;
                        p2.Normalise();
                        p2 = p2 * 1.001f;
                        
                        p3 = explosionPos + tangent1 * offset2X + tangent2 * offset2Y;
                        p3.Normalise();
                        p3 = p3 * 1.001f;
                        
                        p4 = explosionPos + tangent1 * offset1X + tangent2 * offset2Y;
                        p4.Normalise();
                        p4 = p4 * 1.001f;
                        
                        float tex_u1 = uv_u1 + (uv_u2 - uv_u1) * u1;
                        float tex_v1 = uv_v1 + (uv_v2 - uv_v1) * v1;
                        float tex_u2 = uv_u1 + (uv_u2 - uv_u1) * u2;
                        float tex_v2 = uv_v1 + (uv_v2 - uv_v1) * v2;
                        
                        // render it
                        g_renderer3d->BeginTexturedQuad3D(textureID, colour);
                        g_renderer3d->TexturedQuadVertex3D(p1.x, p1.y, p1.z, tex_u1, tex_v1);
                        g_renderer3d->TexturedQuadVertex3D(p2.x, p2.y, p2.z, tex_u2, tex_v1);
                        g_renderer3d->TexturedQuadVertex3D(p3.x, p3.y, p3.z, tex_u2, tex_v2);
                        g_renderer3d->TexturedQuadVertex3D(p4.x, p4.y, p4.z, tex_u1, tex_v2);
                        g_renderer3d->EndTexturedQuad3D();
                    }
                }
                
            } else {

                //
                // smaller explosions such as gunfire hits dont need to be curved
                Vector3<float> normal = explosionPos;
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

                float uv_u1, uv_v1, uv_u2, uv_v2;
                g_renderer->GetImageUVCoords(explosionImg, uv_u1, uv_v1, uv_u2, uv_v2);
                unsigned int textureID = g_renderer->GetEffectiveTextureID(explosionImg);
                
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE); 
                
                Vector3<float> v1 = explosionPos + normal * 0.002f - tangent1 * explosionSize - tangent2 * explosionSize;
                Vector3<float> v2 = explosionPos + normal * 0.002f + tangent1 * explosionSize - tangent2 * explosionSize;
                Vector3<float> v3 = explosionPos + normal * 0.002f + tangent1 * explosionSize + tangent2 * explosionSize;
                Vector3<float> v4 = explosionPos + normal * 0.002f - tangent1 * explosionSize + tangent2 * explosionSize;
                
                // render it
                g_renderer3d->BeginTexturedQuad3D(textureID, colour);
                g_renderer3d->TexturedQuadVertex3D(v1.x, v1.y, v1.z, uv_u1, uv_v2);
                g_renderer3d->TexturedQuadVertex3D(v2.x, v2.y, v2.z, uv_u2, uv_v2);
                g_renderer3d->TexturedQuadVertex3D(v3.x, v3.y, v3.z, uv_u2, uv_v1);
                g_renderer3d->TexturedQuadVertex3D(v4.x, v4.y, v4.z, uv_u1, uv_v1);
                g_renderer3d->EndTexturedQuad3D();
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
                    
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                g_renderer->SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
                   
                //
                // render the two circles
                    
                // inner circle
                g_renderer3d->BeginLineStrip3D(colour);
                for (int seg = 0; seg <= numSegments; ++seg) {
                    float angle = 2.0f * M_PI * seg / numSegments;
                    Vector3<float> offset = tangent1 * sinf(angle) + tangent2 * cosf(angle);
                    Vector3<float> inner = pingPos + offset * innerRadius;
                    inner.Normalise();
                    inner = inner * 1.001f;
                    g_renderer3d->LineStripVertex3D(inner.x, inner.y, inner.z);
                }
                g_renderer3d->EndLineStrip3D();
                    
                // outer circle
                g_renderer3d->BeginLineStrip3D(colour);
                for (int seg = 0; seg <= numSegments; ++seg) {
                    float angle = 2.0f * M_PI * seg / numSegments;
                    Vector3<float> offset = tangent1 * sinf(angle) + tangent2 * cosf(angle);
                    Vector3<float> outer = pingPos + offset * outerRadius;
                    outer.Normalise();
                    outer = outer * 1.001f;
                    g_renderer3d->LineStripVertex3D(outer.x, outer.y, outer.z);
                }
                g_renderer3d->EndLineStrip3D();                             
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