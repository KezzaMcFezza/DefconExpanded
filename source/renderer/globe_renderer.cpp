#include "lib/universal_include.h"

#include <math.h>

#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/resource/tinygltf/model.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"
#include "lib/render/colour.h"
#include "lib/math/matrix4f.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/debug/profiler.h"
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

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"
#include "renderer/animated_icon.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/worldobject.h"
#include "world/movingobject.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/fleet.h"
#include "world/whiteboard.h"

#include "curves.h"

GlobeRenderer::GlobeRenderer()
:   nukeModel(NULL),
    m_zoomFactor(1),
    m_currentHighlightId(-1),
    m_currentSelectionId(-1),
    m_currentStateId(-1),
    m_draggingCamera(false),
    m_maxCameraDistance(3.0f),
    m_minCameraDistance(1.2f),
    m_maxZoomFactor(1.0f),
    m_minZoomFactor(0.35f),
    m_cameraLongitude(0.0f),
    m_cameraLatitude(0.0f),
    m_cameraDistance(3.0f),
    m_targetCameraDistance(3.0f),
    m_cameraIdleTime(0.0f)
{
    SetCameraPosition(m_cameraLongitude, m_cameraLatitude, m_cameraDistance);
    m_targetCameraDistance = m_cameraDistance;
}

GlobeRenderer::~GlobeRenderer()
{
    g_resource->UnloadModel("models/nuke.glb");
    nukeModel = NULL;
    
    Reset();
}

void GlobeRenderer::Init()
{
    Reset();

    nukeModel = g_resource->GetModel("models/nuke.glb");
}

void GlobeRenderer::Update()
{
    if(g_app->IsGlobeMode()) {
        UpdateCameraControl();
    }
}

void GlobeRenderer::Reset()
{
    g_app->GetWorldRenderer()->Reset();
}

//
// This is the function that makes it all happen, we convert 2D coordinates to 3D coordinates.
// The only issue with this which is pretty unavoidable is that the closer we get to the poles
// the units get squashed, it's literally an unsolvable problem.

Vector3<float> GlobeRenderer::ConvertLongLatTo3DPosition(float longitude, float latitude)
{
    
    float lonRad = longitude * M_PI / 180.0f;
    float latRad = latitude * M_PI / 180.0f;
    
    //
    // Spherical to cartesian conversion

    Vector3<float> pos(0, 0, GLOBE_RADIUS);
    pos.RotateAroundY(lonRad);
    Vector3<float> right = pos ^ Vector3<float>(0, 1, 0);
    right.Normalise();
    pos.RotateAround(right * latRad);
    
    return pos;
}

Vector3<float> GlobeRenderer::GetElevatedPosition(const Vector3<float>& position)
{
    Vector3<float> normal = position.Normalized();
    return position + normal * GLOBE_ELEVATION;
}

Vector3<float> GlobeRenderer::GetNormalizedFromLongLat(float longitude, float latitude)
{
    return ConvertLongLatTo3DPosition(longitude, latitude).Normalized();
}

