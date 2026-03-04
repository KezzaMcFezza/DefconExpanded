#include "lib/universal_include.h"


#include "lib/preferences.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/string_utils.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/worldobject.h"
#include "world/team.h"

#include "interface/interface.h"

#include "renderer/map_renderer.h"
#include "renderer/world_renderer.h"
#include "renderer/globe_renderer.h"
#include "lib/render3d/renderer_3d.h"

#include "world/radarstation.h"
#include "world/radar_ew.h"
#include "world/silo.h"
#include "world/sam.h"
#include "world/sub.h"
#include "world/subc.h"
#include "world/subg.h"
#include "world/subk.h"
#include "world/battleship.h"
#include "world/battleship2.h"
#include "world/battleship3.h"
#include "world/airbase.h"
#include "world/airbase2.h"
#include "world/airbase3.h"
#include "world/carrier.h"
#include "world/carrier_light.h"
#include "world/carrier_super.h"
#include "world/carrier_lhd.h"
#include "world/bomber.h"
#include "world/bomber_fast.h"
#include "world/bomber_stealth.h"
#include "world/fighter.h"
#include "world/fighter_light.h"
#include "world/fighter_stealth.h"
#include "world/fighter_navystealth.h"
#include "world/aew.h"
#include "world/tanker.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/lacm.h"
#include "world/cbm.h"
#include "world/lanm.h"
#include "world/abm.h"
#include "world/silomed.h"
#include "world/silomobile.h"
#include "world/silomobilecon.h"
#include "world/ascm.h"
#include "world/tornado.h"
#include "world/fleet.h"
#include "world/unit_graphics.h"

static bool LaunchBomberFrom( WorldObject *wo, Bomber *bomber, int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode );

const int    WorldObject::BURST_FIRE_SHOTS = 3;
const Fixed  WorldObject::BURST_FIRE_COOLDOWN_SECONDS = Fixed(30);


WorldObject::WorldObject()
:   m_teamId(-1),
    m_objectId(-1),
    m_longitude(0),
    m_latitude(0),
    m_type(TypeInvalid),
    m_archetype(ArchetypeInvalid),
    m_classType(ClassTypeInvalid),
    m_radarRange(0),
    m_life(1),
    m_stealthType(100),
	m_lastHitByTeamId( -1 ),
    m_selectable(false),
    m_currentState(0),
    m_previousState(0),
    m_stateTimer(0),
    m_ghostFadeTime(50),
    m_targetObjectId(-1),
    m_isRetaliating(false),
    m_fleetId(-1),
    m_nukeSupply(-1),
    m_previousRadarRange(0),
    m_previousRadarEarly1Range(0),
    m_previousRadarEarly2Range(0),
    m_previousRadarStealth1Range(0),
    m_previousRadarStealth2Range(0),
    m_offensive(false),
    m_aiTimer(0),
    m_aiSpeed(5),
    m_numNukesInFlight(0),
    m_numNukesInQueue(0),
    m_numLACMInFlight(0),
    m_numLACMInQueue(0),
    m_nukeCountTimer(0),
    m_forceAction(false),
    m_maxFighters(0),
    m_maxBombers(0),
    m_maxAEW(0),
    m_retargetTimer(0),
    m_burstFireShotCount(0)
{
    m_visible.Initialise(MAX_TEAMS);
    m_seen.Initialise(MAX_TEAMS);
    m_lastKnownPosition.Initialise(MAX_TEAMS);
    m_lastKnownVelocity.Initialise(MAX_TEAMS);
    m_lastSeenTime.Initialise(MAX_TEAMS);
    m_lastSeenState.Initialise(MAX_TEAMS);

    if( g_app->m_gameRunning )
    {
        m_seen.SetAll( false );
        m_visible.SetAll( false );
        m_lastKnownPosition.SetAll( Vector3<Fixed>::ZeroVector() );
        m_lastKnownVelocity.SetAll( Vector3<Fixed>::ZeroVector() );
        m_lastSeenTime.SetAll( 0 );
        m_lastSeenState.SetAll( 0 );
    }
}

WorldObject::~WorldObject()
{
    BurstFireClear();
    if( g_app->m_gameRunning )
    {
        Team *team = g_app->GetWorld()->GetTeam( m_teamId );
        if( team )
        {
            team->m_unitsInPlay[m_type]--;
        }
    }
    m_actionQueue.EmptyAndDelete();
    m_states.EmptyAndDelete();
}

void WorldObject::InitialiseTimers()
{    
    if( m_type != TypeInvalid )
    {
        m_aiTimer = syncfrand(m_aiSpeed); // start the timer at a random number so that ai updates dont all occur simulatenously
        
        m_nukeCountTimer = frand(5.0f);
        m_nukeCountTimer += (double)GetHighResTime();

        m_radarRange /= World::GetUnitScaleFactor();       
    }
}

void WorldObject::SetType( int type )
{
    m_type = type;
    m_archetype = GetArchetypeForType( type );
    m_classType = GetClassTypeForType( type );
}

void WorldObject::SetTeamId( int teamId )
{
    m_teamId = teamId;
}

void WorldObject::SetPosition( Fixed longitude, Fixed latitude )
{
    m_longitude = longitude;
    m_latitude = latitude;
}

bool WorldObject::IsHiddenFrom()
{
    return false;
}

void WorldObject::AddState( const char *stateName, Fixed prepareTime, Fixed reloadTime, Fixed radarRange,
                            Fixed actionRange, bool isActionable, int numTimesPermitted, int defconPermitted )
{
    WorldObjectState *state = new WorldObjectState();
    state->m_stateName = newStr( stateName);
    state->m_timeToPrepare = prepareTime;
    state->m_timeToReload = reloadTime;
    state->m_radarRange = radarRange;
    state->m_radarearly1Range = radarRange * Fixed::FromDouble(1.5);
    state->m_radarearly2Range = radarRange * Fixed::FromDouble(2.0);
    state->m_radarstealth1Range = radarRange * Fixed::FromDouble(0.33);
    state->m_radarstealth2Range = radarRange * Fixed::FromDouble(0.66);
    state->m_actionRange = actionRange;
    state->m_isActionable = isActionable;
    state->m_numTimesPermitted = numTimesPermitted;
    state->m_defconPermitted = defconPermitted;

    Fixed gameScale = World::GetUnitScaleFactor();
    state->m_radarRange /= gameScale;
    state->m_radarearly1Range /= gameScale;
    state->m_radarearly2Range /= gameScale;
    state->m_radarstealth1Range /= gameScale;
    state->m_radarstealth2Range /= gameScale;
    state->m_actionRange /= gameScale;

    m_states.PutData( state );
}

bool WorldObject::IsActionable()
{
    return( m_states[m_currentState]->m_isActionable );
}

Fixed WorldObject::GetActionRange ()
{
    Fixed result = m_states[m_currentState]->m_actionRange;
    return result;
}

Fixed WorldObject::GetActionRangeSqd()
{
    Fixed result = GetActionRange();
    return( result * result );
}

Fixed WorldObject::GetRadarRange ()
{
    Fixed result = m_states[m_currentState]->m_radarRange;
    return result;
}

Fixed WorldObject::GetRadarEarly1Range ()
{
    Fixed result = m_states[m_currentState]->m_radarearly1Range;
    return result;
}

Fixed WorldObject::GetRadarEarly2Range ()
{
    Fixed result = m_states[m_currentState]->m_radarearly2Range;
    return result;
}

Fixed WorldObject::GetRadarStealth1Range ()
{
    Fixed result = m_states[m_currentState]->m_radarstealth1Range;
    return result;
}

Fixed WorldObject::GetRadarStealth2Range ()
{
    Fixed result = m_states[m_currentState]->m_radarstealth2Range;
    return result;
}


void WorldObject::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    m_stateTimer = m_states[m_currentState]->m_timeToReload;

    WorldObjectState *currentState = m_states[m_currentState];
    if( currentState->m_numTimesPermitted != -1 &&
        currentState->m_numTimesPermitted > 0 )
    {
        --currentState->m_numTimesPermitted;
    }

    if( UsingNukes() &&
        m_nukeSupply > 0 )
    {
        m_nukeSupply--;
    }

}


bool WorldObject::CanSetState( int state )
{
    if( !m_states.ValidIndex(state) )
    {
        return false;
    }

    if( m_currentState == state )
    {
        return false;
    }

    if( m_states[state]->m_numTimesPermitted == 0 )
    {
        return false;
    }

    if( m_states[state]->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        return false;
    }

    return true;
}


void WorldObject::SetState( int state )
{
    if( CanSetState(state) )
    {
        WorldObjectState *theState = m_states[state];
        m_currentState = state;
        m_stateTimer = theState->m_timeToPrepare;
        m_actionQueue.EmptyAndDelete();
        m_targetObjectId = -1;
    }
}


