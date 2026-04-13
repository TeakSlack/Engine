#include <cassert>
#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <stb_image.h>
#include "GltfImporter.h"
#include "Asset/TextureImporter.h"
#include "Math/Vector3.h"
#include "Render/Vertex.h"
#include "Util/Log.h"

// ---------------------------------------------------------------------------
// Image / Texture extraction
// ---------------------------------------------------------------------------

static TextureAsset LoadTextureFromMemory(const stbi_uc* data, int byteSize)
{
    int w, h, ch;
    stbi_uc* pixels = stbi_load_from_memory(data, byteSize, &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels)
        return {};
    TextureAsset tex;
    tex.Width  = static_cast<uint32_t>(w);
    tex.Height = static_cast<uint32_t>(h);
    tex.Data.assign(pixels, pixels + w * h * 4);
    stbi_image_free(pixels);
    return tex;
}

// Returns one TextureAsset per glTF image (parallel to asset.images[]).
// Images with empty Data were not loaded (warn upstream).
static std::vector<TextureAsset> ExtractImages(const fastgltf::Asset& asset,
                                               const std::filesystem::path& gltfDir)
{
    std::vector<TextureAsset> out;
    out.reserve(asset.images.size());

    for (const auto& image : asset.images)
    {
        TextureAsset tex;

        std::visit(fastgltf::visitor{
            // External file URI -- resolve relative to the glTF directory
            [&](const fastgltf::sources::URI& uriSrc) {
                auto imgPath = gltfDir / std::filesystem::path(std::string(uriSrc.uri.path()));
                StbTextureImporter loader;
                if (auto result = loader.Load(imgPath))
                    tex = std::move(*result);
                else
                    LOG_WARN_TO("asset", "Failed to load image: {}", imgPath.string());
            },
            // In-memory raw bytes (e.g., base64-embedded or via LoadExternalImages)
            [&](const fastgltf::sources::Array& arrSrc) {
                tex = LoadTextureFromMemory(
                    reinterpret_cast<const stbi_uc*>(arrSrc.bytes.data()),
                    static_cast<int>(arrSrc.bytes.size()));
            },
            // Buffer-view embedded image (GLB)
            [&](const fastgltf::sources::BufferView& bvSrc) {
                const auto& bv  = asset.bufferViews[bvSrc.bufferViewIndex];
                const auto& buf = asset.buffers[bv.bufferIndex];
                std::visit(fastgltf::visitor{
                    [&](const fastgltf::sources::Array& bufArr) {
                        tex = LoadTextureFromMemory(
                            reinterpret_cast<const stbi_uc*>(bufArr.bytes.data()) + bv.byteOffset,
                            static_cast<int>(bv.byteLength));
                    },
                    [](auto&) {}
                }, buf.data);
            },
            [](auto&) {}
        }, image.data);

        if (tex.Data.empty())
            LOG_WARN_TO("asset", "Image [{}] '{}' could not be decoded", out.size(),
                        std::string(image.name));

        out.push_back(std::move(tex));
    }

    return out;
}

// ---------------------------------------------------------------------------
// Material extraction
// ---------------------------------------------------------------------------

// Maps a glTF texture index to image index. Returns -1 if absent or unresolvable.
static int ResolveImageIndex(const fastgltf::Asset& asset, std::size_t texIdx)
{
    if (texIdx >= asset.textures.size())
        return -1;
    const auto& tex = asset.textures[texIdx];
    if (!tex.imageIndex.has_value())
        return -1;
    return static_cast<int>(*tex.imageIndex);
}

static std::vector<RawMaterialData> ExtractMaterials(const fastgltf::Asset& asset)
{
    std::vector<RawMaterialData> out;
    out.reserve(asset.materials.size());

    static uint32_t matIdx = 0;

    for (const auto& mat : asset.materials)
    {
        RawMaterialData raw;
        raw.Name = mat.name.empty() ? "Material_" + std::to_string(matIdx++) : std::string(mat.name);

        // PBR metallic-roughness
        const auto& pbr     = mat.pbrData;
        raw.BaseColorFactor = Vector4(pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                                      pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        raw.MetallicFactor  = pbr.metallicFactor;
        raw.RoughnessFactor = pbr.roughnessFactor;

        if (pbr.baseColorTexture.has_value())
            raw.AlbedoMapIndex = ResolveImageIndex(asset, pbr.baseColorTexture->textureIndex);
        if (pbr.metallicRoughnessTexture.has_value())
            raw.MetallicRoughnessIndex = ResolveImageIndex(asset, pbr.metallicRoughnessTexture->textureIndex);

        if (mat.normalTexture.has_value())
        {
            raw.NormalMapIndex = ResolveImageIndex(asset, mat.normalTexture->textureIndex);
            raw.NormalScale    = mat.normalTexture->scale;
        }
        if (mat.occlusionTexture.has_value())
        {
            raw.OcclusionMapIndex  = ResolveImageIndex(asset, mat.occlusionTexture->textureIndex);
            raw.OcclusionStrength  = mat.occlusionTexture->strength;
        }
        if (mat.emissiveTexture.has_value())
            raw.EmissiveMapIndex = ResolveImageIndex(asset, mat.emissiveTexture->textureIndex);

        raw.EmissiveFactor = Vector3(mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2]);

        switch (mat.alphaMode)
        {
            case fastgltf::AlphaMode::Opaque: raw.Alpha = MaterialAsset::AlphaMode::Opaque; break;
            case fastgltf::AlphaMode::Mask:   raw.Alpha = MaterialAsset::AlphaMode::Mask;   break;
            case fastgltf::AlphaMode::Blend:  raw.Alpha = MaterialAsset::AlphaMode::Blend;  break;
        }
        raw.AlphaCutoff = mat.alphaCutoff;
        raw.DoubleSided = mat.doubleSided;

        out.push_back(std::move(raw));
    }

    return out;
}