void GlobeRenderer::GetSurfaceTangents(const Vector3<float>& normal, Vector3<float>& tangent1, Vector3<float>& tangent2)
{
    Vector3<float> globeUp = Vector3<float>(0.0f, 1.0f, 0.0f);
    if( fabsf(normal.y) > 0.98f )
    {
        tangent1 = Vector3<float>(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent1 = globeUp ^ normal;
        tangent1.Normalise();
    }
    tangent2 = normal ^ tangent1;
    tangent2.Normalise();
}

Vector3<float> GlobeRenderer::SlerpNormal(const Vector3<float>& fromNormal, const Vector3<float>& toNormal, float t)
{
    float dot = fromNormal.Dot(toNormal);
    Clamp( dot, -1.0f, 1.0f );
    float angle = acosf(dot);
    
    if( fabsf(angle) < 0.001f )
    {
        return fromNormal;
    }
    
    float sinAngle = sinf(angle);
    float w1 = sinf((1.0f - t) * angle) / sinAngle;
    float w2 = sinf(t * angle) / sinAngle;
    Vector3<float> result = fromNormal * w1 + toNormal * w2;
    result.Normalise();
    return result;
}

//
// Get nuke launch position from history or current position

void GlobeRenderer::GetNukeLaunchPosition(Nuke* nuke, float& outLon, float& outLat)
{
    LList<Vector3<Fixed> *>& history = nuke->GetHistory();
    if (history.Size() > 0) {
        Vector3<Fixed> *launchPos = history[history.Size() - 1];
        outLon = launchPos->x.DoubleValue();
        outLat = launchPos->y.DoubleValue();
    } else {
        outLon = nuke->m_longitude.DoubleValue();
        outLat = nuke->m_latitude.DoubleValue();
    }
}

//
// Normalize longitude for great circle calculation (handle wrapping)

void GlobeRenderer::NormalizeLongitudeForGreatCircle(float& lon1, float& lon2)
{
    float lonDiff = lon2 - lon1;
    if (lonDiff > M_PI) lon2 -= 2.0f * M_PI;
    else if (lonDiff < -M_PI) lon2 += 2.0f * M_PI;
}

//
// Calculate great circle distance between two points

void GlobeRenderer::CalculateGreatCircleDistance(float lat1, float lon1, float lat2, float lon2,
                                                 float& outDistance, float& outSinDistance)
{
    float deltaLon = lon2 - lon1;
    float a = sinf(lat1) * sinf(lat2) + cosf(lat1) * cosf(lat2) * cosf(deltaLon);
    outDistance = acosf(fmaxf(-1.0f, fminf(1.0f, a)));
    outSinDistance = (outDistance < 0.001f) ? 1.0f : sinf(outDistance);
}

//
// Convert lat/lon to cartesian coordinates on unit sphere

void GlobeRenderer::CalculateCartesianCoordinates(float lat, float lon, float& outX, float& outY, float& outZ)
{
    float cosLat = cosf(lat);
    outX = cosLat * cosf(lon);
    outY = cosLat * sinf(lon);
    outZ = sinf(lat);
}

//
// Calculate trajectory height scale based on distance

float GlobeRenderer::CalculateTrajectoryHeightScale(float distanceRadians)
{
    float normalizedDistance = distanceRadians / M_PI;
    float scaledDistance = sqrtf(normalizedDistance);
    return 0.03f + (0.19f * scaledDistance);
}

//
// Pre calculate constants for great circle interpolation

void GlobeRenderer::CalculateGreatCircleConstants(float launchLon, float launchLat, float targetLon, float targetLat,
                                                   GreatCircleConstants& outConstants)
{
    outConstants.lat1 = launchLat * M_PI / 180.0f;
    outConstants.lon1 = launchLon * M_PI / 180.0f;
    float lat2 = targetLat * M_PI / 180.0f;
    float lon2 = targetLon * M_PI / 180.0f;
    
    NormalizeLongitudeForGreatCircle(outConstants.lon1, lon2);
    
    outConstants.lon2 = lon2;
    outConstants.lat2 = lat2;
    
    CalculateGreatCircleDistance(outConstants.lat1, outConstants.lon1, lat2, lon2,
                                 outConstants.totalDistanceRadians, outConstants.sinDistance);
    
    CalculateCartesianCoordinates(outConstants.lat1, outConstants.lon1, 
                                  outConstants.x1, outConstants.y1, outConstants.z1);
    CalculateCartesianCoordinates(lat2, lon2, 
                                  outConstants.x2, outConstants.y2, outConstants.z2);
    
    outConstants.trajectoryHeightScale = CalculateTrajectoryHeightScale(outConstants.totalDistanceRadians);
}

//
// Calculate surface position along great circle trajectory

Vector3<float> GlobeRenderer::CalculateTrajectorySurfacePosition(const GreatCircleConstants& constants, float progress)
{
    if (constants.totalDistanceRadians < 0.001f) {
        float lat = constants.lat1 + (constants.lat2 - constants.lat1) * progress;
        float lon = constants.lon1 + (constants.lon2 - constants.lon1) * progress;
        return ConvertLongLatTo3DPosition(lon * 180.0f / M_PI, lat * 180.0f / M_PI);
    }
    
    float A = sinf((1.0f - progress) * constants.totalDistanceRadians) / constants.sinDistance;
    float B = sinf(progress * constants.totalDistanceRadians) / constants.sinDistance;
    
    float x = A * constants.x1 + B * constants.x2;
    float y = A * constants.y1 + B * constants.y2;
    float z = A * constants.z1 + B * constants.z2;
    
    float lat = asinf(z);
    float lon = atan2f(y, x);
    return ConvertLongLatTo3DPosition(lon * 180.0f / M_PI, lat * 180.0f / M_PI);
}

//
// Calculate ballistic arc height at given progress

float GlobeRenderer::CalculateTrajectoryArcHeight(const GreatCircleConstants& constants, float progress)
{
    float arcFactor = 4.0f * progress * (1.0f - progress);
    return constants.trajectoryHeightScale * arcFactor;
}

//
// Calculate 3D trajectory point from constants and progress

Vector3<float> GlobeRenderer::CalculateTrajectoryPointFromConstants(const GreatCircleConstants& constants, float progress)
{
    Vector3<float> surfacePos = CalculateTrajectorySurfacePosition(constants, progress);
    float height = CalculateTrajectoryArcHeight(constants, progress);
    return surfacePos + surfacePos.Normalized() * height;
}

//
// Calculate and cache great circle constants for nuke trajectory

bool GlobeRenderer::CalculateNukeTrajectoryConstants(Nuke* nuke, GreatCircleConstants& outConstants)
{
    if (!nuke || nuke->m_totalDistance <= 0) {
        return false;
    }
    
    float launchLon, launchLat;
    GetNukeLaunchPosition(nuke, launchLon, launchLat);
    
    float targetLon = nuke->m_targetLongitude.DoubleValue();
    float targetLat = nuke->m_targetLatitude.DoubleValue();
    
    CalculateGreatCircleConstants(launchLon, launchLat, targetLon, targetLat, outConstants);
    return true;
}

//
// Calculate nuke trajectory point at given progress

Vector3<float> GlobeRenderer::CalculateNukeTrajectoryPoint(Nuke* nuke, float progress)
{
    if (!g_app->IsGlobeMode() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    GreatCircleConstants constants;
    if (!CalculateNukeTrajectoryConstants(nuke, constants)) {
        return Vector3<float>(0, 0, 0);
    }
    
    return CalculateTrajectoryPointFromConstants(constants, progress);
}

//
// Calculate progress along trajectory from a given position

float GlobeRenderer::CalculateNukeProgressFromPosition(Nuke* nuke, const Vector3<Fixed>& pos)
{
    if (!nuke || nuke->m_totalDistance <= 0) {
        return 0.0f;
    }
    
    Vector3<Fixed> target(nuke->m_targetLongitude, nuke->m_targetLatitude, 0);
    Fixed remainingDistance = (target - pos).Mag();
    return fmaxf(0.0f, fminf(1.0f, (1 - remainingDistance / nuke->m_totalDistance).DoubleValue()));
}

//
// Get predicted progress along nuke trajectory

float GlobeRenderer::CalculateNukePredictedProgress(Nuke* nuke)
{
    if (!nuke || nuke->m_totalDistance <= 0) {
        return 0.0f;
    }
    
    Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
    float currentProgress = CalculateNukeProgressFromPosition(nuke, pos);
    
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    Fixed currentSpeed = nuke->m_vel.Mag();
    Fixed distanceTraveled = currentSpeed * predictionTime;
    float progressIncrement = (distanceTraveled / nuke->m_totalDistance).DoubleValue();
    
    return fmaxf(0.0f, fminf(1.0f, currentProgress + progressIncrement));
}

//
// Calculate 3D nuke position using great circle and ballistic arc

Vector3<float> GlobeRenderer::CalculateNuke3DPosition(Nuke* nuke)
{
    if (!g_app->IsGlobeMode() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
    if (nuke->m_totalDistance <= 0) {
        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (nuke->m_longitude + nuke->m_vel.x * predictionTime).DoubleValue();
        float predictedLatitude = (nuke->m_latitude + nuke->m_vel.y * predictionTime).DoubleValue();
        return ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
    }
    
    float predictedProgress = CalculateNukePredictedProgress(nuke);
    return CalculateNukeTrajectoryPoint(nuke, predictedProgress);
}

//
// Calculate historical nuke position for trail rendering

Vector3<float> GlobeRenderer::CalculateHistoricalNuke3DPosition(Nuke* nuke, const Vector3<Fixed>& historicalPos)
{
    if (!g_app->IsGlobeMode() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    if (nuke->m_totalDistance <= 0) {
        return ConvertLongLatTo3DPosition(historicalPos.x.DoubleValue(), historicalPos.y.DoubleValue());
    }
    
    float progress = CalculateNukeProgressFromPosition(nuke, historicalPos);
    return CalculateNukeTrajectoryPoint(nuke, progress);
}

//
// Calculate historical nuke position using age-based progress

Vector3<float> GlobeRenderer::CalculateHistoricalNuke3DPositionByAge(Nuke* nuke, const Vector3<Fixed>& historicalPos, float historicalProgress)
{ 
    if (!g_app->IsGlobeMode() || !nuke) {
        return Vector3<float>(0, 0, 0);
    }
    
    if (nuke->m_totalDistance <= 0) {
        return ConvertLongLatTo3DPosition(historicalPos.x.DoubleValue(), historicalPos.y.DoubleValue());
    }
    
    return CalculateNukeTrajectoryPoint(nuke, historicalProgress);
}

bool GlobeRenderer::ShouldUse3DNukeTrajectories()
{
    if (!g_app->IsGlobeMode()) return false;

//
// So sync practice clients cannot cheat with 3D trajectories
// as they do not have networking nor are they sync compatible.

#ifdef SYNC_PRACTICE
    return true;
#endif
    
    if (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode()) {
        return true;
    }
    
    return (g_app->GetWorld()->m_myTeamId == -1);
}

void GlobeRenderer::GenerateStarField()
{
    if (g_starField3DInitialized) return;
    
    g_starField3D.Empty();
    
    //
    // Generate random stars on a large sphere around the globe

    int numStars = g_preferences->GetInt(PREFS_GLOBE_STAR_DENSITY, 12500); 
    float starSphereRadius = 20.0f;
    
    for (int i = 0; i < numStars; i++) {
        Star3D star;
        
        //
        // Generate random position on sphere using spherical coordinates

        float theta = frand(2.0f * M_PI); 
        float phi = frand(M_PI) - M_PI/2.0f;
        
        //
        // Convert to 3D position on star sphere

        star.position.x = starSphereRadius * sin(theta) * cos(phi);
        star.position.y = starSphereRadius * sin(phi);
        star.position.z = starSphereRadius * cos(theta) * cos(phi);
        
        float menuStarSize = g_preferences->GetFloat(PREFS_GLOBE_STAR_SIZE, 2.5f);
        float internalStarSize = GlobeRenderer::ConvertMenuToStarSize(menuStarSize);
        float sizeVariation = internalStarSize * 0.5f;
        star.size = internalStarSize + frand(sizeVariation) - (sizeVariation * 0.5f); 
        
        //
        // Random brightness for "realism"

        float brightnessFactor = frand();
        if (brightnessFactor < 0.3f) {
            star.brightness = 0.4f + brightnessFactor * 0.3f; 
        } else {
            star.brightness = 0.7f + (brightnessFactor - 0.3f) * 0.43f; 
        }
        
        g_starField3D.PutData(star);
    }
    
    g_starField3DInitialized = true;
}

void GlobeRenderer::CleanupStarField()
{
    g_starField3D.Empty();
    g_starField3DInitialized = false;
}

void GlobeRenderer::RegenerateStarField()
{ 
    if (g_preferences->GetInt(PREFS_GLOBE_STARFIELD, 1)) {
        CleanupStarField();
        GenerateStarField();
        BuildStarfieldVBO();
    }
}

void GlobeRenderer::BuildStarfieldVBO()
{
    if (g_megavbo3d->IsTexturedMegaVBO3DValid("Starfield")) {
        return;
    }
    
    Image *cityImg = g_resource->GetImage("graphics/city.bmp");
    if (!cityImg) return;
    
    //
    // Calculate actual buffer sizes based on star count
    
    int numStars = g_starField3D.Size();
    int verticesNeeded = numStars * 6;           // 6 vertices per quad
    int indicesNeeded = numStars * 6;            // 6 indices per quad (2 triangles)
    
    //
    // Set buffer sizes with padding (10% extra)

    g_megavbo3d->SetTexturedMegaVBO3DBufferSizes(verticesNeeded, indicesNeeded, "Starfield");
    
    float u1, v1, u2, v2;
    g_renderer->GetImageUVCoords(cityImg, u1, v1, u2, v2);
    
    //
    // Build it

    g_megavbo3d->BeginTexturedMegaVBO3D("Starfield", g_renderer->GetEffectiveTextureID(cityImg));
    
    //
    // Build all billboards at once

    for (int i = 0; i < g_starField3D.Size(); i++) {
        Star3D& star = g_starField3D[i];
        
        Colour starColor;
        int alpha = (int)(255 * star.brightness);
        
        int starType = i % 13; 
        if (starType < 8) {

            //
            // Most stars are white/blue-white

            starColor.Set(255, 255, 255, alpha);
        } else if (starType < 10) {

            //
            // Some blue stars

            starColor.Set(180, 200, 255, alpha);
        } else if (starType < 12) {

            //
            // Some warm/yellow stars

            starColor.Set(255, 230, 180, alpha);
        } else {

            //
            // A few red stars

            starColor.Set(255, 180, 150, alpha);
        }
        
        float r = starColor.m_r / 255.0f;
        float g = starColor.m_g / 255.0f;
        float b = starColor.m_b / 255.0f;
        float a = starColor.m_a / 255.0f;
        
        //
        // Create surface-aligned billboard for this star

        Vector3<float> position(star.position.x, star.position.y, star.position.z);
        Vertex3DTextured billboardVertices[6];
        
        g_renderer3d->CreateSurfaceAlignedBillboard(position, star.size, star.size, 
                                                    billboardVertices, u1, v1, u2, v2, 
                                                    r, g, b, a);
        
        g_megavbo3d->AddTexturedQuadsToMegaVBO3D(billboardVertices, 6, 1);
    }
    
    g_megavbo3d->EndTexturedMegaVBO3D();

}

void GlobeRenderer::RenderStarField()
{ 
    if (!g_megavbo3d->IsTexturedMegaVBO3DValid("Starfield")) {
        if (g_preferences->GetInt(PREFS_GLOBE_STARFIELD, 1)) {
            if (!g_starField3DInitialized) {
                GenerateStarField();
            }
            BuildStarfieldVBO();
        }
    }
    
    g_megavbo3d->RenderTexturedMegaVBO3D("Starfield");
}

//
// Render invisible culling sphere inside the globe
// Renders to depth buffer only to cull objects behind the globe

void GlobeRenderer::BuildCullSphereVBO()
{
    if (g_megavbo3d->IsTriangleMegaVBO3DValid("CullingSphere")) {
        return;
    }
    
    float cullRadius = GLOBE_RADIUS * GLOBE_CULL_RADIUS;
    
    const int numLatitudeSegments = 36;
    const int numLongitudeSegments = 36;
    
    //
    // Calculate buffer sizes: each quad = 2 triangles = 6 vertices
    
    int totalTriangles = numLatitudeSegments * numLongitudeSegments * 2;
    int totalVertices = totalTriangles * 3;
    
    g_megavbo3d->SetTriangleMegaVBO3DBufferSizes(totalVertices, totalVertices, "CullingSphere");
    
    Colour cullColor(255, 255, 255, 255);
    g_megavbo3d->BeginTriangleMegaVBO3D("CullingSphere", cullColor);
    
    //
    // Generate sphere triangles
    
    for (int lat = 0; lat < numLatitudeSegments; ++lat) {
        float lat1 = -M_PI / 2.0f + M_PI * lat / numLatitudeSegments;
        float lat2 = -M_PI / 2.0f + M_PI * (lat + 1) / numLatitudeSegments;
        
        for (int lon = 0; lon < numLongitudeSegments; ++lon) {
            float lon1 = 2.0f * M_PI * lon / numLongitudeSegments;
            float lon2 = 2.0f * M_PI * (lon + 1) / numLongitudeSegments;
            
            //
            // Calculate 4 vertices for quad (will split into 2 triangles)
            
            Vector3<float> v1(cullRadius * cosf(lat1) * cosf(lon1),
                              cullRadius * sinf(lat1),
                              cullRadius * cosf(lat1) * sinf(lon1));
            
            Vector3<float> v2(cullRadius * cosf(lat1) * cosf(lon2),
                              cullRadius * sinf(lat1),
                              cullRadius * cosf(lat1) * sinf(lon2));
            
            Vector3<float> v3(cullRadius * cosf(lat2) * cosf(lon2),
                              cullRadius * sinf(lat2),
                              cullRadius * cosf(lat2) * sinf(lon2));
            
            Vector3<float> v4(cullRadius * cosf(lat2) * cosf(lon1),
                              cullRadius * sinf(lat2),
                              cullRadius * cosf(lat2) * sinf(lon1));
            
            //
            // Add 2 triangles per quad (6 vertices total)
            
            Vector3<float> triangleVerts[6] = { v1, v2, v3, v1, v3, v4 };
            g_megavbo3d->AddTrianglesToMegaVBO3D(triangleVerts, 6);
        }
    }
    
    g_megavbo3d->EndTriangleMegaVBO3D();
}

void GlobeRenderer::RenderCullSphere()
{
    if (!g_megavbo3d->IsTriangleMegaVBO3DValid("CullingSphere")) {
        BuildCullSphereVBO();
    }
    
    //
    // Ensure depth testing and depth writes are enabled
    
    g_renderer->SetDepthBuffer(true, false);
    g_renderer->SetDepthMask(true);
    
    //
    // Disable color writes
    
    g_renderer->SetColorMask(false, false, false, false);
    
    //
    // Cull front faces, only render back faces
    
    g_renderer->SetCullFace(true, 1);
    
    g_megavbo3d->RenderTriangleMegaVBO3D("CullingSphere");
    
    //
    // Re enable color writes and restore culling state
    
    g_renderer->SetColorMask(true, true, true, true);
    g_renderer->SetCullFace(false, 0);
}

void GlobeRenderer::GlobeCoastlines()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_COASTLINES) == 1) {
        if (!g_megavbo3d->IsMegaVBO3DValid("GlobeCoastlines")) {
#ifndef TARGET_EMSCRIPTEN
            g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_COAST_THICKNESS, 1.5f));
#endif
            g_megavbo3d->BeginMegaVBO3D("GlobeCoastlines", g_styleTable->GetPrimaryColour( STYLE_GLOBE_COASTLINES ));

            for( int i = 0; i < g_app->GetEarthData()->m_islands.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_islands[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> coastlineVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];
                    Vector3<float> currentPos = ConvertLongLatTo3DPosition(thePoint->x, thePoint->y);
                    Vector3<float> currentNormal = currentPos.Normalized();
                    
                    if (j > 0) 
                    {
                        Vector3<float> *prevPoint = island->m_points[j - 1];
                        Vector3<float> prevPos = ConvertLongLatTo3DPosition(prevPoint->x, prevPoint->y);
                        Vector3<float> prevNormal = prevPos.Normalized();
                        
                        float distance = (currentPos - prevPos).Mag();
                        int numSegments = (int)(distance / 0.01f) + 1;
                        numSegments = fmaxf(1, fminf(numSegments, 50));
                        
                        for (int seg = 1; seg < numSegments; ++seg) 
                        {
                            float t = (float)seg / (float)numSegments;
                            Vector3<float> interpolatedNormal = SlerpNormal(prevNormal, currentNormal, t);
                            Vector3<float> interpolatedPos = interpolatedNormal * GLOBE_RADIUS;
                            coastlineVertices.PutData(interpolatedPos);
                        }
                    }
                    
                    coastlineVertices.PutData(currentPos);
                }
                AddLineStrip(coastlineVertices);
            }
            
            g_megavbo3d->EndMegaVBO3D();

        }
    }

    //
    // Build it

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_COAST_THICKNESS, 1.5f));
#endif

    g_megavbo3d->RenderMegaVBO3D("GlobeCoastlines");
}

