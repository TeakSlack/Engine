#ifndef ENGINE_API_H
#define ENGINE_API_H

#include <cstdint>

#ifdef _WIN32
  #ifdef ENGINE_DLL_EXPORTS
    #define ENGINE_API __declspec(dllexport)
  #else
    #define ENGINE_API __declspec(dllimport)
  #endif
#else
  #define ENGINE_API __attribute__((visibility("default")))
#endif

extern "C" {

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

// Initialises logging, registers core submodules (SceneManager, AssetManager),
// and calls EditorInit on the engine. Call once at startup.
ENGINE_API void Engine_Init();

// Ticks submodules (scene logic, asset streaming, etc.) for one frame.
ENGINE_API void Engine_Tick(float deltaTime);

// Shuts down all submodules and frees internal resources. Call once at exit.
ENGINE_API void Engine_Shutdown();

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------

// Returns the number of entities in the active scene (0 if no scene).
ENGINE_API int Scene_GetEntityCount();

// Fills outIds with up to maxCount raw entity IDs from the active scene.
// Returns the number of IDs written.
ENGINE_API int Scene_GetEntityIDs(uint32_t* outIds, int maxCount);

// Creates a new entity in the active scene. Returns its raw ID,
// or 0xFFFFFFFF if there is no active scene.
ENGINE_API uint32_t Scene_CreateEntity(const char* name);

// Destroys the entity with the given raw ID from the active scene.
ENGINE_API void Scene_DestroyEntity(uint32_t entityId);

// ---------------------------------------------------------------------------
// Entity — TagComponent
// ---------------------------------------------------------------------------

// Copies the entity name into buf (null-terminated, truncated to bufLen).
ENGINE_API void Entity_GetName(uint32_t entityId, char* buf, int bufLen);

// Renames the entity.
ENGINE_API void Entity_SetName(uint32_t entityId, const char* name);

// ---------------------------------------------------------------------------
// Entity — TransformComponent
// ---------------------------------------------------------------------------

// outXYZ must point to a float[3].
ENGINE_API void Entity_GetPosition(uint32_t entityId, float* outXYZ);
ENGINE_API void Entity_SetPosition(uint32_t entityId, float x, float y, float z);

ENGINE_API void Entity_GetRotation(uint32_t entityId, float* outXYZ);
ENGINE_API void Entity_SetRotation(uint32_t entityId, float x, float y, float z);

ENGINE_API void Entity_GetScale(uint32_t entityId, float* outXYZ);
ENGINE_API void Entity_SetScale(uint32_t entityId, float x, float y, float z);

} // extern "C"

#endif // ENGINE_API_H
