#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "Scene.h"
#include "../Engine.h"

#include <memory>
#include <string>
#include <unordered_map>

// -------------------------------------------------------------------------
// SceneManager — IEngineSubmodule that owns all Scene instances.
//
// Scenes are indexed by both name (for editor/serialization use) and UUID
// (for runtime entity lookups), both in O(1).
//
// C# interop: scene names and UUID values are the stable cross-boundary
// identifiers. Prefer GetScene(SceneID) on the hot path.
// -------------------------------------------------------------------------
class SceneManager : public IEngineSubmodule
{
public:
    static SceneManager& Get();

    SceneManager(const SceneManager&)            = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // IEngineSubmodule
    void Init()      override {}
    void Shutdown()  override;
    void Tick(float) override {}

    // Creates and registers a scene. Returns the existing scene if the name
    // is already taken (with a warning).
    Scene* CreateScene(const std::string& name);

    // Destroys a scene by name. Clears the active scene if it matches.
    void DestroyScene(const std::string& name);

    // Active scene — the one layers should update and render.
    void         SetActiveScene(const std::string& name);
    void         SetActiveScene(SceneID id);
    Scene*       GetActiveScene()       { return m_ActiveScene; }
    const Scene* GetActiveScene() const { return m_ActiveScene; }

    // Returns nullptr if not found.
    Scene* GetScene(const std::string& name);
    Scene* GetScene(SceneID id);

private:
    SceneManager() : IEngineSubmodule("SceneManager") {}

    std::unordered_map<std::string, std::unique_ptr<Scene>> m_Scenes;
    std::unordered_map<SceneID, Scene*>                     m_ScenesByID;
    Scene* m_ActiveScene = nullptr;
};

// =========================================================================
// Entity template bodies — deferred from Entity.h; require full SceneManager.
//
// Any translation unit that calls entity component operations must include
// SceneManager.h (not just Scene.h).
// =========================================================================

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
    return SceneManager::Get().GetScene(m_SceneID)->AddComponent<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
    return SceneManager::Get().GetScene(m_SceneID)->GetComponent<T>(*this);
}

template<typename T>
const T& Entity::GetComponent() const
{
    return SceneManager::Get().GetScene(m_SceneID)->GetComponent<T>(*this);
}

template<typename T>
bool Entity::HasComponent() const
{
    return SceneManager::Get().GetScene(m_SceneID)->HasComponent<T>(*this);
}

template<typename T>
void Entity::RemoveComponent()
{
    SceneManager::Get().GetScene(m_SceneID)->RemoveComponent<T>(*this);
}

inline bool Entity::IsValid() const
{
    if (!m_SceneID.IsValid() || m_EntityID == entt::null)
        return false;
    Scene* scene = SceneManager::Get().GetScene(m_SceneID);
    return scene && scene->IsValid(*this);
}

#endif // SCENE_MANAGER_H
