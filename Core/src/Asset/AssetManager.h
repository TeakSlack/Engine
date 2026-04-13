#ifndef ASSET_SYSTEM_H
#define ASSET_SYSTEM_H

#include <memory>
#include <filesystem>
#include <typeindex>
#include <queue>
#include <mutex>
#include "Engine.h"
#include "Asset/Asset.h"
#include "Asset/AssetLoader.h"
#include "Asset/TextureImporter.h"
#include "Asset/Meta.h"
#include "Util/ThreadPool.h"

// Forward declare so AssetRef can reference it
class AssetManager;

// -------------------------------------------------------------------------
// AssetRef<T> — RAII ref-counted handle to a loaded asset.
// Acquiring increments the ref count; destruction decrements it.
// When the count reaches zero the asset is freed.
// -------------------------------------------------------------------------
template<typename T>
class AssetRef
{
public:
    AssetRef() = default;
    explicit AssetRef(AssetHandle<T> handle);
    ~AssetRef();

    AssetRef(const AssetRef& other);
    AssetRef& operator=(const AssetRef& other);
    AssetRef(AssetRef&& other) noexcept;
    AssetRef& operator=(AssetRef&& other) noexcept;

    T*             Get()        const;
    T*             operator->() const { return Get(); }
    T&             operator*()  const { return *Get(); }
    bool           IsValid()    const { return m_Handle.IsValid(); }
    AssetHandle<T> Handle()     const { return m_Handle; }

private:
    void Acquire();
    void Release();

    AssetHandle<T> m_Handle;
};

struct GltfObject
{
	std::vector<AssetHandle<MeshAsset>>     Meshes;
	std::vector<AssetHandle<MaterialAsset>> MeshMaterials; // parallel to Meshes — the material for each mesh
	std::vector<AssetHandle<MaterialAsset>> Materials;     // all materials in the file, by glTF index
	std::vector<AssetHandle<TextureAsset>>  Textures;
};

class AssetManager : public IEngineSubmodule
{
public:
    AssetManager() : IEngineSubmodule("AssetManager") {}

	void Init() override;
	void Shutdown() override;
	void Tick(float deltaTime) override;

	template<typename T>
	AssetHandle<T> LoadAsset(const std::filesystem::path& path);

	GltfObject LoadGltf(const std::filesystem::path& path);

	template<typename T> void  AcquireAsset(AssetHandle<T> handle);
	template<typename T> void  ReleaseAsset(AssetHandle<T> handle);
	template<typename T> T*    GetAsset(AssetHandle<T> handle);

	std::filesystem::path GetAssetRoot() const { return m_AssetRoot; }

private:
	template<typename T>
	AssetHandle<T> AddAssetWithID(AssetID id, T&& asset, const std::filesystem::path& srcPath);
	void ScanMetaFiles();

	enum class AssetState { Pending, Ready, Failed };

	// Internal storage for loaded assets
	struct AssetRecord
	{
		std::unique_ptr<void, void(*)(void*)> data{ nullptr, [](void*){} };
		std::type_index                       type{ typeid(void) };
		std::filesystem::path                 sourcePath;
		uint32_t                              refCount = 0;
		AssetState                            state    = AssetState::Pending;
	};

	// Completed async job — written by worker thread, read by Tick on main thread
	struct CompletedJob
	{
		AssetID                               id;
		std::unique_ptr<void, void(*)(void*)> data{ nullptr, [](void*){} };
		AssetState                            state = AssetState::Failed;
	};

	std::unordered_map<AssetID, AssetRecord>                   m_Assets;
	std::unordered_map<std::filesystem::path, AssetID>        m_PathToID;
	std::unordered_map<std::filesystem::path, GltfObject>     m_GltfObjects;
	std::filesystem::path                                      m_AssetRoot;

	std::unique_ptr<ThreadPool>  m_ThreadPool;
	std::queue<CompletedJob>     m_CompletedJobs;
	std::mutex                   m_CompletedMutex;
};

template<typename T>
inline AssetHandle<T> AssetManager::AddAssetWithID(AssetID id, T&& asset, const std::filesystem::path& sourcePath)
{
	AssetRecord record;
	record.data = std::unique_ptr<void, void(*)(void*)>(
		new T(std::forward<T>(asset)),
		[](void* p) { delete static_cast<T*>(p); });
	record.type       = std::type_index(typeid(T));
	record.sourcePath = sourcePath;
	record.refCount   = 1;
	record.state      = AssetState::Ready;

	m_Assets.emplace(id, std::move(record));

	if (!sourcePath.empty())
		m_PathToID[sourcePath] = id;

	return AssetHandle<T>{ id };
}

