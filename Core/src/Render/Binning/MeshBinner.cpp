#include "MeshBinner.h"
#include "Render/Vertex.h"
#include "Engine.h"
#include <algorithm>

void MeshBinner::Init(IGpuDevice* device, const Config& config)
{
	m_Device = device;
	m_AssetManager = Engine::Get().GetSubmodule<AssetManager>();

	// --- Vertex mega buffer ----
	m_MaxVertices = config.InitialMaxVertices;
	BufferDesc vbDesc;
	vbDesc.ByteSize = m_MaxVertices * sizeof(Vertex);
	vbDesc.Usage = BufferUsage::Vertex | BufferUsage::Storage;
	vbDesc.DebugName = "MegaVB";
	m_MegaVB = m_Device->CreateBuffer(vbDesc);

	// --- Index mega buffer ----
	m_MaxIndices = config.InitialMaxIndices;
	BufferDesc ibDesc;
	ibDesc.ByteSize = m_MaxIndices * sizeof(uint32_t);
	ibDesc.Usage = BufferUsage::Index | BufferUsage::Storage;
	ibDesc.DebugName = "MegaIB";
	m_MegaIB = m_Device->CreateBuffer(ibDesc);

	// ---- Material buffer (StructuredBuffer<GpuMaterialData> ----
	m_MaxMaterials = config.MaxMaterials;
	BufferDesc matDesc;
	matDesc.ByteSize = m_MaxMaterials * sizeof(GpuMaterialData);
	matDesc.Usage = BufferUsage::Storage;
	matDesc.DebugName = "MegaMat";
	m_MaterialBuffer = m_Device->CreateBuffer(matDesc);

	// ---- Bindless texture table ----
	BindlessLayoutDesc bindlessDesc;
	bindlessDesc.ResourceType = BindlessResourceType::Texture;
	bindlessDesc.MaxResources = config.MaxTextures;
	m_TextureTableLayout = m_Device->CreateBindlessLayout(bindlessDesc);
	m_TextureTable = m_Device->CreateDescriptorTable(m_TextureTableLayout);

	// ---- B&W checkered texture for missing textures ----
	TextureDesc checkerDesc;
	checkerDesc.Width = 2;
	checkerDesc.Height = 2;
	checkerDesc.Format = GpuFormat::RGBA8_UNORM;
	checkerDesc.DebugName = "CheckerTexture";
	m_MissingTexture = m_Device->CreateTexture(checkerDesc);

	// ---- Default sampler ----
	SamplerDesc samplerDesc;
	m_DefaultSampler = m_Device->CreateSampler(samplerDesc);

	// ---- Per-bin GPU buffers ----
	for (size_t b = 0; b < kNumBins; b++)
	{
		BufferDesc drawArgsDesc;
		drawArgsDesc.ByteSize = config.MaxDrawsPerBin * sizeof(DrawIndexedArgs);
		drawArgsDesc.Usage = BufferUsage::Indirect | BufferUsage::Storage;
		drawArgsDesc.DebugName = "DrawArgsBuffer";
		m_Bins[b].Result.DrawArgsBuffer = device->CreateBuffer(drawArgsDesc);

		BufferDesc instanceDesc;
		instanceDesc.ByteSize = config.MaxInstancesPerBin * sizeof(PerInstanceData);
		instanceDesc.Usage = BufferUsage::Storage;
		instanceDesc.DebugName = "InstanceBuffer";
		m_Bins[b].Result.InstanceBuffer = device->CreateBuffer(instanceDesc);

		BufferDesc countDesc;
		countDesc.ByteSize = sizeof(uint32_t);
		countDesc.Usage = BufferUsage::Indirect | BufferUsage::Storage;
		countDesc.DebugName = "DrawCountBuffer";
		m_Bins[b].Result.DrawCountBuffer = device->CreateBuffer(countDesc);
	}

	RebuildSceneBindingSet();
}

