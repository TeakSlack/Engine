#ifndef SKY_TRANSMITTANCE_H
#define SKY_TRANSMITTANCE_H

#include "Render/FrameGraph/FrameGraph.h"
#include "Render/ShaderLoader.h"
#include "Math/Vector3.h"
#include "Render/NvrhiGpuDevice.h"

class SkyTransmittancePass
{
public:
	struct Input  {};

	struct Output
	{
		RGMutableTextureHandle TransmittanceLut;
	};

	struct Config
	{
		float PlanetRadius = 6360000;
		float AtmosphereRadius = 6460000;

		// Earth-atmosphere scattering coefficients (per metre, from Bruneton 2017 [2])
		Vector3 BetaRayleigh = { 5.802e-6f, 1.356e-5f, 3.310e-5f };
		Vector3 BetaMie      = { 3.996e-6f, 3.996e-6f, 3.996e-6f };

		float RayleighScaleHeight = -1.0f / 8.0f; // -0.125km
		float MieScaleHeight = -1.0f / 1.2f; // -0.833km

		uint32_t Width = 256;
		uint32_t Height = 64;
		RenderBackend Backend;
	};

	// Returns the underlying persistent texture for external SRV binding (e.g. a blit pass).
	GpuTexture Lut() const { return m_Lut; }

	void Init(IGpuDevice* device, ICommandContext* uploadCmd, const Config& config)
	{
		// ---- Create push constants ----
		// Config is in SI metres; the shader works in kilometres.
		// Lengths  ÷ 1000 : m  → km
		// Scattering × 1000 : /m → /km  (optical depth = β·ds, both scale together)
		constexpr float mToKm = 1.0f / 1000.0f;
		const float rayleighScaleHeightKm = config.RayleighScaleHeight * mToKm;
		const float mieScaleHeightKm      = config.MieScaleHeight      * mToKm;

		Constants cb = {
			.PlanetRadius       = config.PlanetRadius                             * mToKm,
			.AtmosphereHeight   = (config.AtmosphereRadius - config.PlanetRadius) * mToKm,
			.RayleighScattering = config.BetaRayleigh                             * (1.0f / mToKm),
			.MieScattering      = config.BetaMie                                  * (1.0f / mToKm),
			.RayleighDensity = {
				{ 0.0f, 0.0f, 0.0f,                          0.0f, 0.0f },  // layer 0: zero width, inactive
				{ 0.0f, 1.0f, -1.0f / rayleighScaleHeightKm, 0.0f, 0.0f },  // layer 1: standard exponential
			},
			.MieDensity = {
				{ 0.0f, 0.0f, 0.0f,                     0.0f, 0.0f },
				{ 0.0f, 1.0f, -1.0f / mieScaleHeightKm, 0.0f, 0.0f },
			},
			.OzoneDensity = {
				{ 25.0f, 0.0f, 0.0f,  1.0f / 15.0f, -10.0f / 15.0f },  // 0–25 km: rising
				{  0.0f, 0.0f, 0.0f, -1.0f / 15.0f,  40.0f / 15.0f },  // 25+ km: falling
			},
		};

		m_Backend = config.Backend;

		// ---- Create constant buffer ----
		BufferDesc cbDesc;
		cbDesc.ByteSize = sizeof(Constants);
		cbDesc.Usage = BufferUsage::Constant;
		cbDesc.DebugName = "SkyTransmittanceConstants";
		m_ConstantBuffer = device->CreateBuffer(cbDesc);

		// Caller opens/closes command context around Init
		uploadCmd->WriteBuffer(m_ConstantBuffer, &cb, sizeof(Constants));

		m_Width  = config.Width;
		m_Height = config.Height;

		// ---- Create LUT texture ----
		TextureDesc lutDesc;
		lutDesc.Format    = GpuFormat::RGBA32_FLOAT;
		lutDesc.DebugName = "SkyTransmittanceLut";
		lutDesc.Width     = m_Width;
		lutDesc.Height    = m_Height;
		lutDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;
		m_Lut = device->CreateTexture(lutDesc);

		// ---- Create compute shader ----
		std::vector<uint8_t> csData;
		if(m_Backend == RenderBackend::D3D12)
		{
			csData = ShaderLoader::LoadBinary("shaders/SkyTransmittance.cs.cso");
		}
		else if(m_Backend == RenderBackend::Vulkan)
		{
			csData = ShaderLoader::LoadBinary("shaders/SkyTransmittance.cs.spv");
		}
		CORE_ASSERT(!csData.empty(), "Failed to load compute shader for SkyTransmittancePass");

		ShaderDesc csDesc;
		csDesc.DebugName = "SkyTransmittanceCS";
		csDesc.EntryPoint = "main";
		csDesc.Stage = ShaderStage::Compute;
		csDesc.Bytecode = csData.data();
		csDesc.ByteSize = csData.size();
		m_ComputeShader = device->CreateShader(csDesc);

		// ---- Binding layout ----
		BindingLayoutDesc layoutDesc;
		layoutDesc.Items = {
			BindingLayoutItem::ConstantBuffer(0, ShaderStage::Compute),  // b0
			BindingLayoutItem::StorageTexture(0, ShaderStage::Compute),  // u0 — TransmittanceLUT
		};
		m_BindingLayout = device->CreateBindingLayout(layoutDesc);

		// ---- Binding set ----
		BindingSetDesc bindingDesc;
		bindingDesc.Items = {
			BindingItem::ConstantBuffer(0, m_ConstantBuffer),
			BindingItem::StorageTexture(0, m_Lut),
		};

		m_BindingSet = device->CreateBindingSet(bindingDesc, m_BindingLayout);

		// ---- Compute pipeline ----
		ComputePipelineDesc pipelineDesc;
		pipelineDesc.CS = m_ComputeShader;
		pipelineDesc.BindingLayouts = { m_BindingLayout };
		m_Pipeline = device->CreateComputePipeline(pipelineDesc);

		// ---- Dispatch once — LUT is static, no need to recompute per frame ----
		uploadCmd->SetComputePipeline(m_Pipeline);
		uploadCmd->SetComputeBindingSet(m_BindingSet);

		DispatchArgs dispatch;
		dispatch.GroupX = (m_Width  + 7) / 8;
		dispatch.GroupY = (m_Height + 7) / 8;
		dispatch.GroupZ = 1;
		uploadCmd->Dispatch(dispatch);

		// Transition to ShaderResource so Add() can import it as readable.
		uploadCmd->TransitionTexture(m_Lut, ResourceLayout::ShaderResource);
	}

