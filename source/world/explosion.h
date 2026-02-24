
#ifndef _included_explosion_h
#define _included_explosion_h

#include "world/worldobject.h"


class Explosion : public WorldObject
{
public:
    Fixed m_initialIntensity;
    Fixed m_intensity;
    bool  m_underWater;
    bool  m_nuclear;       // true only for nuke impacts; used for impact symbol and damage
    int   m_targetTeamId;

public:
    Explosion();

    void Render2D();
    void Render3D();
    void RenderImpactSymbol();   // Nuclear only; call in separate overlay pass after explosions for correct layering

    bool Update();

    bool IsActionable() { return false; }

};


#endif
