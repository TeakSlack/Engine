#include "Asset/HlslImporter.h"
#include "Util/Log.h"
#include "Util/Assert.h"

#ifdef CORE_HAS_DXC
#include <dxc/dxcapi.h>
#endif

HlslImporter::HlslImporter(RenderBackend backend, std::vector<std::filesystem::path> includePaths)
    : m_Backend(backend)
    , m_IncludePaths(std::move(includePaths))
{
#ifdef CORE_HAS_DXC
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils,     IID_PPV_ARGS(&m_Utils));
    CORE_ASSERT(SUCCEEDED(hr), "Failed to create IDxcUtils");
    hr          = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler));
    CORE_ASSERT(SUCCEEDED(hr), "Failed to create IDxcCompiler3");
#endif
}

HlslImporter::~HlslImporter()
{
#ifdef CORE_HAS_DXC
    if (m_Compiler) m_Compiler->Release();
    if (m_Utils)    m_Utils->Release();
#endif
}

ShaderStage HlslImporter::StageFromPath(const std::filesystem::path& path)
{
    std::string ext = path.stem().extension().string();
    if (ext == ".vs") return ShaderStage::Vertex;
    if (ext == ".ps") return ShaderStage::Pixel;
    if (ext == ".cs") return ShaderStage::Compute;
    if (ext == ".hs") return ShaderStage::Hull;
    if (ext == ".ds") return ShaderStage::Domain;
    if (ext == ".gs") return ShaderStage::Geometry;
    return ShaderStage::None;
}

const wchar_t* HlslImporter::ProfileFromStage(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex:   return L"vs_6_6";
    case ShaderStage::Hull:     return L"hs_6_6";
    case ShaderStage::Domain:   return L"ds_6_6";
    case ShaderStage::Geometry: return L"gs_6_6";
    case ShaderStage::Pixel:    return L"ps_6_6";
    case ShaderStage::Compute:  return L"cs_6_6";
    default:                    return nullptr;
    }
}

std::unique_ptr<ShaderAsset> HlslImporter::Load(const std::filesystem::path& path)
{
#ifndef CORE_HAS_DXC
    LOG_ERROR_TO("shader", "HlslImporter: built without DXC support");
    return nullptr;
#else
    ShaderStage stage = StageFromPath(path);
    if (stage == ShaderStage::None)
    {
        LOG_ERROR_TO("shader", "HlslImporter: cannot determine stage from '{}'", path.string());
        return nullptr;
    }

    IDxcBlobEncoding* sourceBlob = nullptr;
    HRESULT hr = m_Utils->LoadFile(path.c_str(), nullptr, &sourceBlob);
    if (FAILED(hr) || !sourceBlob)
    {
        LOG_ERROR_TO("shader", "HlslImporter: failed to open '{}'", path.string());
        return nullptr;
    }

    std::vector<std::wstring> includeStrings;
    includeStrings.push_back(path.parent_path().wstring());
    for (auto& p : m_IncludePaths)
        includeStrings.push_back(p.wstring());

    std::vector<LPCWSTR> args;
    args.push_back(L"-E"); args.push_back(L"main");
    args.push_back(L"-T"); args.push_back(ProfileFromStage(stage));
    for (auto& s : includeStrings)
    {
        args.push_back(L"-I");
        args.push_back(s.c_str());
    }
    if (m_Backend == RenderBackend::Vulkan)
    {
        args.push_back(L"-spirv");
        args.push_back(L"-fspv-target-env=vulkan1.1");
        args.push_back(L"-fvk-b-shift"); args.push_back(L"0"); args.push_back(L"0");
        args.push_back(L"-fvk-t-shift"); args.push_back(L"2"); args.push_back(L"0");
        args.push_back(L"-fvk-s-shift"); args.push_back(L"3"); args.push_back(L"0");
        args.push_back(L"-fvk-u-shift"); args.push_back(L"4"); args.push_back(L"0");
    }

    IDxcIncludeHandler* includeHandler = nullptr;
    m_Utils->CreateDefaultIncludeHandler(&includeHandler);

    DxcBuffer srcBuf{ sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize(), DXC_CP_ACP };
    IDxcResult* result = nullptr;
    hr = m_Compiler->Compile(&srcBuf, args.data(), static_cast<UINT32>(args.size()),
                             includeHandler, IID_PPV_ARGS(&result));

    includeHandler->Release();
    sourceBlob->Release();

    IDxcBlobUtf8* errors = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0)
        LOG_ERROR_TO("shader", "DXC [{}]: {}", path.filename().string(), errors->GetStringPointer());
    if (errors) errors->Release();

    HRESULT status;
    result->GetStatus(&status);
    if (FAILED(status) || FAILED(hr))
    {
        result->Release();
        return nullptr;
    }

    IDxcBlob* bytecode = nullptr;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr);
    result->Release();

    if (!bytecode)
        return nullptr;

    auto asset = std::make_unique<ShaderAsset>();
    const auto* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
    asset->Bytecode.assign(data, data + bytecode->GetBufferSize());
    asset->Stage      = stage;
    asset->EntryPoint = "main";
    bytecode->Release();
    return asset;
#endif
}