bool WorldObject::Update()
{
    BurstFireTick();
    if( m_stateTimer > 0 )
    {
        m_stateTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        if( m_stateTimer <= 0 )
        {
            m_stateTimer = 0;
            
            //if( m_teamId == g_app->GetWorld()->m_myTeamId &&
            //    m_type != TypeNuke )
            {
                //g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId,
                //    m_states[m_currentState]->GetReadyName(), WorldMessage::TypeObjectState,false );                
            }
        }
    }

    if( m_actionQueue.Size() > 0 && ShouldProcessActionQueue() )
    {
        ActionOrder *headAction = m_actionQueue[0];
        if( headAction->m_targetObjectId != -1 )
            AcquireTargetFromAction( headAction );
    }

    if( m_stateTimer <= 0 )
    {
        for( int i = m_actionQueue.Size() - 1; i >= 0; --i )
        {
            ActionOrder *action = m_actionQueue[i];
            if( action->m_targetObjectId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                if( !obj || obj->m_life <= 0 )
                {
                    m_actionQueue.RemoveData( i );
                    delete action;
                }
            }
        }
        if( m_actionQueue.Size() > 0 && ShouldProcessActionQueue() && ShouldProcessNextQueuedAction() )
        {
            ActionOrder *action = m_actionQueue[0];
            m_actionQueue.RemoveData(0);

            Action( action->m_targetObjectId, action->m_longitude, action->m_latitude );

            if( action->m_targetObjectId == -1 )
            {
                //Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
                //if( fleet )
                {
                    //fleet->StopFleet();
                }
            }
            else if( action->m_pursueTarget )
            {
                // Ships (battleship, carrier, sub) do not pursue targets - they can attack/launch from position.
                // Subs must remain stationary to stay surfaced; other ships can move but shouldn't auto-chase.
                if( !IsBattleShipClass() && !IsCarrierClass() && !IsSubmarine() )
                {
                    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
                    if( fleet )
                    {
                        MovingObject *obj = (MovingObject *)g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
                        if( obj &&
                            obj->IsMovingObject() &&
                            obj->m_movementType == MovingObject::MovementTypeSea )
                        {
                            fleet->m_pursueTarget = true;
                            fleet->m_targetFleet = obj->m_fleetId;
                            fleet->m_targetTeam = obj->m_teamId;
                        }
                    }
                }
            }
            delete action;
        }
    }

    if( m_currentState != m_previousState )
    {
        m_previousState = m_currentState;
        if( m_teamId == g_app->GetWorld()->m_myTeamId &&
            m_stateTimer > 0 )
        {
            //g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId,
            //    m_states[m_currentState]->GetPreparingName(), WorldMessage::TypeObjectState, false );                                   
        }
    }
        
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i ) 
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if(m_visible[team->m_teamId])
        {
            m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( m_longitude, m_latitude, 0 );
            m_lastKnownVelocity[team->m_teamId] = m_vel;
            m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
            m_lastSeenState[team->m_teamId] = m_currentState;
        }
    }

    if( !IsMovingObject() || IsTargetableSurfaceNavy() )
    {
        float realTimeNow = GetHighResTime();
        if( realTimeNow > m_nukeCountTimer )
        {
            g_app->GetWorld()->GetNumNukers( m_objectId, &m_numNukesInFlight, &m_numNukesInQueue );
            g_app->GetWorld()->GetNumLACMs( m_objectId, &m_numLACMInFlight, &m_numLACMInQueue );
            m_nukeCountTimer = realTimeNow + 2.0f;
        }
    }

    m_aiTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    if( m_retargetTimer > 0 )
    {
        m_retargetTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
        m_retargetTimer = max( m_retargetTimer, 0 );
    }

    if( m_life <= 0 )
    {
        if( m_fleetId != -1 )
        {
            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet )
            {
                fleet->m_lastHitByTeamId.PutData( m_lastHitByTeamId );
            }
        }
    }
                
    return( m_life <= 0 );
}


const char *WorldObject::GetResolvedBmpImageFilename()
{
    if ( m_teamId < 0 ) return bmpImageFilename;
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if ( !team || team->m_territories.Size() == 0 ) return bmpImageFilename;
    int primaryTerritory = team->m_territories[0];
    const char *resolved = GetUnitGraphicForTerritory( primaryTerritory, m_type, bmpImageFilename );
    if ( resolved != bmpImageFilename && !g_resource->HasImage( resolved ) )
        return bmpImageFilename;
    return resolved;
}

char *WorldObject::GetBmpBlurFilename()
{
    static char blurFilename[256];
    strcpy( blurFilename, GetResolvedBmpImageFilename() );
    char *dot = strrchr( blurFilename, '.' );
    if( dot ) sprintf( dot, "_blur.bmp" );
    return blurFilename;
}

static Image *SafeGetOutlineImage( const char *path )
{
    if( !g_resource->HasImage( path ) )
        return NULL;
    return g_resource->GetImage( path );
}

