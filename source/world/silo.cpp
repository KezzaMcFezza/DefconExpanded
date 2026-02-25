#include "lib/universal_include.h"

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/render3d/renderer_3d.h"
#include "renderer/globe_renderer.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "lib/debug/profiler.h"

#include "renderer/world_renderer.h"
#include "renderer/map_renderer.h"
#include "renderer/globe_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/silo.h"
#include "world/nuke.h"


Silo::Silo()
:   WorldObject(),
    m_numNukesLaunched(0)
{
    SetType( TypeSilo );

    strcpy( bmpImageFilename, "graphics/silo.bmp" );

    m_radarRange = 20;
    m_selectable = true;

    m_currentState = 0;
    m_life = 25;

    m_nukeSupply = 10;

    AddState( LANGUAGEPHRASE("state_standby"), 0, 0, 10, Fixed::MAX, true, 10, 5 );
    AddState( LANGUAGEPHRASE("state_silonuke"), 120, 120, 10, Fixed::MAX, true, 10, 1 );

    m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;

    InitialiseTimers();
}

void Silo::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( !CheckCurrentState() )
    {
        return;
    }

    if( m_stateTimer <= 0 )
    {
        if( m_currentState == 1 )
        {
            g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, longitude, latitude, GetNukeLaunchRange() );
            m_numNukesLaunched++;
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                m_visible[team->m_teamId] = true;
                m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                m_seen[team->m_teamId] = true;
            }
            m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;
        }
        WorldObject::Action( targetObjectId, longitude, latitude );
    }
}

void Silo::SetState( int state )
{
    if( CanSetState( state ) )
    {
        bool preserveQueue = ( m_currentState == 0 && state == 1 );

        WorldObjectState *theState = m_states[state];
        m_currentState = state;
        m_stateTimer = theState->m_timeToPrepare;
        m_targetObjectId = -1;
        if( !preserveQueue )
            m_actionQueue.EmptyAndDelete();

        strcpy( bmpImageFilename, "graphics/silo.bmp" );   // silo always uses silo.bmp (no air defense state)
    }
}
        

bool Silo::Update()
{    
//#ifdef _DEBUG
    //if( g_keys[KEY_U] )
    //{
    //    static bool doneIt = false;
    //    //if( !doneIt )
    //    {
    //        // Pretend sync error
    //        Fixed addMe( 10 );
    //        m_longitude = m_longitude + addMe;
    //        doneIt=true;
    //    }
    //}
//#endif

    if( m_currentState == 1 &&
        m_states[1]->m_numTimesPermitted == 0 )
    {
        m_states[0]->m_numTimesPermitted = 0;
        SetState(0);   // return to standby when nukes run out (no air defense state)
    }

    return WorldObject::Update();
}

void Silo::Render2D()
{
    WorldObject::Render2D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numNukesInStore = m_states[1]->m_numTimesPermitted;
        int numNukesInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallnuke.bmp", 0.0625f, 0.10f);
        if( bmpImage )
        {
            float x = m_longitude.DoubleValue();
            float y = m_latitude.DoubleValue() - GetSize().DoubleValue() * 0.9f;       
            float nukeSize = GetSize().DoubleValue() * 0.35f;
            x -= GetSize().DoubleValue()*0.95f;

            for( int i = 0; i < numNukesInStore; ++i )
            {
                if( i >= (numNukesInStore-numNukesInQueue) )
                {
                    colour.Set( 128,128,128,100 );
                }
                
                g_renderer2d->StaticSprite( bmpImage, x+i*nukeSize*0.5f, y, nukeSize, -nukeSize, colour );
            }
        }
    }
}

void Silo::Render3D()
{
    WorldObject::Render3D();

    if( m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetGame()->m_winner != -1 )
    {   
        int numNukesInStore = m_states[1]->m_numTimesPermitted;
        int numNukesInQueue = m_actionQueue.Size();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();            
        colour.m_a = 150;

        Image *bmpImage = g_resource->GetImage("graphics/smallnuke.bmp", 0.0625f, 0.10f);
        if( bmpImage )
        {
            GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
            if( globeRenderer )
            {
                float baseSize = GetSize3D().DoubleValue();
                float nukeSize = baseSize * GLOBE_UNIT_ICON_SIZE;

                Vector3<float> basePos = globeRenderer->ConvertLongLatTo3DPosition(m_longitude.DoubleValue(), m_latitude.DoubleValue());
                Vector3<float> normal = basePos.Normalized();
                basePos = globeRenderer->GetElevatedPosition(basePos);
                
                Vector3<float> tangent1, tangent2;
                globeRenderer->GetSurfaceTangents(normal, tangent1, tangent2);

                float size3D = GetSize3D().DoubleValue();
                
                float iconSpacing = size3D * GLOBE_UNIT_ICON_SIZE * 0.5f;
                float nukeSizeHalf = nukeSize * 0.5f;
                
                Vector3<float> offset = -tangent1 * size3D * 0.95f - tangent2 * size3D * 0.9f;
                offset += tangent1 * nukeSizeHalf - tangent2 * nukeSizeHalf;

                for( int i = 0; i < numNukesInStore; ++i )
                {
                    if( i >= (numNukesInStore-numNukesInQueue) )
                    {
                        colour.Set( 128,128,128,100 );
                    }
                    
                    Vector3<float> iconPos = basePos + offset + tangent1 * (i * iconSpacing);
                    
                    g_renderer3d->StaticSprite3D( bmpImage, iconPos.x, iconPos.y, iconPos.z, 
                                                  nukeSize, nukeSize, colour, BILLBOARD_SURFACE_ALIGNED );
                }
            }
        }
    }
}

