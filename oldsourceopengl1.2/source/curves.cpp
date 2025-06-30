#include "lib/universal_include.h"

#include "curves.h"
#include "lib/debug_utils.h"

Vector3<float> BezierCurve( float u, Vector3<float> const &p1, Vector3<float> const &c1, Vector3<float> const &c2, Vector3<float> const &p2 )
{
    Clamp( u, 0.0f, 1.0f );

    Vector3<float> a = p1 * pow(u,3);
    Vector3<float> b = c1 * 3 * pow(u,2) * (1-u);
    Vector3<float> c = c2 * 3 * u *pow((1-u),2);
    Vector3<float> d = p2 * pow((1-u),3);

    Vector3<float> r = (a+b) + (c+d);

    return r;
}


float BezierCurve( float u, float p1, float c1, float c2, float p2 )
{
    Clamp( u, 0.0f, 1.0f );

    float a = p1 * pow(u,3);
    float b = c1 * 3 * pow(u,2) * (1-u);
    float c = c2 * 3 * u *pow((1-u),2);
    float d = p2 * pow((1-u),3);

    float r = (a+b) + (c+d);

    return r;
}


CurveGenerator::CurveGenerator()
:   m_coefficients(NULL),
    m_committed(false)
{
}

    
void CurveGenerator::AddControlPoint( Vector3<float> const &pos )
{
    m_committed = false;
    if( m_coefficients ) 
    {
        delete m_coefficients;
        m_coefficients = NULL;
    }
        
    m_controlPoints.PutData( pos );
}

    
void CurveGenerator::Commit()
{
    m_committed = true;

    int numControlPoints = m_controlPoints.Size();
    int order = numControlPoints - 1;
    
    //
    // Compute our coefficients
    
    m_coefficients = new int[numControlPoints];

    for( int k=0; k <= order; k++)
    { 
        //compute n! / (k!*(n-k)!)
        m_coefficients[k] = 1;
    
        for( int j = order; j >= k+1; j--)
        {
            m_coefficients[k] *= j;
        }
        
        for( int j = order-k; j>=2; j--)
        {
            m_coefficients[k] /= j;
        }
    }
}
 

void CurveGenerator::Reset()
{
    delete m_coefficients;
    m_coefficients = NULL;

    m_controlPoints.Empty();

    m_committed = false;
}


 
// compute the blending value
float CurveGenerator::CalculateBlendingValue (int order, int k, float fraction)
{ 
    // compute  c[k]*t^k * (1-t)^(n-k)
  
    float bv = m_coefficients[k];
  
    for( int j=1; j<=k; j++)
    {
        bv *= fraction;
    }
    
    for( int j=1; j<=order-k; j++)
    {
        bv *= (1-fraction);
    }
    
    return bv;
}
 
       
Vector3<float> CurveGenerator::CalculatePoint( float fraction )
{
    if( !m_committed )
    {
        AppDebugOut( "CurveGenerator warning : Tried to Calculate Point, but curve is not committed\n" );
        return Vector3<float>::ZeroVector();
    }

    Vector3<float> result;
    
    int order = m_controlPoints.Size() - 1;
    
    for( int k = 0; k <= order; k++)
    {  
        float blend = CalculateBlendingValue( order,k,fraction );
        
        result += m_controlPoints[k] * blend;
    }
    
    return result;
}


// ===========================================================================

JoinedCurveGenerator::JoinedCurveGenerator()
:   m_committed(false),
    m_controlNormals(NULL)
{
}


void JoinedCurveGenerator::AddControlPoint ( Vector3<float> const &pos )
{
    m_committed = false;
    
    if( m_controlNormals )
    {
        delete m_controlNormals;
        m_controlNormals = NULL;
    }
    
    m_controlPoints.PutData(pos);
}


void JoinedCurveGenerator::Commit()
{
    m_committed = true;

    //
    // Remove any points next to each other

    Vector3<float> prevPos = Vector3<float>::ZeroVector();

    for( int i = 0; i < m_controlPoints.Size(); ++i )
    {
        Vector3<float> thisPos = m_controlPoints[i];
    
        if( (thisPos - prevPos).Mag() < 0.1f )
        {
            m_controlPoints.RemoveData(i);
            --i;
        }

        prevPos = thisPos;
    }


    //
    // Calculate control normals

    m_controlNormals = new Vector3<float>[ m_controlPoints.Size() ];
    
    prevPos = Vector3<float>::ZeroVector();
    
    for( int i = 0; i < m_controlPoints.Size(); ++i )
    {
        Vector3<float> thisPos = m_controlPoints[i];
        Vector3<float> thisNormal = ( thisPos - prevPos ).Normalise();
        
        if( m_controlPoints.ValidIndex(i+1) )
        {
            Vector3<float> nextPos = m_controlPoints[i+1];
            Vector3<float> nextNormal = ( nextPos - thisPos ).Normalise();
            thisNormal = (thisNormal + nextNormal) / 2.0f;
        }
        
        float size = ( thisPos - prevPos).Mag() * 0.4f;
        if( prevPos == Vector3<float>::ZeroVector() ) size = 0.0f;

        m_controlNormals[i] = thisNormal * size;
        
        prevPos = thisPos;
    }
}


void JoinedCurveGenerator::Reset()
{
}
    

Vector3<float> JoinedCurveGenerator::CalculatePoint( float fraction )
{
    int numSegments = m_controlPoints.Size() - 1;
    if( numSegments == 0 ) return Vector3<float>::ZeroVector();
    
    float sizePerSeg = 1.0f / (float) numSegments;
    int segment = fraction / sizePerSeg;
    
    int point1Index = segment;
    int point2Index = point1Index + 1;
    
    if( m_controlPoints.ValidIndex(point1Index) &&
        m_controlPoints.ValidIndex(point2Index) )
    {
        Vector3<float> p1 = m_controlPoints[point1Index];
        Vector3<float> p2 = p1 + m_controlNormals[point1Index];
        Vector3<float> p4 = m_controlPoints[point2Index];
        Vector3<float> p3 = p4 - m_controlNormals[point2Index];
        
        float thisFraction = fraction - sizePerSeg * segment;
        thisFraction /= sizePerSeg;
     
        Clamp( thisFraction, 0.0f, 1.0f );
        thisFraction = 1.0f - thisFraction;
        
        return BezierCurve( thisFraction, p1, p2, p3, p4 );
    }
    
	return Vector3<float>::ZeroVector();
}
    
