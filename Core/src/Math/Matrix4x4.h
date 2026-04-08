#ifndef MATRIX_4X4_H
#define MATRIX_4X4_H

#include "Vector4.h"
#include "Vector3.h"
#include <cmath>

// -------------------------------------------------------------------------
// Matrix4x4 — row-major 4x4 float matrix.
//
// Convention: row vectors, left-to-right multiplication (v * M).
//   rows[0] = first row  = X basis + Tx in last element for column-layout,
//                          but here translation sits in rows[3].
// Transform a point:  result = v * M  →  use (v * M) free function below.
// Concatenate:        Model * View * Proj  →  left-to-right as written.
// -------------------------------------------------------------------------
struct Matrix4x4
{
    Vector4 rows[4];

    Matrix4x4() = default;
    Matrix4x4(const Vector4& r0, const Vector4& r1, const Vector4& r2, const Vector4& r3)
        : rows{ r0, r1, r2, r3 } {}

    // ---- Row access ----

    Vector4&       operator[](int i)       { return rows[i]; }
    const Vector4& operator[](int i) const { return rows[i]; }

    Vector4 GetColumn(int j) const
    {
        return Vector4(rows[0][j], rows[1][j], rows[2][j], rows[3][j]);
    }

    // ---- Arithmetic ----

    // Matrix * Matrix
    Matrix4x4 operator*(const Matrix4x4& rhs) const
    {
        Matrix4x4 result = Zero();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.rows[i][j] =
                    rows[i][0] * rhs.rows[0][j] +
                    rows[i][1] * rhs.rows[1][j] +
                    rows[i][2] * rhs.rows[2][j] +
                    rows[i][3] * rhs.rows[3][j];
        return result;
    }

    Matrix4x4& operator*=(const Matrix4x4& rhs) { *this = *this * rhs; return *this; }

    // Scalar multiply
    Matrix4x4 operator*(float rhs) const
    {
        return Matrix4x4(rows[0] * rhs, rows[1] * rhs, rows[2] * rhs, rows[3] * rhs);
    }

    // ---- Comparison ----

    bool operator==(const Matrix4x4& rhs) const
    {
        return rows[0] == rhs.rows[0] && rows[1] == rhs.rows[1]
            && rows[2] == rhs.rows[2] && rows[3] == rhs.rows[3];
    }
    bool operator!=(const Matrix4x4& rhs) const { return !(*this == rhs); }

    // ---- Factory: basic ----

