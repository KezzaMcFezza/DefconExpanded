#ifndef _included_subg_h
#define _included_subg_h

#include "world/subc.h"


class SubG : public SubC
{
public:

    SubG();
    bool Update() override;

    int GetAttackOdds( int _defenderType );
};


#endif