void GlobeRenderer::GlobeBorders()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_BORDERS) == 1) {
        if (!g_megavbo3d->IsMegaVBO3DValid("GlobeBorders")) {
#ifndef TARGET_EMSCRIPTEN
            g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_BORDER_THICKNESS, 1.0f));
#endif
            g_megavbo3d->BeginMegaVBO3D("GlobeBorders", g_styleTable->GetPrimaryColour( STYLE_GLOBE_BORDERS ));

            for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_borders[i];
                AppDebugAssert( island );

                DArray<Vector3<float>> borderVertices;
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];
                    Vector3<float> currentPos = ConvertLongLatTo3DPosition(thePoint->x, thePoint->y);
                    Vector3<float> currentNormal = currentPos.Normalized();
                    
                    if (j > 0) 
                    {
                        Vector3<float> *prevPoint = island->m_points[j - 1];
                        Vector3<float> prevPos = ConvertLongLatTo3DPosition(prevPoint->x, prevPoint->y);
                        Vector3<float> prevNormal = prevPos.Normalized();
                        
                        float distance = (currentPos - prevPos).Mag();
                        int numSegments = (int)(distance / 0.01f) + 1;
                        numSegments = fmaxf(1, fminf(numSegments, 50));
                        
                        for (int seg = 1; seg < numSegments; ++seg) 
                        {
                            float t = (float)seg / (float)numSegments;
                            Vector3<float> interpolatedNormal = SlerpNormal(prevNormal, currentNormal, t);
                            Vector3<float> interpolatedPos = interpolatedNormal * GLOBE_RADIUS;
                            borderVertices.PutData(interpolatedPos);
                        }
                    }
                    
                    borderVertices.PutData(currentPos);
                }
                AddLineStrip(borderVertices);
            }

            g_megavbo3d->EndMegaVBO3D();

        }
    }

    //
    // Build it

#ifndef TARGET_EMSCRIPTEN
    g_renderer->SetLineWidth(g_preferences->GetFloat(PREFS_GLOBE_BORDER_THICKNESS, 1.0f));
#endif

    g_megavbo3d->RenderMegaVBO3D("GlobeBorders");
}

void GlobeRenderer::GlobeGridlines()
{
    if (!g_megavbo3d->IsMegaVBO3DValid("GlobeGridlines")) {
        g_megavbo3d->BeginMegaVBO3D("GlobeGridlines", g_styleTable->GetPrimaryColour( STYLE_GLOBE_GRIDLINES ));
        
    //
    // Longitudinal lines (meridians)

    for( float x = -180; x < 180; x += 10 )
    {
        DArray<Vector3<float>> lineVertices;
        for( float y = -90; y < 90; y += 1.0f )
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
        for( float x = -180; x <= 180; x += 1.0f )
        {
            lineVertices.PutData(ConvertLongLatTo3DPosition(x, y));
        }
            AddLineStrip(lineVertices);
    }
        
    g_megavbo3d->EndMegaVBO3D();

    }

    //
    // Build it

    g_megavbo3d->RenderMegaVBO3D("GlobeGridlines");

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
    
    g_megavbo3d->AddLineStripToMegaVBO3D(vertexArray, vertices.Size());
    delete[] vertexArray;
}

void GlobeRenderer::Render()
{
    //
    // Render the scene

    START_PROFILE("GlobeRenderer");

    float popupScale = g_preferences->GetFloat(PREFS_INTERFACE_POPUPSCALE);
    m_drawScale = m_zoomFactor / (1.0f-popupScale*0.1f);

    float resScale = g_windowManager->WindowH() / 800.0f;
    m_drawScale /= resScale;
    
    if(!g_app->IsGlobeMode()) 
    {
        LobbyCamera(); 

        START_PROFILE("Gridlines");
        GlobeGridlines();
        END_PROFILE("Gridlines");

        g_renderer->SetDepthBuffer( true, true );

    }
    else 
    {
        GameCamera();

        START_PROFILE("Culling Sphere");
        RenderCullSphere();
        END_PROFILE("Culling Sphere");
    }

    START_PROFILE("Coastlines");
    GlobeCoastlines();
    END_PROFILE("Coastlines");

    START_PROFILE("Borders");
    GlobeBorders();
    END_PROFILE("Borders");

    //
    // In the lobby, we only want to cull the starfield
        
    if (!g_app->IsGlobeMode()) 
    {
        g_renderer->SetDepthBuffer(true, false);
        g_renderer->ClearScreen(false, true);
        
        START_PROFILE("Culling Sphere");
        RenderCullSphere();
        END_PROFILE("Culling Sphere");

    }

    //
    // Disable fog before rendering main scene

    g_renderer3d->DisableFog();
    
    START_PROFILE("Starfield");
    g_renderer3d->BeginStaticSpriteBatch3D();      // Star field batching
    RenderStarField();
    g_renderer3d->EndStaticSpriteBatch3D();        // Flush star sprites
    END_PROFILE("Starfield");

    if (!g_app->IsGlobeMode()) {
        g_renderer->SetDepthBuffer(false, false);
        END_PROFILE("GlobeRenderer");
        return;
    }
    
    //
    // Begin scene main scene

    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    
    g_renderer3d->BeginTextBatch3D();              // Text batching
    g_renderer3d->BeginLineBatch3D();              // Unit movement trails
    g_renderer3d->BeginRectFillBatch3D();          // Rectangle fills
    g_renderer3d->BeginRectBatch3D();              // Rectangles
    g_renderer3d->BeginStaticSpriteBatch3D();      // Main unit sprites + city icons
    g_renderer3d->BeginRotatingSpriteBatch3D();    // Rotating sprites (aircraft, but not nukes anymore)

    if( g_app->GetWorldRenderer()->m_showPopulation )          
    { 
        RenderPopulationDensity();
    }

    if( g_app->GetWorldRenderer()->m_showNukeUnits )           
    {
        RenderNukeUnits();
    }

    if (ShouldUse3DNukeTrajectories()) {

        //
        // Set blend mode to normal and disable depth writes for nuke trajectories
        // to prevent trail segments from being visible when using alpha blending

        g_renderer->SetBlendMode(Renderer::BlendModeNormal);
        g_renderer->SetDepthMask(false);
    
        Render3DNukeTrajectories();
    
        g_renderer3d->EndLineBatch3D();
    
        g_renderer->SetDepthMask(true);
        
        Render3DNukes();
    }

    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);

    RenderCities();
    RenderObjects();
    RenderGunfire();
    RenderExplosions();
    RenderAnimations();
    RenderWhiteBoard();
    RenderSanta();
    
    //
    // Now end the main scene and flush
    
    g_renderer3d->EndTextBatch3D();
    g_renderer3d->EndLineBatch3D();
    g_renderer3d->EndRectBatch3D();
    g_renderer3d->EndRectFillBatch3D();
    g_renderer3d->EndRotatingSpriteBatch3D();
    g_renderer3d->EndStaticSpriteBatch3D();

    g_renderer->SetDepthBuffer(false, false);
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    
    RenderDragIcon();

    END_PROFILE("GlobeRenderer");
}

