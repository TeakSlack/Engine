#ifndef GLTF_IMPORTER_H
#define GLTF_IMPORTER_H	

#include "Asset.h"
#include <vector>
#include <filesystem>

struct GltfPackage
{
	std::vector<AssetHandle<MeshAsset>> Meshes;
	std::vector<AssetHandle<MaterialAsset>> Materials;
	std::vector<AssetHandle<TextureAsset>> Textures;
};

class IGltfImporter
{
	virtual ~IGltfImporter() = default;
	virtual GltfPackage Import(const std::filesystem::path& path) = 0;
};

class FastGltfImporter : public IGltfImporter
{
public:
	virtual GltfPackage Import(const std::filesystem::path& path) override;
};

#endif // GLTF_IMPORTER_H