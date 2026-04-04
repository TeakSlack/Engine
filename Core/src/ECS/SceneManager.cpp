#include "SceneManager.h"
#include "../Util/Log.h"

SceneManager& SceneManager::Get()
{
    static SceneManager s_Instance;
    return s_Instance;
}

void SceneManager::Shutdown()
{
    m_ActiveScene = nullptr;
    m_ScenesByID.clear();
    m_Scenes.clear();
}

Scene* SceneManager::CreateScene(const std::string& name)
{
    auto [it, inserted] = m_Scenes.emplace(name, std::make_unique<Scene>(name));
    if (!inserted)
    {
        CORE_WARN("SceneManager: scene '{}' already exists; returning existing scene.", name);
        return it->second.get();
    }

    Scene* scene = it->second.get();
    m_ScenesByID.emplace(scene->GetID(), scene);
    return scene;
}

void SceneManager::DestroyScene(const std::string& name)
{
    auto it = m_Scenes.find(name);
    if (it == m_Scenes.end())
    {
        CORE_WARN("SceneManager: DestroyScene('{}') — scene not found.", name);
        return;
    }

    Scene* scene = it->second.get();
    if (m_ActiveScene == scene)
        m_ActiveScene = nullptr;

    m_ScenesByID.erase(scene->GetID());
    m_Scenes.erase(it);
}

void SceneManager::SetActiveScene(const std::string& name)
{
    Scene* scene = GetScene(name);
    if (!scene)
    {
        CORE_ERROR("SceneManager: SetActiveScene('{}') — scene not found.", name);
        return;
    }
    m_ActiveScene = scene;
}

void SceneManager::SetActiveScene(SceneID id)
{
    Scene* scene = GetScene(id);
    if (!scene)
    {
        CORE_ERROR("SceneManager: SetActiveScene({}) — scene not found.", id.Value());
        return;
    }
    m_ActiveScene = scene;
}

Scene* SceneManager::GetScene(const std::string& name)
{
    auto it = m_Scenes.find(name);
    return it != m_Scenes.end() ? it->second.get() : nullptr;
}

Scene* SceneManager::GetScene(SceneID id)
{
    auto it = m_ScenesByID.find(id);
    return it != m_ScenesByID.end() ? it->second : nullptr;
}