void GlobeRenderer::RenderDragIcon()
{
    if( !m_draggingCamera ) return;
    
    Image *move = g_resource->GetImage( "gui/move.bmp" );
    if( !move ) return;
    
    g_renderer2d->Reset2DViewport();
    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    
    float iconSize = 48.0f;
    float iconX = g_inputManager->m_mouseX - iconSize * 0.5f;
    float iconY = g_inputManager->m_mouseY - iconSize * 0.5f;
    
    g_renderer2d->StaticSprite( move, iconX, iconY, iconSize, iconSize, White, true );
}

//
// Checks if a point on the globe is visible from the 
// camera position using dot product

bool GlobeRenderer::IsPointVisible(const Vector3<float>& globePoint, const Vector3<float>& cameraPos, float globeRadius)
{
    Vector3<float> pointNormal = globePoint.Normalized();
    Vector3<float> toCamera = (cameraPos - Vector3<float>(0,0,0)).Normalized();
    
    //
    // Are we facing the camera?

    float dot = pointNormal.Dot(toCamera);
    float threshold = CullingThreshold(cameraPos.Mag(), globeRadius);
    
    return dot > threshold;
}

float GlobeRenderer::CullingThreshold(float cameraDistance, float globeRadius)
{
    float distanceRatio = globeRadius / cameraDistance;
    return -0.1f + (distanceRatio * 1.0f);
}

void GlobeRenderer::GetCameraPosition( float &longitude, float &latitude, float &distance )
{
    longitude = m_cameraLongitude;
    latitude = m_cameraLatitude;
    distance = m_cameraDistance;
}

Vector3<float> GlobeRenderer::GetCameraPosition()
{
    Vector3<float> camSpherePoint = ConvertLongLatTo3DPosition(m_cameraLongitude, m_cameraLatitude);
    Vector3<float> cameraPos = camSpherePoint * m_cameraDistance;
    return cameraPos;
}

float GlobeRenderer::GetZoomFactor()
{
    return m_zoomFactor;
}

float GlobeRenderer::GetDrawScale()
{
    return m_drawScale;
}

void GlobeRenderer::SetCameraPosition( float longitude, float latitude, float distance )
{
    m_cameraLongitude = longitude;
    m_cameraLatitude = latitude;
    m_cameraDistance = distance;
    
    //
    // Clamp values to valid ranges

    Clamp( m_cameraLatitude, -89.0f, 89.0f );
    Clamp( m_cameraDistance, m_minCameraDistance, m_maxCameraDistance );
    
    //
    // Wrap longitude

    if( m_cameraLongitude < -180.0f ) m_cameraLongitude += 360.0f;
    if( m_cameraLongitude > 180.0f ) m_cameraLongitude -= 360.0f;
    
    float normalizedDistance = (m_cameraDistance - m_minCameraDistance) / (m_maxCameraDistance - m_minCameraDistance);
    normalizedDistance = fmaxf(0.0f, fminf(1.0f, normalizedDistance));
    m_zoomFactor = m_minZoomFactor + normalizedDistance * (m_maxZoomFactor - m_minZoomFactor);
}

void GlobeRenderer::IsCameraIdle(float oldLongitude, float oldLatitude)
{
    //
    // Is the camera idle?
    
    if( m_cameraLongitude == oldLongitude && 
        m_cameraLatitude == oldLatitude )
    {
        m_cameraIdleTime += g_advanceTime;
    }
    else
    {
        m_cameraIdleTime = 0.0f;
    }
}

void GlobeRenderer::DragCamera()
{
    if( g_inputManager->m_mmb )
    {
        g_app->SetMousePointerVisible ( false );
        
        float dragSensitivity = 0.15f * (m_zoomFactor / m_maxZoomFactor);
        m_cameraLongitude -= g_inputManager->m_mouseVelX * dragSensitivity;
        m_cameraLatitude  += g_inputManager->m_mouseVelY * dragSensitivity;
        Clamp( m_cameraLatitude, -89.0f, 89.0f );
        
        m_draggingCamera = true;
    }
    else
    {
        if( !g_app->MousePointerIsVisible() )
        {
            g_app->SetMousePointerVisible ( true );
        }
        m_draggingCamera = false;
    }
}

//
// Handle keyboard input for camera movement

void GlobeRenderer::UpdateCameraControl()
{
    
    bool ignoreKeys = g_app->GetInterface()->UsingChatWindow();
    if( ignoreKeys ) return;
    
    int keyboardLayout = g_preferences->GetInt( PREFS_INTERFACE_KEYBOARDLAYOUT );
    int KeyQ = KEY_Q, KeyW = KEY_W, KeyE = KEY_E, KeyA = KEY_A, KeyS = KEY_S, KeyD = KEY_D;
    
    switch ( keyboardLayout) {
    case 2:         // AZERTY
        KeyQ = KEY_A;
        KeyW = KEY_Z;
        KeyA = KEY_Q;
        break;
    case 3:         // QZERTY
        KeyW = KEY_Z;
        break;
    case 4:         // DVORAK
        KeyQ = KEY_QUOTE;
        KeyW = KEY_COMMA;
        KeyE = KEY_STOP;
        KeyS = KEY_O;
        KeyD = KEY_E;
        break;
    default:        // QWERTY/QWERTZ (1) or invalid pref - do nothing
        break;
    }
    
    //
    // Handle zoom with Q and E
    
    float zoomSpeed = 3.0f * g_advanceTime;
    if( g_keys[KeyE] && !ignoreKeys ) m_targetCameraDistance += zoomSpeed;
    if( g_keys[KeyQ] && !ignoreKeys ) m_targetCameraDistance -= zoomSpeed;
    
    float mouseWheelDelta = g_inputManager->m_mouseVelZ;
    if( fabsf(mouseWheelDelta) > 0.001f )
    {
        float zoomPreference = g_preferences->GetFloat( PREFS_INTERFACE_ZOOM_SPEED, 1.0f );
        m_targetCameraDistance -= mouseWheelDelta * zoomPreference * 0.15f;
    }

    Clamp( m_targetCameraDistance, m_minCameraDistance, m_maxCameraDistance );

    float oldLongitude = m_cameraLongitude;
    float oldLatitude = m_cameraLatitude;

    IsCameraIdle(oldLongitude, oldLatitude);

    if( g_preferences->GetInt( PREFS_INTERFACE_CAMDRAGGING ) == 1 )
    {
        DragCamera();
    }
    else
    {
        if( !g_app->MousePointerIsVisible() )
        {
            g_app->SetMousePointerVisible ( true );
        }
    }
    
    //
    // Handle rotation with WASD (rotate around globe)
    
    float rotationSpeed = 48.0f * g_advanceTime; // degrees per second
    
    if( (g_keys[KeyA] || g_keys[KEY_LEFT]) && !ignoreKeys )
    {
        m_cameraLongitude -= rotationSpeed;
    }
    if( (g_keys[KeyD] || g_keys[KEY_RIGHT]) && !ignoreKeys )
    {
        m_cameraLongitude += rotationSpeed;
    }
    if( (g_keys[KeyW] || g_keys[KEY_UP]) && !ignoreKeys )
    {
        m_cameraLatitude += rotationSpeed;
    }
    if( (g_keys[KeyS] || g_keys[KEY_DOWN]) && !ignoreKeys )
    {
        m_cameraLatitude -= rotationSpeed;
    }
    
    float distanceDiff = m_targetCameraDistance - m_cameraDistance;
    if( fabsf(distanceDiff) > 0.0001f )
    {
        float factor1 = g_advanceTime * 5.0f;
        factor1 = fminf(factor1, 1.0f);
        float factor2 = 1.0f - factor1;
        m_cameraDistance = m_targetCameraDistance * factor1 + m_cameraDistance * factor2;
    }
    else
    {
        m_cameraDistance = m_targetCameraDistance;
    }

    SetCameraPosition(m_cameraLongitude, m_cameraLatitude, m_cameraDistance);  
}

void GlobeRenderer::GameCamera()
{
    float fov = 60.0f;
    float nearPlane = 0.01f;
    float farPlane = 10000.0f;
    float screenW = g_windowManager->WindowW();
    float screenH = g_windowManager->WindowH();

    g_renderer3d->SetPerspective(fov, screenW / screenH, nearPlane, farPlane);

    //
    // Convert longitude/latitude to 3D position on sphere
    // Camera orbits around globe center at distance m_cameraDistance
    
    Vector3<float> spherePoint = ConvertLongLatTo3DPosition(m_cameraLongitude, m_cameraLatitude);
    
    //
    // Camera position is spherePoint scaled by distance
    // Looking at origin (globe center)
    
    Vector3<float> cameraPos = spherePoint * m_cameraDistance;
    Vector3<float> lookAt(0.0f, 0.0f, 0.0f);
    
    //
    // Calculate up vector (always point "north" relative to camera)
    // Up is perpendicular to camera direction, pointing towards north pole
    
    Vector3<float> forward = lookAt - cameraPos;
    forward.Normalise();
    
    Vector3<float> north(0.0f, 1.0f, 0.0f);
    Vector3<float> right = forward ^ north;
    right.Normalise();
    Vector3<float> up = right ^ forward;
    up.Normalise();
    
    m_camFront = forward;
    m_camUp = up;
    
    g_renderer3d->SetLookAt(cameraPos.x, cameraPos.y, cameraPos.z,
              lookAt.x, lookAt.y, lookAt.z,
              up.x, up.y, up.z);

    g_renderer->SetCullFace(false, 0);

    float fogDistanceSetting = (float)g_preferences->GetInt(PREFS_GLOBE_FOG_DISTANCE, 25);
    float distanceMultiplier = (fogDistanceSetting / 20.0f - 1.0f) * 2.0f + 1.0f;
        
    //
    // Scale the camera distance for fog calculations.
    // We pass in fake camera values here to make the
    // fog render further or closer to the real camera.
        
    Vector3<float> fogCameraPos = cameraPos;
    fogCameraPos = fogCameraPos * distanceMultiplier;
        
    g_renderer3d->EnableOrientationFog(0.03f, 0.03f, 0.03f, 25.0f,
                                          fogCameraPos.x,
                                          fogCameraPos.y,
                                          fogCameraPos.z);
}

