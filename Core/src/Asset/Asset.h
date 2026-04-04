#ifndef ASSET_H
#define ASSET_G	

#include <vector>
#include "Render/GpuTypes.h"
#include "Render/Vertex.h"
#include <glm/glm.hpp>

using AssetID = uint64_t;
constexpr AssetID NullAssetId = 0;

template<typename T>
struct AssetHandle
{
	AssetID id = NullAssetId;
	bool IsValid() const { return id != NullAssetId; }
	bool operator==(const AssetHandle&) const = default;
};

struct MeshAsset
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	GpuBuffer VertexBuffer;
	GpuBuffer IndexBuffer;
	glm::vec3 BoundsMin, BoundsMax; // AABB for culling
};

struct TextureAsset
{
	uint32_t Width, Height;
	GpuTexture Texture;
	std::vector<uint8_t> Data; // RGBA8 data for CPU access (e.g. for ImGui)

	GpuTexture Handle;
};

struct MaterialAsset
{
	AssetHandle<TextureAsset> AlbedoMap;
	AssetHandle<TextureAsset> NormalMap;
	AssetHandle<TextureAsset> MetallicRoughnessMap;
	AssetHandle<TextureAsset> OcclusionMap;
	AssetHandle<TextureAsset> EmissiveMap;

	glm::vec4 BaseColorFactor{ 1.0f };
	float     MetallicFactor = 1.0f;
	float     RoughnessFactor = 1.0f;
	glm::vec3 EmissiveFactor{ 0.0f };
	float     NormalScale = 1.0f;
	float     OcclusionStrength = 1.0f;

	enum class AlphaMode { Opaque, Mask, Blend } Alpha = AlphaMode::Opaque;
	float AlphaCutoff = 0.5f;
	bool  DoubleSided = false;
};

struct ShaderAsset
{
	std::vector<uint32_t> Code; // SPIR-V or DXIL bytecode
	GpuShader Shader;
};

#endif // ASSET_H