#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include <cstdint>
#include <cstring>   // memcpy
#include "Asset/Asset.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "DrawBinFlags.h"

// -------------------------------------------------------------------------
// RenderPacket — the atomic unit submitted to the renderer each frame.
//
// Design goals (Gregory ch. 11):
//   • Plain data — no virtual dispatch, cheap to copy and sort.
//   • Asset handles, not raw pointers — stable across async loads and
//     internal storage rehashing inside AssetSystem.
//   • World-space AABB precomputed at submission time so culling never
//     needs to call GetAsset().
//   • Single SortKey drives std::sort: material ID in the high 32 bits
//     (minimises PSO / binding-set switches per draw call), linear depth
//     in the low 32 bits (front-to-back for opaque = early-Z benefit).
// -------------------------------------------------------------------------
struct RenderPacket
{
    // ---- What to draw -----------------------------------------------
    AssetHandle<MeshAsset>     Mesh;
    AssetHandle<MaterialAsset> Material;

    // ---- Where to draw it -------------------------------------------
    Matrix4x4 WorldTransform;
    Matrix4x4 PreviousWorldTransform; // Needed for TAA velocity buffer

    // ---- Which bin to cluster it into --------------------------------

    // ---- World-space AABB (for frustum culling) ----------------------
    // Precomputed from MeshAsset::BoundsMin/Max * WorldTransform.
    // Stored here so the culling loop is a pure AABB test with no asset
    // lookups — keeps the hot path cache-friendly.
    Vector3 WorldBoundsMin;
    Vector3 WorldBoundsMax;

    // ---- Sort key ---------------------------------------------------
    // High 32 bits: low half of material UUID — groups packets by
    //               PSO + binding set, minimising GPU state switches.
    // Low  32 bits: float distance bits — front-to-back for opaque
    //               (maximises early-Z rejection), back-to-front for
    //               transparent (correct alpha blending).
    // Computed by ComputeSortKey(); never set manually.
    uint64_t SortKey = 0;

    // ---- Flags ------------------------------------------------------
	DrawBinFlags Bin = DrawBinFlags::None;
    bool CastsShadow = true;
    bool Transparent = false; // moves packet to the transparent pass and
                              // inverts depth sort order
	bool IsOccluder = false;  // extend: set from MaterialAsset or a separate OccluderComponent
};

// -------------------------------------------------------------------------
// ComputeSortKey
//
// Call this after setting Mesh, Material, WorldTransform, WorldBounds and
// Transparent.  viewPos is the camera's world-space position.
//
// Reinterpreting float bits as uint preserves sort order for positive
// distances (IEEE 754 guarantee), so no extra conversion is needed.
// -------------------------------------------------------------------------
inline void ComputeSortKey(RenderPacket& packet, const Vector3& viewPos, uint8_t layerIndex)
{
	uint64_t layer = static_cast<uint64_t>(layerIndex & 0xFu) << 60; // reserve low 8 bits for layer index
	uint64_t transBit = static_cast<uint64_t>(packet.Transparent ? 1u : 0u) << 59; // reserve 1 bit for transparency

	// Extract low 32 bits of material UUID into a 64-bit value only when shifting
	uint64_t matKey = static_cast<uint64_t>(packet.Material.id.Value() & 0xFFFFFFFFu);

    // Low 32 — linear distance from camera to AABB centre
    Vector3 centre = (packet.WorldBoundsMin + packet.WorldBoundsMax) * 0.5f;
    Vector3 delta  = centre - viewPos;
    float   dist   = delta.Magnitude();

    uint32_t depthKey = 0;
    static_assert(sizeof(float) == sizeof(uint32_t), "Unexpected float size");
    std::memcpy(&depthKey, &dist, sizeof(float));

    // Transparent objects sort back-to-front: invert depth key
    if (packet.Transparent)
        depthKey = ~depthKey;

    packet.SortKey = layer | transBit | (matKey << 32) | static_cast<uint64_t>(depthKey);
}

// -------------------------------------------------------------------------
// MakeWorldBounds
//
// Transforms the 8 corners of a local-space AABB by worldTransform and
// re-fits an AABB in world space.  Pass the result into WorldBoundsMin/Max.
//
// This is not tight for rotated objects (AABB grows with rotation), but it
// is fast and conservative — exactly what frustum culling needs.
// -------------------------------------------------------------------------
inline void MakeWorldBounds(
    const Vector3&   localMin,
    const Vector3&   localMax,
    const Matrix4x4& worldTransform,
    Vector3&         outMin,
    Vector3&         outMax)
{
    // Arvo's method.  For a row-major matrix (v * M convention), the
    // transformed point is:  out[j] = sum_i( in[i] * M[i][j] ) + M[3][j]
    //
    // For each output component j, iterate over each input axis i.
    // If M[i][j] is positive it maps localMax to outMax; if negative it
    // maps localMin to outMax.  This avoids transforming all 8 corners.

    const float localArr[2][3] = {
        { localMin.x, localMin.y, localMin.z },
        { localMax.x, localMax.y, localMax.z },
    };
    float minArr[3] = { worldTransform[3].x, worldTransform[3].y, worldTransform[3].z };
    float maxArr[3] = { worldTransform[3].x, worldTransform[3].y, worldTransform[3].z };

    for (int i = 0; i < 3; ++i)          // input axis (row)
    {
        for (int j = 0; j < 3; ++j)      // output axis (column)
        {
            float e = worldTransform[i][j];
            if (e >= 0.f)
            {
                minArr[j] += e * localArr[0][i];  // e * localMin
                maxArr[j] += e * localArr[1][i];  // e * localMax
            }
            else
            {
                minArr[j] += e * localArr[1][i];  // e * localMax (more negative)
                maxArr[j] += e * localArr[0][i];  // e * localMin (less negative)
            }
        }
    }

    outMin = Vector3(minArr[0], minArr[1], minArr[2]);
    outMax = Vector3(maxArr[0], maxArr[1], maxArr[2]);
}

#endif // RENDER_PACKET_H
