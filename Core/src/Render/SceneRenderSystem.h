#ifndef SCENE_RENDER_SYSTEM_H
#define SCENE_RENDER_SYSTEM_H

#include "ECS/Scene.h"
#include "ECS/Components.h"
#include "Asset/AssetManager.h"
#include "Render/RenderPacket.h"
#include "Render/SceneRenderer.h"

// -------------------------------------------------------------------------
// SceneRenderSystem — the only bridge between the ECS and SceneRenderer.
//
// This is intentionally a thin namespace (not a class) because it has no
// state — it reads from the ECS and writes RenderPackets, nothing more.
//
// Separation of concerns:
//   Scene / Components  — know nothing about the renderer
//   SceneRenderer       — knows nothing about entt or component types
//   SceneRenderSystem   — knows both; translates between them
// -------------------------------------------------------------------------
namespace SceneRenderSystem
{
    // Iterate all renderable entities in the scene, build a RenderPacket for
    // each, and submit it to the renderer.
    //
    // cameraPos — world-space camera position; used to compute sort-key depth.
    //
    // Call once per frame between SceneRenderer::BeginFrame() and
    // SceneRenderer::RenderView().
    inline void CollectAndSubmit(Scene& scene, const Vector3& cameraPos, SceneRenderer& renderer)
    {
        // Iterate every entity that has all three renderable components.
        // entt only visits entities that own all listed component types.
        AssetManager* assetManager = Engine::Get().GetSubmodule<AssetManager>();
        auto view = scene.View<TransformComponent, MeshComponent, MaterialComponent>();

        view.each([&](const TransformComponent& transform,
                      const MeshComponent&       mesh,
                      const MaterialComponent&   material)
        {
            // Skip entities whose assets haven't finished loading yet.
            // Submit() already guards against Pending meshes, but checking
            // here avoids building bounds for an asset we can't use.
            MeshAsset* meshData = assetManager->GetAsset(mesh.Mesh);
            if (!meshData)
                return;

			MaterialAsset* materialData = assetManager->GetAsset(material.Material);

            RenderPacket packet;
            packet.Mesh          = mesh.Mesh;
            packet.Material      = material.Material;
            packet.WorldTransform = transform.GetMatrix();
            packet.CastsShadow   = true;
            packet.Transparent   = false; // extend: read from MaterialAsset::Alpha
			packet.IsOccluder = materialData->IsOccluder; // extend: read from MaterialAsset::Occluder
			packet.Bin = materialData->DrawBin; // extend: read from MaterialAsset::DrawBin

            // Derive world-space AABB from the mesh's local bounds.
            MakeWorldBounds(
                meshData->BoundsMin, meshData->BoundsMax,
                packet.WorldTransform,
                packet.WorldBoundsMin, packet.WorldBoundsMax);

            ComputeSortKey(packet, cameraPos);

            renderer.Submit(packet);
        });
    }
}

#endif // SCENE_RENDER_SYSTEM_H
