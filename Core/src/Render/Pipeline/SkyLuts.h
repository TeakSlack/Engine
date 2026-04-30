#ifndef SKY_LUTS_H
#define SKY_LUTS_H

#include "Render/FrameGraph/FrameGraph.h"
#include "Render/ShaderLoader.h"
#include "Math/Vector3.h"
#include "Render/NvrhiGpuDevice.h"

// Precomputes all static atmospheric LUTs in a single Init dispatch:
//   1. Transmittance LUT  — 2D, RGBA32_FLOAT, 256×64
//   2. Single Scattering  — 3D, RGBA16_FLOAT, (NuSize*MuSSize)×MuSize×RSize
// Both are computed once on startup and imported into the FrameGraph each frame.
class SkyLutsPass
{
public:
    struct Output
    {
        RGMutableTextureHandle TransmittanceLut;
        RGMutableTextureHandle SingleScatteringLut;
    };

    struct Config
    {
        float PlanetRadius    = 6360000.0f;  // metres
        float AtmosphereRadius = 6460000.0f; // metres

        // Earth-atmosphere scattering coefficients (per metre, Bruneton 2017)
        Vector3 BetaRayleigh = { 5.802e-6f, 1.356e-5f, 3.310e-5f };
        Vector3 BetaMie      = { 3.996e-6f, 3.996e-6f, 3.996e-6f };

        // Exp-scale values (km⁻¹): -1/ScaleHeight_km
        float RayleighScaleHeight = -1.0f / 8.0f;   // -0.125 /km
        float MieScaleHeight      = -1.0f / 1.2f;   // -0.833 /km

        // Transmittance LUT dimensions
        uint32_t TransmittanceWidth  = 256;
        uint32_t TransmittanceHeight = 64;

        // Single scattering LUT dimensions (stored as 3D, width = NuSize * MuSSize)
        uint32_t ScatteringNuSize   = 8;
        uint32_t ScatteringMuSSize  = 32;
        uint32_t ScatteringMuSize   = 128;
        uint32_t ScatteringRSize    = 32;

        RenderBackend Backend;
    };

    GpuTexture TransmittanceLut()    const { return m_TransmittanceLut; }
    GpuTexture SingleScatteringLut() const { return m_ScatteringLut; }

