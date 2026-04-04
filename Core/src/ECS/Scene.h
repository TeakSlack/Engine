#ifndef SCENE_H
#define SCENE_H

#include "Entity.h"
#include <entt/entt.hpp>
#include <string>

// -------------------------------------------------------------------------
// Scene — owns an entt::registry and the entities within it.
//
// Each Scene has a stable UUID assigned at construction. Entities store this
// ID instead of a raw pointer; SceneManager resolves UUID → Scene* on demand.
//
// To use Entity component operations (AddComponent, GetComponent, etc.)
// include SceneManager.h — the template bodies live there because they
// require the full SceneManager definition.
// -------------------------------------------------------------------------
class Scene
{
public:
    explicit Scene(std::string name = "Scene");
    ~Scene();

    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&)                 = default;
    Scene& operator=(Scene&&)      = default;

    // Creates an entity and adds TagComponent + TransformComponent.
    Entity CreateEntity(const std::string& name = "Entity");
    void   DestroyEntity(Entity entity);
    bool   IsValid(Entity entity) const;

    const std::string& GetName() const { return m_Name; }
    SceneID            GetID()   const { return m_ID; }

    // Direct registry views for ECS systems (render, physics, etc.).
    template<typename... Components>
    auto View() { return m_Registry.view<Components...>(); }

    template<typename... Components>
    auto View() const { return m_Registry.view<Components...>(); }

    // Component operations — called by Entity member functions and internally.
    template<typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args);

    template<typename T>
    T& GetComponent(Entity entity);

    template<typename T>
    const T& GetComponent(Entity entity) const;

    template<typename T>
    bool HasComponent(Entity entity) const;

    template<typename T>
    void RemoveComponent(Entity entity);

private:
    std::string    m_Name;
    SceneID        m_ID;
    entt::registry m_Registry;
};

// =========================================================================
// Scene template implementations
// =========================================================================

template<typename T, typename... Args>
T& Scene::AddComponent(Entity entity, Args&&... args)
{
    return m_Registry.emplace<T>(entity.m_EntityID, std::forward<Args>(args)...);
}

template<typename T>
T& Scene::GetComponent(Entity entity)
{
    return m_Registry.get<T>(entity.m_EntityID);
}

template<typename T>
const T& Scene::GetComponent(Entity entity) const
{
    return m_Registry.get<T>(entity.m_EntityID);
}

template<typename T>
bool Scene::HasComponent(Entity entity) const
{
    return m_Registry.all_of<T>(entity.m_EntityID);
}

template<typename T>
void Scene::RemoveComponent(Entity entity)
{
    m_Registry.remove<T>(entity.m_EntityID);
}

#endif // SCENE_H
