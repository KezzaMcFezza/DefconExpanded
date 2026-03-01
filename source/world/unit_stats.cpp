#include "lib/universal_include.h"
#include "world/unit_stats.h"
#include "world/worldobject.h"
#include "world/movingobject.h"
#include "world/world.h"

static void SetStateTiming( WorldObject *unit, int state,
                            Fixed prepare, Fixed reload, Fixed actionRange )
{
    if( state < unit->m_states.Size() )
    {
        unit->m_states[state]->m_timeToPrepare = prepare;
        unit->m_states[state]->m_timeToReload  = reload;
        unit->m_states[state]->m_actionRange   = actionRange / World::GetUnitScaleFactor();
    }
}

static void SetAllStatesPrepareReload( WorldObject *unit,
                                       Fixed prepare, Fixed reload )
{
    for( int i = 0; i < unit->m_states.Size(); ++i )
    {
        unit->m_states[i]->m_timeToPrepare = prepare;
        unit->m_states[i]->m_timeToReload  = reload;
    }
}

static void SetAllStatesPrepare( WorldObject *unit, Fixed prepare )
{
    for( int i = 0; i < unit->m_states.Size(); ++i )
        unit->m_states[i]->m_timeToPrepare = prepare;
}

void ApplyTerritoryStatOverrides( WorldObject *unit, int territory )
{
    int type = unit->m_type;
    MovingObject *mo = unit->IsMovingObject() ? (MovingObject *)unit : nullptr;
    Fixed gs = World::GetUnitScaleFactor();

    // -----------------------------------------------------------------------
    // BattleShip (CG) — Ticonderoga / Slava / Type 055
    // -----------------------------------------------------------------------
    if( type == WorldObject::TypeBattleShip )
    {
        if( territory == World::TerritoryUSA )
        {
            SetStateTiming( unit, 0, 50,  5, 10 );
            SetStateTiming( unit, 1, 50,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 10,  3 );
        }
        else if( territory == World::TerritoryRussia )
        {
            SetStateTiming( unit, 0, 60, 10, 10 );
            SetStateTiming( unit, 1, 60,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 20,  2 );
        }
        else if( territory == World::TerritoryChina )
        {
            SetStateTiming( unit, 0, 60,  5, 10 );
            SetStateTiming( unit, 1, 50,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 15,  2 );
        }
    }

    // -----------------------------------------------------------------------
    // BattleShip2 (DDG) — Arleigh Burke / Horizon / Udaloy / Type 052
    // -----------------------------------------------------------------------
    else if( type == WorldObject::TypeBattleShip2 )
    {
        if( territory == World::TerritoryUSA )
        {
            SetStateTiming( unit, 0, 50,  5, 10 );
            SetStateTiming( unit, 1, 50,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 15,  3 );
        }
        else if( territory == World::TerritoryNATO )
        {
            SetStateTiming( unit, 0, 55,  5, 10 );
            SetStateTiming( unit, 1, 55,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 15,  2 );
        }
        else if( territory == World::TerritoryRussia )
        {
            SetStateTiming( unit, 0, 65, 10, 10 );
            SetStateTiming( unit, 1, 60,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 20,  2 );
        }
        else if( territory == World::TerritoryChina )
        {
            SetStateTiming( unit, 0, 50,  5, 10 );
            SetStateTiming( unit, 1, 50,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 15,  2 );
        }
    }

    // -----------------------------------------------------------------------
    // BattleShip3 (FG) — Independence / FREMM / Grigorovich / Type 054
    // -----------------------------------------------------------------------
    else if( type == WorldObject::TypeBattleShip3 )
    {
        if( territory == World::TerritoryUSA )
        {
            unit->m_stealthType = 80;
            if( mo ) { mo->SetSpeed( Fixed::Hundredths(4) / gs ); mo->SetTurnRate( Fixed::Hundredths(2) / gs ); }
            SetStateTiming( unit, 0, 55,  5,  3 );
            SetStateTiming( unit, 1, 60,  5,  3 );
            SetStateTiming( unit, 2, 100, 25,  3 );
            SetStateTiming( unit, 3, 60, 20,  3 );
        }
        else if( territory == World::TerritoryNATO )
        {
            unit->m_life = 3;
            SetStateTiming( unit, 0, 55,  5, 10 );
            SetStateTiming( unit, 1, 55,  5, 25 );
            SetStateTiming( unit, 2, 100, 25, 10 );
            SetStateTiming( unit, 3, 60, 15,  2 );
        }
        else if( territory == World::TerritoryRussia )
        {
            SetStateTiming( unit, 0, 65,  5, 10 );
            SetStateTiming( unit, 1, 60,  5, 25 );
            SetStateTiming( unit, 2, 110, 25, 10 );
            SetStateTiming( unit, 3, 60, 20,  2 );
        }
        else if( territory == World::TerritoryChina )
        {
            SetStateTiming( unit, 0, 60,  5,  5 );
            SetStateTiming( unit, 1, 60,  5, 20 );
            SetStateTiming( unit, 2, 110, 25, 10 );
            SetStateTiming( unit, 3, 60, 20,  2 );
        }
    }

    // -----------------------------------------------------------------------
    // Carrier (CV) — de Gaulle / Kuznetsov
    // -----------------------------------------------------------------------
    else if( type == WorldObject::TypeCarrier )
    {
        if( territory == World::TerritoryNATO )
        {
            unit->m_life = 4;
        }
        else if( territory == World::TerritoryRussia )
        {
            unit->m_life = 2;
            if( mo ) mo->SetSpeed( Fixed::Hundredths(2) / gs );
            SetAllStatesPrepareReload( unit, 70, 35 );
        }
    }

    // -----------------------------------------------------------------------
    // CarrierLight (CVL) — Wasp / Queen / Kuznetsov / Izumo / Canberra
    // -----------------------------------------------------------------------
    else if( type == WorldObject::TypeCarrierLight )
    {
        if( territory == World::TerritoryUSA ||
            territory == World::TerritoryJapan ||
            territory == World::TerritoryAustralia )
        {
            SetAllStatesPrepare( unit, 60 );
        }
        else if( territory == World::TerritoryNATO )
        {
            unit->m_life = 4;
            SetAllStatesPrepare( unit, 60 );
        }
        else if( territory == World::TerritoryRussia ||
                 territory == World::TerritoryChina )
        {
            unit->m_life = 2;
            if( mo ) mo->SetSpeed( Fixed::Hundredths(2) / gs );
        }
    }

    // -----------------------------------------------------------------------
    // CarrierSuper (CVN) — Nimitz
    // -----------------------------------------------------------------------
    else if( type == WorldObject::TypeCarrierSuper )
    {
        if( territory == World::TerritoryUSA )
        {
            SetAllStatesPrepareReload( unit, 50, 20 );
        }
    }
}
