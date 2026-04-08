#ifndef VECTOR_4D_H
#define VECTOR_4D_H

#include <cmath>

class Vector4
{
public:
	float x, y, z, w;

	Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
	Vector4(float a) : x(a), y(a), z(a), w(a) {}
	Vector4(const Vector4& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}
	Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	// Addition operator overload (vector and scalar)
	Vector4 operator+(const Vector4& rhs) const
	{
		Vector4 res(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
		return res;
	}

	Vector4 operator+(const float rhs) const
	{
		Vector4 res(x + rhs, y + rhs, z + rhs, w + rhs);
		return res;
	}

	Vector4& operator+=(const Vector4& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}

	Vector4& operator+=(const float rhs)
	{
		x += rhs;
		y += rhs;
		z += rhs;
		w += rhs;
		return *this;
	}

	// Subtraction operator overload (vector and scalar)
	Vector4 operator-(const Vector4& rhs) const
	{
		Vector4 res(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
		return res;
	}

	Vector4 operator-(const float rhs) const
	{
		Vector4 res(x - rhs, y - rhs, z - rhs, w - rhs);
		return res;
	}

	Vector4& operator-=(const Vector4& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}

	Vector4& operator-=(const float rhs)
	{
		x -= rhs;
		y -= rhs;
		z -= rhs;
		w -= rhs;
		return *this;
	}

	// Multiplication and division by scalar
	Vector4 operator*(const float rhs) const
	{
		Vector4 res(x * rhs, y * rhs, z * rhs, w * rhs);
		return res;
	}

	Vector4& operator*=(const float rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}

	Vector4 operator/(const float rhs) const
	{
		Vector4 res(x / rhs, y / rhs, z / rhs, w / rhs);
		return res;
	}

	Vector4& operator/=(const float rhs)
	{
		x /= rhs;
		y /= rhs;
		z /= rhs;
		w /= rhs;
		return *this;
	}

	// Unary negation
	Vector4 operator-() const
	{
		Vector4 res(-x, -y, -z, -w);
		return res;
	}

	// Element access
	float& operator[](int i)
	{
		switch (i) { case 0: return x; case 1: return y; case 2: return z; default: return w; }
	}
	float operator[](int i) const
	{
		switch (i) { case 0: return x; case 1: return y; case 2: return z; default: return w; }
	}

	// Equality operators
	bool operator==(const Vector4& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y) && (z == rhs.z) && (w == rhs.w);
	}

	bool operator!=(const Vector4& rhs) const
	{
		return !(*this == rhs);
	}

	// Magnitude
	float Magnitude() const
	{
		return std::sqrt(x * x + y * y + z * z + w * w);
	}

	static float Magnitude(const Vector4& vec)
	{
		return std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
	}

	// Normalize
	Vector4 Normalize()
	{
		float mag = Magnitude();
		if (mag > 0)
		{
			x /= mag;
			y /= mag;
			z /= mag;
			w /= mag;
		}
		return *this;
	}

	static Vector4 Normalize(const Vector4& vec)
	{
		float mag = Magnitude(vec);
		if (mag > 0)
		{
			Vector4 res(vec.x / mag, vec.y / mag, vec.z / mag, vec.w / mag);
			return res;
		}
		return vec; // Return original if magnitude is zero
	}

	// Dot product
	float Dot(const Vector4& other) const
	{
		float x2 = x * other.x;
		float y2 = y * other.y;
		float z2 = z * other.z;
		float w2 = w * other.w;
		return x2 + y2 + z2 + w2;
	}

	static float Dot(const Vector4& a, const Vector4& b)
	{
		float x2 = a.x * b.x;
		float y2 = a.y * b.y;
		float z2 = a.z * b.z;
		float w2 = a.w * b.w;
		return x2 + y2 + z2 + w2;
	}

	// Vector projection
	Vector4 Project(const Vector4& other)
	{
		float dot = Dot(other);
		float mag2 = std::powf(Vector4::Magnitude(other), 2);
		float scalar = dot / mag2;
		Vector4 res(other.x * scalar, other.y * scalar, other.z * scalar, other.w * scalar);
		return res;
	}

	static Vector4 Project(const Vector4& a, const Vector4& b)
	{
		float dot = Dot(a, b);
		float mag2 = std::powf(Vector4::Magnitude(b), 2);
		float scalar = dot / mag2;
		Vector4 res(b.x * scalar, b.y * scalar, b.z * scalar, b.w * scalar);
		return res;
	}

	// Linear interpolation
	Vector4 Lerp(const Vector4& target, float t)
	{
		Vector4 res(x + (target.x - x) * t,
			y + (target.y - y) * t,
			z + (target.z - z) * t,
			w + (target.w - w) * t);
		return res;
	}

	static Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
	{
		Vector4 res(a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
		return res;
	}
};

#endif // VECTOR_4D_H