    static Matrix4x4 Identity()
    {
        return Matrix4x4(
            Vector4(1.0f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, 1.0f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 1.0f, 0.0f),
            Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    static Matrix4x4 Zero()
    {
        return Matrix4x4(
            Vector4(0.0f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    // ---- Factory: transform ----

    // Translation (row-vector convention: translation in last row).
    static Matrix4x4 Translate(const Vector3& t)
    {
        Matrix4x4 m = Identity();
        m.rows[3] = Vector4(t.x, t.y, t.z, 1.0f);
        return m;
    }

    // Uniform scale
    static Matrix4x4 Scale(const Vector3& s)
    {
        return Matrix4x4(
            Vector4(s.x,  0.0f, 0.0f, 0.0f),
            Vector4(0.0f, s.y,  0.0f, 0.0f),
            Vector4(0.0f, 0.0f, s.z,  0.0f),
            Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Axis-angle rotation (angle in degrees, axis need not be normalized).
    static Matrix4x4 Rotate(float angleDegrees, const Vector3& axis)
    {
        float rad = angleDegrees * (3.14159265358979323846f / 180.0f);
        float c   = std::cosf(rad);
        float s   = std::sinf(rad);
        float t   = 1.0f - c;
        Vector3 n = Vector3::Normalize(axis);
        float x = n.x, y = n.y, z = n.z;

        return Matrix4x4(
            Vector4(t*x*x + c,     t*x*y - s*z,   t*x*z + s*y,   0.0f),
            Vector4(t*x*y + s*z,   t*y*y + c,     t*y*z - s*x,   0.0f),
            Vector4(t*x*z - s*y,   t*y*z + s*x,   t*z*z + c,     0.0f),
            Vector4(0.0f,          0.0f,           0.0f,          1.0f));
    }

    // Euler-angle rotation: apply X, then Y, then Z (degrees).
    static Matrix4x4 RotateEuler(float pitchDeg, float yawDeg, float rollDeg)
    {
        return Rotate(pitchDeg, Vector3(1, 0, 0))
             * Rotate(yawDeg,   Vector3(0, 1, 0))
             * Rotate(rollDeg,  Vector3(0, 0, 1));
    }

    // Combined TRS — equivalent to Scale * Rotate * Translate.
    static Matrix4x4 TRS(const Vector3& translation, const Vector3& eulerDegrees, const Vector3& scale)
    {
        return Scale(scale) * RotateEuler(eulerDegrees.x, eulerDegrees.y, eulerDegrees.z) * Translate(translation);
    }

    // ---- Factory: view / projection ----

    // Right-handed look-at view matrix (row-vector convention).
    // eye    — camera world position
    // target — point the camera looks at
    // up     — world up vector (usually {0,1,0})
    static Matrix4x4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        Vector3 f = Vector3::Normalize(target - eye);          // forward
        Vector3 r = Vector3::Normalize(Vector3::Cross(f, up)); // right = forward × up (RH)
        Vector3 u = Vector3::Cross(r, f);                      // up    = right × forward (RH)

        return Matrix4x4(
            Vector4(r.x, u.x, f.x, 0.0f),
            Vector4(r.y, u.y, f.y, 0.0f),
            Vector4(r.z, u.z, f.z, 0.0f),
            Vector4(-Vector3::Dot(r, eye), -Vector3::Dot(u, eye), -Vector3::Dot(f, eye), 1.0f));
    }

    // Perspective projection — right-handed, depth [0, 1] (Vulkan / D3D12).
    // fovYDeg — vertical field of view in degrees
    // aspect  — width / height
    static Matrix4x4 Perspective(float fovYDeg, float aspect, float nearZ, float farZ)
    {
        float f = 1.0f / std::tanf(fovYDeg * (3.14159265358979323846f / 180.0f) * 0.5f);
        float d = farZ / (farZ - nearZ);

        return Matrix4x4(
            Vector4(f / aspect, 0.0f, 0.0f,          0.0f),
            Vector4(0.0f,       f,    0.0f,          0.0f),
            Vector4(0.0f,       0.0f, d,             1.0f),
            Vector4(0.0f,       0.0f, -nearZ * d,    0.0f));
    }

    // Orthographic projection — right-handed, depth [0, 1] (Vulkan / D3D12).
    static Matrix4x4 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ)
    {
        float rw = 1.0f / (right - left);
        float rh = 1.0f / (top   - bottom);
        float rd = 1.0f / (farZ  - nearZ);

        return Matrix4x4(
            Vector4(2.0f * rw,              0.0f,                   0.0f,         0.0f),
            Vector4(0.0f,                   2.0f * rh,              0.0f,         0.0f),
            Vector4(0.0f,                   0.0f,                   rd,           0.0f),
            Vector4(-(right + left) * rw,   -(top + bottom) * rh,   -nearZ * rd,  1.0f));
    }

    // ---- Operations ----

    Matrix4x4 Transpose() const
    {
        return Matrix4x4(GetColumn(0), GetColumn(1), GetColumn(2), GetColumn(3));
    }

    // General 4x4 inverse via cofactor expansion.
    // Returns Identity if the matrix is singular (determinant ≈ 0).
    Matrix4x4 Inverse() const
    {
        const auto& m = rows;

        float c00 =  m[1][1]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[1][2]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) + m[1][3]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]);
        float c10 = -(m[1][0]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[1][2]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[1][3]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]));
        float c20 =  m[1][0]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) - m[1][1]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[1][3]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]);
        float c30 = -(m[1][0]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]) - m[1][1]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]) + m[1][2]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]));

        float det = m[0][0]*c00 + m[0][1]*c10 + m[0][2]*c20 + m[0][3]*c30;
        if (std::fabsf(det) < 1e-6f)
            return Identity();

        float invDet = 1.0f / det;

        float c01 = -(m[0][1]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[0][2]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) + m[0][3]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]));
        float c11 =  m[0][0]*(m[2][2]*m[3][3] - m[2][3]*m[3][2]) - m[0][2]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[0][3]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]);
        float c21 = -(m[0][0]*(m[2][1]*m[3][3] - m[2][3]*m[3][1]) - m[0][1]*(m[2][0]*m[3][3] - m[2][3]*m[3][0]) + m[0][3]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]));
        float c31 =  m[0][0]*(m[2][1]*m[3][2] - m[2][2]*m[3][1]) - m[0][1]*(m[2][0]*m[3][2] - m[2][2]*m[3][0]) + m[0][2]*(m[2][0]*m[3][1] - m[2][1]*m[3][0]);

        float c02 =  m[0][1]*(m[1][2]*m[3][3] - m[1][3]*m[3][2]) - m[0][2]*(m[1][1]*m[3][3] - m[1][3]*m[3][1]) + m[0][3]*(m[1][1]*m[3][2] - m[1][2]*m[3][1]);
        float c12 = -(m[0][0]*(m[1][2]*m[3][3] - m[1][3]*m[3][2]) - m[0][2]*(m[1][0]*m[3][3] - m[1][3]*m[3][0]) + m[0][3]*(m[1][0]*m[3][2] - m[1][2]*m[3][0]));
        float c22 =  m[0][0]*(m[1][1]*m[3][3] - m[1][3]*m[3][1]) - m[0][1]*(m[1][0]*m[3][3] - m[1][3]*m[3][0]) + m[0][3]*(m[1][0]*m[3][1] - m[1][1]*m[3][0]);
        float c32 = -(m[0][0]*(m[1][1]*m[3][2] - m[1][2]*m[3][1]) - m[0][1]*(m[1][0]*m[3][2] - m[1][2]*m[3][0]) + m[0][2]*(m[1][0]*m[3][1] - m[1][1]*m[3][0]));

        float c03 = -(m[0][1]*(m[1][2]*m[2][3] - m[1][3]*m[2][2]) - m[0][2]*(m[1][1]*m[2][3] - m[1][3]*m[2][1]) + m[0][3]*(m[1][1]*m[2][2] - m[1][2]*m[2][1]));
        float c13 =  m[0][0]*(m[1][2]*m[2][3] - m[1][3]*m[2][2]) - m[0][2]*(m[1][0]*m[2][3] - m[1][3]*m[2][0]) + m[0][3]*(m[1][0]*m[2][2] - m[1][2]*m[2][0]);
        float c23 = -(m[0][0]*(m[1][1]*m[2][3] - m[1][3]*m[2][1]) - m[0][1]*(m[1][0]*m[2][3] - m[1][3]*m[2][0]) + m[0][3]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]));
        float c33 =  m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1]) - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0]) + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);

        // Adjugate is the transpose of the cofactor matrix; divide by determinant.
        return Matrix4x4(
            Vector4(c00, c01, c02, c03) * invDet,
            Vector4(c10, c11, c12, c13) * invDet,
            Vector4(c20, c21, c22, c23) * invDet,
            Vector4(c30, c31, c32, c33) * invDet);
    }
};

