#ifndef INCLUDED_VECTOR2_H
#define INCLUDED_VECTOR2_H

template <class T> class Vector3;

class Vector2
{
private:
	bool Compare(Vector2 const &b) const;

public:
	float x, y;

	constexpr Vector2();
	constexpr Vector2(Vector3<float> const &);
	constexpr Vector2(float _x, float _y);

	constexpr void	Zero();
    constexpr void	Set	(float _x, float _y);

	constexpr float	operator ^ (Vector2 const &b) const;	// Cross product
	constexpr float   operator * (Vector2 const &b) const;	// Dot product

	constexpr Vector2 operator - () const;					// Negate
	constexpr Vector2 operator + (Vector2 const &b) const;
	constexpr Vector2 operator - (Vector2 const &b) const;
	constexpr Vector2 operator * (float const b) const;		// Scale
	constexpr Vector2 operator / (float const b) const;

    void	operator = (Vector2 const &b);
    void	operator = (Vector3<float> const &b);
	void	operator *= (float const b);
	void	operator /= (float const b);
	void	operator += (Vector2 const &b);
	void	operator -= (Vector2 const &b);

	constexpr bool operator == (Vector2 const &b) const;		// Uses FLT_EPSILON
	constexpr bool operator != (Vector2 const &b) const;		// Uses FLT_EPSILON

	Vector2 const &Normalise();
	void	SetLength		(float _len);
	
    float	Mag				() const;
	constexpr float	MagSquared		() const;

	float  *GetData			();
};


// Operator * between float and Vector2
constexpr inline Vector2 operator * (	float _scale, Vector2 const &_v )
{
	return _v * _scale;
}


#endif
