#ifndef _included_curves_h
#define _included_curves_h

/*
 * Curves
 *
 *  Generalised curve-generation functions
 *
 *  The control points should be in 'visit order', ie the order the curve will pass through them
 *
 */

#include "lib/math/vector3.h"
#include "lib/tosser/llist.h"
#include "lib/tosser/darray.h"

// ============================================================================
// Bezier Curve Function
//
// Calculates 3rd Degree Polynomial Based On four control points
// and A single variable (u) Which is in range 0 - 1


Vector3<float>     BezierCurve     ( float u, 
                              Vector3<float> const &p1, 
                              Vector3<float> const &c1, 
                              Vector3<float> const &c2, 
                              Vector3<float> const &p2 );


float       BezierCurve     ( float u, 
                              float p1,
                              float c1, 
                              float c2, 
                              float p2 );



// ============================================================================
// Curve Generator class
// Generalised Bezier curve generator 
// Will generate curves with any number of control points
// The curve will touch the first and last points only, and will attempt to smoothly pass in the direction of all others


class CurveGenerator
{
public:
    DArray       < Vector3<float> > m_controlPoints;

protected:
    int         *m_coefficients;
    bool        m_committed;

    float       CalculateBlendingValue (int order, int k, float fraction);

public:
    CurveGenerator();
        
    void        AddControlPoint ( Vector3<float> const &pos );
    void        Commit          ();
    void        Reset           ();    

    Vector3<float>     CalculatePoint  ( float fraction );
};




// ============================================================================
// Joined Curve Generator class
// Generalised Bezier curve generator 
// Will generate curves with any number of control points
// The curve will pass through every control point, with smooth curves inbetween each


class JoinedCurveGenerator
{
public:
    DArray       < Vector3<float> > m_controlPoints;
    Vector3<float>     *m_controlNormals;
    
protected:
    bool        m_committed;
    
public:
    JoinedCurveGenerator();

    void        AddControlPoint ( Vector3<float> const &pos );
    void        Commit          ();
    void        Reset           ();    

    Vector3<float>     CalculatePoint  ( float fraction );    
};




#endif