template<typename T>
inline T* AssetManager::GetAsset(AssetHandle<T> handle)
{
    auto it = m_Assets.find(handle.id);
    if (it == m_Assets.end() || it->second.state != AssetState::Ready)
        return nullptr;
    return static_cast<T*>(it->second.data.get());
}

template<typename T>
inline void AssetManager::AcquireAsset(AssetHandle<T> handle)
{
    auto it = m_Assets.find(handle.id);
    if (it != m_Assets.end())
        ++it->second.refCount;
}

template<typename T>
inline void AssetManager::ReleaseAsset(AssetHandle<T> handle)
{
    auto it = m_Assets.find(handle.id);
    if (it == m_Assets.end())
        return;
    if (--it->second.refCount == 0)
    {
        m_PathToID.erase(it->second.sourcePath);
        m_Assets.erase(it);
    }
}

// ---------------------------------------------------------------------------
// LoadAsset<T> — kicks off an async load and returns a Pending handle
// immediately. The asset becomes available once Tick drains the job queue.
// ---------------------------------------------------------------------------
template<typename T>
inline AssetHandle<T> AssetManager::LoadAsset(const std::filesystem::path& path)
{
    std::filesystem::path fullPath = std::filesystem::weakly_canonical(m_AssetRoot / path);

    // Return existing handle if already queued or loaded
    auto it = m_PathToID.find(fullPath);
    if (it != m_PathToID.end())
        return AssetHandle<T>{ it->second };

    // Resolve stable ID from meta now (fast, main thread — no file I/O race)
    MetaFile meta = MetaFile::LoadOrCreate(fullPath);
    AssetID  id   = meta.RootGuid;
    meta.Save(fullPath);

    // Reserve a Pending slot so callers can hold a valid handle immediately
    AssetRecord record;
    record.type       = std::type_index(typeid(T));
    record.sourcePath = fullPath;
    record.refCount   = 1;
    record.state      = AssetState::Pending;
    m_Assets.emplace(id, std::move(record));
    m_PathToID[fullPath] = id;

    // Dispatch load to worker thread
    m_ThreadPool->Submit([this, id, fullPath]()
    {
        std::unique_ptr<T> asset;

        if constexpr (std::is_same_v<T, TextureAsset>)
        {
            StbTextureImporter loader;
            asset = loader.Load(fullPath);
        }
        else
        {
            static_assert(sizeof(T) == 0, "No IAssetLoader registered for this type");
        }

        CompletedJob job;
        job.id    = id;
        job.state = asset ? AssetState::Ready : AssetState::Failed;
        if (asset)
        {
            job.data = std::unique_ptr<void, void(*)(void*)>(
                asset.release(),
                [](void* p) { delete static_cast<T*>(p); });
        }

        std::lock_guard lock(m_CompletedMutex);
        m_CompletedJobs.push(std::move(job));
    });

    return AssetHandle<T>{ id };
}

// -------------------------------------------------------------------------
// AssetRef<T> implementation
// -------------------------------------------------------------------------
template<typename T>
AssetRef<T>::AssetRef(AssetHandle<T> handle) : m_Handle(handle)
{
    Acquire();
}

template<typename T>
AssetRef<T>::~AssetRef() { Release(); }

template<typename T>
AssetRef<T>::AssetRef(const AssetRef& other) : m_Handle(other.m_Handle)
{
    Acquire();
}

template<typename T>
AssetRef<T>& AssetRef<T>::operator=(const AssetRef& other)
{
    if (this != &other)
    {
        Release();
        m_Handle = other.m_Handle;
        Acquire();
    }
    return *this;
}

template<typename T>
AssetRef<T>::AssetRef(AssetRef&& other) noexcept : m_Handle(other.m_Handle)
{
    other.m_Handle = {};
}

template<typename T>
AssetRef<T>& AssetRef<T>::operator=(AssetRef&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_Handle       = other.m_Handle;
        other.m_Handle = {};
    }
    return *this;
}

template<typename T>
T* AssetRef<T>::Get() const
{
    AssetManager* assetManager = Engine::Get().GetSubmodule<AssetManager>();
    return assetManager->GetAsset(m_Handle);
}

template<typename T>
void AssetRef<T>::Acquire()
{
    AssetManager* assetManager = Engine::Get().GetSubmodule<AssetManager>();
    if (m_Handle.IsValid())
        assetManager->AcquireAsset(m_Handle);
}

template<typename T>
void AssetRef<T>::Release()
{
    AssetManager* assetManager = Engine::Get().GetSubmodule<AssetManager>();
    if (m_Handle.IsValid())
        assetManager->ReleaseAsset(m_Handle);
}

#endif // ASSET_SYSTEM_H