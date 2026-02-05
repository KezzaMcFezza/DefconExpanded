#ifndef _included_earthdata_h
#define _included_earthdata_h

class Island;
class City;

#include "lib/tosser/llist.h"
#include "lib/math/vector3.h"


class EarthData
{
public:
    LList           <Island *>          m_islands;
    LList           <Island *>          m_borders;
    LList           <Island *>          m_gridlinesLow;
    LList           <Island *>          m_gridlinesMedium;
    LList           <Island *>          m_gridlinesHigh;
    LList           <City *>            m_cities;

public:
    EarthData();
    ~EarthData();

    void Initialise();

    void LoadBorders();
    void LoadCities();
    void LoadCoastlines();
    void LoadGridlines();
    void ClearGridlines();
    void CalculateAndSetBufferSizes();
    
private:
    void LoadOneGridlineFile( const char *filename, LList<Island *> &outList );
};



// ============================================================================

class Island
{
public:
    ~Island();

    LList<Vector3<float> *> m_points;

};



#endif