void MeshBinner::Build(std::span<const RenderPacket* const> packets, ICommandContext* cmd)
{
	// ---- Clear per-frame staging data ----
	for (auto& bin : m_Bins)
	{
		bin.DrawArgsStaging.clear();
		bin.InstanceStaging.clear();
		bin.Result.CpuDrawCount = 0;
	}

	// ---- Group packets by mesh handle ----
	std::unordered_map<AssetID, std::vector<uint32_t>, std::hash<CoreUUID>> meshGroups;
	meshGroups.reserve(packets.size());

	for (uint32_t i = 0; i < packets.size(); ++i)
		meshGroups[packets[i]->Mesh.id].push_back(i);

	// ---- For each group, emit one draw per bin ----
	for (auto& [meshId, indices] : meshGroups)
	{
		AssetHandle<MeshAsset> meshHandle = packets[indices[0]]->Mesh;
		const MeshRecord& mesh = EnsureMeshAppended(meshHandle, cmd);

		for (int b = 0; b < kNumBins; b++)
		{
			DrawBinFlags flag = kBinFlags[b];
			uint32_t firstInstance = static_cast<uint32_t>(m_Bins[b].InstanceStaging.size());
			uint32_t instanceCount = 0;

			for (uint32_t idx : indices)
			{
				const RenderPacket* p = packets[idx];
				if (!(static_cast<uint8_t>(p->Bin) & static_cast<uint8_t>(flag)))
					continue;

				uint32_t matIdx = RegisterMaterial(p->Material, cmd);
				m_Bins[b].InstanceStaging.push_back(PerInstanceData{
					p->WorldTransform,
					matIdx,
					{0, 0, 0}
					});
				instanceCount++;
			}

			if (instanceCount == 0) continue;

			m_Bins[b].DrawArgsStaging.push_back(DrawIndexedArgs{
				.IndexCount = mesh.IndexCount,
				.InstanceCount = instanceCount,
				.StartIndex = mesh.FirstIndex,
				.BaseVertex = static_cast<int32_t>(mesh.BaseVertex),
				.StartInstance = firstInstance,  // shader reads g_Instances[SV_InstanceID]
				});
			m_Bins[b].Result.CpuDrawCount++;
		}
	}
	// ---- Upload material buffer if dirty ----
	if (m_MaterialDirty)
	{
		cmd->WriteBuffer(m_MaterialBuffer,
			m_MaterialStaging.data(),
			m_MaterialStaging.size() * sizeof(GpuMaterialData));
		RebuildSceneBindingSet(); // Recreate binding sets to bind the new buffer (could be optimized by using dynamic offsets or a bindless array)
		m_MaterialDirty = false;
	}

	// ---- Upload per-bin draw args and instance data ----
	for (auto& bin : m_Bins)
	{
		uint32_t drawCount = bin.Result.CpuDrawCount;
		if (drawCount == 0) continue;

		cmd->WriteBuffer(bin.Result.DrawArgsBuffer,
			bin.DrawArgsStaging.data(),
			drawCount * sizeof(DrawIndexedArgs));

		cmd->WriteBuffer(bin.Result.InstanceBuffer,
			bin.InstanceStaging.data(),
			bin.InstanceStaging.size() * sizeof(PerInstanceData));

		// Seed the count buffer with the CPU count.
		// A GPU culling pass overwrites it with the surviving draw count.
		cmd->WriteBuffer(bin.Result.DrawCountBuffer, &drawCount, sizeof(uint32_t));
	}
}

MeshBinner::BinResult& MeshBinner::GetBin(DrawBinFlags bin)
{
	for (size_t b = 0; b < kNumBins; b++)
	{
		if (kBinFlags[b] == bin)
			return m_Bins[b].Result;
	}

	CORE_ASSERT(false, "Invalid bin flag");
	return m_Bins[0].Result; // Fallback to avoid compiler warning
}

const MeshBinner::MeshRecord& MeshBinner::EnsureMeshAppended(AssetHandle<MeshAsset> handle, ICommandContext* cmd)
{
	auto it = m_MeshRecords.find(handle.id);
	if (it != m_MeshRecords.end()) return it->second;

	const MeshAsset* mesh = m_AssetManager->GetAsset(handle);
	CORE_ASSERT(mesh, "Caller must guarantee asset is Ready"); // caller guarantees asset is Ready

	uint32_t numVerts = static_cast<uint32_t>(mesh->Vertices.size());
	uint32_t numIdx = static_cast<uint32_t>(mesh->Indices.size());

	if (m_NextVertex + numVerts > m_MaxVertices) GrowMegaVB(m_NextVertex + numVerts, cmd);
	if (m_NextIndex + numIdx > m_MaxIndices)  GrowMegaIB(m_NextIndex + numIdx, cmd);

	cmd->WriteBuffer(m_MegaVB, mesh->Vertices.data(),
		numVerts * sizeof(Vertex), m_NextVertex * sizeof(Vertex));
	cmd->WriteBuffer(m_MegaIB, mesh->Indices.data(),
		numIdx * sizeof(uint32_t), m_NextIndex * sizeof(uint32_t));

	MeshRecord& rec = m_MeshRecords[handle.id];
	rec = { m_NextVertex, m_NextIndex, numIdx };
	m_NextVertex += numVerts;
	m_NextIndex += numIdx;
	return rec;
}

