#ifndef SCENE_H
#define SCENE_H

#include "Entity.h"
#include <entt/entt.hpp>
#include <string>

// -------------------------------------------------------------------------
// Scene — owns an entt::registry and the entities within it.
//
// Not a singleton. SceneManager owns Scene instances by name; use
// SceneManager::Get().CreateScene() / GetActiveScene() to obtain one.
//
// View<Components...>() exposes the registry for ECS systems so they can
// query without going through Scene's component API.
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

	// Creates an entity and adds TagComponent + TransformComponent automatically.
	Entity CreateEntity(const std::string& name = "Entity");
	void   DestroyEntity(Entity entity);
	bool   IsValid(Entity entity) const;

	const std::string& GetName() const { return m_Name; }

	// Direct registry views for ECS systems (e.g. render, physics).
	template<typename... Components>
	auto View() { return m_Registry.view<Components...>(); }

	template<typename... Components>
	auto View() const { return m_Registry.view<Components...>(); }

	// Component operations — called by Entity member functions.
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
	entt::registry m_Registry;
};

// =========================================================================
// Template implementations — require full Scene definition.
// =========================================================================

// ---- Scene template bodies ----

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

// ---- Entity template bodies (deferred from Entity.h) ----

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
	return m_Scene->AddComponent<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
	return m_Scene->GetComponent<T>(*this);
}

template<typename T>
const T& Entity::GetComponent() const
{
	return m_Scene->GetComponent<T>(*this);
}

template<typename T>
bool Entity::HasComponent() const
{
	return m_Scene->HasComponent<T>(*this);
}

template<typename T>
void Entity::RemoveComponent()
{
	m_Scene->RemoveComponent<T>(*this);
}

inline bool Entity::IsValid() const
{
	return m_Scene != nullptr && m_Scene->IsValid(*this);
}

#endif // SCENE_H
