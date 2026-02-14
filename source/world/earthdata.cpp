#include "lib/universal_include.h"

#include "lib/filesys/file_system.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/string_utils.h"
#include "lib/math/vector3.h"
#include "lib/hi_res_time.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render2d/megavbo/megavbo_2d.h"
#include "lib/render3d/megavbo/megavbo_3d.h"

#include "city.h"
#include "earthdata.h"

#include "app/modsystem.h"


EarthData::EarthData()
{
}


EarthData::~EarthData()
{
    m_islands.EmptyAndDelete();
    m_borders.EmptyAndDelete();
    m_gridlinesLow.EmptyAndDelete();
    m_gridlinesMedium.EmptyAndDelete();
    m_gridlinesHigh.EmptyAndDelete();
    m_cities.EmptyAndDelete();
}


void EarthData::Initialise()
{
    LoadCoastlines();
    LoadBorders();

    if (g_modSystem && g_modSystem->GeographyModsContainGridlines())
    {
        LoadGridlines();
    }
    else if (g_modSystem)
    {
        ClearGridlines();
    }
    else
    {
        LoadGridlines();
    }

    CalculateAndSetBufferSizes();
    LoadCities();
}


void EarthData::LoadBorders()
{
    double startTime = GetHighResTime();
    
    m_borders.EmptyAndDelete();

    int numIslands = 0;    
    std::unique_ptr<Island> island;

    TextReader *international = g_fileSystem->GetTextReader( "data/earth/international.dat" );
    AppAssert( international && international->IsOpen() );

    while( international->ReadLine() )
    {
        char *line = international->GetRestOfLine();        
        if( line[0] == 'b' )
        {
            if( island.get() )
            {
                m_borders.PutData( island.release() );
                ++numIslands;
            }
            island.reset( new Island() );
        }
        else
        {
            float longitude, latitude;
            sscanf( line, "%f %f", &longitude, &latitude );
            island->m_points.PutData( new Vector3<float>( longitude, latitude, 0.0f ) );
        }
    }
    
    delete international;

    double totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing International data (%d islands) : %dms\n", numIslands, int( totalTime * 1000.0f ) );
}


void EarthData::LoadCities()
{
    float startTime = GetHighResTime();

    m_cities.EmptyAndDelete();

    TextReader *cities = g_fileSystem->GetTextReader( "data/earth/cities.dat" );
    AppAssert( cities && cities->IsOpen() );
    
    int numCities = 0;
    
    while( cities->ReadLine() )
    {
        char *line = cities->GetRestOfLine();

        if( !line || strlen(line) == 0 )
        {
            continue;
        }

        char name[256];
        char country[256];
        float latitude, longitude;
        int population;
        int capital;
        
        strncpy( name, line, 40 );
        for( int i = 39; i >= 0; --i )
        {
            if( name[i] != ' ' ) 
            {
                name[i+1] = '\x0';
                break;
            }
        }

        strncpy( country, line+41, 40 );
        for( int i = 39; i >= 0; --i )
        {
            if( country[i] != ' ' )
            {
                country[i+1] = '\x0';
                break;
            }
        }

        sscanf( line+82, "%f %f %d %d", &longitude, &latitude, &population, &capital );

        City *city = new City( strupr(name) );
        city->m_country = newStr( strupr(country) );
        city->m_longitude = Fixed::FromDouble(longitude);
        city->m_latitude = Fixed::FromDouble(latitude);
        city->m_population = population;
        city->m_capital = capital;         
        city->SetRadarRange( Fixed::FromDouble(sqrtf( sqrtf(city->m_population) ) / 4.0f) );

        m_cities.PutData( city );
        ++numCities;
    }
    
    delete cities;

    float totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing City data (%d cities) : %dms\n", numCities, int( totalTime * 1000.0f ) );
}