Image *WorldObject::GetOutlineImage()
{
    if( IsBlip() || g_preferences->GetInt( PREFS_GRAPHICS_OUTLINES ) != 1 )
        return NULL;
    if( IsBallisticMissileClass() )
        return NULL;
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    bool isSpectator = ( g_app->GetWorld()->m_myTeamId == -1 );
    if( !team || ( !myTeam && !isSpectator ) )
        return NULL;

    const char *bmpImageFilename = GetResolvedBmpImageFilename();
    const char *base = ( strncmp( bmpImageFilename, "graphics/", 9 ) == 0 ) ? ( bmpImageFilename + 9 ) : bmpImageFilename;
    char outlinePath[256];
    Image *outlineimage = NULL;
    bool surfaced = ( m_currentState == 2 );

    const char *outlineBase = base;
    if( IsFighterClass() )       outlineBase = "fighter.bmp";
    else if( IsBomberClass() )   outlineBase = "bomber.bmp";
    else if( IsAEWClass() && m_type != TypeTanker ) outlineBase = "aew.bmp";
    else if( m_type == TypeTanker ) outlineBase = "tanker.bmp";
    else if( m_type == TypeLACM || m_type == TypeLANM ) outlineBase = "lacm.bmp";
    bool useOwnOutline = ( m_type == TypeASCM );

    if( isSpectator )
    {
        int usaOwner = g_app->GetWorld()->GetTerritoryOwner( World::TerritoryUSA );
        Team *usaTeam = ( usaOwner != -1 ) ? g_app->GetWorld()->GetTeam( usaOwner ) : NULL;
        bool isBlufor = ( usaTeam && usaTeam->m_allianceId == team->m_allianceId );
        bool isNeutral = !isBlufor && ( !usaTeam || usaTeam->m_ceaseFire[team->m_teamId] );

        if( isBlufor )
        {
            if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_battleship.bmp" );
            else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_carrier.bmp" );
            else if( IsSubmarine() )
            {
                if( m_type == TypeSub ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subn_surfaced.bmp" : "graphics/outline_blufor_subn.bmp" );
                else if( m_type == TypeSubC ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subc_surfaced.bmp" : "graphics/outline_blufor_subc.bmp" );
                else if( m_type == TypeSubG ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subg_surfaced.bmp" : "graphics/outline_blufor_subg.bmp" );
                else if( m_type == TypeSubK ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subk_surfaced.bmp" : "graphics/outline_blufor_subk.bmp" );
                else outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_sub_surfaced.bmp" : "graphics/outline_blufor_sub.bmp" );
            }
            else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_airbase.bmp" );
            else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_silo.bmp" );
            else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_radarstation.bmp" );
            else { const char *ob = (m_type == TypeLANM) ? "lanm.bmp" : outlineBase; sprintf( outlinePath, "graphics/outline_blufor_%s", ob ); outlineimage = SafeGetOutlineImage( outlinePath ); }
        }
        else if( isNeutral )
        {
            if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_battleship.bmp" );
            else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_carrier.bmp" );
            else if( IsSubmarine() ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_sub_surfaced.bmp" : "graphics/outline_sub.bmp" );
            else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_airbase.bmp" );
            else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_silo.bmp" );
            else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_radarstation.bmp" );
            else { sprintf( outlinePath, "graphics/outline_%s", outlineBase ); outlineimage = SafeGetOutlineImage( outlinePath ); }
        }
        else
        {
            if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_battleship.bmp" );
            else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_carrier.bmp" );
            else if( IsSubmarine() ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_opfor_sub_surfaced.bmp" : "graphics/outline_opfor_sub.bmp" );
            else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_airbase.bmp" );
            else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_silo.bmp" );
            else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_radarstation.bmp" );
            else { sprintf( outlinePath, "graphics/outline_opfor_%s", outlineBase ); outlineimage = SafeGetOutlineImage( outlinePath ); }
        }
    }
    else
    {
        bool selfOrCeasefire = ( myTeam == team ) || ( myTeam->m_ceaseFire[team->m_teamId] );
        if( selfOrCeasefire )
        {
            if( myTeam->m_allianceId == team->m_allianceId )
            {
                if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_battleship.bmp" );
                else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_carrier.bmp" );
                else if( IsSubmarine() )
                {
                    if( m_type == TypeSub ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subn_surfaced.bmp" : "graphics/outline_blufor_subn.bmp" );
                    else if( m_type == TypeSubC ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subc_surfaced.bmp" : "graphics/outline_blufor_subc.bmp" );
                    else if( m_type == TypeSubG ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subg_surfaced.bmp" : "graphics/outline_blufor_subg.bmp" );
                    else if( m_type == TypeSubK ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_subk_surfaced.bmp" : "graphics/outline_blufor_subk.bmp" );
                    else outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_blufor_sub_surfaced.bmp" : "graphics/outline_blufor_sub.bmp" );
                }
                else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_airbase.bmp" );
                else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_silo.bmp" );
                else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_blufor_radarstation.bmp" );
                else { const char *ob = (m_type == TypeLANM) ? "lanm.bmp" : outlineBase; sprintf( outlinePath, "graphics/outline_blufor_%s", ob ); outlineimage = SafeGetOutlineImage( outlinePath ); }
            }
            else
            {
                if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_battleship.bmp" );
                else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_carrier.bmp" );
                else if( IsSubmarine() ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_sub_surfaced.bmp" : "graphics/outline_sub.bmp" );
                else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_airbase.bmp" );
                else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_silo.bmp" );
                else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_radarstation.bmp" );
                else { sprintf( outlinePath, "graphics/outline_%s", outlineBase ); outlineimage = SafeGetOutlineImage( outlinePath ); }
            }
        }
        else
        {
            if( IsBattleShipClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_battleship.bmp" );
            else if( IsCarrierClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_carrier.bmp" );
            else if( IsSubmarine() ) outlineimage = SafeGetOutlineImage( surfaced ? "graphics/outline_opfor_sub_surfaced.bmp" : "graphics/outline_opfor_sub.bmp" );
            else if( IsAirbaseClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_airbase.bmp" );
            else if( IsSiloClass() && !useOwnOutline ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_silo.bmp" );
            else if( IsRadarClass() ) outlineimage = SafeGetOutlineImage( "graphics/outline_opfor_radarstation.bmp" );
            else { sprintf( outlinePath, "graphics/outline_opfor_%s", outlineBase ); outlineimage = SafeGetOutlineImage( outlinePath ); }
        }
    }
    return outlineimage;
}


void WorldObject::Render2D()
{
    // Clamp to avoid extrapolating backward (negative causes sprite to lag behind)
    float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
    Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue();

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();            
    colour.m_a = 255;

    Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );

    float size = GetSize().DoubleValue();

    float x = predictedLongitude-size;
    float y = predictedLatitude+size;
    float thisSize = size*2;

    // All ships/buildings face same direction; no territory-based flip

    Image *outlineImage = GetOutlineImage();
    if( outlineImage )
    {
        Colour white( 255, 255, 255, 255 );
        float outX = predictedLongitude - size;
        float outY = predictedLatitude + size;
        g_renderer2d->StaticSprite( outlineImage, outX, outY, size*2, -size*2, white );
    }

    if( bmpImage )
    {
        g_renderer2d->StaticSprite( bmpImage, x, y, thisSize, size*-2, colour );        
    }

        colour.Set(255,255,255,255);
        int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
        for( int i = 0; i < 2; ++i )
        {
            if( i == 1 && highlightId != -1 && g_app->GetWorldRenderer()->IsSelected( highlightId ) )
                break;
            bool selected = false;
            bool sameFleet = false;
            if( i == 0 )
            {
                // Only highlight explicitly selected units - not whole fleet on partial multi-selection
                selected = g_app->GetWorldRenderer()->IsSelected( m_objectId );
            }
            else
            {
                WorldObject *highlight = g_app->GetWorld()->GetWorldObject( highlightId );
                if( highlight )
                {
                    selected = ( highlight == this );
                    sameFleet = highlight->m_teamId == m_teamId &&
                                highlight->m_fleetId != -1 &&
                                highlight->m_fleetId == m_fleetId;
                }
            }

        if( ( selected || sameFleet ) && !IsBlip() )
        {
            char *blurFile = GetBmpBlurFilename();
            if( g_resource->HasImage( blurFile ) )
            {
                bmpImage = g_resource->GetImage( blurFile );
                g_renderer2d->StaticSprite( bmpImage, x, y, thisSize, size*-2, colour );
            }
        }
        colour.m_a /= 2;
    }
}

void WorldObject::Render3D()
{
    // Clamp to avoid extrapolating backward (negative causes sprite to lag behind)
    float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
    Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * predictionTime).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * predictionTime).DoubleValue();

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();            
    colour.m_a = 255;

    Image *bmpImage = g_resource->GetImage( GetResolvedBmpImageFilename() );

    GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
    if( globeRenderer && bmpImage )
    {
        float size = GetSize3D().DoubleValue();

        Vector3<float> objPos = globeRenderer->ConvertLongLatTo3DPosition(predictedLongitude, predictedLatitude);
        Vector3<float> normal = objPos;
        normal.Normalise();
        float elevation = GLOBE_ELEVATION;
        Vector3<float> renderPos = objPos + normal * elevation;

        float spriteSize = size * 2.0f;
        // All ships/buildings face same direction; no territory-based flip

        Image *outlineImage3d = GetOutlineImage();
        if( outlineImage3d )
        {
            Colour white( 255, 255, 255, 255 );
            float outlineSize = size * 2.0f;
            g_renderer3d->StaticSprite3D( outlineImage3d, renderPos.x, renderPos.y, renderPos.z,
                                          outlineSize, outlineSize, white, BILLBOARD_SURFACE_ALIGNED );
        }

        g_renderer3d->StaticSprite3D( bmpImage, renderPos.x, renderPos.y, renderPos.z, 
                                      spriteSize, spriteSize, colour, BILLBOARD_SURFACE_ALIGNED );

        colour.Set(255,255,255,255);
        int highlightId = g_app->GetWorldRenderer()->GetCurrentHighlightId();
        for( int i = 0; i < 2; ++i )
        {
            if( i == 1 && highlightId != -1 && g_app->GetWorldRenderer()->IsSelected( highlightId ) )
                break;
            bool selected = false;
            bool sameFleet = false;
            if( i == 0 )
            {
                // Only highlight explicitly selected units - not whole fleet on partial multi-selection
                selected = g_app->GetWorldRenderer()->IsSelected( m_objectId );
            }
            else
            {
                WorldObject *highlight = g_app->GetWorld()->GetWorldObject( highlightId );
                if( highlight )
                {
                    selected = ( highlight == this );
                    sameFleet = highlight->m_teamId == m_teamId &&
                                highlight->m_fleetId != -1 &&
                                highlight->m_fleetId == m_fleetId;
                }
            }

            if( ( selected || sameFleet ) && !IsBlip() )
            {
                char *blurFile = GetBmpBlurFilename();
                if( g_resource->HasImage( blurFile ) )
                {
                    bmpImage = g_resource->GetImage( blurFile );
                    g_renderer3d->StaticSprite3D( bmpImage, renderPos.x, renderPos.y, renderPos.z,
                                                  spriteSize, spriteSize, colour, BILLBOARD_SURFACE_ALIGNED );
                }
            }
            colour.m_a /= 2;
        }
    }
}

void WorldObject::RenderCounter( int counter )
{

    char count[8];
    sprintf( count, "%u", counter );

    float size = GetSize().DoubleValue();
    float clampedPrediction = (g_predictionTime > 0.0f) ? g_predictionTime : 0.0f;
    Fixed predictionTime = Fixed::FromDouble(clampedPrediction) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = ( m_longitude + m_vel.x * predictionTime ).DoubleValue();
    float predictedLatitude = ( m_latitude + m_vel.y * predictionTime ).DoubleValue();

    g_renderer->SetFont( "kremlin", true );

    g_renderer2d->Text( predictedLongitude + ( size / 5 ), predictedLatitude + ( size / 5 ), 
                        White, size, count);


    if( m_nukeSupply > -1 &&
        CanLaunchBomber() )
    {
        char num[8];
        Image *bmp = g_resource->GetImage( "graphics/nukesymbol.bmp");
        sprintf( num, "%d", m_nukeSupply );
        float textWidth = g_renderer2d->TextWidth( num, size );
        float xModifier = textWidth/2;
        if( m_nukeSupply < 10 ) xModifier = 0;
        g_renderer2d->Text( predictedLongitude + ( size / 5 ) - xModifier, predictedLatitude + ( size ), 
                White, size, num );

        g_renderer2d->StaticSprite( bmp, predictedLongitude - textWidth, predictedLatitude + ( size * 1.75f), size * 0.75f, size * -0.75f, White );
    }

   // g_app->GetRenderer()->Text( m_longitude + ( size / 5 ) +  + g_app->GetRenderer()->GetMapRenderer()->GetLongitudeMod(), m_latitude + ( size / 5 ), 
      //  count, Colour(255, 255, 255, 255), size, "fonts/bitlow.ttf", true);

    g_renderer->SetFont();
}

Fixed WorldObject::GetSize()
{
	Fixed size = 2;

    if( g_app->GetMapRenderer() )
    {
        float zf = g_app->GetMapRenderer()->GetZoomFactor();
        double zfClamped = fmax( (double)zf, 0.001 );
        size *= Fixed::FromDouble( 2.0 * pow( zfClamped, 0.75 ) );
    }

    Fixed gameScale = g_app->GetWorld()->GetGameScale();
    if( gameScale > Fixed::Hundredths(1) )
        size /= Fixed::FromDouble(sqrt(gameScale.DoubleValue()));

    return size;
}

Fixed WorldObject::GetSize3D()
{
    Fixed size = 2;

    if( m_archetype == ArchetypeAircraft )
    {
        size *= 1;
    }

    size *= Fixed::FromDouble(GLOBE_OBJECT_SIZE);

    Fixed gameScale = g_app->GetWorld()->GetUnitScaleFactor();
    size /= gameScale;
    if( gameScale >= Fixed(4) )
    {
        float scaleComp = 1.0f + (gameScale.DoubleValue() - 4.0f) * (7.0f / 12.0f);
        size *= Fixed::FromDouble(scaleComp);
    }

    return size;
}

bool WorldObject::CheckCurrentState()
{
    WorldObjectState *currentState = m_states[m_currentState];
    if( currentState->m_numTimesPermitted == 0 )
    {
        return false;
    }

    if( currentState->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
    {
        if(m_teamId == g_app->GetWorld()->m_myTeamId)
        {
            char msg[128];
            strcpy( msg, LANGUAGEPHRASE("message_invalid_action") );
            LPREPLACEINTEGERFLAG( 'D', currentState->m_defconPermitted, msg );
            g_app->GetWorld()->AddWorldMessage( m_objectId, m_teamId, msg, WorldMessage::TypeInvalidAction, false );
        }
        return false;
    }

    /*if( m_stateTimer > 0.0f )
    {
        return false;
    }*/
    return true;
}

const char *WorldObject::GetName (int _type)
{
    switch( _type )
    {
        case TypeSilo:          return LANGUAGEPHRASE("unit_silo");
        case TypeSAM:           return LANGUAGEPHRASE("unit_sam");
        case TypeNuke:          return LANGUAGEPHRASE("unit_nuke");
        case TypeCity:          return LANGUAGEPHRASE("unit_city");
        case TypeExplosion:     return LANGUAGEPHRASE("unit_explosion");
        case TypeSub:           return LANGUAGEPHRASE("unit_sub");
        case TypeSubG:          return LANGUAGEPHRASE("unit_subg");
        case TypeSubC:          return LANGUAGEPHRASE("unit_subc");
        case TypeSubK:          return LANGUAGEPHRASE("unit_subk");
        case TypeRadarStation:  return LANGUAGEPHRASE("unit_radar");
        case TypeRadarEW:       return LANGUAGEPHRASE("unit_radar_ew");
        case TypeBattleShip:    return LANGUAGEPHRASE("unit_battleship");
        case TypeBattleShip2:   return LANGUAGEPHRASE("unit_battleship_ddg");
        case TypeBattleShip3:   return LANGUAGEPHRASE("unit_battleship_fg");
        case TypeAirBase:       return LANGUAGEPHRASE("unit_airbase");
        case TypeAirBase2:      return LANGUAGEPHRASE("unit_airbase2");
        case TypeAirBase3:      return LANGUAGEPHRASE("unit_airbase3");
        case TypeFighter:       return LANGUAGEPHRASE("unit_fighter");
        case TypeFighterLight:  return LANGUAGEPHRASE("unit_fighter_light");
        case TypeFighterStealth: return LANGUAGEPHRASE("unit_fighter_stealth");
        case TypeFighterNavyStealth: return LANGUAGEPHRASE("unit_fighter_navy_stealth");
        case TypeBomber:        return LANGUAGEPHRASE("unit_bomber");
        case TypeBomberFast:    return LANGUAGEPHRASE("unit_bomber_fast");
        case TypeBomberStealth: return LANGUAGEPHRASE("unit_bomber_stealth");
        case TypeAEW:           return LANGUAGEPHRASE("unit_aew");
        case TypeTanker:        return LANGUAGEPHRASE("unit_tanker");
        case TypeCarrier:       return LANGUAGEPHRASE("unit_carrier");
        case TypeCarrierLight:  return LANGUAGEPHRASE("unit_carrier_light");
        case TypeCarrierSuper:  return LANGUAGEPHRASE("unit_carrier_super");
        case TypeCarrierLHD:    return LANGUAGEPHRASE("unit_carrier_lhd");
		case TypeTornado:       return LANGUAGEPHRASE("unit_tornado");
        case TypeSaucer:        return LANGUAGEPHRASE("unit_saucer");
        case TypeLACM:         return LANGUAGEPHRASE("unit_lacm");
        case TypeCBM:          return LANGUAGEPHRASE("unit_cbm");
        case TypeLANM:         return LANGUAGEPHRASE("unit_lanm");
        case TypeABM:          return LANGUAGEPHRASE("unit_abm");
        case TypeSiloMed:      return LANGUAGEPHRASE("unit_silomed");
        case TypeSiloMobile:   return LANGUAGEPHRASE("unit_silomobile");
        case TypeSiloMobileCon: return LANGUAGEPHRASE("unit_silomobilecon");
        case TypeASCM:         return LANGUAGEPHRASE("unit_ascm");
    }

    return "?";
}


namespace
{
    struct TerritoryNameOverride
    {
        int territory;
        int unitType;
        const char *langKey;
    };

    static const TerritoryNameOverride s_nameOverrides[] =
    {
        // USA
        { World::TerritoryUSA, WorldObject::TypeFighter,            "unit_fighter_f18" },
        { World::TerritoryUSA, WorldObject::TypeFighterStealth,     "unit_fighter_stealth_f22" },
        { World::TerritoryUSA, WorldObject::TypeBomber,             "unit_bomber_b52" },
        { World::TerritoryUSA, WorldObject::TypeBomberFast,         "unit_bomber_fast_b1b" },
        { World::TerritoryUSA, WorldObject::TypeBomberStealth,      "unit_bomber_stealth_b2" },
        { World::TerritoryUSA, WorldObject::TypeCarrierSuper,       "unit_carrier_super_nimitz" },
        { World::TerritoryUSA, WorldObject::TypeCarrierLight,       "unit_carrier_light_wasp" },
        { World::TerritoryUSA, WorldObject::TypeBattleShip,         "unit_battleship_cg_tico" },
        { World::TerritoryUSA, WorldObject::TypeBattleShip2,        "unit_battleship_ddg_arleigh" },
        { World::TerritoryUSA, WorldObject::TypeBattleShip3,        "unit_battleship_ffg_lcs" },
        { World::TerritoryUSA, WorldObject::TypeSub,                "unit_sub_ohio" },
        { World::TerritoryUSA, WorldObject::TypeSubC,               "unit_subc_virginia" },
        { World::TerritoryUSA, WorldObject::TypeSubG,               "unit_subg_ohio" },
        { World::TerritoryUSA, WorldObject::TypeAirBase,            "unit_airbase_usfighter" },
        { World::TerritoryUSA, WorldObject::TypeAirBase2,           "unit_airbase2_usbomber" },
        { World::TerritoryUSA, WorldObject::TypeAirBase3,           "unit_airbase3_usnavy" },

        // NATO
        { World::TerritoryNATO, WorldObject::TypeFighter,           "unit_fighter_rafale" },
        { World::TerritoryNATO, WorldObject::TypeCarrier,           "unit_carrier_degaulle" },
        { World::TerritoryNATO, WorldObject::TypeCarrierLight,      "unit_carrier_light_qe" },
        { World::TerritoryNATO, WorldObject::TypeBattleShip2,       "unit_battleship_ddg_horizon" },
        { World::TerritoryNATO, WorldObject::TypeBattleShip3,       "unit_battleship_ffg_fremm" },
        { World::TerritoryNATO, WorldObject::TypeAirBase,           "unit_airbase_nato" },
        { World::TerritoryNATO, WorldObject::TypeAirBase2,          "unit_airbase2_otan" },

        // Russia
        { World::TerritoryRussia, WorldObject::TypeFighterLight,    "unit_fighter_light_fulcrum" },
        { World::TerritoryRussia, WorldObject::TypeFighter,         "unit_fighter_flanker" },
        { World::TerritoryRussia, WorldObject::TypeFighterStealth,  "unit_fighter_stealth_su57" },
        { World::TerritoryRussia, WorldObject::TypeBomber,          "unit_bomber_tu95" },
        { World::TerritoryRussia, WorldObject::TypeBomberFast,      "unit_bomber_fast_tu22" },
        { World::TerritoryRussia, WorldObject::TypeCarrier,         "unit_carrier_kuznetsov" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip,      "unit_battleship_cg_slava" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip2,     "unit_battleship_ddg_udaloy" },
        { World::TerritoryRussia, WorldObject::TypeBattleShip3,     "unit_battleship_ffg_grigorovich" },
        { World::TerritoryRussia, WorldObject::TypeSub,             "unit_sub_dolgorukiy" },
        { World::TerritoryRussia, WorldObject::TypeSubC,            "unit_subc_akula" },
        { World::TerritoryRussia, WorldObject::TypeSubG,            "unit_subg_sev" },
        { World::TerritoryRussia, WorldObject::TypeSubK,            "unit_subk_kilo" },
        { World::TerritoryRussia, WorldObject::TypeAirBase,         "unit_airbase_rufighter" },
        { World::TerritoryRussia, WorldObject::TypeAirBase2,        "unit_airbase2_rubomber" },

        // China
        { World::TerritoryChina, WorldObject::TypeFighterLight,     "unit_fighter_light_j10c" },
        { World::TerritoryChina, WorldObject::TypeFighterStealth,   "unit_fighter_stealth_j20" },
        { World::TerritoryChina, WorldObject::TypeFighterNavyStealth,"unit_fighter_navy_stealth_j35" },
        { World::TerritoryChina, WorldObject::TypeBomber,           "unit_bomber_h6" },
        { World::TerritoryChina, WorldObject::TypeCarrierSuper,     "unit_carrier_super_fujian" },
        { World::TerritoryChina, WorldObject::TypeCarrier,          "unit_carrier_chi" },
        { World::TerritoryChina, WorldObject::TypeCarrierLight,     "unit_carrier_light_chi" },
        { World::TerritoryChina, WorldObject::TypeCarrierLHD,       "unit_carrier_lhd_type075" },
        { World::TerritoryChina, WorldObject::TypeBattleShip,       "unit_battleship_cg_type055" },
        { World::TerritoryChina, WorldObject::TypeBattleShip2,      "unit_battleship_ddg_type052d" },
        { World::TerritoryChina, WorldObject::TypeBattleShip3,      "unit_battleship_ffg_type054a" },
        { World::TerritoryChina, WorldObject::TypeSub,              "unit_sub_jin" },
        { World::TerritoryChina, WorldObject::TypeSubC,             "unit_subc_shang" },
        { World::TerritoryChina, WorldObject::TypeSubK,             "unit_subk_yuan" },
        { World::TerritoryChina, WorldObject::TypeAirBase,          "unit_airbase_prcaf" },
        { World::TerritoryChina, WorldObject::TypeAirBase2,         "unit_airbase2_prcnavy" },

        // India
        { World::TerritoryIndia, WorldObject::TypeFighterLight,     "unit_fighter_flanker" },
        { World::TerritoryIndia, WorldObject::TypeFighter,          "unit_fighter_rafale" },
        { World::TerritoryIndia, WorldObject::TypeCarrier,          "unit_carrier_vikrant" },

        // Pakistan
        { World::TerritoryPakistan, WorldObject::TypeFighter,       "unit_fighter_light_j10c" },

        // Japan
        { World::TerritoryJapan, WorldObject::TypeCarrierLight,     "unit_carrier_light_izumo" },

        // Australia
        { World::TerritoryAustralia, WorldObject::TypeCarrierLight, "unit_carrier_light_canberra" },

        // Ukraine
        { World::TerritoryUkraine, WorldObject::TypeFighterLight,   "unit_fighter_light_fulcrum" },

        // Iran
        { World::TerritoryIran, WorldObject::TypeFighterLight,      "unit_fighter_light_fulcrum" },
        { World::TerritoryIran, WorldObject::TypeFighter,           "unit_fighter_f14" },

        // Saudi
        { World::TerritorySaudi, WorldObject::TypeFighter,          "unit_fighter_rafale" },

        // Egypt
        { World::TerritoryEgypt, WorldObject::TypeFighter,          "unit_fighter_rafale" },

        // North Korea
        { World::TerritoryNorthKorea, WorldObject::TypeFighterLight,"unit_fighter_light_fulcrum" },
    };

    static const int s_numNameOverrides = sizeof(s_nameOverrides) / sizeof(s_nameOverrides[0]);

    struct GenericNameDefault
    {
        int unitType;
        const char *langKey;
    };

    static const GenericNameDefault s_genericNameDefaults[] =
    {
        { WorldObject::TypeFighterLight,        "unit_fighter_light_f16" },
        { WorldObject::TypeFighter,             "unit_fighter_flanker" },
        { WorldObject::TypeFighterStealth,      "unit_fighter_stealth_su57" },
        { WorldObject::TypeFighterNavyStealth,  "unit_fighter_navy_stealth_f35" },
    };

    static const int s_numGenericNameDefaults = sizeof(s_genericNameDefaults) / sizeof(s_genericNameDefaults[0]);
}


const char *WorldObject::GetTerritoryName( int _type, int _territory )
{
    if( _territory >= 0 )
    {
        for( int i = 0; i < s_numNameOverrides; ++i )
        {
            if( s_nameOverrides[i].territory == _territory &&
                s_nameOverrides[i].unitType == _type )
            {
                return LANGUAGEPHRASE( s_nameOverrides[i].langKey );
            }
        }
    }

    for( int i = 0; i < s_numGenericNameDefaults; ++i )
    {
        if( s_genericNameDefaults[i].unitType == _type )
            return LANGUAGEPHRASE( s_genericNameDefaults[i].langKey );
    }

    return GetName( _type );
}


const char *WorldObject::GetTerritoryUnitInfoSuffix( int _type, int _territory )
{
    const char *langKey = NULL;

    if( _territory >= 0 )
    {
        for( int i = 0; i < s_numNameOverrides; ++i )
        {
            if( s_nameOverrides[i].territory == _territory &&
                s_nameOverrides[i].unitType == _type )
            {
                langKey = s_nameOverrides[i].langKey;
                break;
            }
        }
    }

    if( !langKey )
    {
        for( int i = 0; i < s_numGenericNameDefaults; ++i )
        {
            if( s_genericNameDefaults[i].unitType == _type )
            {
                langKey = s_genericNameDefaults[i].langKey;
                break;
            }
        }
    }

    if( langKey && strncmp( langKey, "unit_", 5 ) == 0 )
        return langKey + 5;

    return GetTypeName( _type );
}


int WorldObject::GetType( const char *_name )
{
    for( int i = 0; i < NumObjectTypes; ++i )
    {
        if( stricmp( GetName(i), _name ) == 0 )
        {
            return i;
        }
    }

    return -1;
}


const char *WorldObject::GetTypeName (int _type)
{
    switch( _type )
    {
        case TypeSilo:          return "silo";
        case TypeSAM:           return "sam";
        case TypeNuke:          return "nuke";
        case TypeCity:          return "city";
        case TypeExplosion:     return "explosion";
        case TypeSub:           return "sub";
        case TypeSubG:          return "subg";
        case TypeSubC:          return "subc";
        case TypeSubK:          return "subk";
        case TypeRadarStation:  return "radar";
        case TypeRadarEW:       return "radar_ew";
        case TypeBattleShip:    return "battleship";
        case TypeBattleShip2:   return "battleship_ddg";
        case TypeBattleShip3:   return "battleship_fg";
        case TypeAirBase:       return "airbase";
        case TypeAirBase2:      return "airbase2";
        case TypeAirBase3:      return "airbase3";
        case TypeFighter:       return "fighter";
        case TypeFighterLight:  return "fighter_light";
        case TypeFighterStealth: return "fighter_stealth";
        case TypeFighterNavyStealth: return "fighter_navy_stealth";
        case TypeBomber:        return "bomber";
        case TypeBomberFast:    return "bomber_fast";
        case TypeBomberStealth: return "bomber_stealth";
        case TypeAEW:           return "aew";
        case TypeTanker:        return "tanker";
        case TypeCarrier:       return "carrier";
        case TypeCarrierLight:  return "carrier_light";
        case TypeCarrierSuper:  return "carrier_super";
        case TypeCarrierLHD:    return "carrier_lhd";
		case TypeTornado:       return "tornado";
        case TypeSaucer:        return "saucer";
        case TypeLACM:         return "lacm";
        case TypeCBM:          return "cbm";
        case TypeLANM:         return "lanm";
        case TypeABM:          return "abm";
        case TypeSiloMed:      return "silomed";
        case TypeSiloMobile:   return "silomobile";
        case TypeSiloMobileCon: return "silomobilecon";
        case TypeASCM:         return "ascm";
    }

    return "?";
}


void WorldObject::SetRadarRange ( Fixed radarRange )
{
    m_radarRange = radarRange;
}

void WorldObject::RunAI()
{

}

void WorldObject::RenderGhost2D( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        float size = GetSize().DoubleValue();       

        int transparency = (255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ) ).IntValue();
        Colour col = Colour(150, 150, 150, transparency);
        Image *img = GetBmpImage( m_lastSeenState[ teamId ] );

        float x = m_lastKnownPosition[teamId].x.DoubleValue() - size;
        float y = m_lastKnownPosition[teamId].y.DoubleValue() + size;
        float thisSize = size*2;

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);

        if( team->m_territories[0] >= 3 )
        {
            x = m_lastKnownPosition[teamId].x.DoubleValue() + size;
            thisSize = size*-2;
        }       

        g_renderer2d->StaticSprite( img, x, y, thisSize, size*-2, col );
    }
}

void WorldObject::RenderGhost3D( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        GlobeRenderer *globeRenderer = g_app->GetGlobeRenderer();
        if( globeRenderer )
        {
            float size = GetSize3D().DoubleValue();

            int transparency = (255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ) ).IntValue();
            Colour col = Colour(150, 150, 150, transparency);
            Image *img = GetBmpImage( m_lastSeenState[ teamId ] );

            float longitude = m_lastKnownPosition[teamId].x.DoubleValue();
            float latitude = m_lastKnownPosition[teamId].y.DoubleValue();
            
            Vector3<float> objPos = globeRenderer->ConvertLongLatTo3DPosition(longitude, latitude);
            Vector3<float> normal = objPos;
            normal.Normalise();
            float elevation = GLOBE_ELEVATION;
            Vector3<float> renderPos = objPos + normal * elevation;

            float spriteSize = size * 2.0f;
            // All ships face same direction; no territory-based flip

            g_renderer3d->StaticSprite3D( img, renderPos.x, renderPos.y, renderPos.z, 
                                          spriteSize, spriteSize, col, BILLBOARD_SURFACE_ALIGNED );
        }
    }
}

Image *WorldObject::GetBmpImage( int state )
{
    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    return bmpImage;
}


bool WorldObject::IsMovingObject()
{
    return IsNavy() || IsAircraft() || IsBallisticMissileClass() || IsCruiseMissileClass();
}

void WorldObject::RequestAction( ActionOrder *_action )
{
    m_actionQueue.PutData( _action);
}

bool WorldObject::IsActionQueueable()
{
    return false;
}

bool WorldObject::ShouldProcessNextQueuedAction()
{
    return true;
}

bool WorldObject::ShouldProcessActionQueue()
{
    return true;
}

bool WorldObject::UsingGuns()
{
    return false;
}

void WorldObject::NukeStrike()
{
}

bool WorldObject::UsingNukes()
{
    return false;
}

void WorldObject::Retaliate( int attackerId )
{
}

bool WorldObject::IsPinging()
{
    return false;
}

void WorldObject::BurstFireTick()
{
    Fixed dt = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    for( int i = m_burstFireTargetIds.Size() - 1; i >= 0; --i )
    {
        int targetId = m_burstFireTargetIds.GetData( i );
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( targetId );
        if( !obj || obj->m_life <= 0 )
        {
            m_burstFireTargetIds.RemoveData( i );
            m_burstFireCountdowns.RemoveData( i );
            continue;
        }
        Fixed *cd = m_burstFireCountdowns.GetPointer( i );
        if( cd )
        {
            *cd -= dt;
            if( *cd <= 0 )
            {
                m_burstFireTargetIds.RemoveData( i );
                m_burstFireCountdowns.RemoveData( i );
            }
        }
    }
}

int WorldObject::GetBurstFireShots() const
{
    return BURST_FIRE_SHOTS;
}

bool WorldObject::BurstFireOnFired( int currentTargetId )
{
    if( currentTargetId == -1 )
        return false;
    m_burstFireShotCount++;
    if( m_burstFireShotCount >= GetBurstFireShots() )
    {
        m_burstFireTargetIds.PutData( currentTargetId );
        m_burstFireCountdowns.PutData( BURST_FIRE_COOLDOWN_SECONDS );
        return true;
    }
    return false;
}

void WorldObject::BurstFireResetShotCount()
{
    m_burstFireShotCount = 0;
}

void WorldObject::BurstFireRemoveTarget( int targetId )
{
    for( int i = m_burstFireTargetIds.Size() - 1; i >= 0; --i )
    {
        if( m_burstFireTargetIds.GetData( i ) == targetId )
        {
            m_burstFireTargetIds.RemoveData( i );
            m_burstFireCountdowns.RemoveData( i );
            return;
        }
    }
}

void WorldObject::BurstFireClear()
{
    m_burstFireTargetIds.Empty();
    m_burstFireCountdowns.Empty();
    m_burstFireShotCount = 0;
}

void WorldObject::BurstFireGetExcludedIds( LList<int> &out ) const
{
    out.Empty();
    for( int i = 0; i < m_burstFireTargetIds.Size(); ++i )
    {
        out.PutData( m_burstFireTargetIds.GetData( i ) );
    }
}

void WorldObject::BurstFireAccelerateCountdowns( Fixed amount )
{
    for( int i = 0; i < m_burstFireCountdowns.Size(); ++i )
    {
        Fixed *cd = m_burstFireCountdowns.GetPointer( i );
        if( cd )
        {
            *cd -= amount;
            if( *cd < 0 )
                *cd = 0;
        }
    }
}

void WorldObject::FireGun( Fixed range )
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    AppAssert( targetObject );
    GunFire *bullet = new GunFire( range );
    bullet->SetPosition( m_longitude, m_latitude );
    bullet->SetTargetObjectId( targetObject->m_objectId );
    bullet->SetTeamId( m_teamId );
    bullet->m_origin = m_objectId;
    Fixed interceptLongitude, interceptLatitude;
    bullet->GetCombatInterceptionPoint( targetObject, &interceptLongitude, &interceptLatitude );
    bullet->SetWaypoint( interceptLongitude, interceptLatitude );
    bullet->SetInitialVelocityTowardWaypoint();
    bullet->m_distanceToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, interceptLongitude, interceptLatitude );
    bullet->m_attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, targetObject->m_type, m_objectId );
    (void)g_app->GetWorld()->m_gunfire.PutData( bullet );

    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( team ) team->RegisterEngagedTarget( m_targetObjectId );
}

int WorldObject::GetAttackOdds( int _defenderType )
{
    return g_app->GetWorld()->GetAttackOdds( m_type, _defenderType );
}

WorldObject *WorldObject::CreateObject( int _type )
{
    switch( _type )
	{
        case TypeCity:                  return new City( "City" );
		case TypeSilo:                  return new Silo();
		case TypeSAM:                   return new SAM();
		case TypeRadarStation:          return new RadarStation();
		case TypeRadarEW:               return new RadarEW();
        case TypeNuke:                  return new Nuke();
        case TypeLACM:                  return new LACM();
        case TypeCBM:                   return new CBM();
        case TypeLANM:                  return new LANM();
        case TypeABM:                   return new ABM();
        case TypeSiloMed:               return new SiloMed();
        case TypeSiloMobile:            return new SiloMobile();
        case TypeSiloMobileCon:         return new SiloMobileCon();
        case TypeASCM:                  return new ASCM();
        case TypeExplosion:             return new Explosion();
		case TypeSub:                   return new Sub();
		case TypeSubG:                  return new SubG();
		case TypeSubC:                  return new SubC();
		case TypeSubK:                  return new SubK();
		case TypeBattleShip:            return new BattleShip();
		case TypeBattleShip2:           return new BattleShip2();
		case TypeBattleShip3:           return new BattleShip3();
		case TypeAirBase:               return new AirBase();
		case TypeAirBase2:              return new AirBase2();
		case TypeAirBase3:              return new AirBase3();
        case TypeFighter:               return new Fighter();
        case TypeFighterLight:          return new FighterLight();
        case TypeFighterStealth:        return new FighterStealth();
        case TypeFighterNavyStealth:    return new FighterNavyStealth();
        case TypeBomber:                return new Bomber();
        case TypeBomberFast:            return new BomberFast();
        case TypeBomberStealth:         return new BomberStealth();
        case TypeAEW:                   return new AEW();
        case TypeTanker:                return new Tanker();
		case TypeCarrier:               return new Carrier();
		case TypeCarrierLight:          return new CarrierLight();
		case TypeCarrierSuper:          return new CarrierSuper();
		case TypeCarrierLHD:            return new CarrierLHD();
        case TypeTornado:               return new Tornado();
        //case TypeSaucer:                return new Saucer();
	}

	return NULL;
}

void WorldObject::ClearActionQueue()
{
    m_actionQueue.EmptyAndDelete();
}

void WorldObject::ClearLastAction()
{
    if( m_actionQueue.Size() > 0 )
    {
        ActionOrder *action = m_actionQueue[ m_actionQueue.Size() -1 ];
        m_actionQueue.RemoveDataAtEnd();
        delete action;
    }
}

void WorldObject::SetTargetObjectId( int targetObjectId )
{
	m_targetObjectId = targetObjectId;
}

int WorldObject::GetTargetObjectId()
{
    return m_targetObjectId;
}

void WorldObject::AcquireTargetFromAction( ActionOrder *action )
{
}

void WorldObject::FleetAction( int targetObjectId )
{
}

bool WorldObject::LaunchBomber( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    Bomber *bomber = new Bomber();
    return LaunchBomberFrom( this, bomber, targetObjectId, longitude, latitude, aircraftMode );
}

bool WorldObject::LaunchFighter( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    if( targetObjectId >= OBJECTID_CITYS && aircraftMode != 2 )
    {
        targetObjectId = -1;
    }

    Fighter *fighter = new Fighter();
    if( fighter->IsValidPosition( longitude, latitude ) )
    {
        fighter->SetTeamId( m_teamId );
        fighter->SetPosition( m_longitude, m_latitude );

        // aircraftMode: 1=CAP (6 air attack, 0 LACM), 2=Strike (2 air attack, 2 LACM)
        if( aircraftMode == 2 )
        {
            fighter->m_lacmLoadout = true;
            fighter->m_states[0]->m_numTimesPermitted = 2;
            fighter->m_states[1]->m_numTimesPermitted = 2;
            fighter->SetState( 1 );
        }
        else
        {
            fighter->m_lacmLoadout = false;
            fighter->m_states[0]->m_numTimesPermitted = 6;
            fighter->m_states[1]->m_numTimesPermitted = 0;
        }

	    WorldObject *target = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) &&
            ( target->IsAircraftLauncher() || target->m_type == TypeTanker || target->IsAircraft() ) )
        {
            if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
            {
                fighter->Land( targetObjectId );
            }
            else
            {
                fighter->SetWaypoint( target->m_longitude, target->m_latitude );
                fighter->m_isEscorting = targetObjectId;
            }
        }
        else if( aircraftMode == 2 && target &&
                 ( (!target->IsMovingObject() && GetArchetypeForType(target->m_type) == ArchetypeBuilding) ||
                   target->IsTargetableSurfaceNavy() || target->IsCityClass() ||
                   ( target->IsAircraftLauncher() && !g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) ) ) )
        {
            fighter->SetWaypoint( longitude, latitude );
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            fighter->RequestAction( order );
        }
        else if( target && fighter->GetAttackOdds( target->m_type ) > 0 )
        {
            fighter->SetTargetObjectId(targetObjectId);
            fighter->SetWaypoint(target->m_longitude, target->m_latitude);
            if( aircraftMode == 2 )
            {
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = targetObjectId;
                order->m_longitude = target->m_longitude;
                order->m_latitude = target->m_latitude;
                fighter->RequestAction( order );
            }
        }
	    else
	    {
		    fighter->SetWaypoint( longitude, latitude );
            fighter->m_playerSetWaypoint = true;
	    }
        g_app->GetWorld()->AddWorldObject( fighter );
        return true;
    }
    else
    {
        delete fighter;
        return false;
    }
}

bool WorldObject::LaunchFighterLight( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    if( targetObjectId >= OBJECTID_CITYS && aircraftMode != 1 )
    {
        targetObjectId = -1;
    }

    FighterLight *fighter = new FighterLight();
    if( fighter->IsValidPosition( longitude, latitude ) )
    {
        fighter->SetTeamId( m_teamId );
        fighter->SetPosition( m_longitude, m_latitude );
        if( aircraftMode == 1 )
        {
            fighter->SetState( 1 );
        }
        else
        {
            fighter->m_states[1]->m_numTimesPermitted = 0;
        }
        WorldObject *target = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
        {
            if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
            {
                // FighterLight cannot land on carriers - only airbases
                if( target->IsCarrierClass() )
                    fighter->Land( fighter->GetClosestLandingPad() );
                else
                    fighter->Land( targetObjectId );
            }
            else if( target->IsAircraft() )
            {
                fighter->SetWaypoint( target->m_longitude, target->m_latitude );
                fighter->m_isEscorting = targetObjectId;
            }
            else
            {
                fighter->SetWaypoint( longitude, latitude );
                fighter->m_playerSetWaypoint = true;
            }
        }
        else if( aircraftMode == 1 && target &&
                 ( (!target->IsMovingObject() && GetArchetypeForType(target->m_type) == ArchetypeBuilding) ||
                   target->IsTargetableSurfaceNavy() || target->IsCityClass() ||
                   ( target->IsAircraftLauncher() && !g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) ) ) )
        {
            fighter->SetWaypoint( longitude, latitude );
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            fighter->RequestAction( order );
        }
        else if( target && fighter->GetAttackOdds( target->m_type ) > 0 )
        {
            fighter->SetTargetObjectId(targetObjectId);
            fighter->SetWaypoint(target->m_longitude, target->m_latitude);
            if( aircraftMode == 1 )
            {
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = targetObjectId;
                order->m_longitude = target->m_longitude;
                order->m_latitude = target->m_latitude;
                fighter->RequestAction( order );
            }
        }
        else
        {
            fighter->SetWaypoint( longitude, latitude );
            fighter->m_playerSetWaypoint = true;
        }
        g_app->GetWorld()->AddWorldObject( fighter );
        return true;
    }
    else
    {
        delete fighter;
        return false;
    }
}

bool WorldObject::LaunchNavyStealthFighter( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    if( targetObjectId >= OBJECTID_CITYS && aircraftMode != 1 )
    {
        targetObjectId = -1;
    }

    FighterNavyStealth *fighter = new FighterNavyStealth();
    if( fighter->IsValidPosition( longitude, latitude ) )
    {
        fighter->SetTeamId( m_teamId );
        fighter->SetPosition( m_longitude, m_latitude );
        if( aircraftMode == 1 )
        {
            fighter->m_states[1]->m_numTimesPermitted = 1;
            fighter->SetState( 1 );
        }
        WorldObject *target = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
        {
            if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
                fighter->Land( targetObjectId );
            else if( target->IsAircraft() )
            {
                fighter->SetWaypoint( target->m_longitude, target->m_latitude );
                fighter->m_isEscorting = targetObjectId;
            }
            else
            {
                fighter->SetWaypoint( longitude, latitude );
                fighter->m_playerSetWaypoint = true;
            }
        }
        else if( aircraftMode == 1 && target &&
                 ( (!target->IsMovingObject() && GetArchetypeForType(target->m_type) == ArchetypeBuilding) ||
                   target->IsTargetableSurfaceNavy() || target->IsCityClass() ||
                   ( target->IsAircraftLauncher() && !g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) ) ) )
        {
            fighter->SetWaypoint( longitude, latitude );
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            fighter->RequestAction( order );
        }
        else if( target && fighter->GetAttackOdds( target->m_type ) > 0 )
        {
            fighter->SetTargetObjectId(targetObjectId);
            fighter->SetWaypoint(target->m_longitude, target->m_latitude);
            if( aircraftMode == 1 )
            {
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = targetObjectId;
                order->m_longitude = target->m_longitude;
                order->m_latitude = target->m_latitude;
                fighter->RequestAction( order );
            }
        }
        else
        {
            fighter->SetWaypoint( longitude, latitude );
            fighter->m_playerSetWaypoint = true;
        }
        g_app->GetWorld()->AddWorldObject( fighter );
        return true;
    }
    else
    {
        delete fighter;
        return false;
    }
}

