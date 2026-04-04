#include "Scene.h"
#include "Components.h"

Scene::Scene(std::string name)
	: m_Name(std::move(name))
{
}

Scene::~Scene() = default;

Entity Scene::CreateEntity(const std::string& name)
{
	Entity entity{ m_Registry.create(), this };
	entity.AddComponent<TagComponent>(name);
	entity.AddComponent<TransformComponent>();
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
