#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render3d/renderer_3d.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/globe_renderer.h"

#include "world/world.h"
#include "world/explosion.h"



Explosion::Explosion()
:   WorldObject(),
    m_intensity(0),
    m_initialIntensity(-1),
    m_underWater(false),
    m_targetTeamId(-1)
{
    SetType( TypeExplosion );

    m_stealthType = 200;  // early2 - very visible
    strcpy( bmpImageFilename, "graphics/explosion.bmp" );
}


bool Explosion::Update()
{
    if( m_initialIntensity == Fixed::FromDouble(-1.0f) )
    {
        m_initialIntensity = m_intensity;
    }

    Fixed dropOff = 0;

    if( m_underWater )
    {
        dropOff = Fixed::Hundredths(80);
    }
    else
    {
        dropOff = Fixed::Hundredths(20);
    }

    m_intensity -= Fixed::Hundredths(20) * SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    
    return( m_intensity <= 0 );
}

void Explosion::Render2D()
{
    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    if( bmpImage )
    {
        Colour colour;
        if( m_underWater )
        {
            colour = Colour( 50, 50, 100, 150 );
        }
        else
        {           
            colour = Colour( 255, 255, 255, 230 );
        }

        if( m_initialIntensity != Fixed::FromDouble(-1.0f) )
        {
            float fraction = ( m_intensity / m_initialIntensity ).DoubleValue();
            if( fraction < 0.7f ) fraction = 0.7f;
            colour.m_a *= fraction;
        }

        float predictedIntensity = m_intensity.DoubleValue() - 0.2f *
								   g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float size = predictedIntensity / 20.0f;
        float gameScale = g_app->GetWorld()->GetGameScale().DoubleValue();
        if( gameScale > 0.01f )
            size /= (float)sqrt(gameScale);

        float predictedLongitude = m_longitude.DoubleValue();
        float predictedLatitude = m_latitude.DoubleValue();

        g_renderer2d->StaticSprite( bmpImage, predictedLongitude-size, predictedLatitude-size,
                                    size*2, size*2, colour );
    }      
}

void Explosion::RenderImpactSymbol()
{
    // Nuclear explosions only: impactsymbol in red, rendered in separate overlay pass after all explosions
    Fixed initIntensity = ( m_initialIntensity > Fixed::FromDouble(-0.5f) ) ? m_initialIntensity : m_intensity;
    if( initIntensity <= 90 ) return;

    float predictedIntensity = m_intensity.DoubleValue() - 0.2f *
        g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
    if( predictedIntensity <= 0 ) return;

    float size = predictedIntensity / 20.0f;
    float gameScale = g_app->GetWorld()->GetGameScale().DoubleValue();
    if( gameScale > 0.01f )
        size /= (float)sqrt(gameScale);

    float predictedLongitude = m_longitude.DoubleValue();
    float predictedLatitude = m_latitude.DoubleValue();

    Image *impactImg = g_resource->GetImage( "graphics/impactsymbol.bmp" );
    if( impactImg )
    {
        Colour impactCol( 255, 0, 0, 255 );
        float impactSize = size * 0.6f;
        g_renderer2d->StaticSprite( impactImg, predictedLongitude - impactSize, predictedLatitude + impactSize,
                                    impactSize * 2, -impactSize * 2, impactCol );
    }
}

void Explosion::Render3D()
{
    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    if( bmpImage )
    {
        Colour colour;
        if( m_underWater )
        {
            colour = Colour( 50, 50, 100, 150 );
        }
        else
        {           
            colour = Colour( 255, 255, 255, 230 );
        }

        if( m_initialIntensity != Fixed::FromDouble(-1.0f) )
        {
            float fraction = ( m_intensity / m_initialIntensity ).DoubleValue();
            if( fraction < 0.7f ) fraction = 0.7f;
            colour.m_a *= fraction;
        }

        float predictedIntensity = m_intensity.DoubleValue() - 0.2f *
								   g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float size = predictedIntensity / 20.0f;
        float gameScale = g_app->GetWorld()->GetGameScale().DoubleValue();
        if( gameScale > 0.01f )
            size /= (float)sqrt(gameScale);

        float predictedLongitude = m_longitude.DoubleValue();
        float predictedLatitude = m_latitude.DoubleValue();

        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        Vector3<float> explosionPos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
        
        Vector3<float> normal = explosionPos;
        normal.Normalise();
        explosionPos += normal * GLOBE_ELEVATION;
        
        float size3D = size * 0.01f;
        
        g_renderer3d->StaticSprite3D( bmpImage, explosionPos.x, explosionPos.y, explosionPos.z,
                                      size3D * 2.0f, size3D * 2.0f, colour, BILLBOARD_SURFACE_ALIGNED );
    }      
}