bool WorldObject::LaunchStealthFighter( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    if( targetObjectId >= OBJECTID_CITYS && aircraftMode != 1 )
    {
        targetObjectId = -1;
    }

    FighterStealth *fighter = new FighterStealth();
    if( fighter->IsValidPosition( longitude, latitude ) )
    {
        fighter->SetTeamId( m_teamId );
        fighter->SetPosition( m_longitude, m_latitude );
        if( aircraftMode == 1 )
        {
            fighter->m_states[1]->m_numTimesPermitted = 1;
            fighter->SetState( 1 );
        }
        WorldObject *target = g_app->GetWorld()->GetWorldObject(targetObjectId);
        if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
        {
            if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
            {
                // FighterStealth cannot land on carriers - only airbases
                if( target->IsCarrierClass() )
                    fighter->Land( fighter->GetClosestLandingPad() );
                else
                    fighter->Land( targetObjectId );
            }
            else if( target->IsAircraft() )
            {
                fighter->SetWaypoint( target->m_longitude, target->m_latitude );
                fighter->m_isEscorting = targetObjectId;
            }
            else
            {
                fighter->SetWaypoint( longitude, latitude );
                fighter->m_playerSetWaypoint = true;
            }
        }
        else if( aircraftMode == 1 && target &&
                 ( (!target->IsMovingObject() && GetArchetypeForType(target->m_type) == ArchetypeBuilding) ||
                   target->IsTargetableSurfaceNavy() || target->IsCityClass() ||
                   ( target->IsAircraftLauncher() && !g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) ) ) )
        {
            fighter->SetWaypoint( longitude, latitude );
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            fighter->RequestAction( order );
        }
        else if( target && fighter->GetAttackOdds( target->m_type ) > 0 )
        {
            fighter->SetTargetObjectId(targetObjectId);
            fighter->SetWaypoint(target->m_longitude, target->m_latitude);
            if( aircraftMode == 1 )
            {
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = targetObjectId;
                order->m_longitude = target->m_longitude;
                order->m_latitude = target->m_latitude;
                fighter->RequestAction( order );
            }
        }
        else
        {
            fighter->SetWaypoint( longitude, latitude );
            fighter->m_playerSetWaypoint = true;
        }
        g_app->GetWorld()->AddWorldObject( fighter );
        return true;
    }
    else
    {
        delete fighter;
        return false;
    }
}

