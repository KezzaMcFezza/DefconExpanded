#include "lib/universal_include.h"
#include "world/aircraft_loads.h"
#include "world/worldobject.h"
#include "world/world.h"

#include <cstring>

// Airbase state indices: 0,1 fighter_light; 2,3 fighter; 4,5 fighter_navy_stealth; 6,7 fighter_stealth; 8,9 bomber; 10,11 bomber_fast; 12,13 bomber_stealth; 14 AEW; 15 Tanker
// Carrier state indices: 0,1 fighter; 2,3 fighter_navy_stealth; 4 AEW

static void SetAirbaseLoad( WorldObject *obj, int fl, int f, int fs, int b, int bf, int bs, int aew, int fns, int tanker = 0 )
{
    if ( obj->m_states.Size() < 15 ) return;
    obj->m_states[0]->m_numTimesPermitted = fl;
    obj->m_states[1]->m_numTimesPermitted = fl;
    obj->m_states[2]->m_numTimesPermitted = f;
    obj->m_states[3]->m_numTimesPermitted = f;
    obj->m_states[4]->m_numTimesPermitted = fns;
    obj->m_states[5]->m_numTimesPermitted = fns;
    obj->m_states[6]->m_numTimesPermitted = fs;
    obj->m_states[7]->m_numTimesPermitted = fs;
    obj->m_states[8]->m_numTimesPermitted = b;
    obj->m_states[9]->m_numTimesPermitted = b;
    obj->m_states[10]->m_numTimesPermitted = bf;
    obj->m_states[11]->m_numTimesPermitted = bf;
    obj->m_states[12]->m_numTimesPermitted = bs;
    obj->m_states[13]->m_numTimesPermitted = bs;
    obj->m_states[14]->m_numTimesPermitted = aew;
    if( obj->m_states.Size() > 15 )
        obj->m_states[15]->m_numTimesPermitted = tanker;
    int totalF = fl + f + fs + fns;
    int totalB = b + bf + bs;
    obj->m_maxFighters = ( totalF > 10 ) ? totalF : 10;
    obj->m_maxBombers = ( totalB > 10 ) ? totalB : 10;
    obj->m_maxAEW = ( aew > 4 ) ? aew : 4;
}

static void SetCarrierLoad( WorldObject *obj, int f, int fns, int aew )
{
    if ( obj->m_states.Size() < 5 ) return;
    obj->m_states[0]->m_numTimesPermitted = f;
    obj->m_states[1]->m_numTimesPermitted = f;
    obj->m_states[2]->m_numTimesPermitted = fns;
    obj->m_states[3]->m_numTimesPermitted = fns;
    obj->m_states[4]->m_numTimesPermitted = aew;
    int total = f + fns;
    obj->m_maxFighters = ( total > 5 ) ? total : 5;
    obj->m_maxAEW = ( aew > 2 ) ? aew : 2;
}

