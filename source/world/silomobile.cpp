#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/silomobile.h"


SiloMobile::SiloMobile()
:   Silo()
{
    SetType( TypeSiloMobile );
    strcpy( bmpImageFilename, "graphics/silomob.bmp" );

    m_stealthType = 1;
    m_life = 1;
    m_nukeSupply = 2;
    m_states[0]->m_numTimesPermitted = 2;
    m_states[1]->m_numTimesPermitted = 2;

    // Launch state (state 1): reduced radar range
    Fixed gameScale = World::GetUnitScaleFactor();
    m_states[1]->m_radarRange = Fixed( 1 ) / gameScale;
    m_states[1]->m_radarearly1Range = m_states[1]->m_radarRange * Fixed::FromDouble( 1.5 );
    m_states[1]->m_radarearly2Range = m_states[1]->m_radarRange * Fixed::FromDouble( 2.0 );
    m_states[1]->m_radarstealth1Range = m_states[1]->m_radarRange * Fixed::FromDouble( 0.33 );
    m_states[1]->m_radarstealth2Range = m_states[1]->m_radarRange * Fixed::FromDouble( 0.66 );
}


void SiloMobile::SetState( int state )
{
    if( CanSetState( state ) )
    {
        WorldObjectState *theState = m_states[state];
        m_currentState = state;
        m_stateTimer = theState->m_timeToPrepare;
        m_targetObjectId = -1;
        if( state == 1 )
            strcpy( bmpImageFilename, "graphics/silomob_erected.bmp" );
        else
            strcpy( bmpImageFilename, "graphics/silomob.bmp" );
    }
}


Image *SiloMobile::GetBmpImage( int state )
{
    if( state == 1 )
        return g_resource->GetImage( "graphics/silomob_erected.bmp" );
    return g_resource->GetImage( "graphics/silomob.bmp" );
}
