#ifndef ENTITY_H
#define ENTITY_H

#include <entt/entt.hpp>
#include <cstdint>
#include "Util/UUID.h"

class Scene;
class SceneManager;

using SceneID = UUID;

// -------------------------------------------------------------------------
// Entity — a typed handle into an entt::registry owned by a Scene.
//
// Stores a SceneID (UUID) rather than a raw Scene* so that entities remain
// safe to hold across scene transitions; IsValid() returns false if the
// scene has been destroyed.
//
// Template method bodies are defined at the bottom of SceneManager.h (after
// the full SceneManager definition), so include SceneManager.h to use
// component operations.
//
// C# interop: GetRawID() and GetSceneID().Value() both map to uint64_t /
// System.UInt64 across the managed boundary.
// -------------------------------------------------------------------------
class Entity
{
public:
    Entity() = default;
    Entity(entt::entity id, SceneID sceneID) : m_EntityID(id), m_SceneID(sceneID) {}

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

    // Defined inline at the bottom of SceneManager.h.
    bool IsValid() const;

    entt::entity GetID()      const { return m_EntityID; }
    SceneID      GetSceneID() const { return m_SceneID; }
    uint32_t     GetRawID()   const { return static_cast<uint32_t>(m_EntityID); }

    explicit operator bool() const { return IsValid(); }

    bool operator==(const Entity& other) const = default;

    static Entity Null() { return Entity{}; }

private:
    entt::entity m_EntityID = entt::null;
    SceneID      m_SceneID;   // default-constructed UUID is invalid (value 0)

    friend class Scene;
};

#endif // ENTITY_H
