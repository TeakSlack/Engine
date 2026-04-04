#ifndef UUID_H
#define UUID_H

#include <cstdint>
#include <functional>
#include <random>

// -------------------------------------------------------------------------
// UUID — a 64-bit unique identifier.
// Value 0 is reserved as the null/invalid sentinel.
// -------------------------------------------------------------------------
class UUID
{
public:
    UUID() : m_Value(0) {}
    explicit UUID(uint64_t value) : m_Value(value) {}

    static UUID Generate()
    {
        static std::mt19937_64                        rng(std::random_device{}());
        static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        return UUID(dist(rng));
    }

    bool     IsValid() const { return m_Value != 0; }
    uint64_t Value()   const { return m_Value; }

    bool operator==(const UUID&) const = default;

private:
    uint64_t m_Value;
};

template<>
struct std::hash<UUID>
{
    size_t operator()(const UUID& id) const noexcept
    {
        return std::hash<uint64_t>{}(id.Value());
    }
};

#endif // UUID_H
