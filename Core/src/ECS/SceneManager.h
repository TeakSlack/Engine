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
// Register it with Engine::RegisterSubmodule before calling Engine::Run().
// Use CreateScene() to allocate a scene, SetActiveScene() to make one live,
// and GetActiveScene() from render/update code to access its entities.
//
// C# interop: scene names are the stable cross-boundary identifiers.
// -------------------------------------------------------------------------
class SceneManager : public IEngineSubmodule
{
public:
	// Meyer's singleton — pass &SceneManager::Get() to RegisterSubmodule().
	static SceneManager& Get();

	SceneManager(const SceneManager&)            = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	// IEngineSubmodule
	void Init()                override {}
	void Shutdown()            override;
	void Tick(float deltaTime) override {}

	// Creates and registers a scene by name. Returns the existing scene if the
	// name is already taken (with a warning).
	Scene* CreateScene(const std::string& name);

	// Destroys a scene by name. Clears the active scene pointer if it matches.
	void DestroyScene(const std::string& name);

	// Active scene — the one Engine layers should update and render.
	void   SetActiveScene(const std::string& name);
	void   SetActiveScene(Scene* scene);
	Scene* GetActiveScene()       { return m_ActiveScene; }
	const Scene* GetActiveScene() const { return m_ActiveScene; }

	// Returns nullptr if not found.
	Scene* GetScene(const std::string& name);

private:
	SceneManager() : IEngineSubmodule("SceneManager") {}

	std::unordered_map<std::string, std::unique_ptr<Scene>> m_Scenes;
	Scene* m_ActiveScene = nullptr;
};

#endif // SCENE_MANAGER_H