// ---------------------------------------------------------------------------
// Mesh extraction -- one MeshAsset per glTF primitive
// ---------------------------------------------------------------------------

static MeshAsset ExtractPrimitive(const fastgltf::Asset& asset,
                                   const fastgltf::Primitive& prim,
                                   std::string name)
{
    MeshAsset out;
    out.Name = std::move(name);

    // --- Positions (required) ---
    auto* posAttr = prim.findAttribute("POSITION");
    if (posAttr == prim.attributes.end())
        return out;

    auto& posAccessor = asset.accessors[posAttr->accessorIndex];
    out.Vertices.resize(posAccessor.count);

    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, posAccessor,
        [&](const fastgltf::math::fvec3& pos, size_t i) {
            out.Vertices[i].Position = Vector3(pos.x(), pos.y(), pos.z());
            out.BoundsMin.x = std::min(out.BoundsMin.x, pos.x());
            out.BoundsMin.y = std::min(out.BoundsMin.y, pos.y());
            out.BoundsMin.z = std::min(out.BoundsMin.z, pos.z());
            out.BoundsMax.x = std::max(out.BoundsMax.x, pos.x());
            out.BoundsMax.y = std::max(out.BoundsMax.y, pos.y());
            out.BoundsMax.z = std::max(out.BoundsMax.z, pos.z());
        });

    // --- Normals ---
    if (auto* attr = prim.findAttribute("NORMAL"); attr != prim.attributes.end())
    {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, asset.accessors[attr->accessorIndex],
            [&](const fastgltf::math::fvec3& n, size_t i) {
                out.Vertices[i].Normal = Vector3(n.x(), n.y(), n.z());
            });
    }

    // --- UVs ---
    if (auto* attr = prim.findAttribute("TEXCOORD_0"); attr != prim.attributes.end())
    {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, asset.accessors[attr->accessorIndex],
            [&](const fastgltf::math::fvec2& uv, size_t i) {
                out.Vertices[i].TexCoord = Vector2(uv.x(), uv.y());
            });
    }

    // --- Tangents ---
    if (auto* attr = prim.findAttribute("TANGENT"); attr != prim.attributes.end())
    {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset, asset.accessors[attr->accessorIndex],
            [&](const fastgltf::math::fvec4& t, size_t i) {
                out.Vertices[i].Tangent     = Vector3(t.x(), t.y(), t.z());
                out.Vertices[i].TangentSign = t.w();
            });
    }

    // --- Indices ---
    if (prim.indicesAccessor.has_value())
    {
        auto& indexAccessor = asset.accessors[*prim.indicesAccessor];
        out.Indices.resize(indexAccessor.count);
        fastgltf::copyFromAccessor<uint32_t>(asset, indexAccessor, out.Indices.data());
    }
    else
    {
        // Non-indexed -- generate sequential indices
        out.Indices.resize(posAccessor.count);
        for (uint32_t i = 0; i < (uint32_t)posAccessor.count; ++i)
            out.Indices[i] = i;
    }

    return out;
}

RawGltfPackage FastGltfImporter::Import(const std::filesystem::path& path)
{
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None)
	{
		LOG_WARN_TO("asset",  "Failed to read GLTF file : {}", path.string());
		return {};
	}

	fastgltf::Parser parser;
	auto assetResult = parser.loadGltf(data.get(), path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);

	if (assetResult.error() != fastgltf::Error::None)
	{
		LOG_WARN_TO("asset", "Failed to parse GLTF : {}", fastgltf::getErrorMessage(assetResult.error()));
		return {};
	}

    const fastgltf::Asset& asset = assetResult.get();
	RawGltfPackage out;

    out.Textures  = ExtractImages(asset, path.parent_path());
    out.Materials = ExtractMaterials(asset);

    // One MeshAsset per primitive so each can carry its own material.
    static uint32_t s_PrimIdx = 0;

    for (const auto& mesh : asset.meshes)
    {
        std::string baseName = mesh.name.empty()
            ? "Mesh_" + std::to_string(s_PrimIdx) : std::string(mesh.name);
        bool multiPrim = mesh.primitives.size() > 1;

        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
        {
            const auto& prim = mesh.primitives[pi];
            std::string primName = multiPrim
                ? baseName + "_" + std::to_string(pi) : baseName;

            out.Meshes.push_back(ExtractPrimitive(asset, prim, std::move(primName)));

            int matIdx = prim.materialIndex.has_value()
                ? static_cast<int>(*prim.materialIndex) : -1;
            out.MeshMaterialIndices.push_back(matIdx);

            ++s_PrimIdx;
        }
    }

    LOG_INFO_TO("asset", "GLTF imported: {} primitives, {} materials, {} images -- {}",
        out.Meshes.size(), out.Materials.size(), out.Textures.size(), path.filename().string());

    return out;
}