static bool LaunchBomberFrom( WorldObject *wo, Bomber *bomber, int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    bomber->SetTeamId( wo->m_teamId );
    bomber->SetPosition( wo->m_longitude, wo->m_latitude );
    bomber->SetWaypoint( longitude, latitude );

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target && g_app->GetWorld()->IsFriend( wo->m_teamId, target->m_teamId ) &&
        ( target->IsAircraftLauncher() || target->m_type == WorldObject::TypeTanker || target->IsAircraft() ) )
    {
        if( target->IsAircraftLauncher() || target->m_type == WorldObject::TypeTanker )
            bomber->Land( targetObjectId );
        else
        {
            bomber->SetWaypoint( target->m_longitude, target->m_latitude );
            bomber->m_isEscorting = targetObjectId;
        }
        g_app->GetWorld()->AddWorldObject( bomber );
        return true;
    }
    if( aircraftMode == 1 )
    {
        bomber->m_lacmLoadout = false;
        bomber->m_states[2]->m_numTimesPermitted = 0;
        if( bomber->m_states[1]->m_numTimesPermitted == 0 )
            bomber->m_states[1]->m_numTimesPermitted = 2;
        bomber->m_states[0]->m_numTimesPermitted = bomber->m_states[1]->m_numTimesPermitted;
        bool validNukeTarget = target && ( !target->IsMovingObject() ||
            target->IsTargetableSurfaceNavy() );
        if( validNukeTarget )
        {
            bomber->SetState(1);
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            bomber->RequestAction( order );
            bomber->m_bombingRun = true;
            if( bomber->m_currentState == 0 )
                bomber->m_targetObjectId = targetObjectId;
        }
    }
    else if( aircraftMode == 2 )
    {
        bomber->m_lacmLoadout = true;
        bomber->m_states[1]->m_numTimesPermitted = 0;
        if( bomber->m_states[2]->m_numTimesPermitted == 0 )
            bomber->m_states[2]->m_numTimesPermitted = 6;
        bomber->m_states[0]->m_numTimesPermitted = bomber->m_states[2]->m_numTimesPermitted;
        if( target && ( (!target->IsMovingObject() && WorldObject::GetArchetypeForType(target->m_type) == WorldObject::ArchetypeBuilding) ||
                        target->IsTargetableSurfaceNavy() ) )
        {
            bomber->SetState(2);
            ActionOrder *order = new ActionOrder();
            order->m_targetObjectId = targetObjectId;
            order->m_longitude = longitude;
            order->m_latitude = latitude;
            bomber->RequestAction( order );
            bomber->m_bombingRun = true;
            if( bomber->m_currentState == 0 )
                bomber->m_targetObjectId = targetObjectId;
        }
    }
    else
    {
        bomber->m_lacmLoadout = false;
        bomber->m_states[2]->m_numTimesPermitted = 0;
        bomber->m_states[0]->m_numTimesPermitted = bomber->m_states[1]->m_numTimesPermitted;
        if( target )
        {
            if( g_app->GetWorld()->GetAttackOdds( bomber->m_type, target->m_type ) > 0 )
            {
                bomber->SetState(0);
                bomber->m_targetObjectId = target->m_objectId;
            }
            else if( !target->IsMovingObject() )
            {
                bomber->SetState(1);
                ActionOrder *order = new ActionOrder();
                order->m_targetObjectId = targetObjectId;
                order->m_longitude = longitude;
                order->m_latitude = latitude;
                bomber->RequestAction( order );
                bomber->m_bombingRun = true;
                if( bomber->m_currentState == 0 )
                    bomber->m_targetObjectId = targetObjectId;
            }
        }
        else if( targetObjectId != -1 )
        {
            bomber->SetState(0);
        }
        if( bomber->m_currentState == 0 )
        {
            for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
            {
                if( g_app->GetWorld()->m_cities.ValidIndex(i) )
                {
                    City *city = g_app->GetWorld()->m_cities[i];
                    if( city->m_longitude == longitude && city->m_latitude == latitude )
                    {
                        bomber->SetState(1);
                        ActionOrder *order = new ActionOrder();
                        order->m_targetObjectId = city->m_objectId;
                        order->m_longitude = longitude;
                        order->m_latitude = latitude;
                        bomber->RequestAction( order );
                        bomber->m_bombingRun = true;
                        break;
                    }
                }
            }
        }
    }
    g_app->GetWorld()->AddWorldObject( bomber );
    if( wo->m_nukeSupply == 0 )
    {
        bomber->ClearActionQueue();
        bomber->m_bombingRun = false;
        bomber->m_states[1]->m_numTimesPermitted = 0;
    }
    return true;
}

