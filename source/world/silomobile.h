
#ifndef _included_silomobile_h
#define _included_silomobile_h

#include "world/silo.h"


/** Mobile silo: less health, reduced radar in launch state, stealth. */
class SiloMobile : public Silo
{
public:
    SiloMobile();

    void   SetState( int state ) override;
    Image *GetBmpImage( int state ) override;
};


#endif
