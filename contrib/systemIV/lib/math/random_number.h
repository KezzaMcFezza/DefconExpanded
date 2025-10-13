#ifndef _included_randomnumbers_h
#define _included_randomnumbers_h

#include "lib/math/fixed.h"

/*
 * =============================================
 *	Basic random number generators
 *
 */

#define APP_RAND_MAX    32767


void    AppSeedRandom   (unsigned int seed);                                
int     AppRandom       ();                                                 // 0 <= result < APP_RAND_MAX


float   frand           (float range = 1.0f);                               // 0 <= result < range
float   sfrand          (float range = 1.0f);                               // -range < result < range




/*
 * =============================================
 *	Network Syncronised random number generators
 *  
 *  These must only be called in deterministic
 *  net-safe code.
 *
 *  Don't call the underscored functions directly -
 *  instead call syncrand/syncfrand and the preprocessor
 *  will do the rest.
 *
 */


void            syncrandseed    ( unsigned long seed = 0 );
unsigned long   _syncrand       ();
Fixed           _syncfrand      ( Fixed range = 1 );                     // 0 <= result < range
Fixed           _syncsfrand     ( Fixed range = 1 );                     // -range < result < range

// Added by Robin for AI API
// get and set seed state
unsigned long   getSeed         ( int index );
void            setSeed         ( int index, unsigned long value );



/*
 * ==============================================
 *  Debug versions of those Net sync'd generators
 *
 *  Handy for tracking down Net Syncronisation errors
 *  To use : #define TRACK_SYNC_RAND in universal_include.h
 *         : then call DumpSyncRandLog when you know its gone wrong
 *
 *
 */

unsigned long   DebugSyncRand   (const char *_file, int _line );
Fixed           DebugSyncFrand  (const char *_file, int _line, Fixed range);
Fixed           DebugSyncsFrand (const char *_file, int _line, Fixed range);
void            DumpSyncRandLog (const char *_filename);
void 			FlushSyncRandLog ();


#ifdef TRACK_SYNC_RAND
    #define         syncrand()      DebugSyncRand(__FILE__,__LINE__)
    #define         syncfrand(x)    DebugSyncFrand(__FILE__,__LINE__,x)
    #define         syncsfrand(x)   DebugSyncsFrand(__FILE__,__LINE__,x)
    #define         RandomNormalNumber(m,r)  DebugRandomNormalNumber(__FILE__,__LINE__,m,r)
    #define         RandomApplyVariance(n,v) DebugRandomApplyVariance(__FILE__,__LINE__,n,v)
#else
    #define        syncrand          _syncrand
    #define        syncfrand(x)      _syncfrand(x)
    #define        syncsfrand(x)     _syncsfrand(x)
    #define        RandomNormalNumber(m,r) _RandomNormalNumber(m,r)
    #define        RandomApplyVariance(n,v) _RandomApplyVariance(n,v)
#endif


// Temporary until Net Sync problem is fixed

void            SyncRandLog     (const char *_message, ...);

/*
 * =============================================
 *	Statistics based random number generatores
 */


Fixed       _RandomNormalNumber  ( Fixed _mean, Fixed _range );	            // result ~ N ( mean, range/3 ),
                                                                            // mean - range < result < mean + range	
  
Fixed       _RandomApplyVariance ( Fixed _num, Fixed _variance );			// Applies +-percentage Normally distributed variance to num
// variance should be in fractional form eg 1.0, 0.5 etc                
                                                                            // _num - _variance * _num < result < _num + _variance * _num

Fixed       DebugRandomNormalNumber  ( const char *_file, int _line, Fixed _mean, Fixed _range );
Fixed       DebugRandomApplyVariance ( const char *_file, int _line, Fixed _num, Fixed _variance );


#endif
