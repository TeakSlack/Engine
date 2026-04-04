#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <string>

#include "Asset/Asset.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"

// -------------------------------------------------------------------------
// TagComponent — human-readable name for an entity.
// Added automatically by Scene::CreateEntity().
// C# interop: Name maps directly to System.String.
// -------------------------------------------------------------------------
struct TagComponent
{
	std::string Name;

	TagComponent() = default;
	explicit TagComponent(std::string name) : Name(std::move(name)) {}
};

// -------------------------------------------------------------------------
// TransformComponent — world-space position, rotation (Euler degrees), scale.
// Added automatically by Scene::CreateEntity().
// -------------------------------------------------------------------------
struct TransformComponent
{
	Vector3 Position{ 0.0f };
	Vector3 Rotation{ 0.0f }; // Euler angles, degrees
	Vector3 Scale   { 1.0f };

	Matrix4x4 GetMatrix() const
	{
		Matrix4x4 m = Matrix4x4::TRS(Position, Rotation, Scale);
		return m;
	}
};

// -------------------------------------------------------------------------
// MeshComponent / MaterialComponent
// -------------------------------------------------------------------------
struct MeshComponent
{
	AssetHandle<MeshAsset> Mesh;
};

struct MaterialComponent
{
	AssetHandle<MaterialAsset> Material;
};

#endif // COMPONENTS_H