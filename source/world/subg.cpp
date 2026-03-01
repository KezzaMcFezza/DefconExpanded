#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "lib/debug/profiler.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/subg.h"
#include "world/subc.h"
#include "world/sub.h"
#include "world/lacm.h"
#include "world/fleet.h"


SubG::SubG()
:   SubC()
{
    SetType( TypeSubG );

    strcpy( bmpImageFilename, "graphics/subg.bmp" );

    m_states[0]->m_timeToPrepare = 45;
    m_states[1]->m_timeToPrepare = 60;
    m_states[0]->m_timeToReload = 15;
    m_states[1]->m_timeToReload = 30;

    Fixed gameScale = World::GetUnitScaleFactor();
    m_states[0]->m_actionRange = Fixed(2) / gameScale;
    m_states[1]->m_actionRange = Fixed(3) / gameScale;
    // LACM mode (state 3): 40 ammo instead of 20
    if( m_states.Size() > 3 )
    {
        m_states[3]->m_timeToReload = 5;
        m_states[2]->m_numTimesPermitted = 30;
        m_states[3]->m_numTimesPermitted = 30;
    }
}

bool SubG::Update()
{
    bool result = SubC::Update();
    bool isEnemy = ( g_app->GetWorld()->m_myTeamId >= 0 &&
                     !g_app->GetWorld()->IsFriend( m_teamId, g_app->GetWorld()->m_myTeamId ) );
    bool surfaced = ( m_currentState == 3 );
    if( isEnemy )
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/sub_surfaced.bmp" : "graphics/sub.bmp" );
    }
    else
    {
        strcpy( bmpImageFilename, surfaced ? "graphics/subg_surfaced.bmp" : "graphics/subg.bmp" );
    }
    return result;
}

int SubG::GetAttackOdds( int _defenderType )
{
    // SubG uses same attack odds as Sub (SSBN)
    return g_app->GetWorld()->GetAttackOdds( TypeSub, _defenderType );
}
