#ifndef ENTITY_H
#define ENTITY_H

#include <entt/entt.hpp>
#include <cstdint>

class Scene; // forward declare — full definition in Scene.h

// -------------------------------------------------------------------------
// Entity — a typed handle into an entt::registry owned by a Scene.
//
// Template method bodies are defined at the bottom of Scene.h (after the
// full Scene definition), so include Scene.h to use component operations.
//
// C# interop: use GetRawID() to exchange entity identity across the managed
// boundary. uint32_t maps directly to C# uint / System.UInt32.
// -------------------------------------------------------------------------
class Entity
{
public:
	Entity() = default;
	Entity(entt::entity id, Scene* scene) : m_EntityID(id), m_Scene(scene) {}

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T>
	T& GetComponent();

	template<typename T>
	const T& GetComponent() const;

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	void RemoveComponent();

	// IsValid() is defined inline at the bottom of Scene.h.
	bool IsValid() const;

	entt::entity GetID()    const { return m_EntityID; }
	uint32_t     GetRawID() const { return static_cast<uint32_t>(m_EntityID); }

	explicit operator bool() const { return IsValid(); }

	bool operator==(const Entity& other) const = default;

	static Entity Null() { return Entity{}; }

private:
	entt::entity m_EntityID = entt::null;
	Scene*       m_Scene    = nullptr;

	friend class Scene;
};

#endif // ENTITY_H