    void Init(IGpuDevice* device, ICommandContext* cmd, const Config& config)
    {
        m_Backend = config.Backend;
        m_TransmittanceWidth  = config.TransmittanceWidth;
        m_TransmittanceHeight = config.TransmittanceHeight;
        m_ScatteringWidth     = config.ScatteringNuSize * config.ScatteringMuSSize;
        m_ScatteringHeight    = config.ScatteringMuSize;
        m_ScatteringDepth     = config.ScatteringRSize;

        // ---- Constants ----
        // Config lengths are in SI metres; shader works in kilometres.
        constexpr float mToKm = 1.0f / 1000.0f;

        Constants cb = {
            .PlanetRadius       = config.PlanetRadius                              * mToKm,
            .AtmosphereHeight   = (config.AtmosphereRadius - config.PlanetRadius)  * mToKm,
            .RayleighScattering = config.BetaRayleigh                              * (1.0f / mToKm),
            .MieScattering      = config.BetaMie                                   * (1.0f / mToKm),
            .RayleighDensity = {
                { 0.0f, 0.0f, 0.0f,                      0.0f, 0.0f },  // layer 0: inactive (zero width)
                { 0.0f, 1.0f, config.RayleighScaleHeight, 0.0f, 0.0f },  // layer 1: exponential
            },
            .MieDensity = {
                { 0.0f, 0.0f, 0.0f,                  0.0f, 0.0f },
                { 0.0f, 1.0f, config.MieScaleHeight,  0.0f, 0.0f },
            },
            .OzoneDensity = {
                { 25.0f, 0.0f, 0.0f,  1.0f / 15.0f, -10.0f / 15.0f },  // 0–25 km: rising
                {  0.0f, 0.0f, 0.0f, -1.0f / 15.0f,  40.0f / 15.0f },  // 25+ km: falling
            },
        };

        BufferDesc cbDesc;
        cbDesc.ByteSize  = sizeof(Constants);
        cbDesc.Usage     = BufferUsage::Constant;
        cbDesc.DebugName = "SkyLutsConstants";
        m_ConstantBuffer = device->CreateBuffer(cbDesc);
        cmd->WriteBuffer(m_ConstantBuffer, &cb, sizeof(Constants));

        // ---- Transmittance LUT texture (2D) ----
        TextureDesc transmittanceDesc;
        transmittanceDesc.Format    = GpuFormat::RGBA32_FLOAT;
        transmittanceDesc.DebugName = "SkyTransmittanceLut";
        transmittanceDesc.Width     = m_TransmittanceWidth;
        transmittanceDesc.Height    = m_TransmittanceHeight;
        transmittanceDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;
        m_TransmittanceLut = device->CreateTexture(transmittanceDesc);

        // ---- Single scattering LUT texture (3D) ----
        TextureDesc scatteringDesc;
        scatteringDesc.Format    = GpuFormat::RGBA16_FLOAT;
        scatteringDesc.DebugName = "SkySingleScatteringLut";
        scatteringDesc.Width     = m_ScatteringWidth;
        scatteringDesc.Height    = m_ScatteringHeight;
        scatteringDesc.Depth     = m_ScatteringDepth;
        scatteringDesc.Dimension = TextureDimension::Texture3D;
        scatteringDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;
        m_ScatteringLut = device->CreateTexture(scatteringDesc);

        // ---- Sampler (used by scattering pass to read transmittance LUT) ----
        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = Filter::Linear;
        samplerDesc.MagFilter = Filter::Linear;
        samplerDesc.AddressU  = AddressMode::Clamp;
        samplerDesc.AddressV  = AddressMode::Clamp;
        m_LinearSampler = device->CreateSampler(samplerDesc);

        const bool vulkan = (m_Backend == RenderBackend::Vulkan);
        const char* transmittanceExt = vulkan ? "shaders/SkyLuts.mainTransmittance.cs.spv"
                                               : "shaders/SkyLuts.mainTransmittance.cs.cso";
        const char* scatteringExt    = vulkan ? "shaders/SkyLuts.mainScattering.cs.spv"
                                               : "shaders/SkyLuts.mainScattering.cs.cso";

        // ---- Transmittance pipeline ----
        {
            auto bytecode = ShaderLoader::LoadBinary(transmittanceExt);
            CORE_ASSERT(!bytecode.empty(), "Failed to load SkyLuts transmittance shader");

            ShaderDesc csDesc;
            csDesc.DebugName  = "SkyLutsTransmittanceCS";
            csDesc.EntryPoint = "mainTransmittance";
            csDesc.Stage      = ShaderStage::Compute;
            csDesc.Bytecode   = bytecode.data();
            csDesc.ByteSize   = bytecode.size();
            m_TransmittanceShader = device->CreateShader(csDesc);

            BindingLayoutDesc layoutDesc;
            layoutDesc.Items = {
                BindingLayoutItem::ConstantBuffer(0, ShaderStage::Compute),  // b0
                BindingLayoutItem::StorageTexture(0, ShaderStage::Compute),  // u0 — TransmittanceLUT
            };
            m_TransmittanceLayout = device->CreateBindingLayout(layoutDesc);

            BindingSetDesc setDesc;
            setDesc.Items = {
                BindingItem::ConstantBuffer(0, m_ConstantBuffer),
                BindingItem::StorageTexture(0, m_TransmittanceLut),
            };
            m_TransmittanceSet = device->CreateBindingSet(setDesc, m_TransmittanceLayout);

            ComputePipelineDesc pipelineDesc;
            pipelineDesc.CS             = m_TransmittanceShader;
            pipelineDesc.BindingLayouts = { m_TransmittanceLayout };
            m_TransmittancePipeline = device->CreateComputePipeline(pipelineDesc);
        }

        // ---- Scattering pipeline ----
        {
            auto bytecode = ShaderLoader::LoadBinary(scatteringExt);
            CORE_ASSERT(!bytecode.empty(), "Failed to load SkyLuts scattering shader");

            ShaderDesc csDesc;
            csDesc.DebugName  = "SkyLutsScatteringCS";
            csDesc.EntryPoint = "mainScattering";
            csDesc.Stage      = ShaderStage::Compute;
            csDesc.Bytecode   = bytecode.data();
            csDesc.ByteSize   = bytecode.size();
            m_ScatteringShader = device->CreateShader(csDesc);

            BindingLayoutDesc layoutDesc;
            layoutDesc.Items = {
                BindingLayoutItem::ConstantBuffer(0, ShaderStage::Compute),  // b0
                BindingLayoutItem::Texture(0, ShaderStage::Compute),         // t0 — TransmittanceSRV
                BindingLayoutItem::Sampler(0, ShaderStage::Compute),         // s0
                BindingLayoutItem::StorageTexture(1, ShaderStage::Compute),  // u1 — ScatteringLUT
            };
            m_ScatteringLayout = device->CreateBindingLayout(layoutDesc);

            BindingSetDesc setDesc;
            setDesc.Items = {
                BindingItem::ConstantBuffer(0, m_ConstantBuffer),
                BindingItem::Texture(0, m_TransmittanceLut),
                BindingItem::Sampler(0, m_LinearSampler),
                BindingItem::StorageTexture(1, m_ScatteringLut),
            };
            m_ScatteringSet = device->CreateBindingSet(setDesc, m_ScatteringLayout);

            ComputePipelineDesc pipelineDesc;
            pipelineDesc.CS             = m_ScatteringShader;
            pipelineDesc.BindingLayouts = { m_ScatteringLayout };
            m_ScatteringPipeline = device->CreateComputePipeline(pipelineDesc);
        }

        // ---- Dispatch transmittance ----
        cmd->SetComputePipeline(m_TransmittancePipeline);
        cmd->SetComputeBindingSet(m_TransmittanceSet);
        {
            DispatchArgs d;
            d.GroupX = (m_TransmittanceWidth  + 7) / 8;
            d.GroupY = (m_TransmittanceHeight + 7) / 8;
            d.GroupZ = 1;
            cmd->Dispatch(d);
        }

        // Transition transmittance LUT to SRV so the scattering pass can read it.
        cmd->TransitionTexture(m_TransmittanceLut, ResourceLayout::ShaderResource);

        // ---- Dispatch single scattering ----
        cmd->SetComputePipeline(m_ScatteringPipeline);
        cmd->SetComputeBindingSet(m_ScatteringSet);
        {
            DispatchArgs d;
            d.GroupX = (m_ScatteringWidth  + 7) / 8;
            d.GroupY = (m_ScatteringHeight + 7) / 8;
            d.GroupZ = (m_ScatteringDepth  + 7) / 8;
            cmd->Dispatch(d);
        }

        cmd->TransitionTexture(m_TransmittanceLut, ResourceLayout::ShaderResource);
        cmd->TransitionTexture(m_ScatteringLut,    ResourceLayout::ShaderResource);
    }