void EarthData::LoadCoastlines()
{
    double startTime = GetHighResTime();

    m_islands.EmptyAndDelete();

    int numIslands = 0;

    char coastFile[1024];

    strcpy(coastFile, "data/earth/coastlines.dat");

    TextReader *coastlines = g_fileSystem->GetTextReader( coastFile );
    AppAssert( coastlines && coastlines->IsOpen() );
    std::unique_ptr<Island> island;
    
    while( coastlines->ReadLine() )
    {        
        char *line = coastlines->GetRestOfLine();
        if( line[0] == 'b' )
        {
            if( island.get() )
            {           
                m_islands.PutData( island.release() );
            }
            island.reset( new Island() );
            ++numIslands;
        }
        else
        {
            float longitude, latitude;
            sscanf( line, "%f %f", &longitude, &latitude );
            island->m_points.PutData( new Vector3<float>( longitude, latitude, 0.0f ) );
        }
    }

    delete coastlines;

    double totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing Coastline data (%d islands) : %dms\n", numIslands, int( totalTime * 1000.0f ) );
}


void EarthData::LoadOneGridlineFile( const char *filename, LList<Island *> &outList )
{
    double startTime = GetHighResTime();

    outList.EmptyAndDelete();

    int numSegments = 0;

    std::unique_ptr<Island> segment;

    TextReader *reader = g_fileSystem->GetTextReader( filename );
    if ( !reader || !reader->IsOpen() )
    {
        if ( reader )
        {
            delete reader;
        }
        return;
    }

    while ( reader->ReadLine() )
    {
        char *line = reader->GetRestOfLine();
        if ( !line )
        {
            continue;
        }
        if ( line[0] == 'b' )
        {
            if ( segment.get() )
            {
                outList.PutData( segment.release() );
                ++numSegments;
            }
            segment.reset( new Island() );
        }
        else
        {
            float longitude, latitude;
            if ( sscanf( line, "%f %f", &longitude, &latitude ) >= 2 )
                segment->m_points.PutData( new Vector3<float>( longitude, latitude, 0.0f ) );
        }
    }

    if ( segment.get() )
    {
        outList.PutData( segment.release() );
        ++numSegments;
    }

    delete reader;

    double totalTime = GetHighResTime() - startTime;
    AppDebugOut( "Parsing Gridline data (%d segments) : %dms\n", numSegments, int( totalTime * 1000.0f ) );
}

void EarthData::LoadGridlines()
{
    LoadOneGridlineFile( "data/earth/gridlines_low.dat", m_gridlinesLow );
    LoadOneGridlineFile( "data/earth/gridlines_medium.dat", m_gridlinesMedium );
    LoadOneGridlineFile( "data/earth/gridlines_high.dat", m_gridlinesHigh );
}

void EarthData::ClearGridlines()
{
    m_gridlinesLow.EmptyAndDelete();
    m_gridlinesMedium.EmptyAndDelete();
    m_gridlinesHigh.EmptyAndDelete();
}


// ================================================================================

void EarthData::CalculateAndSetBufferSizes()
{
    int totalVertices = 0;
    int totalIndices = 0;
    
    //
    // count vertices and indices for coastlines

    for (int i = 0; i < m_islands.Size(); ++i) {
        Island* island = m_islands[i];
        int pointCount = island->m_points.Size();
        if (pointCount >= 2) {
            totalVertices += pointCount;
            totalIndices += pointCount + 1;
        }
    }
    
    //
    // count vertices and indices for borders

    for (int i = 0; i < m_borders.Size(); ++i) {
        Island* border = m_borders[i];
        int pointCount = border->m_points.Size();
        if (pointCount >= 2) {
            totalVertices += pointCount;
            totalIndices += pointCount + 1;
        }
    }
    
    //
    // set buffer sizes for both 2D and 3D renderers
    // both renderers use the same data

    if (g_renderer2d) {
        g_megavbo2d->SetMegaVBOBufferSizes(totalVertices, totalIndices, "MapCoastlines");
        g_megavbo2d->SetMegaVBOBufferSizes(totalVertices, totalIndices, "MapBorders");
    }
    if (g_renderer3d) {
        g_megavbo3d->SetMegaVBO3DBufferSizes(totalVertices, totalIndices, "GlobeCoastlines");
        g_megavbo3d->SetMegaVBO3DBufferSizes(totalVertices, totalIndices, "GlobeBorders");
    }
}

// ================================================================================

Island::~Island()
{
    m_points.EmptyAndDelete();
}
