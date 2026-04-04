#include "Scene.h"
#include "Components.h"

Scene::Scene(std::string name)
    : m_Name(std::move(name))
    , m_ID(UUID::Generate())
{
}

Scene::~Scene() = default;

Entity Scene::CreateEntity(const std::string& name)
{
    // Call Scene::AddComponent directly (bypasses SceneManager lookup) so
    // Scene.cpp does not need to include SceneManager.h.
    Entity entity{ m_Registry.create(), m_ID };
    AddComponent<TagComponent>(entity, name);
    AddComponent<TransformComponent>(entity);
    return entity;
}

void Scene::DestroyEntity(Entity entity)
{
    if (IsValid(entity))
        m_Registry.destroy(entity.m_EntityID);
}

bool Scene::IsValid(Entity entity) const
{
    return entity.m_EntityID != entt::null
        && m_Registry.valid(entity.m_EntityID);
}
