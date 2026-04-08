#ifndef VECTOR_2_H
#define VECTOR_2_H

#include <cmath>

class Vector2
{
public:
	float x, y;

	Vector2(float x, float y) : x(x), y(y) {}
	Vector2(float a) : x(a), y(a) {}
	Vector2(const Vector2& other) : x(other.x), y(other.y) {}
	Vector2() : x(0.0f), y(0.0f) {}

	// Addition operator overload
	Vector2 operator+(const Vector2& rhs) const
	{
		return Vector2(x + rhs.x, y + rhs.y);
	}

	Vector2 operator+(const float rhs) const
	{
		return Vector2(x + rhs, y + rhs);
	}

	Vector2& operator+=(const Vector2& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	Vector2& operator+=(const float rhs)
	{
		x += rhs;
		y += rhs;
		return *this;
	}

	// Subtraction operator overload
	Vector2 operator-(const Vector2& rhs) const
	{
		return Vector2(x - rhs.x, y - rhs.y);
	}

	Vector2 operator-(const float rhs) const
	{
		return Vector2(x - rhs, y - rhs);
	}

	Vector2& operator-=(const Vector2& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	Vector2& operator-=(const float rhs)
	{
		x -= rhs;
		y -= rhs;
		return *this;
	}

	// Multiplication operator overload
	Vector2 operator*(const float rhs) const
	{
		return Vector2(x * rhs, y * rhs);
	}

	Vector2 operator*(const Vector2& rhs) const
	{
		return Vector2(x * rhs.x, y * rhs.y);
	}

	Vector2& operator*=(const float rhs)
	{
		x *= rhs;
		y *= rhs;
		return *this;
	}

	Vector2& operator*=(const Vector2& rhs)
	{
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	// Division operator overload
	Vector2 operator/(const float rhs) const
	{
		return Vector2(x / rhs, y / rhs);
	}

	Vector2 operator/(const Vector2& rhs) const
	{
		return Vector2(x / rhs.x, y / rhs.y);
	}

	Vector2& operator/=(const float rhs)
	{
		x /= rhs;
		y /= rhs;
		return *this;
	}

	Vector2& operator/=(const Vector2& rhs)
	{
		x /= rhs.x;
		y /= rhs.y;
		return *this;
	}

	// Unary negation operator overload
	Vector2 operator-() const
	{
		return Vector2(-x, -y);
	}

	// Equality operators
	bool operator==(const Vector2& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y);
	}

	bool operator!=(const Vector2& rhs) const
	{
		return !(*this == rhs);
	}

	// Element access
	float& operator[](int i)
	{
		switch (i) { case 0: return x; default: return y; }
	}

	// Magnitude
	float Magnitude() const
	{
		return std::hypotf(x, y);
	}

	static float Magnitude(const Vector2& vec)
	{
		return std::hypotf(vec.x, vec.y);
	}

	// Normalize vector
	Vector2 Normalize()
	{
		float mag = Magnitude();
		if (mag > 0)
		{
			x /= mag;
			y /= mag;
		}

		return *this;
	}

	static Vector2 Normalize(const Vector2& vec)
	{
		Vector2 ret(vec);
		float mag = Magnitude(vec);
		if (mag > 0)
		{
			ret.x /= mag;
			ret.y /= mag;
		}

		return ret;
	}

	// Dot product
	float Dot(const Vector2& other) const
	{
		float x2 = x * other.x;
		float y2 = y * other.y;

		return x2 + y2;
	}

	static float Dot(const Vector2& a, const Vector2& b)
	{
		float x2 = a.x * b.x;
		float y2 = a.y * b.y;

		return x2 + y2;
	}

	// Vector projection
	Vector2 Project(const Vector2& other)
	{
		float dot = Dot(other);
		float mag2 = std::powf(Vector2::Magnitude(other), 2);
		float scalar = dot / mag2;
		Vector2 res = Vector2(other.x * scalar, other.y * scalar);
		return res;
	}

	static Vector2 Project(const Vector2& a, const Vector2& b)
	{
		float dot = Dot(a, b);
		float mag2 = std::powf(Vector2::Magnitude(b), 2);
		float scalar = dot / mag2;
		Vector2 res = Vector2(b.x * scalar, b.y * scalar);
		return res;
	}

	// Lerp
	Vector2 Lerp(const Vector2& a, float t)
	{
		float x2 = x + (a.x - x) * t;
		float y2 = y + (a.y - y) * t;
		return Vector2(x2, y2);
	}

	static Vector2 Lerp(const Vector2& a, const Vector2& b, float t)
	{
		float x2 = a.x + (b.x - a.x) * t;
		float y2 = a.y + (b.y - a.y) * t;
		return Vector2(x2, y2);
	}

};

#endif // VECTOR_2_H