void MeshBinner::GrowMegaVB(uint32_t requiredVertices, ICommandContext* cmd)
{
	uint32_t newMax = std::max(m_MaxVertices * 2u, requiredVertices);

	BufferDesc desc;
	desc.ByteSize = newMax * sizeof(Vertex);
	desc.Usage = BufferUsage::Vertex | BufferUsage::Storage;
	desc.DebugName = "MegaVertexBuffer";
	GpuBuffer newBuf = m_Device->CreateBuffer(desc);

	if (m_NextVertex > 0)
		cmd->CopyBuffer(newBuf, m_MegaVB, m_NextVertex * sizeof(Vertex));

	m_Device->DestroyBuffer(m_MegaVB);
	m_MegaVB = newBuf;
	m_MaxVertices = newMax;
}

void MeshBinner::GrowMegaIB(uint32_t requiredIndices, ICommandContext* cmd)
{
	uint32_t newMax = std::max(m_MaxIndices * 2u, requiredIndices);

	BufferDesc desc;
	desc.ByteSize = newMax * sizeof(uint32_t);
	desc.Usage = BufferUsage::Index | BufferUsage::Storage;
	desc.DebugName = "MegaIndexBuffer";
	GpuBuffer newBuf = m_Device->CreateBuffer(desc);

	if (m_NextIndex > 0)
		cmd->CopyBuffer(newBuf, m_MegaIB, m_NextIndex * sizeof(uint32_t));
	m_Device->DestroyBuffer(m_MegaIB);
	m_MegaIB = newBuf;
	m_MaxIndices = newMax;
}

uint32_t MeshBinner::RegisterMaterial(AssetHandle<MaterialAsset> material, ICommandContext* cmd)
{
	auto it = m_MaterialIndices.find(material.id);
	if (it != m_MaterialIndices.end()) return it->second;

	const MaterialAsset* mat = m_AssetManager->GetAsset(material);
	CORE_ASSERT(mat, "Asset must signal Ready before use!");

	uint32_t idx = static_cast<uint32_t>(m_MaterialStaging.size());

	GpuMaterialData gpuMat{};
	gpuMat.BaseColorFactor = mat->BaseColorFactor;
	gpuMat.MetallicFactor = mat->MetallicFactor;
	gpuMat.RoughnessFactor = mat->RoughnessFactor;
	gpuMat.NormalScale = mat->NormalScale;
	gpuMat.OcculusionStrength = mat->OcclusionStrength;
	gpuMat.AlbedoIdx = RegisterTexture(mat->AlbedoMap);
	gpuMat.NormalIdx = RegisterTexture(mat->NormalMap);
	gpuMat.MetallicRoughnessIdx = RegisterTexture(mat->MetallicRoughnessMap);
	gpuMat.OcclusionIdx = RegisterTexture(mat->OcclusionMap);
	gpuMat.EmissiveIdx = RegisterTexture(mat->EmissiveMap);

	m_MaterialStaging.push_back(gpuMat);
	m_MaterialIndices[material.id] = idx;
	m_MaterialDirty = true;
	return idx;
}

uint32_t MeshBinner::RegisterTexture(AssetHandle<TextureAsset> texture)
{
	if (!texture.IsValid()) return m_TextureSlots[{}]; // white fallback slot 0

	auto it = m_TextureSlots.find(texture.id);
	if (it != m_TextureSlots.end()) return it->second;

	auto* assets = Engine::Get().GetSubmodule<AssetManager>();
	const TextureAsset* tex = m_AssetManager->GetAsset(texture);
	// Asset may still be Pending — fall back to white, try again next frame.
	if (!tex) return 0;

	uint32_t slot = m_NextTextureSlot++;
	m_Device->WriteTexture(m_TextureTable, tex->Texture, slot);
	m_TextureSlots[texture.id] = slot;
	return slot;
}

void MeshBinner::RebuildSceneBindingSet()
{
	BindingLayoutDesc layoutDesc;
	layoutDesc.Items = {
		BindingLayoutItem::StorageBuffer(2, ShaderStage::Vertex),  // g_Instances
		BindingLayoutItem::StorageBuffer(3, ShaderStage::Pixel),   // g_Materials
	};
	m_SceneBindingLayout = m_Device->CreateBindingLayout(layoutDesc);

	for (auto& bin : m_Bins)
	{
		BindingSetDesc setDesc;
		setDesc.Items = {
			{ BindingType::StorageBuffer, 2, bin.Result.InstanceBuffer },
			{ BindingType::StorageBuffer, 3, m_MaterialBuffer          },
		};
		bin.Result.BindingSet = m_Device->CreateBindingSet(setDesc, m_SceneBindingLayout);
}