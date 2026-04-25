#ifndef SUBMITTED_LIGHT_H
#define SUBMITTED_LIGHT_H

#include "ECS/Components.h"
#include "Math/Vector3.h"

struct SubmittedLight
{
    LightType Type;

    Vector3   Color;        // Color * Intensity, premultiplied
    Vector3   Position;     // world space — from TransformComponent
    Vector3   Direction;    // world space — derived from TransformComponent rotation
    float     Range;
    float     InnerConeCos; // cos(InnerConeAngle) — precomputed for fast shader eval
    float     OuterConeCos; // cos(OuterConeAngle)
    bool      CastsShadows;

    // AABB used for frustum culling and cluster assignment (point/spot only)
    Vector3   BoundsMin;
    Vector3   BoundsMax;
};

struct alignas(16) SubmittedLightData
{
    // Row 0
    Vector3  Position;      // world space
    float    Range;

    // Row 1
    Vector3  Color;         // premultiplied intensity, linear
    float    InvRangeSq;    // 1 / (range * range) — baked for fast attenuation

    // Row 2
    Vector3  Direction;     // normalised, world space (spot/directional)
    uint32_t Type;          // 0 = directional, 1 = point, 2 = spot

    // Row 3
    float    InnerConeCos;
    float    OuterConeCos;
    float    _pad[2];
};

#endif // SUBMITTED_LIGHT_H