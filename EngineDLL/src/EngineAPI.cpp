#include "EngineAPI.h"

#include <Engine.h>
#include <ECS/SceneManager.h>
#include <ECS/Components.h>
#include <Asset/AssetManager.h>

#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Submodule instances — owned by the DLL, lifetime matches Engine_Init/Shutdown
// ---------------------------------------------------------------------------
static AssetManager* s_AssetManager  = nullptr;
static SceneManager* s_SceneManager  = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Scene* ActiveScene()
{
    if (!s_SceneManager) return nullptr;
    return s_SceneManager->GetActiveScene();
}

static Entity FindEntity(uint32_t rawId)
{
    Scene* scene = ActiveScene();
    if (!scene) return Entity::Null();
    Entity e(static_cast<entt::entity>(rawId), scene->GetID());
    return scene->IsValid(e) ? e : Entity::Null();
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

void Engine_Init()
{
    s_AssetManager = new AssetManager();
    s_SceneManager = new SceneManager();

    Engine::Get().RegisterSubmodule(s_AssetManager);
    Engine::Get().RegisterSubmodule(s_SceneManager);

    Engine::Get().EditorInit();

    // Create a default scene so there is always something to work with.
    Scene* scene = s_SceneManager->CreateScene("Main");
    s_SceneManager->SetActiveScene("Main");
    (void)scene;
}

void Engine_Tick(float deltaTime)
{
    Engine::Get().EditorTick(deltaTime);
}

void Engine_Shutdown()
{
    Engine::Get().EditorShutdown();

    delete s_SceneManager;  s_SceneManager = nullptr;
    delete s_AssetManager;  s_AssetManager = nullptr;
}

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

int Scene_GetEntityCount()
{
    Scene* scene = ActiveScene();
    if (!scene) return 0;

    int count = 0;
    auto view = scene->View<TagComponent>();
    for (auto _ : view) ++count;
    return count;
}

int Scene_GetEntityIDs(uint32_t* outIds, int maxCount)
{
    Scene* scene = ActiveScene();
    if (!scene || !outIds || maxCount <= 0) return 0;

    int written = 0;
    auto view = scene->View<TagComponent>();
    for (auto enttId : view)
    {
        if (written >= maxCount) break;
        outIds[written++] = static_cast<uint32_t>(enttId);
    }
    return written;
}

uint32_t Scene_CreateEntity(const char* name)
{
    Scene* scene = ActiveScene();
    if (!scene) return 0xFFFFFFFF;
    Entity e = scene->CreateEntity(name ? name : "Entity");
    return e.GetRawID();
}

void Scene_DestroyEntity(uint32_t entityId)
{
    Scene* scene = ActiveScene();
    if (!scene) return;
    Entity e = FindEntity(entityId);
    if (e.IsValid())
        scene->DestroyEntity(e);
}

// ---------------------------------------------------------------------------
// Entity — TagComponent
// ---------------------------------------------------------------------------

void Entity_GetName(uint32_t entityId, char* buf, int bufLen)
{
    if (!buf || bufLen <= 0) return;
    Entity e = FindEntity(entityId);
    if (!e.IsValid()) { buf[0] = '\0'; return; }

    Scene* scene = ActiveScene();
    const auto& tag = scene->GetComponent<TagComponent>(e);
    std::strncpy(buf, tag.Name.c_str(), static_cast<size_t>(bufLen - 1));
    buf[bufLen - 1] = '\0';
}

void Entity_SetName(uint32_t entityId, const char* name)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid() || !name) return;
    ActiveScene()->GetComponent<TagComponent>(e).Name = name;
}

// ---------------------------------------------------------------------------
// Entity — TransformComponent
// ---------------------------------------------------------------------------

void Entity_GetPosition(uint32_t entityId, float* outXYZ)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid() || !outXYZ) return;
    const auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    outXYZ[0] = t.Position.x;
    outXYZ[1] = t.Position.y;
    outXYZ[2] = t.Position.z;
}

void Entity_SetPosition(uint32_t entityId, float x, float y, float z)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid()) return;
    auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    t.Position = { x, y, z };
}

void Entity_GetRotation(uint32_t entityId, float* outXYZ)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid() || !outXYZ) return;
    const auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    outXYZ[0] = t.Rotation.x;
    outXYZ[1] = t.Rotation.y;
    outXYZ[2] = t.Rotation.z;
}

void Entity_SetRotation(uint32_t entityId, float x, float y, float z)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid()) return;
    auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    t.Rotation = { x, y, z };
}

void Entity_GetScale(uint32_t entityId, float* outXYZ)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid() || !outXYZ) return;
    const auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    outXYZ[0] = t.Scale.x;
    outXYZ[1] = t.Scale.y;
    outXYZ[2] = t.Scale.z;
}

void Entity_SetScale(uint32_t entityId, float x, float y, float z)
{
    Entity e = FindEntity(entityId);
    if (!e.IsValid()) return;
    auto& t = ActiveScene()->GetComponent<TransformComponent>(e);
    t.Scale = { x, y, z };
}
