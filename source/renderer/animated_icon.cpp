#include "lib/universal_include.h"

#include "lib/render/colour.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"

#include "lib/resource/resource.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"
#include "renderer/animated_icon.h"

AnimatedIcon::AnimatedIcon()
:   m_longitude(0.0f),
    m_latitude(0.0f),
    m_fromObjectId(-1)
{
    m_visible.Initialise(MAX_TEAMS);
    m_visible.SetAll( false );

    m_startTime = GetHighResTime();
}


// ============================================================================


ActionMarker::ActionMarker()
:   AnimatedIcon(),
    m_combatTarget(false),
    m_targetType(WorldObject::TargetTypeInvalid),
    m_size(0.0f)
{
}

void ActionMarker::Update()
{
    float timePassed = GetHighResTime() - m_startTime;
    m_size = 4.0 - timePassed * 8.0f;
    m_size = max( m_size, 0.0f );
}

void ActionMarker::Render2D()
{
    float size = m_size;
    
    Colour col = Colour(0,0,255,255);
    if( m_combatTarget ) col.Set(255,0,0,255);

    if( m_targetType >= WorldObject::TargetTypeValid )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fromObjectId );
        if( obj )
        {
            Colour lineCol = col;
            int thisAlpha = size * 64;
            Clamp( thisAlpha, 0, 255 );
            lineCol.m_a = thisAlpha;
            g_app->GetMapRenderer()->RenderActionLine( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), m_longitude, m_latitude, lineCol, 1.0f, false );
        }

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp");
        g_renderer2d->StaticSprite( img, m_longitude - size/2, m_latitude - size/2, size, size, col );
    
        if( m_targetType > WorldObject::TargetTypeValid )
        {
            img = NULL;
            switch( m_targetType )
            {
                case WorldObject::TargetTypeLaunchFighter:      img = g_resource->GetImage( "graphics/fighter.bmp" );       break;
                case WorldObject::TargetTypeLaunchBomber:       img = g_resource->GetImage( "graphics/bomber.bmp" );        break;
                case WorldObject::TargetTypeLaunchNuke:         img = g_resource->GetImage( "graphics/nuke.bmp" );    break;
                case WorldObject::TargetTypeLaunchLACM:         img = g_resource->GetImage( "graphics/lacm.bmp" );    break;
                case WorldObject::TargetTypeLaunchCBM:          img = g_resource->GetImage( "graphics/nuke.bmp" );    break;
            }

            if( img )
            {
                g_renderer2d->StaticSprite( img, m_longitude - size/4, m_latitude - size/4, size/2, size/2, col );
            }
        }
    }
}

void ActionMarker::Render3D()
{
    float size = m_size;
    
    Colour col = Colour(0,0,255,255);
    if( m_combatTarget ) col.Set(255,0,0,255);

    if( m_targetType >= WorldObject::TargetTypeValid )
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fromObjectId );
        if( obj )
        {
            Colour lineCol = col;
            int thisAlpha = size * 64;
            Clamp( thisAlpha, 0, 255 );
            lineCol.m_a = thisAlpha;
            globeRenderer->RenderActionLine( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), m_longitude, m_latitude, lineCol, 2.0f, false );
        }

        Vector3<float> markerPos = globeRenderer->ConvertLongLatTo3DPosition(m_longitude, m_latitude);
        Vector3<float> normal = markerPos;
        normal.Normalise();
        markerPos += normal * GLOBE_ELEVATION;
        
        float size3D = size * 0.01f;
        
        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp");
        g_renderer3d->StaticSprite3D( img, markerPos.x, markerPos.y, markerPos.z, size3D, size3D, col, BILLBOARD_SURFACE_ALIGNED );
    
        if( m_targetType > WorldObject::TargetTypeValid )
        {
            img = NULL;
            switch( m_targetType )
            {
                case WorldObject::TargetTypeLaunchFighter:      img = g_resource->GetImage( "graphics/fighter.bmp" );       break;
                case WorldObject::TargetTypeLaunchBomber:       img = g_resource->GetImage( "graphics/bomber.bmp" );        break;
                case WorldObject::TargetTypeLaunchNuke:         img = g_resource->GetImage( "graphics/nuke.bmp" );    break;
                case WorldObject::TargetTypeLaunchLACM:         img = g_resource->GetImage( "graphics/lacm.bmp" );    break;
                case WorldObject::TargetTypeLaunchCBM:          img = g_resource->GetImage( "graphics/nuke.bmp" );    break;
            }

            if( img )
            {
                g_renderer3d->StaticSprite3D( img, markerPos.x, markerPos.y, markerPos.z, size3D/2, size3D/2, col, BILLBOARD_SURFACE_ALIGNED );
            }
        }
    }
}

