#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include <memory>
#include <filesystem>

// TODO: implement mesh-material loading for OBJ and FBX
// and material loading using stb_image
template<typename T>
class IAssetLoader
{
public:
	virtual ~IAssetLoader();
	virtual std::unique_ptr<T> Load(const std::filesystem::path& path);
};

#endif // ASSET_LOADER_H