bool WorldObject::LaunchBomberFast( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    BomberFast *bomber = new BomberFast();
    return LaunchBomberFrom( this, bomber, targetObjectId, longitude, latitude, aircraftMode );
}

bool WorldObject::LaunchStealthBomber( int targetObjectId, Fixed longitude, Fixed latitude, int aircraftMode )
{
    BomberStealth *bomber = new BomberStealth();
    return LaunchBomberFrom( this, bomber, targetObjectId, longitude, latitude, aircraftMode );
}

bool WorldObject::LaunchAEW( int targetObjectId, Fixed longitude, Fixed latitude )
{
    AEW *aew = new AEW();
    if( !aew->IsValidPosition( longitude, latitude ) )
    {
        delete aew;
        return false;
    }
    aew->SetTeamId( m_teamId );
    aew->SetPosition( m_longitude, m_latitude );

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
    {
        if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
            aew->Land( targetObjectId );
        else if( target->IsAircraft() )
        {
            aew->SetWaypoint( target->m_longitude, target->m_latitude );
            aew->m_isEscorting = targetObjectId;
        }
        else
            aew->SetWaypoint( longitude, latitude );
    }
    else
        aew->SetWaypoint( longitude, latitude );

    g_app->GetWorld()->AddWorldObject( aew );
    return true;
}