bool ActionMarker::IsFinished()
{
    float timePassed = GetHighResTime() - m_startTime;
    return ( timePassed > 0.5f );
}


// ============================================================================


SonarPing::SonarPing()
:   AnimatedIcon(),
    m_size(0.0f)
{
}

void SonarPing::Update()
{
    float timePassed = GetHighResTime() - m_startTime;
    m_size = timePassed * 2.5f;
    Clamp( m_size, 0.0f, 5.0f );
}
            
void SonarPing::Render2D()
{
    float size = m_size;

    if( g_app->GetWorld()->m_myTeamId == -1 ||
        m_visible[ g_app->GetWorld()->m_myTeamId ] )
    {
        int alpha = 255 - 255 * (size / 5.0f);
        alpha *= 0.5f;
        
        Colour colour(255, 255, 255, alpha);
        int numPoints = 40;
        float angleStep = 2.0f * M_PI / (float)numPoints;
        
        for( int i = 0; i < numPoints; ++i )
        {
            float angle1 = (float)i * angleStep;
            float angle2 = (float)(i + 1) * angleStep;
            
            float x1 = m_longitude + size * sinf(angle1);
            float y1 = m_latitude + size * cosf(angle1);
            float x2 = m_longitude + size * sinf(angle2);
            float y2 = m_latitude + size * cosf(angle2);
            
            g_renderer2d->Line( x1, y1, x2, y2, colour );
        }
    }
}

void SonarPing::Render3D()
{
    float size = m_size;

    if( g_app->GetWorld()->m_myTeamId == -1 ||
        m_visible[ g_app->GetWorld()->m_myTeamId ] )
    {
        int alpha = 255 - 255 * (size / 5.0f);
        alpha *= 0.5f;
        
        Colour colour(255, 255, 255, alpha);
        int numPoints = 40;
        float angleStep = 2.0f * M_PI / (float)numPoints;
        
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        Vector3<float> centerPos = globeRenderer->ConvertLongLatTo3DPosition(m_longitude, m_latitude);
        Vector3<float> centerNormal = centerPos;
        centerNormal.Normalise();
        
        Vector3<float> tangent1, tangent2;
        globeRenderer->GetSurfaceTangents(centerNormal, tangent1, tangent2);
        
        float size3D = size * (M_PI / 180.0f) * GLOBE_RADIUS;
        
        for( int i = 0; i < numPoints; ++i )
        {
            float angle1 = (float)i * angleStep;
            float angle2 = (float)(i + 1) * angleStep;
            
            Vector3<float> offset1 = tangent1 * (size3D * cosf(angle1)) + tangent2 * (size3D * sinf(angle1));
            Vector3<float> offset2 = tangent1 * (size3D * cosf(angle2)) + tangent2 * (size3D * sinf(angle2));
            
            Vector3<float> pos1 = centerPos + offset1;
            Vector3<float> pos2 = centerPos + offset2;
            
            pos1.Normalise();
            pos1 = pos1 * GLOBE_RADIUS + pos1 * GLOBE_ELEVATION;
            
            pos2.Normalise();
            pos2 = pos2 * GLOBE_RADIUS + pos2 * GLOBE_ELEVATION;
            
            g_renderer3d->Line3D( pos1.x, pos1.y, pos1.z, pos2.x, pos2.y, pos2.z, colour );
        }
    }
}

bool SonarPing::IsFinished()
{
    return( m_size == 5.0f );
}


// ============================================================================


NukePointer::NukePointer()
:   AnimatedIcon(),
    m_lifeTime(10.0f),
    m_targetId(-1),
    m_mode(0),
    m_offScreen(false),
    m_seen(false)
{
}


void NukePointer::Merge()
{
    //
    // Delete all nuke pointers currently at this location
    // (this new one will take over now)

    for( int i = 0; i < g_app->GetWorldRenderer()->GetAnimations().Size(); ++i )
    {
        if( g_app->GetWorldRenderer()->GetAnimations().ValidIndex(i) )
        {
            NukePointer *anim = (NukePointer *)g_app->GetWorldRenderer()->GetAnimations()[i];
            if( anim->m_animationType == WorldRenderer::AnimationTypeNukePointer &&
                anim != this &&
                anim->m_targetId == m_targetId )
            {
                anim->m_lifeTime = 0.0f;
                anim->m_targetId = -1;
            }
        }
    }
}


