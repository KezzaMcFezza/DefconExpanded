#ifndef _INCLUDED_MATRIX34_H
#define _INCLUDED_MATRIX34_H


#include <stdlib.h>

#include "matrix33.h"
#include "vector3.h"



class Matrix34
{
public:
	Vector3<float> r;
    Vector3<float> u;
    Vector3<float> f;
    Vector3<float> pos;

public:

#ifdef TARGET_OS_MACOSX
	constexpr Matrix34 ();
#else
	Matrix34 ();
#endif

	Matrix34 ( int ignored );
	constexpr Matrix34 ( Matrix34 const &other ) : r(other.r), u(other.u), f(other.f), pos(other.pos) {}
	constexpr Matrix34 ( Matrix33 const &orientation, Vector3<float> const &pos ) : r(orientation.r), u(orientation.u), f(orientation.f), pos(pos) {}
	Matrix34 ( Vector3<float> const &f, Vector3<float> const &u, Vector3<float> const &pos );
	Matrix34 ( float yaw, float dive, float roll );

	void OrientRU ( Vector3<float> const &r, Vector3<float> const &u );
	void OrientRF ( Vector3<float> const &r, Vector3<float> const &f );
	void OrientUF ( Vector3<float> const &u, Vector3<float> const &f );
	void OrientUR ( Vector3<float> const &u, Vector3<float> const &r );
	void OrientFR ( Vector3<float> const &f, Vector3<float> const &r );
	void OrientFU ( Vector3<float> const &f, Vector3<float> const &u );

	Matrix34 const &RotateAroundR    ( float angle );
	Matrix34 const &RotateAroundU    ( float angle );
	Matrix34 const &RotateAroundF    ( float angle );
	Matrix34 const &RotateAroundX    ( float angle );
	Matrix34 const &RotateAroundY    ( float angle );
	Matrix34 const &RotateAroundZ    ( float angle );
	Matrix34 const &RotateAround     ( Vector3<float> const &rotationAxis );
	Matrix34 const &RotateAround     ( Vector3<float> const &norm, float angle );
	Matrix34 const &FastRotateAround ( Vector3<float> const &norm, float angle );
	
    Matrix34 const &Scale            ( float factor );

	Matrix34 const &Transpose();
    Matrix34 const &Invert();
	
	Vector3<float>			DecomposeToYDR	        () const;	                                            //x = dive, y = yaw, z = roll
	void			DecomposeToYDR	        ( float *y, float *d, float *r ) const;
    float           *ConvertToOpenGLFormat  () const;
	void			SetToIdentity	        ();
    bool            IsNormalised            () const;
    void			Normalise		        ();
    constexpr Matrix33        GetOr                   () const {
		return Matrix33(r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z);
	}
    constexpr Vector3<float>			InverseMultiplyVector   (Vector3<float> const &s) const {
		Vector3<float> v = s - pos;
		return Vector3<float>(r.x*v.x + u.x*v.y + f.x*v.z,
				   r.y*v.x + u.y*v.y + f.y*v.z,
				   r.z*v.x + u.z*v.y + f.z*v.z);
	}


    void            WriteToDebugStream();
    void            Test();

    //
	// Operators

	bool            operator == ( Matrix34 const &b ) const {
		return (r == b.r && u == b.u && f == b.f && pos == b.pos);
	}
	Matrix34 const &operator *= ( Matrix34 const &o );
	constexpr Matrix34		operator *  ( Matrix34 const &b ) const {
		Matrix34 result;
		result.r.x  =  r.x * b.r.x  +  r.y * b.u.x  +  r.z * b.f.x;
		result.r.y  =  r.x * b.r.y  +  r.y * b.u.y  +  r.z * b.f.y;
		result.r.z  =  r.x * b.r.z  +  r.y * b.u.z  +  r.z * b.f.z;
		result.u.x  =  u.x * b.r.x  +  u.y * b.u.x  +  u.z * b.f.x;
		result.u.y  =  u.x * b.r.y  +  u.y * b.u.y  +  u.z * b.f.y;
		result.u.z  =  u.x * b.r.z  +  u.y * b.u.z  +  u.z * b.f.z;
		result.f.x  =  f.x * b.r.x  +  f.y * b.u.x  +  f.z * b.f.x;
		result.f.y  =  f.x * b.r.y  +  f.y * b.u.y  +  f.z * b.f.y;
		result.f.z  =  f.x * b.r.z  +  f.y * b.u.z  +  f.z * b.f.z;
		result.pos.x  =  pos.x * b.r.x  +  pos.y * b.u.x  +  pos.z * b.f.x  +  b.pos.x;
		result.pos.y  =  pos.x * b.r.y  +  pos.y * b.u.y  +  pos.z * b.f.y  +  b.pos.y;
		result.pos.z  =  pos.x * b.r.z  +  pos.y * b.u.z  +  pos.z * b.f.z  +  b.pos.z;
		return result;
	}
};


extern Matrix34 const g_identityMatrix34;


//
// ============================================================================



// Operator * between matrix34 and vector3
constexpr inline Vector3<float> operator * ( Matrix34 const &m, Vector3<float> const &v )
{
	return Vector3<float>(m.r.x*v.x + m.u.x*v.y + m.f.x*v.z + m.pos.x,
    			   m.r.y*v.x + m.u.y*v.y + m.f.y*v.z + m.pos.y,
			   m.r.z*v.x + m.u.z*v.y + m.f.z*v.z + m.pos.z);
}


// Operator * between vector3 and matrix34
constexpr inline Vector3<float> operator * ( Vector3<float> const &v, Matrix34 const &m )
{
	return Vector3<float>(m.r.x*v.x + m.u.x*v.y + m.f.x*v.z + m.pos.x,
    			   m.r.y*v.x + m.u.y*v.y + m.f.y*v.z + m.pos.y,
			   m.r.z*v.x + m.u.z*v.y + m.f.z*v.z + m.pos.z);
}


#endif
