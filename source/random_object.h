#ifndef _included_randomobject_h
#define _included_randomobject_h

/*
 *      Random Number generator
 *      =======================
 *
 *      Use g_random if you just want some totally random values
 *      Or instantiate your own Random if you want deterministic random 
 *
 *
 */



/* ============================================================================
 * Random base class
 *
 * This is a virtual class, don't use this directly
 *
 */

class Random
{
protected:
    unsigned int    m_randMax;

public:
    Random();
	virtual ~Random();

    virtual void    Seed            ( unsigned long s ) = 0;
    virtual long    Next            () = 0;
    
    unsigned int    rand            ();                                         // 0 <= result < m_randMax
    float           frand           ( float range = 1.0f );                     // 0 <= result < range
    float           sfrand          ( float range = 1.0f );                     // -range < result < range
    float           nrand           ( float mean, float range );	            // result ~ N ( mean, range/3 ), mean - range < result < mean + range	
                                                                                
    float           ApplyVariance   ( float num, float variance );			    // Applies +-percentage Normally distributed variance to num
                                                                                // variance should be in fractional form eg 1.0, 0.5 etc                
                                                                                // _num - _variance * _num < result < _num + _variance * _num

    void            Test();                                                     // Runs lots of tests, outputs to AppDebugOut
    
    static int      MakeRandomSeed  ();                                         // Make a good random seed, eg GetTickCount
};





/* ============================================================================
 * Implementation based on system rand()
 *
 * Very very fast
 *
 */


class FastRandom : public Random
{
protected:
    long m_holdRand;

public:
    FastRandom();

    void Seed( unsigned long s );
    long Next();

};

#endif