void GlobeRenderer::LobbyCamera()
{
    float fov = 60.0f;
    float nearPlane = 0.01f;
    float farPlane = 10000.0f;
    float screenW = g_windowManager->WindowW();
    float screenH = g_windowManager->WindowH();

    g_renderer3d->SetPerspective(fov, screenW / screenH, nearPlane, farPlane);

    static float timeVal = 0.0f;
    timeVal += g_advanceTime * 1;

    float timeNow = timeVal;
    float camDist = 1.7f;
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

    g_renderer3d->SetLookAt(pos.x, pos.y, pos.z,
              forwards.x, forwards.y, forwards.z,
              up.x, up.y, up.z);

    g_renderer->SetCullFace(false, 0);

    g_renderer3d->EnableDistanceFog(camDist/2.0f, camDist*2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.25f);
}

void GlobeRenderer::RenderObjects()
{
    START_PROFILE( "Objects" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    g_renderer->SetFont( "kremlin", true );
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
     
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {            
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            START_PROFILE( WorldObject::GetName(wobj->m_type) );

            bool onScreen = IsPointVisible( ConvertLongLatTo3DPosition(wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue() ), GetCameraPosition(), GLOBE_RADIUS );
            if( onScreen || wobj->m_type == WorldObject::TypeNuke )
            {
                if( myTeamId == -1 ||
                    wobj->m_teamId == myTeamId ||
                    wobj->m_visible[myTeamId] ||
                    g_app->GetWorldRenderer()->CanRenderEverything() )
                {
                    wobj->Render3D();
                }
                else
                {
                    wobj->RenderGhost3D(myTeamId);
                }
            }

#ifndef NON_PLAYABLE

            if( g_app->GetWorldRenderer()->m_showOrders )
            {
                if(myTeamId == -1 || wobj->m_teamId == myTeamId )                
                {
                    RenderWorldObjectTargets(wobj, false);                // This shows ALL object orders on screen at once
                }
            }

            //
            // Render num nukes on the way

            if( wobj->m_numNukesInFlight || wobj->m_numNukesInQueue )
            {
                Vector3<float> normal = GetNormalizedFromLongLat(wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue());
                Vector3<float> renderPos = GetElevatedPosition(normal * GLOBE_RADIUS);
                
                Colour col(255,0,0,255);
                if( !wobj->m_numNukesInFlight ) col.m_a = 100;
                float iconSize = GLOBE_ANIMATED_ICON_SIZE;
                float textSize = 0.01f;

                if( wobj->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.005f;

                Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                g_renderer3d->RotatingSprite3D( img, renderPos.x, renderPos.y, renderPos.z, iconSize, iconSize, col, 0, BILLBOARD_SURFACE_ALIGNED );

                Vector3<float> surfacePos = normal * GLOBE_RADIUS;
                Vector3<float> tangent1, tangent2;
                GetSurfaceTangents(normal, tangent1, tangent2);
                
                float offsetDistance = 0.01f;
                Vector3<float> textPos = surfacePos + tangent2 * offsetDistance;
                if( wobj->m_numNukesInQueue )
                {
                    col.m_a = 100;
                    char caption[128];
                    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
                    LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInQueue, caption );
                    g_renderer3d->TextCentreSimple3D( textPos.x, textPos.y, textPos.z, col, textSize, caption, BILLBOARD_SURFACE_ALIGNED );
                }

                if( wobj->m_numNukesInFlight )
                {
                    col.m_a = 255;
                    char caption[128];
                    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
                    LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInFlight, caption );
                    Vector3<float> textPos2 = textPos + tangent2 * 0.0075f;
                    g_renderer3d->TextCentreSimple3D( textPos2.x, textPos2.y, textPos2.z, col, textSize, caption, BILLBOARD_SURFACE_ALIGNED );
                }
            }
#endif
            END_PROFILE( WorldObject::GetName(wobj->m_type) );
        }
    }
    
    g_renderer->SetFont();
    
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);

    END_PROFILE( "Objects" );
}

void GlobeRenderer::Render3DNukes()
{
    if (!ShouldUse3DNukeTrajectories()) return;

    START_PROFILE("3D Nukes");
    
    Model *nukeModel = GetNukeModel();
    if (!nukeModel || !nukeModel->IsLoaded()) {
        END_PROFILE("3D Nukes");
        return;
    }
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    //
    // Get first VBO cache key from model
    
    if (nukeModel->GetVBOCount() == 0) {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
        END_PROFILE("3D Nukes");
        return;
    }
    
    const char* cacheKey = nukeModel->GetVBOCacheKey(0);
    if (!cacheKey || !cacheKey[0]) {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
        END_PROFILE("3D Nukes");
        return;
    }
    
    if (!g_megavbo3d->IsAny3DVBOValid(cacheKey)) {
        g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
        END_PROFILE("3D Nukes");
        return;
    }
    
    //
    // Clear all sub batches from previous frame
    
    const char* baseBatchKey = "nuke_instances";
    g_megavbo3d->InvalidateInstanceBatchPrefix(baseBatchKey);
    
    //
    // Begin instanced batch for nukes
    
    int batchIndex = 0;
    char currentBatchKey[256];
    sprintf(currentBatchKey, "%s_%d", baseBatchKey, batchIndex);
    
    g_megavbo3d->BeginInstancedMegaVBO(currentBatchKey, cacheKey);
    
    //
    // Collect all visible nukes as instances
    
    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            if (wobj->m_type != WorldObject::TypeNuke) continue;
            if (!wobj->IsMovingObject()) continue;
            
            Nuke* nuke = (Nuke*)wobj;
            
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetWorldRenderer()->CanRenderEverything());
            
            if (!shouldRender) continue;
            
            Vector3<float> unitPos = CalculateNuke3DPosition(nuke);
            
            //
            // Calculate size
            
            float baseSize = nuke->GetSize3D().DoubleValue();
            float size = baseSize * GLOBE_NUKE_MODEL_SIZE;
            
            //
            // Calculate orientation using trajectory derivative
            
            Vector3<float> direction(0, 0, 1);
            
            if (nuke->m_totalDistance > 0) {

                //
                // Get predicted progress and sample slightly ahead for direction
                
                float predictedProgress = CalculateNukePredictedProgress(nuke);
                float deltaProgress = 0.01f;
                float aheadProgress = fminf(1.0f, predictedProgress + deltaProgress);
                
                Vector3<float> aheadPos = CalculateNukeTrajectoryPoint(nuke, aheadProgress);
                
                direction = aheadPos - unitPos;
                direction.Normalise();
            } else {

                //
                // Fallback to history if trajectory shits itself
                
                LList<Vector3<Fixed> *>& history = nuke->GetHistory();
                if (history.Size() > 1) {
                    Vector3<float> currentPos = unitPos;
                    Vector3<Fixed>* olderHistoryPos = history[1];
                    Vector3<float> olderPos = CalculateHistoricalNuke3DPosition(nuke, *olderHistoryPos);
                    direction = currentPos - olderPos;
                    direction.Normalise();
                }
            }
            
            Vector3<float> forward = direction;
            forward.Normalise();
            
            Vector3<float> up = Vector3<float>(0.0f, 1.0f, 0.0f);
            if (fabsf(forward.y) > 0.9f) {
                up = Vector3<float>(1.0f, 0.0f, 0.0f);
            }
            
            Vector3<float> right = up ^ forward;
            right.Normalise();
            up = forward ^ right;
            up.Normalise();
            
            float modelRadius = nukeModel->GetBoundsRadius();
            float scale = size / modelRadius;
            
            //
            // Build model matrix from position, orientation, and scale
            
            Matrix4f modelMatrix;
            modelMatrix.m[0] = right.x * scale;
            modelMatrix.m[1] = right.y * scale;
            modelMatrix.m[2] = right.z * scale;
            modelMatrix.m[3] = 0.0f;
            
            modelMatrix.m[4] = up.x * scale;
            modelMatrix.m[5] = up.y * scale;
            modelMatrix.m[6] = up.z * scale;
            modelMatrix.m[7] = 0.0f;
            
            modelMatrix.m[8] = forward.x * scale;
            modelMatrix.m[9] = forward.y * scale;
            modelMatrix.m[10] = forward.z * scale;
            modelMatrix.m[11] = 0.0f;
            
            modelMatrix.m[12] = unitPos.x;
            modelMatrix.m[13] = unitPos.y;
            modelMatrix.m[14] = unitPos.z;
            modelMatrix.m[15] = 1.0f;
            
            //
            // Get team color for this nuke
            
            Team *team = g_app->GetWorld()->GetTeam(nuke->m_teamId);
            Colour color = team ? team->GetTeamColour() : White;
            
            if (!g_megavbo3d->AddInstanceIfNotFull(modelMatrix, color)) {
                g_megavbo3d->EndInstancedMegaVBO();
                g_megavbo3d->RenderInstancedMegaVBO3D(currentBatchKey);
                
                batchIndex++;
                sprintf(currentBatchKey, "%s_%d", baseBatchKey, batchIndex);
                
                g_megavbo3d->BeginInstancedMegaVBO(currentBatchKey, cacheKey);
                g_megavbo3d->AddInstance(modelMatrix, color);
            }
        }
    }
    
    g_megavbo3d->EndInstancedMegaVBO();
    g_megavbo3d->RenderInstancedMegaVBO3D(currentBatchKey);
    
    g_renderer->SetBlendMode(Renderer::BlendModeAdditive);
    
    END_PROFILE("3D Nukes");
}


