#ifndef HLSL_IMPORTER_H
#define HLSL_IMPORTER_H

#include "Asset/AssetLoader.h"
#include "Asset/Asset.h"
#include "Render/NvrhiGpuDevice.h"
#include <filesystem>
#include <vector>

#ifdef CORE_HAS_DXC
struct IDxcUtils;
struct IDxcCompiler3;
#endif

class HlslImporter : public IAssetLoader<ShaderAsset>
{
public:
    HlslImporter(RenderBackend backend, std::vector<std::filesystem::path> includePaths);
    ~HlslImporter();

    std::unique_ptr<ShaderAsset> Load(const std::filesystem::path& path) override;

private:
    static ShaderStage    StageFromPath(const std::filesystem::path& path);
    static const wchar_t* ProfileFromStage(ShaderStage stage);

    RenderBackend                      m_Backend;
    std::vector<std::filesystem::path> m_IncludePaths;

#ifdef CORE_HAS_DXC
    IDxcUtils*     m_Utils    = nullptr;
    IDxcCompiler3* m_Compiler = nullptr;
#endif
};

#endif // HLSL_IMPORTER_H