void Silo::RunAI()
{
    START_PROFILE("SiloAI");
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( (team->m_currentState >= Team::StateStrike &&
         team->m_subState >= Team::SubStateLaunchNukes) ||
        team->m_currentState == Team::StatePanic )
    {
        if( m_currentState != 1 &&
            m_states[1]->m_numTimesPermitted > 0 )
        {
            if( team->m_siloSwitchTimer <= 0 )
            {
                SetState(1);
                team->m_siloSwitchTimer = 30;
            }
        }

        if( m_currentState == 1 &&
            m_states[1]->m_numTimesPermitted > 0 &&
            m_stateTimer <= 0 )
        {
            Fixed longitude = 0;
            Fixed latitude  = 0;

            Nuke::FindTarget( m_teamId, team->m_targetTeam, m_objectId, Fixed::MAX, &longitude, &latitude );

            if( longitude != 0 && latitude != 0 )
            {
                ActionOrder *action = new ActionOrder();
                action->m_longitude = longitude;
                action->m_latitude = latitude;
                RequestAction( action );
            }
        }
    }
    END_PROFILE("SiloAI");
}

int Silo::GetTargetObjectId()
{
    return m_targetObjectId;
}

bool Silo::IsActionQueueable()
{
    return ( m_currentState == 0 || m_currentState == 1 );
}

bool Silo::ShouldProcessActionQueue()
{
    return ( m_currentState != 0 );
}

bool Silo::UsingGuns()
{
    return false;   // silo has no air defense; SAM handles that
}

bool Silo::UsingNukes()
{
    return ( m_currentState == 1 );
}

void Silo::AcquireTargetFromAction( ActionOrder *action )
{
    (void)action;   // silo has no air defense; SAM handles that
}

void Silo::NukeStrike()
{
    Fixed lossFraction = Fixed(1) / 2;
    m_states[1]->m_numTimesPermitted = (m_states[1]->m_numTimesPermitted * lossFraction).IntValue();
    m_states[0]->m_numTimesPermitted = m_states[1]->m_numTimesPermitted;
    m_nukeSupply = m_states[1]->m_numTimesPermitted;
}

void Silo::AirDefense()
{
    (void)0;   // silo has no air defense; SAM handles that
}


int Silo::GetAttackOdds( int _defenderType )
{
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        return g_app->GetWorld()->GetAttackOdds( TypeNuke, _defenderType );
    }
    return 0;
}


int Silo::IsValidCombatTarget( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    int basicResult = WorldObject::IsValidCombatTarget( _objectId );
    if( basicResult < TargetTypeInvalid ) return basicResult;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );
    int attackOdds = g_app->GetWorld()->GetAttackOdds( TypeNuke, obj->m_type );
    if( ( m_currentState == 0 || m_currentState == 1 ) && 
        m_states[1]->m_numTimesPermitted > 0 &&
        ( attackOdds > 0 || ( isFriend && !obj->IsMovingObject() ) ) )
    {
        return TargetTypeLaunchNuke;
    }

    return TargetTypeInvalid;
}

int Silo::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
        return TargetTypeInvalid;
    if( ( m_currentState == 0 || m_currentState == 1 ) &&
        m_states[1]->m_numTimesPermitted > 0 )
    {
        return TargetTypeLaunchNuke;
    }
    return TargetTypeInvalid;
}


Image *Silo::GetBmpImage( int state )
{
    (void)state;
    return g_resource->GetImage( "graphics/silo.bmp" );
}


void Silo::CeaseFire( int teamId )
{
    for( int i = 0; i < m_actionQueue.Size(); ++i )
    {
        ActionOrder *action = m_actionQueue[i];
        if(g_app->GetWorldRenderer()->IsValidTerritory( teamId, action->m_longitude, action->m_latitude, false ) )
        {
            m_actionQueue.RemoveData(i);
            delete action;
            --i;
        }
    }
    WorldObject::CeaseFire( teamId );
}
