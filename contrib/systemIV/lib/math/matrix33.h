#ifndef MATRIX33_H
#define MATRIX33_H

#include "vector3.h"


class Matrix33
{
public:
    Vector3<float> f;
    Vector3<float> u;
    Vector3<float> r;

public:
	constexpr Matrix33 () : f(0.0f, 0.0f, 0.0f), u(0.0f, 0.0f, 0.0f), r(0.0f, 0.0f, 0.0f) {}
	Matrix33 ( int ignored );
	constexpr Matrix33 ( Matrix33 const &other ) : f(other.f), u(other.u), r(other.r) {}
	constexpr Matrix33 ( Vector3<float> const &f, Vector3<float> const &u ) : f(f), u(u), r(u ^ f) {}
	Matrix33 ( float yaw, float dive, float roll );
	constexpr Matrix33 ( float rx, float ry, float rz, float ux, float uy, float uz, float fx, float fy, float fz ) : f(fx, fy, fz), u(ux, uy, uz), r(rx, ry, rz) {}
	
	void OrientRU ( Vector3<float> const &r, Vector3<float> const &u );
	void OrientRF ( Vector3<float> const &r, Vector3<float> const &f );
	void OrientUF ( Vector3<float> const &u, Vector3<float> const &f );
	void OrientUR ( Vector3<float> const &u, Vector3<float> const &r );
	void OrientFR ( Vector3<float> const &f, Vector3<float> const &r );
	void OrientFU ( Vector3<float> const &f, Vector3<float> const &u );

	Matrix33 const &RotateAroundR    ( float angle );
	Matrix33 const &RotateAroundU    ( float angle );
	Matrix33 const &RotateAroundF    ( float angle );
	Matrix33 const &RotateAroundX    ( float angle );
	Matrix33 const &RotateAroundY    ( float angle );
	Matrix33 const &RotateAroundZ    ( float angle );
	Matrix33 const &RotateAround     ( Vector3<float> const &rotationAxis );
	Matrix33 const &RotateAround     ( Vector3<float> const &norm, float angle );
	Matrix33 const &FastRotateAround ( Vector3<float> const &norm, float angle );
	
    Matrix33 const &Transpose();
	Matrix33 const &Invert();
	
    
    Vector3<float>     DecomposeToYDR          () const;	                                //x = dive, y = yaw, z = roll
	void        DecomposeToYDR          ( float *y, float *d, float *r ) const;
	void        SetToIdentity           ();
	void        Normalise               ();
	constexpr Vector3<float> InverseMultiplyVector   (Vector3<float> const &v) const {
		return Vector3<float>(r.x*v.x + u.x*v.y + f.x*v.z,
				   r.y*v.x + u.y*v.y + f.y*v.z,
				   r.z*v.x + u.z*v.y + f.z*v.z);
	}
    float          *ConvertToOpenGLFormat  (Vector3<float> const *pos = NULL);

	void        OutputToDebugStream     ();

    //
	// Operators

	Matrix33 const &operator =  ( Matrix33 const &o );
    bool            operator == ( Matrix33 const &b ) const {
		return (r == b.r && u == b.u && f == b.f);
	}
	Matrix33 const &operator *= ( Matrix33 const &o );
	constexpr Matrix33		operator *  ( Matrix33 const &b ) const {
		return Matrix33(
			r.x*b.r.x + r.y*b.u.x + r.z*b.f.x,
			r.x*b.r.y + r.y*b.u.y + r.z*b.f.y,
			r.x*b.r.z + r.y*b.u.z + r.z*b.f.z,
			u.x*b.r.x + u.y*b.u.x + u.z*b.f.x,
			u.x*b.r.y + u.y*b.u.y + u.z*b.f.y,
			u.x*b.r.z + u.y*b.u.z + u.z*b.f.z,
			f.x*b.r.x + f.y*b.u.x + f.z*b.f.x,
			f.x*b.r.y + f.y*b.u.y + f.z*b.f.y,
			f.x*b.r.z + f.y*b.u.z + f.z*b.f.z);
	}
};


extern Matrix33 const g_identityMatrix33;



//
// ============================================================================


// Operator * between matrix33 and vector3
constexpr inline Vector3<float> operator * ( Matrix33 const &_m, Vector3<float> const &_v )
{
	return Vector3<float>(_m.r.x * _v.x + _m.r.y * _v.y + _m.r.z * _v.z,
			   _m.u.x * _v.x + _m.u.y * _v.y + _m.u.z * _v.z,
			   _m.f.x * _v.x + _m.f.y * _v.y + _m.f.z * _v.z);
}


// Operator * between vector3 and matrix33
constexpr inline Vector3<float> operator * (	Vector3<float> const & _v, Matrix33 const &_m )
{
	return Vector3<float>(_m.r.x * _v.x + _m.r.y * _v.y + _m.r.z * _v.z,
			   _m.u.x * _v.x + _m.u.y * _v.y + _m.u.z * _v.z,
			   _m.f.x * _v.x + _m.f.y * _v.y + _m.f.z * _v.z);
}


#endif