bool WorldObject::LaunchTanker( int targetObjectId, Fixed longitude, Fixed latitude )
{
    Tanker *tanker = new Tanker();
    if( !tanker->IsValidPosition( longitude, latitude ) )
    {
        delete tanker;
        return false;
    }
    tanker->SetTeamId( m_teamId );
    tanker->SetPosition( m_longitude, m_latitude );

    WorldObject *target = g_app->GetWorld()->GetWorldObject( targetObjectId );
    if( target && g_app->GetWorld()->IsFriend( m_teamId, target->m_teamId ) )
    {
        if( target->IsAircraftLauncher() || target->m_type == TypeTanker )
            tanker->Land( targetObjectId );
        else if( target->IsAircraft() )
        {
            tanker->SetWaypoint( target->m_longitude, target->m_latitude );
            tanker->m_isEscorting = targetObjectId;
        }
        else
            tanker->SetWaypoint( longitude, latitude );
    }
    else
        tanker->SetWaypoint( longitude, latitude );

    g_app->GetWorld()->AddWorldObject( tanker );
    return true;
}


int WorldObject::IsValidCombatTarget( int _objectId )
{
    if( _objectId == m_objectId ) return TargetTypeInvalid;

    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( !obj ) return TargetTypeInvalid;

    bool isFriend = g_app->GetWorld()->IsFriend( m_teamId, obj->m_teamId );
    int attackOdds = g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type, m_objectId );

    int currentDefcon = g_app->GetWorld()->GetDefcon();
    if( !isFriend && 
        attackOdds > 0 && 
        m_states[m_currentState]->m_defconPermitted < currentDefcon )
    {
        return TargetTypeDefconRequired;
    }


    if( !IsMovingObject() )
    {
        Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, 
                                                         obj->m_longitude, obj->m_latitude );
        
        if( distance > m_states[m_currentState]->m_actionRange )         
        {
            return TargetTypeOutOfRange;
        }
    }

    if( m_states[m_currentState]->m_numTimesPermitted > -1 &&
        m_states[m_currentState]->m_numTimesPermitted - m_actionQueue.Size() <= 0 )
    {
        return TargetTypeOutOfStock;
    }
    
    if( attackOdds > 0 )
    {
        return TargetTypeValid;
    }

    return TargetTypeInvalid;
}

void WorldObject::CeaseFire( int teamId )
{
    if( m_targetObjectId != -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
        if( obj )
        {
            if( obj->m_teamId == teamId )
            {
                m_targetObjectId = -1;
            }
        }
    }

    for( int i = 0; i < m_actionQueue.Size(); ++i )
    {
        ActionOrder *action = m_actionQueue[i];
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( action->m_targetObjectId );
        if( obj )
        {
            if( obj->m_teamId == teamId )
            {
                m_actionQueue.RemoveData(i);
                delete action;
                --i;
            }
        }
    }
}


