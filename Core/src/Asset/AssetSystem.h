#ifndef ASSET_SYSTEM_H
#define ASSET_SYSTEM_H

#include <memory>
#include <filesystem>
#include <typeindex>
#include "Engine.h"
#include "Asset/Asset.h"

class AssetSystem : public IEngineSubmodule
{
	static AssetSystem& Get();

	void Init() override;
	void Shutdown() override;
	void Tick(float deltaTime) override;

	template<typename T>
	AssetHandle<T> Load(const std::filesystem::path& path);

	template<typename T>
	void Unload(AssetHandle<T> handle);

	template<typename T>
	T* Get(AssetHandle<T> handle);

private:
	// Internal storage for loaded assets
	struct AssetRecord
	{
		std::unique_ptr<void, void(*)(void*)> data; // type-erased
		std::type_index type;
		std::filesystem::path sourcePath;
		uint32_t refCount = 0;
	};

	std::unordered_map<AssetID, AssetRecord> m_Assets;
	std::unordered_map<std::filesystem::path, AssetID> m_PathToID;
};

#endif // ASSET_SYSTEM_H