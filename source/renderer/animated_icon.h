
#ifndef _included_animatedicon_h
#define _included_animatedicon_h

#include "lib/tosser/bounded_array.h"

class AnimatedIcon
{
public:
    float   m_startTime;
    int     m_animationType;
    float   m_longitude;
    float   m_latitude;
    int     m_fromObjectId;

    BoundedArray   <bool>m_visible;

public:
    AnimatedIcon();
    virtual ~AnimatedIcon() {}
    virtual void Update() {}
    virtual void Render2D() {}
    virtual void Render3D() {}
    virtual bool IsFinished() { return true; }
};


// ============================================================================


class ActionMarker : public AnimatedIcon
{
public:
    bool    m_combatTarget;
    int     m_targetType;
    float   m_size;
    
public:
    ActionMarker();
    void Update();
    void Render2D();
    void Render3D();
    bool IsFinished();
};


// ============================================================================


class SonarPing : public AnimatedIcon
{
public:
    int m_teamId;
    float m_size;
    
public:
    SonarPing();
    void Update();
    void Render2D();
    void Render3D();
    bool IsFinished();
};



// ============================================================================


class NukePointer : public AnimatedIcon 
{
public:
    float m_lifeTime;
    int   m_targetId;
    int   m_mode;
    bool  m_offScreen;
    bool  m_seen;

public:
    NukePointer();
    
    void Merge();               // Look for nearby nuke pointers, merge in
    void Update();
    void Render2D();
    void Render3D();
    bool IsFinished();
};



#endif