//
// render 3D nuke trajectories and trails

void GlobeRenderer::Render3DNukeTrajectories()
{
    if (g_preferences->GetInt(PREFS_GRAPHICS_TRAILS) != 1) return;
    if (!ShouldUse3DNukeTrajectories()) return;

    START_PROFILE("3D Nuke Trajectories");
    
    int myTeamId = g_app->GetWorld()->m_myTeamId;

    for (int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i) {
        if (g_app->GetWorld()->m_objects.ValidIndex(i)) {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            
            //
            // Make sure to only process nukes

            if (wobj->m_type != WorldObject::TypeNuke) continue;
            if (!wobj->IsMovingObject()) continue;
            
            MovingObject *mobj = (MovingObject*)wobj;
            Nuke* nuke = (Nuke*)wobj;
            
            bool shouldRender = (myTeamId == -1 ||
                               wobj->m_teamId == myTeamId ||
                               wobj->m_visible[myTeamId] ||
                               g_app->GetWorldRenderer()->CanRenderEverything());
            
            if (!shouldRender) continue;
            
            //
            // Check if history exists

            LList<Vector3<Fixed> *>& history = mobj->GetHistory();
            if (history.Size() == 0) continue;
            
            Team *team = g_app->GetWorld()->GetTeam(mobj->m_teamId);
            Colour colour;
            if (team) {
                colour = team->GetTeamColour();
            } else {
                colour = COLOUR_SPECIALOBJECTS;
            }
            
            //
            // Calculate trail length based on zoom 

            int maxSize = history.Size();
            int sizeCap = (int)(80 * 0.5f); 
            
            //
            // Account for increased sampling rate to maintain trail length

            sizeCap *= 4;
            
            sizeCap /= World::GetGameScale().DoubleValue();
            maxSize = (maxSize > sizeCap ? sizeCap : maxSize);
            
            if (maxSize <= 0) continue;
            
            Vector3<float> currentPos = CalculateNuke3DPosition(nuke);
            Vector3<Fixed> pos(nuke->m_longitude, nuke->m_latitude, 0);
            float currentProgress = CalculateNukeProgressFromPosition(nuke, pos);
            
            GreatCircleConstants trajectoryConstants;
            bool hasTrajectory = CalculateNukeTrajectoryConstants(nuke, trajectoryConstants);
            
            //
            // Use 4x more segments to match renderhistory sampling rate
            
            int maxTrailLength = fmaxf(4, fminf(maxSize, 128));
            Vector3<float> prevPos = currentPos;
            float lineWidth = 1.5f;
            float invMaxTrailLength = 1.0f / (float)maxTrailLength;
            
            for (int j = 0; j < maxTrailLength; ++j) {

                //
                // Work backwards: j=0 is newest, j=maxTrailLength-1 is oldest

                float jNorm = (float)j * invMaxTrailLength;
                float historicalProgress = currentProgress * (1.0f - jNorm);
                historicalProgress = fmaxf(0.0f, fminf(1.0f, historicalProgress));
                
                Vector3<float> pos3D;
                if (!hasTrajectory) {
                    int actualHistoryIndex = (j * maxSize) / maxTrailLength;
                    if (actualHistoryIndex >= history.Size()) break;
                    Vector3<Fixed> *historyPos = history[actualHistoryIndex];
                    pos3D = ConvertLongLatTo3DPosition(historyPos->x.DoubleValue(), historyPos->y.DoubleValue());
                } else {
                    pos3D = CalculateTrajectoryPointFromConstants(trajectoryConstants, historicalProgress);
                }
                
                Colour segmentColour = colour;
                segmentColour.m_a = (unsigned char)(255.0f * (1.0f - jNorm));
                
                g_renderer3d->Line3D(prevPos.x, prevPos.y, prevPos.z,
                                             pos3D.x, pos3D.y, pos3D.z, segmentColour, lineWidth );
                
                prevPos = pos3D;
            }
        }
    }

    END_PROFILE("3D Nuke Trajectories");
}

void GlobeRenderer::RenderGunfire()
{
    START_PROFILE( "Gunfire" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;

    for( int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i )
    {
        if( g_app->GetWorld()->m_gunfire.ValidIndex(i) )
        {
            GunFire *gunFire = g_app->GetWorld()->m_gunfire[i];
            if( IsPointVisible( ConvertLongLatTo3DPosition(gunFire->m_longitude.DoubleValue(), gunFire->m_latitude.DoubleValue() ), GetCameraPosition(), GLOBE_RADIUS ) &&
                (gunFire->m_teamId == myTeamId ||
                 myTeamId == -1 ||
                 gunFire->m_visible[myTeamId] ||
                 g_app->GetWorldRenderer()->CanRenderEverything() ) )
            {
                g_app->GetWorld()->m_gunfire[i]->Render3D();
            }
        }
    }


    END_PROFILE( "Gunfire" );
}

void GlobeRenderer::RenderExplosions()
{
    START_PROFILE( "Explosions" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
    {
        if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
        {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            if( IsPointVisible( ConvertLongLatTo3DPosition(explosion->m_longitude.DoubleValue(), explosion->m_latitude.DoubleValue() ), GetCameraPosition(), GLOBE_RADIUS ) )
            {
                if( myTeamId == -1 ||
                    explosion->m_visible[myTeamId] || 
                    g_app->GetWorldRenderer()->CanRenderEverything() )
                {
                    g_app->GetWorld()->m_explosions[i]->Render3D();
                }
            }
        }
    }
    
    
    END_PROFILE( "Explosions" );
}

void GlobeRenderer::RenderPopulationDensity()
{
    START_PROFILE( "Population Density" );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    
    Vector3<float> cameraPos = GetCameraPosition();
    Image *populationImg = g_resource->GetImage( "graphics/population.bmp" );
    if( !populationImg ) return;
    
    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_teamId != -1 )
            {
                Vector3<float> cityPos = ConvertLongLatTo3DPosition(city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue());
                
                if (!IsPointVisible(cityPos, cameraPos, GLOBE_RADIUS))
                {
                    continue;
                }
                
                float size = sqrtf( sqrtf((float) city->m_population) ) / 2.0f;
                float size3D = size * GLOBE_OBJECT_SIZE;

                Colour col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                col.m_a = 255.0f * min( 1.0f, city->m_population / 10000000.0f );
                
                Vector3<float> renderPos = GetElevatedPosition(cityPos);
                                    
                g_renderer3d->StaticSprite3D( populationImg,
                                             renderPos.x, renderPos.y, renderPos.z,
                                             size3D * 2.0f, size3D * 2.0f, col, BILLBOARD_SURFACE_ALIGNED );
            }
        }
    }

    END_PROFILE( "Population Density" );
}

void GlobeRenderer::RenderNukeUnits()
{
    Team *team = g_app->GetWorld()->GetMyTeam();
    if( team )
    {
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == team->m_teamId )
                {
                    int nukeCount = 0;

                    switch( obj->m_type )
                    {
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
                            if( obj->m_nukeSupply > 0 )
                            {
                                nukeCount = obj->m_states[1]->m_numTimesPermitted;
                            }
                            break;
                    }

                    if( obj->UsingNukes() )
                    {
                        nukeCount -= obj->m_actionQueue.Size();
                    }

                    if( nukeCount > 0 )
                    {
                        RenderUnitHighlight( obj->m_objectId );
                    }
                }
            }
        }
    }
}

void GlobeRenderer::RenderUnitHighlight( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( obj )
    {
        float predictedLongitude = obj->m_longitude.DoubleValue() + 
								   obj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = obj->m_latitude.DoubleValue() + 
							      obj->m_vel.y.DoubleValue() * g_predictionTime  * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        
        Vector3<float> unitPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
        Vector3<float> cameraPos = GetCameraPosition();
        
        if (!IsPointVisible(unitPos, cameraPos, GLOBE_RADIUS))
        {
            return;
        }
        
        //
        // Position above globe surface
        
        Vector3<float> normal = unitPos.Normalized();
        unitPos = GetElevatedPosition(unitPos);
        
        float size2D = 5.0f;
        if( m_zoomFactor <=0.25f )
        {
            size2D *= m_zoomFactor * 4;
        }
        
        float size = size2D * (M_PI / 180.0f);
        
        Colour col = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetTeamColour();
        col.m_a = 255;
        if( fmod((float)g_gameTime*2, 2.0f) < 1.0f ) col.m_a = 100;
        
        //
        // Calculate surface-aligned tangent vectors
        
        Vector3<float> tangent1, tangent2;
        GetSurfaceTangents(normal, tangent1, tangent2);
        
        float halfSize = size * 0.4f;
        float extend = size * 0.5f;
        float rectWidth = 2.0f;
        float lineWidth = 1.0f;
        
        //
        // Render 4 extension lines 
        
        Vector3<float> leftStart = unitPos - tangent1 * (halfSize + extend);
        Vector3<float> leftEnd = unitPos - tangent1 * halfSize;
        g_renderer3d->Line3D( leftStart.x, leftStart.y, leftStart.z,
                             leftEnd.x, leftEnd.y, leftEnd.z, col, lineWidth );
        
        Vector3<float> rightStart = unitPos + tangent1 * (halfSize + extend);
        Vector3<float> rightEnd = unitPos + tangent1 * halfSize;
        g_renderer3d->Line3D( rightStart.x, rightStart.y, rightStart.z,
                             rightEnd.x, rightEnd.y, rightEnd.z, col, lineWidth );
        
        Vector3<float> topStart = unitPos + tangent2 * (halfSize + extend);
        Vector3<float> topEnd = unitPos + tangent2 * halfSize;
        g_renderer3d->Line3D( topStart.x, topStart.y, topStart.z,
                             topEnd.x, topEnd.y, topEnd.z, col, lineWidth );
        
        Vector3<float> bottomStart = unitPos - tangent2 * (halfSize + extend);
        Vector3<float> bottomEnd = unitPos - tangent2 * halfSize;
        g_renderer3d->Line3D( bottomStart.x, bottomStart.y, bottomStart.z,
                             bottomEnd.x, bottomEnd.y, bottomEnd.z, col, lineWidth );
        
        //
        // Render central square using Rect3D
        
        float rectSize = halfSize * 2.0f;
        g_renderer3d->Rect3D(unitPos, tangent1, tangent2, rectSize, rectSize, col, rectWidth);
    }
}

