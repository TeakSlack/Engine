#ifndef VECTOR_3_H
#define VECTOR_3_H

#include <cmath>

class Vector3
{
public:
	float x, y, z;

	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
	Vector3(float a) : x(a), y(a), z(a) {}
	Vector3(const Vector3& other) : x(other.x), y(other.y), z(other.z) {}
	Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

	// Addition operator overload
	Vector3 operator+(const Vector3& other) const
	{
		Vector3 res = Vector3(x + other.x, y + other.y, z + other.z);
		return res;
	}

	Vector3 operator+(float scalar) const
	{
		Vector3 res = Vector3(x + scalar, y + scalar, z + scalar);
		return res;
	}

	// Subtraction operator overload
		Vector3 operator-(const Vector3& other) const
	{
		Vector3 res = Vector3(x - other.x, y - other.y, z - other.z);
		return res;
	}

	Vector3 operator-(float scalar) const
	{
		Vector3 res = Vector3(x - scalar, y - scalar, z - scalar);
		return res;
	}

	Vector3 operator-() const // Unary negation
	{
		Vector3 res = Vector3(-x, -y, -z);
		return res;
	}

	Vector3 operator*(float scalar) const
	{
		Vector3 res = Vector3(x * scalar, y * scalar, z * scalar);
		return res;
	}

	Vector3 operator/(float scalar) const
	{
		Vector3 res = Vector3(x / scalar, y / scalar, z / scalar);
		return res;
	}

	bool operator==(const Vector3& other) const
	{
		return (x == other.x) && (y == other.y) && (z == other.z);
	}

	bool operator!=(const Vector3& other) const
	{
		return !(*this == other);
	}

	// Normalize
	Vector3 Normalize()
	{
		float mag = Magnitude();
		if (mag > 0)
		{
			x /= mag;
			y /= mag;
			z /= mag;
		}
		return *this;
	}

	static Vector3 Normalize(const Vector3& vec)
	{
		float mag = Magnitude(vec);
		if (mag > 0)
		{
			Vector3 res = Vector3(vec.x / mag, vec.y / mag, vec.z / mag);
			return res;
		}
		return vec; // Return original if magnitude is zero
	}

	// Magnitude
	float Magnitude()
	{
		return std::sqrt(x*x + y*y + z*z);
	}

	static float Magnitude(const Vector3& vec)
	{
		return std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
	}

	// Dot product
	float Dot(const Vector3& other)
	{
		float x2 = x * other.x;
		float y2 = y * other.y;
		float z2 = z * other.z;

		return x2 + y2 + z2;
	}

	static float Dot(const Vector3& a, const Vector3& b)
	{
		float x2 = a.x * b.x;
		float y2 = a.y * b.y;
		float z2 = a.z * b.z;
		return x2 + y2 + z2;
	}

	// Cross product
	Vector3 Cross(const Vector3& other)
	{
		float x2 = y * other.z - z * other.y;
		float y2 = z * other.x - x * other.z;
		float z2 = x * other.y - y * other.x;
		Vector3 res = Vector3(x2, y2, z2);
		return res;
	}

	static Vector3 Cross(const Vector3& a, const Vector3& b)
	{
		float x2 = a.y * b.z - a.z * b.y;
		float y2 = a.z * b.x - a.x * b.z;
		float z2 = a.x * b.y - a.y * b.x;
		Vector3 res = Vector3(x2, y2, z2);
		return res;
	}

	// Vector projection
	Vector3 Project(const Vector3& other)
	{
		float dot = Dot(other);
		float mag2 = std::powf(Vector3::Magnitude(other), 2);
		float scalar = dot / mag2;
		Vector3 res = Vector3(other.x * scalar, other.y * scalar, other.z * scalar);
		return res;
	}

	static Vector3 Project(const Vector3& a, const Vector3& b)
	{
		float dot = Dot(a, b);
		float mag2 = std::powf(Vector3::Magnitude(b), 2);
		float scalar = dot / mag2;
		Vector3 res = Vector3(b.x * scalar, b.y * scalar, b.z * scalar);
		return res;
	}

	// Linear interpolation
	Vector3 Lerp(const Vector3& other, float t)
	{
		float x2 = x + (other.x - x) * t;
		float y2 = y + (other.y - y) * t;
		float z2 = z + (other.z - z) * t;
		Vector3 res = Vector3(x2, y2, z2);
		return res;
	}

	static Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
	{
		float x2 = a.x + (b.x - a.x) * t;
		float y2 = a.y + (b.y - a.y) * t;
		float z2 = a.z + (b.z - a.z) * t;
		Vector3 res = Vector3(x2, y2, z2);
		return res;
	}

	// Spherical linear interpolation
	Vector3 Slerp(const Vector3& other, float t)
	{
		float dot = Dot(other);
		dot = std::fmaxf(std::fminf(dot, 1.0f), -1.0f); // Clamp dot product to avoid numerical issues
		float theta = std::acosf(dot) * t;
		Vector3 relativeVec = other - (*this * dot);
		relativeVec.Normalize();
		Vector3 res = (*this * std::cosf(theta)) + (relativeVec * std::sinf(theta));
		return res;
	}

	static Vector3 Slerp(const Vector3& a, const Vector3& b, float t)
	{
		float dot = Dot(a, b);
		dot = std::fmaxf(std::fminf(dot, 1.0f), -1.0f); // Clamp dot product to avoid numerical issues
		float theta = std::acosf(dot) * t;
		Vector3 relativeVec = b - (a * dot);
		relativeVec.Normalize();
		Vector3 res = (a * std::cosf(theta)) + (relativeVec * std::sinf(theta));
		return res;
	}
};

#endif // VECTOR_3_H