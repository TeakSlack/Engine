#ifndef ASSET_H
#define ASSET_H

#include <vector>
#include "Render/GpuTypes.h"
#include "Render/Vertex.h"
#include "Math/Vector3.h"
#include "Util/UUID.h"
#include "Math/Vector4.h"
#include "Render/DrawBinFlags.h"

using AssetID = CoreUUID;
inline const AssetID NullAssetId = CoreUUID{};

template<typename T>
struct AssetHandle
{
    AssetID id = NullAssetId;
    bool IsValid() const { return id != NullAssetId; }
    bool operator==(const AssetHandle&) const = default;
};

struct MeshAsset
{
	std::string           Name;
    std::vector<Vertex>   Vertices;
    std::vector<uint32_t> Indices;
    Vector3               BoundsMin, BoundsMax; // AABB for culling
};

struct TextureAsset
{
    uint32_t             Width, Height;
    GpuTexture           Texture;
    std::vector<uint8_t> Data; // RGBA8 CPU copy (e.g. for editor thumbnails)
};

struct MaterialAsset
{
    AssetHandle<TextureAsset> AlbedoMap;
    AssetHandle<TextureAsset> NormalMap;
    AssetHandle<TextureAsset> MetallicRoughnessMap;
    AssetHandle<TextureAsset> OcclusionMap;
    AssetHandle<TextureAsset> EmissiveMap;

    Vector4 BaseColorFactor{ 1.0f };
    float   MetallicFactor    = 1.0f;
    float   RoughnessFactor   = 1.0f;
    Vector3 EmissiveFactor{ 0.0f };
    float   NormalScale       = 1.0f;
    float   OcclusionStrength = 1.0f;

    enum class AlphaMode { Opaque, Mask, Blend } Alpha = AlphaMode::Opaque;
    float AlphaCutoff = 0.5f;
    bool  DoubleSided = false;
	bool  IsOccluder = true; // whether this material should be considered an occluder for occlusion culling
	DrawBinFlags DrawBin = DrawBinFlags::ForwardOpaque; // which render bin to submit this material's RenderPackets into
};

struct ShaderAsset
{
    std::vector<uint8_t> Bytecode;
    ShaderStage          Stage      = ShaderStage::None;
    std::string          EntryPoint = "main";
};

#endif // ASSET_H