void GlobeRenderer::RenderWhiteBoard()
{
	if ( !g_app->GetWorldRenderer()->GetShowWhiteBoard() && !g_app->GetWorldRenderer()->GetEditWhiteBoard() ) 
	{
		return;
	}

	// Get effective team for whiteboard viewing - use perspective team if set, otherwise myTeam
	Team *effectiveTeam = g_app->GetWorldRenderer()->GetEffectiveWhiteBoardTeam();
	if( !effectiveTeam )
	{
		return;
	}

	START_PROFILE( "WhiteBoard" );

	int sizeteams = g_app->GetWorld()->m_teams.Size();
    for( int i = 0; i < sizeteams; ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[ i ];

		if ( ( g_app->GetWorldRenderer()->GetShowAllWhiteBoards() && g_app->GetWorld()->IsFriend ( effectiveTeam->m_teamId, team->m_teamId ) ) || 
		     effectiveTeam->m_teamId == team->m_teamId )
		{
			WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ team->m_teamId ];
			char whiteboardname[32];
			snprintf( whiteboardname, sizeof(whiteboardname), "WhiteBoard%d", team->m_teamId );

			Colour colourBoard = team->GetTeamColour();

			const LList<WhiteBoardPoint *> *points = whiteBoard->GetListPoints();
			int sizePoints = points->Size();
			float lineWidth = 2.0f;

			if ( sizePoints > 1 )
			{
				WhiteBoardPoint *prevPt = points->GetData( 0 );
				
				for ( int j = 1; j < sizePoints; j++ )
				{
					WhiteBoardPoint *currPt = points->GetData( j );

                    //
					// if this is a start point, skip the line from previous point

					if ( currPt->m_startPoint )
					{
						prevPt = currPt;
						continue;
					}

					Vector3<float> startPos = ConvertLongLatTo3DPosition(prevPt->m_longitude, prevPt->m_latitude);
					Vector3<float> endPos = ConvertLongLatTo3DPosition(currPt->m_longitude, currPt->m_latitude);
					g_renderer3d->Line3D( startPos.x, startPos.y, startPos.z, 
									  endPos.x, endPos.y, endPos.z, 
									  colourBoard, lineWidth );
					
					prevPt = currPt;
				}
			}
		}
	}

	END_PROFILE( "WhiteBoard" );
}

void GlobeRenderer::RenderAnimations()
{
    for( int i = 0; i <  g_app->GetWorldRenderer()->GetAnimations().Size(); ++i )
    {
        if(  g_app->GetWorldRenderer()->GetAnimations().ValidIndex(i) )
        {
            AnimatedIcon *anim =  g_app->GetWorldRenderer()->GetAnimations()[i];
            anim->Update();
            anim->Render3D();
            
            if( anim->IsFinished() )
            {
                 g_app->GetWorldRenderer()->GetAnimations().RemoveData(i);
                delete anim;
            }
        }
    }
    
}

void GlobeRenderer::RenderCities()
{
    START_PROFILE( "Cities" );

    g_renderer->SetFont( "kremlin", true );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    Image *cityImg = g_resource->GetImage( "graphics/city.bmp" );

    Vector3<float> cameraPos = GetCameraPosition();

    //
    // City icons

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetWorld()->m_cities.GetData(i);

        if( city->m_capital || city->m_teamId != -1 )
        {
            Vector3<float> cityPos = ConvertLongLatTo3DPosition(city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue());
            
            if (!IsPointVisible(cityPos, cameraPos, GLOBE_RADIUS))
            {
                continue;
            }
            
            float size = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;
            float size3D = size * GLOBE_OBJECT_SIZE;
            
            if( m_zoomFactor <= 0.2f )
            {
                size3D *= m_zoomFactor * 5;
            }

            Vector3<float> renderPos = GetElevatedPosition(cityPos);
            Vector3<float> normal = cityPos.Normalized();

            Colour col(100,100,100,100);
            if( city->m_teamId > -1 ) 
            {
                col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
            }                            
            col.m_a = 200.0f * ( 1.0f - min(0.8f, m_zoomFactor) );
                        
            g_renderer3d->StaticSprite3D( cityImg, renderPos.x, renderPos.y, renderPos.z,
                                          size3D * 2.0f, size3D * 2.0f, col, BILLBOARD_SURFACE_ALIGNED );

        
            //
            // Nuke icons

            if( city->m_numNukesInFlight || city->m_numNukesInQueue )
            {
                Colour col(255,0,0,255);
                if( !city->m_numNukesInFlight ) col.m_a = 100;
                float iconSize = 0.035f;
                float textSize = 0.01f;

                if( city->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.0035f;

                Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                g_renderer3d->RotatingSprite3D( img, renderPos.x, renderPos.y, renderPos.z, iconSize, iconSize, col, 0, BILLBOARD_SURFACE_ALIGNED );

                Vector3<float> surfacePos = normal * GLOBE_RADIUS;
                Vector3<float> tangent1, tangent2;
                GetSurfaceTangents(normal, tangent1, tangent2);
                
                float offsetDistance = 0.01f;
                Vector3<float> textPos = surfacePos + tangent2 * offsetDistance;
                if( city->m_numNukesInQueue )
                {
                    col.m_a = 100;
                    char caption[128];
                    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
                    LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInQueue, caption );
                    g_renderer3d->TextCentreSimple3D( textPos.x, textPos.y, textPos.z, col, textSize, caption, BILLBOARD_SURFACE_ALIGNED );
                }

                if( city->m_numNukesInFlight )
                {
                    col.m_a = 255;
                    char caption[128];
                    strcpy( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
                    LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInFlight, caption );
                    Vector3<float> textPos2 = textPos + tangent2 * 0.0075f;
                    g_renderer3d->TextCentreSimple3D( textPos2.x, textPos2.y, textPos2.z, col, textSize, caption, BILLBOARD_SURFACE_ALIGNED );
                }
            }
        }
    }
        
    
    //
    // City / country names

    bool showCityNames      = g_preferences->GetInt( PREFS_GRAPHICS_CITYNAMES );
    bool showCountryNames   = g_preferences->GetInt( PREFS_GRAPHICS_COUNTRYNAMES );

    if( showCityNames || showCountryNames )
    {
        Colour cityColour       (120,180,255,255);
        Colour countryColour    (150,255,150,0);

        cityColour.m_a      = 200.0f * (1.0f - sqrtf(m_zoomFactor));
        countryColour.m_a   = 200.0f * (1.0f - sqrtf(m_zoomFactor));

        for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_capital || city->m_teamId != -1 )
            {
                Vector3<float> cityPos = ConvertLongLatTo3DPosition(city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue());
                
                if (!IsPointVisible(cityPos, cameraPos, GLOBE_RADIUS))
                {
                    continue;
                }
                
                float textSize = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;
                float textSize3D = textSize * 0.01f;

                Vector3<float> renderPos = GetElevatedPosition(cityPos);
                Vector3<float> normal = cityPos.Normalized();

                if( showCityNames )
                {                                                  
                    textSize3D *= sqrtf( sqrtf( m_zoomFactor ) ) * 0.8f;

                    g_renderer3d->TextCentreSimple3D( renderPos.x, renderPos.y, renderPos.z, cityColour, textSize3D, LANGUAGEPHRASEADDITIONAL(city->m_name), BILLBOARD_SURFACE_ALIGNED );
                }                

                if( showCountryNames && city->m_capital )
                {
                    float countrySize = textSize3D;
                    Vector3<float> surfacePos = normal * GLOBE_RADIUS;
                    Vector3<float> tangent1, tangent2;
                    GetSurfaceTangents(normal, tangent1, tangent2);
                    
                    float offsetDistance = textSize3D * 1.0f;
                    Vector3<float> countryPos = surfacePos - tangent2 * offsetDistance;
                    g_renderer3d->TextCentreSimple3D( countryPos.x, countryPos.y, countryPos.z, countryColour, countrySize, LANGUAGEPHRASEADDITIONAL(city->m_country), BILLBOARD_SURFACE_ALIGNED );
                }
            }
        }
    }

	g_renderer->SetFont();

    END_PROFILE( "Cities" );
}

//
// Render Santa on the 3D globe
// identical logic to map renderer but adapted for 3D coordinates

void GlobeRenderer::RenderSanta()
{
#ifdef ENABLE_SANTA_EASTEREGG
	Vector3<float> cameraPos = GetCameraPosition();
	
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
			g_app->GetWorld()->m_santaCurrent = 0;
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

        if( g_app->GetWorld()->m_santaCurrent >= g_app->GetWorld()->m_santaRouteLength.Size() ) 
        {
            g_app->GetWorld()->m_santaAlive = false;
            return;
        }
        
        //
		// If index is odd then we are inside a city so don't render santa

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
				//
				// Convert 2D coordinates to 3D globe position

				Vector3<float> santaPos = ConvertLongLatTo3DPosition(x, y);
				
				if (!IsPointVisible(santaPos, cameraPos, GLOBE_RADIUS)) {
					return;
				}
				
				//
				// Position above globe surface like other units
				
				santaPos = GetElevatedPosition(santaPos);
				
				// convert 2D size to 3D world scale
				float santaSize = thisSize * GLOBE_OBJECT_SIZE;
				
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, -santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				else
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}			
			}
		}
		else
		{
			City *city;

			int index = ( g_app->GetWorld()->m_santaCurrent + 1 )/ 2;
			if (index >= g_app->GetWorld()->m_santaRoute.Size())
			{
				index = 0;
			}
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
				Vector3<float> santaPos = ConvertLongLatTo3DPosition(x, y);
				
				if (!IsPointVisible(santaPos, cameraPos, GLOBE_RADIUS)) {
					return;
				}
				
				santaPos = GetElevatedPosition(santaPos);
				
				float santaSize = thisSize * GLOBE_OBJECT_SIZE;
				
				if ( g_app->GetWorld()->m_santaPrevFlipped )
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, -santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}
				else
				{
					g_renderer3d->RotatingSprite3D( bmpImage, santaPos.x, santaPos.y, santaPos.z, santaSize * 2.0f, santaSize * 2.0f, colour, 0, BILLBOARD_SURFACE_ALIGNED );
				}			
			}		
		}
		
	}

    //Nuke Santa and thereby end Christmas for everyone, for ever more
