#ifndef MESH_BINNER_H
#define MESH_BINNER_H

#include "Math/Matrix4x4.h"
#include "Render/ICommandContext.h"
#include "Render/RenderPacket.h"
#include "Render/IGpuDevice.h"
#include "Asset/Asset.h"
#include "Asset/AssetManager.h"
#include <span>

class MeshBinner
{
public:
	struct Config
	{
		uint32_t InitialMaxVertices = 1000000;
		uint32_t InitialMaxIndices = 1000000;
		uint32_t MaxMaterials = 4096;
		uint32_t MaxTextures = 65535;
		uint32_t MaxDrawsPerBin = 8192;
		uint32_t MaxInstancesPerBin = 65535;
	};

	// Per-bin result — valid until the next Build() call.
	struct BinResult
	{
		GpuBuffer	  DrawArgsBuffer;  // DrawIndexedArgs[], GPU-readable
		GpuBuffer	  InstanceBuffer;  // PerInstanceData[], GPU-readable
		GpuBuffer	  DrawCountBuffer; // uint32_t — GPU culling writes here
		GpuBindingSet BindingSet;	   // InstanceBuffer (t2) + MaterialBuffer (t3) + TextureTable (t4)		
		uint32_t	  CpuDrawCount;    // CPU-side count before culling
	};

	void Init(IGpuDevice* device, const Config& config);
	void Build(std::span<const RenderPacket* const> packets, ICommandContext* cmd);
	BinResult& GetBin(DrawBinFlags bin);
	GpuBuffer MegaVertexBuffer() const { return m_MegaVB; }
	GpuBuffer MegaIndexBuffer() const { return m_MegaIB; }
	GpuDescriptorTable TextureTable() const { return m_TextureTable; }
	
private:
	struct alignas(16) GpuMaterialData
	{
		Vector4 BaseColorFactor;
		float MetallicFactor, RoughnessFactor, NormalScale, OcculusionStrength;
		uint32_t AlbedoIdx, NormalIdx, MetallicRoughnessIdx, OcclusionIdx, EmissiveIdx;
		uint32_t Pad[2];
	};
	static_assert(sizeof(GpuMaterialData) % 16 == 0, "GpuMaterialData must be 16-byte aligned");

	struct alignas(16) PerInstanceData
	{
		Matrix4x4 WorldMatrix;
		uint32_t MaterialIndex;
		uint32_t Pad[3];
	};
	static_assert(sizeof(PerInstanceData) % 16 == 0, "PerInstanceData must be 16-byte aligned");

	struct MeshRecord
	{
		uint32_t BaseVertex; // offset into MegaVB (in vertices, not bytes)
		uint32_t FirstIndex; // offset into MegaIB (in indices, not bytes)
		uint32_t IndexCount;
	};

	// ---- Per-bin CPU/GPU double state ----
	static constexpr int kNumBins = 4;
	static constexpr DrawBinFlags kBinFlags[kNumBins] = {
		DrawBinFlags::DepthPrepass,
		DrawBinFlags::ForwardOpaque,
		DrawBinFlags::Shadow,
		DrawBinFlags::Transparent,
	};

	struct BinState
	{
		BinResult                    Result;
		std::vector<DrawIndexedArgs> DrawArgsStaging;
		std::vector<PerInstanceData> InstanceStaging;
	};
	BinState m_Bins[kNumBins];

	const MeshRecord& EnsureMeshAppended(AssetHandle<MeshAsset> handle, ICommandContext* cmd);
	void              GrowMegaVB(uint32_t requiredVertices, ICommandContext* cmd);
	void              GrowMegaIB(uint32_t requiredIndices, ICommandContext* cmd);
	uint32_t		  RegisterMaterial(AssetHandle<MaterialAsset> material, ICommandContext* cmd);
	uint32_t		  RegisterTexture(AssetHandle<TextureAsset> texture);
	void			  RebuildSceneBindingSet();

	IGpuDevice*			m_Device = nullptr;
	AssetManager*		m_AssetManager = nullptr;

	GpuBuffer			m_MegaVB;
	GpuBuffer			m_MegaIB;
	uint32_t			m_NextVertex = 0;
	uint32_t			m_MaxVertices = 0;
	uint32_t			m_NextIndex = 0;
	uint32_t			m_MaxIndices = 0;

	GpuBindlessLayout	m_TextureTableLayout;
	GpuDescriptorTable	m_TextureTable;
	GpuTexture			m_MissingTexture;
	GpuSampler			m_DefaultSampler;
	uint32_t			m_NextTextureSlot = 0;

	GpuBuffer m_MaterialBuffer;
	uint32_t  m_MaxMaterials;
	bool      m_MaterialDirty = false;
	GpuBindingLayout m_SceneBindingLayout;

	// ---- Caches ----
	std::unordered_map<AssetID, MeshRecord, std::hash<CoreUUID>>	m_MeshRecords;
	std::unordered_map<AssetID, uint32_t, std::hash<CoreUUID>>		m_MaterialIndices;
	std::unordered_map<AssetID, uint32_t, std::hash<CoreUUID>>		m_TextureSlots;
	std::vector<GpuMaterialData>									m_MaterialStaging;
};

#endif // MESH_BINNER_H