int WorldObject::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    return TargetTypeInvalid;
}


bool WorldObject::CanLaunchBomber()
{
    return false;
}

bool WorldObject::CanLaunchBomberFast()
{
    return false;
}

bool WorldObject::CanLaunchStealthBomber()
{
    return false;
}

bool WorldObject::CanLaunchFighter()
{
    return false;
}

bool WorldObject::CanLaunchFighterLight()
{
    return false;
}

bool WorldObject::CanLaunchStealthFighter()
{
    return false;
}

bool WorldObject::CanLaunchNavyStealthFighter()
{
    return false;
}

bool WorldObject::CanLaunchAEW()
{
    return false;
}

bool WorldObject::CanLaunchTanker()
{
    return false;
}


int WorldObject::CountIncomingFighters()
{
    return 0;
}

bool WorldObject::SetWaypointOnAction()
{
    return false;
}

// ================================================================================

WorldObjectState::WorldObjectState()
:   m_stateName( NULL )
{
}

WorldObjectState::~WorldObjectState()
{
    delete[] m_stateName;
}

static char tempStateName[256];

char *WorldObjectState::GetStateName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetPrepareName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetPreparingName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}

char *WorldObjectState::GetReadyName()
{
    sprintf( tempStateName, "%s", m_stateName );
    return tempStateName;
}


char *HashDouble( double value, char *buffer )
{
	union
	{
		double doublePart;
		unsigned long long intPart;
	} v;
	
	v.doublePart = value;
	const unsigned long long i = v.intPart;
	
    for( int j = 0; j < 16; ++j )
    {
		sprintf(buffer + j, "%01x", (unsigned int) ((i >> (j*4)) & 0xF) );
    }

    return buffer;
};


char *WorldObject::LogState()
{
    char buf1[17], buf2[17], buf3[17], buf4[17], buf5[17], buf6[17], buf7[17];

    static char s_result[10240];
    snprintf( s_result, 10240, "obj[%d] [%10s] team[%d] fleet[%d] long[%s] lat[%s] velX[%s] velY[%s] state[%d] target[%d] life[%d] timer[%s] retarget[%s] ai[%s]",
                m_objectId,
                GetName(m_type),
                m_teamId,
                m_fleetId,
                HashDouble( m_longitude.DoubleValue(), buf1 ),
                HashDouble( m_latitude.DoubleValue(), buf2 ),
                HashDouble( m_vel.x.DoubleValue(), buf3 ),
                HashDouble( m_vel.y.DoubleValue(), buf4 ),
                m_currentState,
                m_targetObjectId,
                m_life,
                HashDouble( m_stateTimer.DoubleValue(), buf5 ),
                HashDouble( m_retargetTimer.DoubleValue(), buf6 ),
                HashDouble( m_aiTimer.DoubleValue(), buf7 ) );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        char thisTeam[512];
        sprintf( thisTeam, "\n\tTeam %d visible[%d] seen[%d] pos[%s %s] vel[%s %s] seen[%s] state[%d]",
                            team->m_teamId,
                            static_cast<int>(static_cast<bool>(m_visible[team->m_teamId])),
                            static_cast<int>(static_cast<bool>(m_seen[team->m_teamId])),
                            HashDouble( m_lastKnownPosition[team->m_teamId].x.DoubleValue(), buf1 ),
                            HashDouble( m_lastKnownPosition[team->m_teamId].y.DoubleValue(), buf2 ),
                            HashDouble( m_lastKnownVelocity[team->m_teamId].x.DoubleValue(), buf3 ),
                            HashDouble( m_lastKnownVelocity[team->m_teamId].y.DoubleValue(), buf4 ),
                            HashDouble( m_lastSeenTime[team->m_teamId].DoubleValue(), buf5 ),
                            m_lastSeenState[team->m_teamId] );

        strcat( s_result, thisTeam );
    }

    return s_result;
}


// -----------------------------------------------------------------------------
// Archetype and ClassType
// -----------------------------------------------------------------------------

WorldObject::Archetype WorldObject::GetArchetypeForType( int type )
{
    switch( type )
    {
        case TypeSilo:
        case TypeSiloMed:
        case TypeSiloMobile:
        case TypeSiloMobileCon:
        case TypeASCM:
        case TypeSAM:
        case TypeABM:
        case TypeRadarStation:
        case TypeRadarEW:
        case TypeAirBase:
        case TypeAirBase2:
        case TypeAirBase3:
        case TypeCity:
            return ArchetypeBuilding;

        case TypeSub:
        case TypeSubG:
        case TypeSubC:
        case TypeSubK:
        case TypeBattleShip:
        case TypeBattleShip2:
        case TypeBattleShip3:
        case TypeCarrier:
        case TypeCarrierLight:
        case TypeCarrierSuper:
        case TypeCarrierLHD:
            return ArchetypeNavy;

        case TypeFighter:
        case TypeFighterLight:
        case TypeFighterStealth:
        case TypeFighterNavyStealth:
        case TypeBomber:
        case TypeBomberFast:
        case TypeBomberStealth:
        case TypeAEW:
        case TypeTanker:
        case TypeLACM:
        case TypeLANM:
            return ArchetypeAircraft;

        case TypeNuke:
        case TypeCBM:
            return ArchetypeBallisticMissile;

        default:
            return ArchetypeInvalid;
    }
}


WorldObject::ClassType WorldObject::GetClassTypeForType( int type )
{
    switch( type )
    {
        case TypeSilo:           return ClassTypeSilo;
        case TypeSiloMed:        return ClassTypeSilo;
        case TypeSiloMobile:     return ClassTypeSilo;
        case TypeSiloMobileCon:  return ClassTypeSilo;
        case TypeASCM:           return ClassTypeSilo;
        case TypeSAM:            return ClassTypeSAM;
        case TypeABM:            return ClassTypeABM;
        case TypeRadarStation:   return ClassTypeRadar;
        case TypeRadarEW:        return ClassTypeRadar;
        case TypeAirBase:       return ClassTypeAirbase;
        case TypeAirBase2:      return ClassTypeAirbase;
        case TypeAirBase3:      return ClassTypeAirbase;
        case TypeCity:          return ClassTypeCity;

        case TypeBattleShip:    return ClassTypeBattleShip;
        case TypeBattleShip2:   return ClassTypeBattleShip;
        case TypeBattleShip3:   return ClassTypeBattleShip;
        case TypeCarrier:       return ClassTypeCarrier;
        case TypeCarrierLight:
        case TypeCarrierSuper:
        case TypeCarrierLHD:    return ClassTypeCarrier;
        case TypeSub:           return ClassTypeSub;
        case TypeSubG:          return ClassTypeSub;
        case TypeSubC:          return ClassTypeSub;
        case TypeSubK:          return ClassTypeSub;

        case TypeFighter:       return ClassTypeFighter;
        case TypeFighterLight:  return ClassTypeFighter;
        case TypeFighterStealth:
        case TypeFighterNavyStealth: return ClassTypeFighter;
        case TypeBomber:        return ClassTypeBomber;
        case TypeBomberFast:    return ClassTypeBomber;
        case TypeBomberStealth: return ClassTypeBomber;
        case TypeAEW:           return ClassTypeAEW;
        case TypeTanker:        return ClassTypeAEW;

        case TypeNuke:          return ClassTypeBallisticMissile;
        case TypeLACM:          return ClassTypeCruiseMissile;
        case TypeCBM:           return ClassTypeBallisticMissile;
        case TypeLANM:           return ClassTypeCruiseMissile;

        default:
            return ClassTypeInvalid;
    }
}


bool WorldObject::IsBuilding() const
{
    return m_archetype == ArchetypeBuilding;
}


bool WorldObject::IsNavy() const
{
    return m_archetype == ArchetypeNavy;
}


bool WorldObject::IsAircraft() const
{
    return m_archetype == ArchetypeAircraft;
}


bool WorldObject::IsSubmarine() const
{
    return m_classType == ClassTypeSub;
}


bool WorldObject::IsSam() const
{
    return m_classType == ClassTypeSAM;
}


bool WorldObject::IsAircraftLauncher() const
{
    return m_classType == ClassTypeCarrier || m_classType == ClassTypeAirbase;
}


bool WorldObject::IsNuke() const
{
    return m_type == TypeNuke;
}


bool WorldObject::IsBallisticMissileClass() const
{
    return m_classType == ClassTypeBallisticMissile;
}


bool WorldObject::IsCruiseMissileClass() const
{
    return m_classType == ClassTypeCruiseMissile;
}


bool WorldObject::IsCityClass() const
{
    return m_classType == ClassTypeCity;
}


bool WorldObject::IsSiloClass() const
{
    return m_classType == ClassTypeSilo;
}


bool WorldObject::IsRadarClass() const
{
    return m_classType == ClassTypeRadar;
}


bool WorldObject::IsAirbaseClass() const
{
    return m_classType == ClassTypeAirbase;
}


bool WorldObject::IsCarrierClass() const
{
    return m_classType == ClassTypeCarrier;
}


bool WorldObject::IsBattleShipClass() const
{
    return m_classType == ClassTypeBattleShip;
}


bool WorldObject::IsTargetableSurfaceNavy()
{
    return IsCarrierClass() || IsBattleShipClass() || ( IsSubmarine() && !IsHiddenFrom() );
}


bool WorldObject::IsFighterClass() const
{
    return m_classType == ClassTypeFighter;
}


bool WorldObject::IsBomberClass() const
{
    return m_classType == ClassTypeBomber;
}


bool WorldObject::IsAEWClass() const
{
    return m_classType == ClassTypeAEW;
}


int WorldObject::GetFighterCount()
{
    return 0;
}

int WorldObject::GetBomberCount()
{
    return 0;
}

int WorldObject::GetAEWCount()
{
    return 0;
}

void WorldObject::OnFighterLanded( int aircraftType )
{
}

void WorldObject::OnBomberLanded( int aircraftType )
{
}

void WorldObject::OnAEWLanded()
{
}

void WorldObject::MaybeRemoveRandomStoredAircraft()
{
}