#endif
}

void GlobeRenderer::RenderWorldObjectTargets( WorldObject *wobj, bool maxRanges )
{
#ifndef NON_PLAYABLE
    if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 )
    {
        float predictedLongitude = wobj->m_longitude.DoubleValue() +
								   wobj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = wobj->m_latitude.DoubleValue() +
								  wobj->m_vel.y.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

        Vector3<float> predictedPos = ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
        Vector3<float> predictedRenderPos = GetElevatedPosition(predictedPos);
        Vector3<float> predictedNormal = predictedPos.Normalized();

        if( maxRanges )
        {
            int renderTooltip = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );

            //
            // Render red circle for our action range

            float range = wobj->GetActionRange().DoubleValue();
            if( range <= 180.0f && range > 0.0f )
            {
                Colour col(255,0,0,255);
                float range3D = range * 0.01f;
                g_renderer3d->Circle3D( predictedRenderPos.x, predictedRenderPos.y, predictedRenderPos.z, range3D, 40, col );
                
                if( renderTooltip )
                {
                    g_renderer->SetFont( "kremlin", true );
                    Vector3<float> textPos = predictedRenderPos + predictedNormal * range3D;
                    g_renderer3d->TextCentreSimple3D( textPos.x, textPos.y, textPos.z, col, 0.01f, LANGUAGEPHRASE("dialog_mapr_combat_range"), BILLBOARD_SURFACE_ALIGNED );
                    g_renderer->SetFont();
                }
            }


            //
            // Render blue circle for our movement range

            if( wobj->IsMovingObject() )
            {
                MovingObject *mobj = (MovingObject *) wobj;
                if( mobj->m_range <= 180 && mobj->m_range > 0 )
                {                    
                    float predictedRange = mobj->m_range.DoubleValue();

                    Colour col(0,0,255,255);
                    float range3D = predictedRange * 0.01f;
                    g_renderer3d->Circle3D( predictedRenderPos.x, predictedRenderPos.y, predictedRenderPos.z, range3D, 50, col );

                }
            }   
        }
        
        
        //
        // Render red action line to our combat target

        int targetObjectId = wobj->GetTargetObjectId();
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( targetObject )
        {
            float TpredictedLongitude = targetObject->m_longitude.DoubleValue() + 
										targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
            float TpredictedLatitude = targetObject->m_latitude.DoubleValue() + 
									   targetObject->m_vel.y.DoubleValue() * g_predictionTime * 
									   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

            Vector3<float> targetPos = ConvertLongLatTo3DPosition(TpredictedLongitude, TpredictedLatitude);
            Vector3<float> targetRenderPos = GetElevatedPosition(targetPos);

            Colour actionCursorCol( 255,0,0,150 );
            float actionCursorSize = GLOBE_ANIMATED_ICON_SIZE;
            float actionCursorAngle = g_gameTime * -1.0f;

            Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
            g_renderer3d->RotatingSprite3D( img, targetRenderPos.x, targetRenderPos.y, targetRenderPos.z, 
                                actionCursorSize, actionCursorSize, 
                                actionCursorCol, actionCursorAngle, BILLBOARD_SURFACE_ALIGNED );

            RenderActionLine( predictedLongitude, predictedLatitude, 
                              TpredictedLongitude, TpredictedLatitude, 
                              actionCursorCol, 0.5f );
        }
        

        //
        // Render blue action line to our movement target

        if( !targetObject && wobj->IsMovingObject() )
        {
            MovingObject *mobj = (MovingObject *) wobj;
            float actionCursorLongitude = mobj->m_targetLongitude.DoubleValue();
            float actionCursorLatitude = mobj->m_targetLatitude.DoubleValue();

            if( mobj->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(mobj->m_teamId)->GetFleet(mobj->m_fleetId);
                if( fleet && 
                    fleet->m_targetLongitude != 0 && 
                    fleet->m_targetLatitude != 0 )
                {
                    actionCursorLongitude = fleet->m_targetLongitude.DoubleValue();
                    actionCursorLatitude = fleet->m_targetLatitude.DoubleValue();                    
                }
            }
            if( actionCursorLongitude != 0.0f || actionCursorLatitude != 0.0f )
            {
                Vector3<float> targetPos = ConvertLongLatTo3DPosition(actionCursorLongitude, actionCursorLatitude);
                Vector3<float> targetRenderPos = GetElevatedPosition(targetPos);

                Colour actionCursorCol( 0,0,255,150 );
                float actionCursorSize = GLOBE_ANIMATED_ICON_SIZE;
                float actionCursorAngle = 0;

                if( mobj->m_type == WorldObject::TypeNuke )
                {
                    actionCursorCol.Set( 255,0,0,150 );
                }

                if( mobj->m_type == WorldObject::TypeBomber &&
                    mobj->m_currentState == 1 )
                {
                    actionCursorCol.Set( 255,0,0,150 );
                }

                Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
                g_renderer3d->RotatingSprite3D( img, targetRenderPos.x, targetRenderPos.y, targetRenderPos.z, 
                                    actionCursorSize, actionCursorSize, 
                                    actionCursorCol, actionCursorAngle, BILLBOARD_SURFACE_ALIGNED );

                RenderActionLine( predictedLongitude, predictedLatitude, 
                                  actionCursorLongitude, actionCursorLatitude, 
                                  actionCursorCol, 0.5f );
            }
        }


        //
        // Render our action queue

        if( wobj->m_actionQueue.Size() )
        {
            Image *img = NULL;
            float size = 0.025f;
            Colour col(255,255,255,100);

            switch( wobj->m_type )
            {
                case WorldObject::TypeAirBase:  
                case WorldObject::TypeCarrier:
                    if( wobj->m_currentState == 0 ) img = g_resource->GetImage( "graphics/fighter.bmp" );
                    if( wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/bomber.bmp" );
                    break;

                case WorldObject::TypeSilo:
                    if( wobj->m_currentState == 0 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;

                case WorldObject::TypeSub:
                    if( wobj->m_currentState == 2 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;
            }
            
            if( img )
            {
                for( int i = 0; i < wobj->m_actionQueue.Size(); ++i )
                {
                    ActionOrder *order = wobj->m_actionQueue[i];
                    float targetLongitude = order->m_longitude.DoubleValue();
                    float targetLatitude = order->m_latitude.DoubleValue();

                    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( order->m_targetObjectId );
                    if( targetObject )
                    {
                        targetLongitude = targetObject->m_longitude.DoubleValue() +
										   targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                        targetLatitude = targetObject->m_latitude.DoubleValue() +
										 targetObject->m_vel.y.DoubleValue() * g_predictionTime *
										 g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                    }

                    Vector3<float> targetPos = ConvertLongLatTo3DPosition(targetLongitude, targetLatitude);
                    Vector3<float> targetRenderPos = GetElevatedPosition(targetPos);

                    float lineX = ( targetLongitude - predictedLongitude );
                    float lineY = ( targetLatitude - predictedLatitude );

                    float angle = atan( -lineX / lineY );
                    if( lineY < 0.0f ) angle += M_PI;
                
                    g_renderer3d->RotatingSprite3D( img, targetRenderPos.x, targetRenderPos.y, targetRenderPos.z, 
                                                  size, size, col, angle, BILLBOARD_SURFACE_ALIGNED );
                    RenderActionLine( predictedLongitude, predictedLatitude,
                                      targetLongitude, targetLatitude,
                                      col, 0.5f );
                }
            }
        }
    }
#endif
}

void GlobeRenderer::RenderActionLine( float fromLong, float fromLat, float toLong, float toLat, Colour col, float width, bool animate )
{
#ifndef NON_PLAYABLE
    float distance      = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceLeft  = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong - 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceRight = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong + 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    if( distanceLeft < distance )
    {
        if( fromLong < 0.0f && toLong > 0.0f )
        {
            toLong -= 360.0f;
        }
    }
    else if( distanceRight < distance )
    {
        if( fromLong > 0.0f && toLong < 0.0f )
        {
            toLong += 360.0f;
        }
    }

    Vector3<float> fromPos = ConvertLongLatTo3DPosition(fromLong, fromLat);
    Vector3<float> toPos = ConvertLongLatTo3DPosition(toLong, toLat);
    
    Vector3<float> fromNormal = fromPos.Normalized();
    Vector3<float> toNormal = toPos.Normalized();
    
    int numSegments = (int)(distance / 2.5f) + 1;
    numSegments = fmaxf(1, fminf(numSegments, 200));
    
    Vector3<float> prevRenderPos = GetElevatedPosition(fromPos);
    
    for( int i = 1; i <= numSegments; ++i )
    {
        float t = (float)i / (float)numSegments;
        
        Vector3<float> interpolatedNormal = SlerpNormal(fromNormal, toNormal, t);
        Vector3<float> interpolatedPos = interpolatedNormal * GLOBE_RADIUS;
        Vector3<float> currentRenderPos = GetElevatedPosition(interpolatedPos);
        
        g_renderer3d->Line3D( prevRenderPos.x, prevRenderPos.y, prevRenderPos.z, 
                             currentRenderPos.x, currentRenderPos.y, currentRenderPos.z, col, width );
        
        prevRenderPos = currentRenderPos;
    }

    if( animate )
    {
        //
        // Animate along the subdivided path
        
        float factor1 = fmodf(GetHighResTime(), 1.0f );
        float factor2 = fmodf(GetHighResTime(), 1.0f ) + 0.2f;
        Clamp( factor1, 0.0f, 1.0f );
        Clamp( factor2, 0.0f, 1.0f );
        
        if( factor2 > factor1 )
        {
            Vector3<float> normal1 = SlerpNormal(fromNormal, toNormal, factor1);
            Vector3<float> normal2 = SlerpNormal(fromNormal, toNormal, factor2);
            
            Vector3<float> pos1 = GetElevatedPosition(normal1 * GLOBE_RADIUS);
            Vector3<float> pos2 = GetElevatedPosition(normal2 * GLOBE_RADIUS);
            
            g_renderer3d->Line3D( pos1.x, pos1.y, pos1.z, 
                                 pos2.x, pos2.y, pos2.z, col, width );
        }
    }

#endif
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