void ApplyTerritoryAircraftLoads( WorldObject *obj, int primaryTerritory )
{
    int t = primaryTerritory;
    int type = obj->m_type;

    // Generic defaults
    if ( type == WorldObject::TypeAirBase || type == WorldObject::TypeAirBase2 || type == WorldObject::TypeAirBase3 )
    {
        SetAirbaseLoad( obj, 2, 2, 0, 0, 0, 0, 0, 0 );  // fighter_light 2, fighter 2
        if ( t == World::TerritoryUSA )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 0, 0, 0, 0, 0, 2, 2, 2 );  // USA Airbase (2 tankers)
            else if ( type == WorldObject::TypeAirBase2 )
                SetAirbaseLoad( obj, 0, 0, 2, 5, 2, 2, 2, 0 );  // USA Airbase2
            else if ( type == WorldObject::TypeAirBase3 )
                SetAirbaseLoad( obj, 0, 2, 0, 0, 0, 0, 2, 2, 2 );  // USA Airbase3 (2 tankers)
        }
        else if ( t == World::TerritoryNATO )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 0, 5, 0, 0, 0, 0, 2, 0 );
            else if ( type == WorldObject::TypeAirBase2 )
                SetAirbaseLoad( obj, 2, 0, 0, 0, 0, 0, 2, 2 );
        }
        else if ( t == World::TerritoryJapan )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 2, 0, 0, 0, 0, 0, 2, 2 );
        }
        else if ( t == World::TerritoryKorea )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 2, 0, 0, 0, 0, 0, 2, 2 );
        }
        else if ( t == World::TerritoryTaiwan )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 0, 0, 0, 0, 0, 2, 0 );  // AEW listed without number, use 2
        }
        else if ( t == World::TerritoryUkraine )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 2, 0, 0, 0, 0, 0, 0 );
        }
        else if ( t == World::TerritoryAustralia )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 0, 5, 0, 0, 0, 0, 2, 2 );
        }
        else if ( t == World::TerritoryRussia )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 2, 0, 0, 2, 3, 0, 2, 0 );
            else if ( type == WorldObject::TypeAirBase2 )
                SetAirbaseLoad( obj, 0, 5, 2, 0, 0, 0, 2, 0 );
        }
        else if ( t == World::TerritoryChina )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 0, 5, 2, 5, 0, 0, 2, 0 );
            else if ( type == WorldObject::TypeAirBase2 )
                SetAirbaseLoad( obj, 5, 5, 0, 0, 0, 0, 0, 2 );
        }
        else if ( t == World::TerritoryIndia )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 2, 0, 0, 0, 0, 2, 0 );
        }
        else if ( t == World::TerritoryEgypt )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 2, 0, 0, 0, 0, 2, 0 );
        }
        else if ( t == World::TerritorySaudi )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 0, 3, 0, 0, 0, 0, 2, 0 );
        }
        else if ( t == World::TerritoryIsrael )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 5, 0, 0, 0, 0, 0, 2, 2 );  // figher_navy_stealth typo in plan
        }
        else if ( t == World::TerritoryIran )
        {
            if ( type == WorldObject::TypeAirBase )
                SetAirbaseLoad( obj, 2, 2, 0, 0, 0, 0, 0, 0 );
        }
    }
    else if ( obj->IsCarrierClass() )
    {
        if ( type == WorldObject::TypeCarrierLHD )
        {
            SetCarrierLoad( obj, 0, 0, 0 );
            obj->m_maxFighters = 0;
            obj->m_maxAEW = 0;
        }
        else
        {
        SetCarrierLoad( obj, 2, 0, 0 );  // Generic: fighter 2
        if ( t == World::TerritoryUSA )
        {
            if ( type == WorldObject::TypeCarrierSuper )
                SetCarrierLoad( obj, 4, 2, 2 );
            else if ( type == WorldObject::TypeCarrier || type == WorldObject::TypeCarrierLight )
                SetCarrierLoad( obj, 0, 2, 0 );
        }
        else if ( t == World::TerritoryNATO )
        {
            if ( type == WorldObject::TypeCarrier )
                SetCarrierLoad( obj, 3, 0, 1 );
            else if ( type == WorldObject::TypeCarrierLight )
                SetCarrierLoad( obj, 0, 3, 0 );
        }
        else if ( t == World::TerritoryJapan )
        {
            if ( type == WorldObject::TypeCarrier || type == WorldObject::TypeCarrierLight )
                SetCarrierLoad( obj, 0, 2, 0 );
        }
        else if ( t == World::TerritoryAustralia )
        {
            if ( type == WorldObject::TypeCarrier || type == WorldObject::TypeCarrierLight )
                SetCarrierLoad( obj, 0, 2, 0 );
        }
        else if ( t == World::TerritoryRussia )
        {
            if ( type == WorldObject::TypeCarrier )
                SetCarrierLoad( obj, 3, 0, 0 );
        }
        else if ( t == World::TerritoryChina )
        {
            if ( type == WorldObject::TypeCarrierSuper )
                SetCarrierLoad( obj, 3, 2, 2 );
            else if ( type == WorldObject::TypeCarrier || type == WorldObject::TypeCarrierLight )
                SetCarrierLoad( obj, 3, 0, 0 );
        }
        else if ( t == World::TerritoryIndia )
        {
            if ( type == WorldObject::TypeCarrier )
                SetCarrierLoad( obj, 3, 0, 0 );
        }
        }
    }
}
