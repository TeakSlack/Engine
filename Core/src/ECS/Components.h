#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

#include "Asset/Asset.h"
#include "Math/Vector3D.h"

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
	glm::vec3 Position{ 0.0f };
	glm::vec3 Rotation{ 0.0f }; // Euler angles, degrees
	glm::vec3 Scale   { 1.0f };

	glm::mat4 GetMatrix() const
	{
		glm::mat4 m = glm::translate(glm::mat4(1.0f), Position);
		m = glm::rotate(m, glm::radians(Rotation.x), { 1, 0, 0 });
		m = glm::rotate(m, glm::radians(Rotation.y), { 0, 1, 0 });
		m = glm::rotate(m, glm::radians(Rotation.z), { 0, 0, 1 });
		m = glm::scale(m, Scale);
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