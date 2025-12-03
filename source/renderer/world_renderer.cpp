#include "lib/universal_include.h"

#include <math.h>

#include "app/app.h"

#include "renderer/world_renderer.h"
#include "renderer/animated_icon.h"

WorldRenderer::WorldRenderer()
:   m_renderEverything(false)
{
}

WorldRenderer::~WorldRenderer()
{
    for( int i = 0; i < m_animations.Size(); ++i )
    {
        delete m_animations[i];
    }
    m_animations.Empty();
}

void WorldRenderer::Init()
{
    
}

void WorldRenderer::Reset()
{
    m_animations.EmptyAndDelete();
}

void WorldRenderer::SetRenderEverything( bool _renderEverything )
{
    m_renderEverything = _renderEverything;
}

int WorldRenderer::CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude )
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