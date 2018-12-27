/*
    MathTypes.h

    Copyright © 2017 Occipital, Inc. All rights reserved.
    This file is part of the Structure SDK.
    Unauthorized copying of this file, via any medium is strictly prohibited.
    Proprietary and confidential.

    http://structure.io
*/

#pragma once

#include <ST/Macros.h>

#include <array>
#include <cmath>
#include <ostream>
#include <limits>

namespace ST
{

//------------------------------------------------------------------------------

static inline bool epsilonEquals          (float  first,  float second,  float epsilon = std::numeric_limits<float> ::epsilon());
static inline bool epsilonEqualsOrBothNan (float  first,  float second,  float epsilon = std::numeric_limits<float> ::epsilon());
static inline bool epsilonEquals          (double first, double second, double epsilon = std::numeric_limits<double>::epsilon());
static inline bool epsilonEqualsOrBothNan (double first, double second, double epsilon = std::numeric_limits<double>::epsilon());

//------------------------------------------------------------------------------

template <class Scalar>
struct Vector2
{
    union
    {
        struct { Scalar x, y; };
        struct { Scalar s, t; };

        Scalar v[2];
    };

    static inline constexpr Vector2 zero();

    static Vector2 lerp(const Vector2& from, const Vector2& to, Scalar t);

    Vector2() = default;
    Vector2(const Vector2&) = default;

    constexpr Vector2(Scalar _x, Scalar _y);

    Vector2& operator=(const Vector2&) = default;

    bool epsilonEquals(const Vector2& other, Scalar epsilon = std::numeric_limits<Scalar>::epsilon());
    bool epsilonEqualsOrBothNan(const Vector2& other, Scalar epsilon = std::numeric_limits<Scalar>::epsilon());
};

using Vector2u = Vector2<unsigned>;
using Vector2f = Vector2<float>;
using Vector2d = Vector2<double>;

//------------------------------------------------------------------------------

template <class Scalar>
struct Vector3
{
    union
    {
        struct { Scalar x, y, z; };
        struct { Scalar r, g, b; };
        struct { Scalar s, t, p; };

        float v[3];
    };

    static constexpr Vector3 zero();

    Vector3() = default;
    Vector3(const Vector3&) = default;

    constexpr Vector3(Scalar _x, Scalar _y, Scalar _z);

    Vector3& operator=(const Vector3&) = default;

    Vector3 operator-() const;

    Vector3 operator-(const Vector3& rv) const;
    Vector3 operator+(const Vector3& rv) const;

    Vector3& operator+=(const Vector3& rv);

    Vector3 operator*(Scalar s) const;

    float length() const;
    Vector3 normalized() const;
    Scalar dot(const Vector3& v) const;
    Vector3 cross(const Vector3& v) const;

    bool epsilonEquals(const Vector3& other, Scalar epsilon = std::numeric_limits<Scalar>::epsilon());
    bool epsilonEqualsOrBothNan(const Vector3& other, Scalar epsilon = std::numeric_limits<Scalar>::epsilon());
};

using Vector3f = Vector3<float>;
using Vector3d = Vector3<double>;

//------------------------------------------------------------------------------

struct ST_ALIGNED(16) Vector4
{
    union
    {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
        struct { float s, t, p, q; };

        float v[4];
    };

    Vector4() = default;
    Vector4(const Vector4&) = default;

    Vector4& operator=(const Vector4&) = default;

    inline constexpr Vector4(float _x, float _y, float _z, float _w);

    bool epsilonEquals(const Vector4& other, float epsilon = std::numeric_limits<float>::epsilon());
    bool epsilonEqualsOrBothNan(const Vector4& other, float epsilon = std::numeric_limits<float>::epsilon());
};

//------------------------------------------------------------------------------

enum class Handedness
{
    Left,
    Right
};

//------------------------------------------------------------------------------

// Column major.
// Indices are named: mColumnRow.
// Translation component is thus m30, m31, m32.
struct ST_API ST_ALIGNED(16) Matrix4
{
    union
    {
        struct
        {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };

        float m[16];
    };

    inline static Matrix4 identity();
    inline static Matrix4 nan();

    inline static Matrix4 lookAt(const Vector3f& eyePosition, const Vector3f& lookAt, const Vector3f& up);
    inline static Matrix4 projection(float fovY, float aspectRatio, float minZ, float maxZ, Handedness handedness = Handedness::Left);

    Matrix4() = default;
    Matrix4(const Matrix4& copy) = default;

    inline Matrix4(const std::array<float, 16>& data);

    Matrix4& operator=(const Matrix4& rhs) = default;

    inline float& operator()(int col, int row);
    inline const float& operator()(int col, int row) const;

    inline Matrix4  operator*(const Matrix4& rv) const;
    inline Vector4  operator*(const Vector4& v) const;
    inline Vector3f operator*(const Vector3f& v) const;

    inline Matrix4 transposed() const;

    inline Matrix4 inversed() const;

    bool isZero() const;
    bool hasNan() const;
    bool isApprox(const Matrix4& rhs) const;

    Matrix4 scaled(float scaleX, float scaleY, float scaleZ) const;

    // Applies the translation on the right side: output = input * translation
    Matrix4 translated(float tX, float tY, float tZ) const;

    // Applies the rotation on the right side: output = input * rotation
    Matrix4 rotated(const Vector4& quaternion) const;

    // Applies the rotation on the right side: output = input * rotation
    // The eulerAngles will be applied using the Z * Y * X order, assuming radians.
    Matrix4 rotated(const Vector3f& eulerAnglesRad) const;

    Vector3f translation() const;

    Vector4 rotationAsQuaternion() const;
    
    float angleBetweenPoses(const Matrix4& worldFromFrame) const;
    
    Vector3f rvecBetweenPoses(const ST::Matrix4& worldFromFrame) const;
    
    Matrix4 extrapolatePose(ST::Vector3f& linearVelocityInWorld,
                            ST::Vector3f& linearAccelerationInWorld,
                            ST::Vector3f& angularVelocityInFrame,
                            ST::Vector3f& angularAccelerationInFrame,
                            const double dt) const;
    
    void fromExtrinsics(float tx, float ty, float tz, float qx, float qy, float qz, float qw);

    inline Vector3f rotationAsZYXEuler() const;

    // Returns a transform that has the same rotation but a zero translation.
    inline Matrix4 rotationOnly() const;

    std::string toString() const;

    Matrix4 interpolateBetweenPoses(const double alpha, const Matrix4& other) const;
};

inline std::ostream&
operator<< (std::ostream& o, const Matrix4& m);

//------------------------------------------------------------------------------

} // ST namespace

#include <ST/MathTypes.hpp>