// ---- Free functions: vector * matrix (row-vector convention) ----

// Transform a Vector4 by a Matrix4x4 (v * M).
inline Vector4 operator*(const Vector4& lhs, const Matrix4x4& rhs)
{
    return Vector4(
        lhs.x*rhs.rows[0][0] + lhs.y*rhs.rows[1][0] + lhs.z*rhs.rows[2][0] + lhs.w*rhs.rows[3][0],
        lhs.x*rhs.rows[0][1] + lhs.y*rhs.rows[1][1] + lhs.z*rhs.rows[2][1] + lhs.w*rhs.rows[3][1],
        lhs.x*rhs.rows[0][2] + lhs.y*rhs.rows[1][2] + lhs.z*rhs.rows[2][2] + lhs.w*rhs.rows[3][2],
        lhs.x*rhs.rows[0][3] + lhs.y*rhs.rows[1][3] + lhs.z*rhs.rows[2][3] + lhs.w*rhs.rows[3][3]);
}

// Transform a point (w=1) — applies translation.
inline Vector3 TransformPoint(const Vector3& p, const Matrix4x4& m)
{
    Vector4 r = Vector4(p.x, p.y, p.z, 1.0f) * m;
    float invW = (r.w != 0.0f) ? 1.0f / r.w : 1.0f;
    return Vector3(r.x * invW, r.y * invW, r.z * invW);
}

// Transform a direction (w=0) — ignores translation.
inline Vector3 TransformDirection(const Vector3& d, const Matrix4x4& m)
{
    Vector4 r = Vector4(d.x, d.y, d.z, 0.0f) * m;
    return Vector3(r.x, r.y, r.z);
}

#endif // MATRIX_4X4_H