    void Shutdown(IGpuDevice* device)
    {
        device->DestroyTexture(m_TransmittanceLut);
        device->DestroyTexture(m_ScatteringLut);
        device->DestroyBuffer(m_ConstantBuffer);
        device->DestroySampler(m_LinearSampler);

        device->DestroyShader(m_TransmittanceShader);
        device->DestroyBindingLayout(m_TransmittanceLayout);
        device->DestroyBindingSet(m_TransmittanceSet);
        device->DestroyComputePipeline(m_TransmittancePipeline);

        device->DestroyShader(m_ScatteringShader);
        device->DestroyBindingLayout(m_ScatteringLayout);
        device->DestroyBindingSet(m_ScatteringSet);
        device->DestroyComputePipeline(m_ScatteringPipeline);

        m_TransmittanceShader = {};
        m_ScatteringShader    = {};
    }

    Output Add(FrameGraph& fg)
    {
        TextureDesc transmittanceDesc;
        transmittanceDesc.Format    = GpuFormat::RGBA32_FLOAT;
        transmittanceDesc.DebugName = "SkyTransmittanceLut";
        transmittanceDesc.Width     = m_TransmittanceWidth;
        transmittanceDesc.Height    = m_TransmittanceHeight;
        transmittanceDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;

        TextureDesc scatteringDesc;
        scatteringDesc.Format    = GpuFormat::RGBA16_FLOAT;
        scatteringDesc.DebugName = "SkySingleScatteringLut";
        scatteringDesc.Width     = m_ScatteringWidth;
        scatteringDesc.Height    = m_ScatteringHeight;
        scatteringDesc.Depth     = m_ScatteringDepth;
        scatteringDesc.Dimension = TextureDimension::Texture3D;
        scatteringDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;

        return {
            fg.ImportMutableTexture(m_TransmittanceLut, transmittanceDesc, ResourceLayout::ShaderResource),
            fg.ImportMutableTexture(m_ScatteringLut,    scatteringDesc,    ResourceLayout::ShaderResource),
        };
    }

private:
    // Transmittance pass
    GpuComputePipeline m_TransmittancePipeline;
    GpuBindingSet      m_TransmittanceSet;
    GpuBindingLayout   m_TransmittanceLayout;
    GpuShader          m_TransmittanceShader;
    GpuTexture         m_TransmittanceLut;

    // Single scattering pass
    GpuComputePipeline m_ScatteringPipeline;
    GpuBindingSet      m_ScatteringSet;
    GpuBindingLayout   m_ScatteringLayout;
    GpuShader          m_ScatteringShader;
    GpuTexture         m_ScatteringLut;

    // Shared
    GpuBuffer  m_ConstantBuffer;
    GpuSampler m_LinearSampler;

    uint32_t m_TransmittanceWidth = 0, m_TransmittanceHeight = 0;
    uint32_t m_ScatteringWidth = 0, m_ScatteringHeight = 0, m_ScatteringDepth = 0;
    RenderBackend m_Backend;

    struct DensityProfileLayer
    {
        float Width;
        float ExpTerm;
        float ExpScale;
        float LinearTerm;
        float ConstantTerm;
        float _pad[3]; // pad to 32 bytes for cbuffer alignment
    };

    struct alignas(16) Constants
    {
        float   PlanetRadius;       // offset 0
        float   AtmosphereHeight;   // offset 4
        float   _pad0[2];           // offset 8  — pad to 16

        Vector3 RayleighScattering; // offset 16
        float   _pad1;              // offset 28 — pad to 32

        Vector3 MieScattering;      // offset 32
        float   _pad2;              // offset 44 — pad to 48

        DensityProfileLayer RayleighDensity[2]; // offset 48  (64 bytes)
        DensityProfileLayer MieDensity[2];      // offset 112 (64 bytes)
        DensityProfileLayer OzoneDensity[2];    // offset 176 (64 bytes)
        // Total: 240 bytes
    };
    static_assert(sizeof(Constants) % 16 == 0, "Constants must be 16-byte aligned");
};

#endif // SKY_LUTS_H
