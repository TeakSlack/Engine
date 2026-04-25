#ifndef FRAME_GRAPH_BLACKBOARD_H
#define FRAME_GRAPH_BLACKBOARD_H

#include "Util/Assert.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

// -----------------------------------------------------------------------
// Compile-time type ID — incremented once per unique type T.
// -----------------------------------------------------------------------
namespace detail
{
    inline uint32_t NextTypeID()
    {
        static uint32_t counter = 0;
        return counter++;
    }

    template<typename T>
    uint32_t TypeID()
    {
        static uint32_t id = NextTypeID();
        return id;
    }
}

// -----------------------------------------------------------------------
// FrameGraphBlackboard
// Type-safe heterogeneous store for per-frame pass data shared between passes.
// Usage:
//   auto& gbuffer = blackboard.Add<GBufferData>();
//   auto& gbuffer = blackboard.Get<GBufferData>();
// -----------------------------------------------------------------------
class FrameGraphBlackboard
{
public:
    template<typename T>
    T& Add()
    {
        uint32_t id = detail::TypeID<T>();
        CORE_ASSERT(m_Entries.find(id) == m_Entries.end(),
                    "Blackboard already contains an entry for this type!");
        auto slot = std::make_unique<Entry<T>>();
        T& ref    = slot->value;
        m_Entries.emplace(id, std::move(slot));
        return ref;
    }

    template<typename T>
    T& Get()
    {
        uint32_t id = detail::TypeID<T>();
        auto it     = m_Entries.find(id);
        CORE_ASSERT(it != m_Entries.end(),
                    "Blackboard does not contain an entry for this type!");
        return static_cast<Entry<T>*>(it->second.get())->value;
    }

    template<typename T>
    const T& Get() const
    {
        uint32_t id = detail::TypeID<T>();
        auto it     = m_Entries.find(id);
        CORE_ASSERT(it != m_Entries.end(),
                    "Blackboard does not contain an entry for this type!");
        return static_cast<const Entry<T>*>(it->second.get())->value;
    }

    template<typename T>
    bool Has() const
    {
        return m_Entries.count(detail::TypeID<T>()) != 0;
    }

    void Clear() { m_Entries.clear(); }

private:
    struct EntryBase { virtual ~EntryBase() = default; };

    template<typename T>
    struct Entry : EntryBase { T value{}; };

    std::unordered_map<uint32_t, std::unique_ptr<EntryBase>> m_Entries;
};

#endif // FRAME_GRAPH_BLACKBOARD_H
