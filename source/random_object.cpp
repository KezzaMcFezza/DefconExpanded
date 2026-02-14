#include "lib/universal_include.h"

#include "lib/math/math_utils.h"
#include "random_object.h"

Random::Random()
:   m_randMax(0)
{
}

Random::~Random()
{
}


unsigned int Random::rand()
{
    unsigned long next = Next();
    return (unsigned int) next;
}


float Random::frand( float range )
{
    unsigned long next = Next();

    return range * (next/(float)m_randMax);
}


float Random::sfrand( float range )
{
    unsigned long next = Next();

    return 2.0f * range * ((next/(float)m_randMax) - 0.5f);
}


float Random::nrand( float mean, float range )
{
    float s = 0;

    for ( int i = 0; i < 12; ++i )
    {
        s += frand(1.0f);
    }

    s = ( s-6.0f ) * ( range/3.0f ) + mean;

    Clamp( s, mean - range, mean + range );

    return s;
}


float Random::ApplyVariance( float num, float variance )
{
    float variancefactor = 1.0f + nrand( 0.0f, variance );
    num *= variancefactor;								

    return num;
}

int Random::MakeRandomSeed()
{
#ifdef TARGET_MSVC
    return (int) GetTickCount();
#else
    return (int) time(NULL);
#endif
}



// ============================================================================
// Fast Random Implementation

FastRandom::FastRandom()
:   Random(),
    m_holdRand(1L)
{
    m_randMax = 32768;
}


void FastRandom::Seed( unsigned long s )
{
    m_holdRand = (long)s;
}


long FastRandom::Next()
{
    return (((m_holdRand = m_holdRand * 214013L + 2531011L) >> 16) & 0x7fff);
}