void NukePointer::Update()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetId );
    if( !obj ) 
    {
        return;
    }

    if( m_seen )
    {
        m_lifeTime -= g_advanceTime;
        m_lifeTime = max( m_lifeTime, 0.0f );
    }

    if( g_app->IsGlobeMode() )
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        Vector3<float> objPos = globeRenderer->ConvertLongLatTo3DPosition(obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue());
        Vector3<float> cameraPos = globeRenderer->GetCameraPosition();
        m_offScreen = !globeRenderer->IsPointVisible(objPos, cameraPos, GLOBE_RADIUS);
    }
    else
    {
        m_offScreen = !g_app->GetMapRenderer()->IsOnScreen(obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue());
    }

    if( !m_offScreen && !g_app->m_hidden )
    {
        m_seen = true;
    }
}

void NukePointer::Render2D()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetId );
    if( !obj ) 
    {
        return;
    }

    int transparency = 200;
    if( m_lifeTime < 5.0f )
    {
        transparency *= m_lifeTime * 0.2f;
    }

    transparency = 255;
    
    Colour col = Colour(255,255,0,transparency);
    float size = 2.0f;
    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25 )
    {
        size *= g_app->GetMapRenderer()->GetZoomFactor() * 4;
    }

    if( !m_offScreen )
    {
        if( m_mode == 0 )
        {
            Image *img = g_resource->GetImage( "graphics/launchsymbol.bmp" );
            if( img )
                g_renderer2d->RotatingSprite( img, obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), size*2, size*2, col, 0 );
        }
    }

    if( m_offScreen && !m_seen )
    {
        size = 2 * g_app->GetMapRenderer()->GetDrawScale();

        if( m_mode == 1 )
        {
            m_mode = 0;
            return;
        }

        Fixed distance1 = g_app->GetWorld()->GetDistance( obj->m_longitude, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );

        Fixed distance2 = g_app->GetWorld()->GetDistance( obj->m_longitude + 360, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );

        Fixed distance3 = g_app->GetWorld()->GetDistance( obj->m_longitude - 360, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );
		
		float longitude = obj->m_longitude.DoubleValue();
		float latitude = obj->m_latitude.DoubleValue();

        if( distance2 < distance1 ) longitude += 360;
        if( distance3 < distance1 ) longitude -= 360;

        g_app->GetMapRenderer()->FindScreenEdge( Vector3<float>(longitude, latitude, 0), &m_longitude, &m_latitude);
        Vector3<float> targetDir = (Vector3<float>( longitude, latitude, 0 ) -
									Vector3<float>( m_longitude, m_latitude, 0 )).Normalise();
        float angle = atan( -targetDir.x / targetDir.y );
        if( targetDir.y < 0.0f ) angle += M_PI;

        Image *img = g_resource->GetImage( "graphics/arrow.bmp" );
        g_renderer2d->RotatingSprite( img, m_longitude, m_latitude, size, size, col, angle );
    }
}

void NukePointer::Render3D()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetId );
    if( !obj ) 
    {
        return;
    }

    int transparency = 200;
    if( m_lifeTime < 5.0f )
    {
        transparency *= m_lifeTime * 0.2f;
    }

    transparency = 255;
    
    Colour col = Colour(255,255,0,transparency);
    float size = 2.0f;

    GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
    Vector3<float> objPos = globeRenderer->ConvertLongLatTo3DPosition(obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue());
    
    if( !m_offScreen )
    {
        if( m_mode == 0 )
        {
            Vector3<float> normal = objPos;
            normal.Normalise();
            objPos += normal * GLOBE_ELEVATION;
            
            float size3D = size * 0.035f;
            
            Image *img = g_resource->GetImage( "graphics/launchsymbol.bmp" );
            if( img )
                g_renderer3d->RotatingSprite3D( img, objPos.x, objPos.y, objPos.z, size3D*2, size3D*2, col, 0, BILLBOARD_SURFACE_ALIGNED );
        }
    }
}

bool NukePointer::IsFinished()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetId );
    if( !obj ) 
    {
        return true;
    }
    
    if( m_offScreen && !m_seen )
    {
        return ( m_lifeTime <= 0.0f );
    }
    
    return ( m_lifeTime <= 0.0f );
}

