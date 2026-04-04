#ifndef META_H
#define META_H

#include <unordered_map>

#include "Asset/Asset.h"
#include "Util/UUID.h"

struct MetaFile
{
	AssetID RootGuid = NullAssetId;
	std::unordered_map<std::string, AssetID> SubAssets;

	bool HasSubAsset(const std::string& name) const
	{
		return SubAssets.find(name) != SubAssets.end();
	}

	AssetID GetOrCreate(const std::string& name)
	{
		auto it = SubAssets.find(name);
		if (it != SubAssets.end())
			return it->second;
		AssetID newId = GenerateUUID();
		SubAssets[name] = newId;
		return newId;
	}
};

#endif // META_H