	void Shutdown(IGpuDevice* device)
	{
		device->DestroyTexture(m_Lut);
		device->DestroyBuffer(m_ConstantBuffer);
		device->DestroyShader(m_ComputeShader);
		device->DestroyBindingLayout(m_BindingLayout);
		device->DestroyBindingSet(m_BindingSet);
		device->DestroyComputePipeline(m_Pipeline);
		m_ComputeShader = {};
	}

	// Imports the pre-computed LUT into the FrameGraph as a readable resource.
	// The LUT was dispatched once during Init and never changes, so no pass is
	// added — downstream passes simply declare a ReadTexture dependency on it.
	Output Add(FrameGraph& fg)
	{
		TextureDesc lutDesc;
		lutDesc.Format    = GpuFormat::RGBA16_FLOAT;
		lutDesc.DebugName = "SkyTransmittanceLut";
		lutDesc.Width     = m_Width;
		lutDesc.Height    = m_Height;
		lutDesc.Usage     = TextureUsage::Storage | TextureUsage::ShaderResource;

		return { fg.ImportMutableTexture(m_Lut, lutDesc, ResourceLayout::ShaderResource) };
	}

private:
	GpuComputePipeline m_Pipeline;
	GpuBindingSet      m_BindingSet;
	GpuBindingLayout   m_BindingLayout;
	GpuShader          m_ComputeShader;
	GpuBuffer          m_ConstantBuffer;
	GpuTexture         m_Lut;
	uint32_t           m_Width, m_Height;
	RenderBackend      m_Backend;

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

#endif // SKY_TRANSMITTANCE_H