#ifndef HANDLE_H
#define HANDLE_H

#include <vector>
#include <cstdint>
#include <utility>

// -------------------------------------------------------------------------
// Handle<Tag> — strongly-typed opaque resource handle.
// Tag is never instantiated; it just makes Handle<BufferTag> and
// Handle<TextureTag> incompatible types at compile time.
//
// Layout (one uint32_t):
//   bits  [0, 19] — slot index
//   bits [20, 31] — generation (stale-handle detection)
//
// id == 0 is the null/invalid sentinel.
// -------------------------------------------------------------------------
template <typename Tag>
struct Handle
{
	uint32_t id			: 20 = 0;
	uint32_t generation : 12 = 0;

	bool IsValid() const { return id != 0; }
	bool operator==(const Handle& other) const = default;
};

// -------------------------------------------------------------------------
// SlotMap<T, Tag> — dense pool with O(1) insert, remove, and lookup.
//
// Slot 0 is a permanent null sentinel — id == 0 is always invalid.
// Generation is incremented on Remove so stale handles are detected.
// -------------------------------------------------------------------------
template <typename T, typename Tag>
class SlotMap
{
public:
	struct Slot
	{
		T		 value;
		uint32_t generation = 1;
		bool	 occupied   = false;
	};

	Handle<Tag> Insert(T value)
	{
		uint32_t id;

		if (!m_FreeList.empty())
		{
			id = m_FreeList.back();
			m_FreeList.pop_back();
			m_Slots[id].value    = std::move(value);
			m_Slots[id].occupied = true;
			// generation was already incremented during Remove — leave it
		}
		else
		{
			id = static_cast<uint32_t>(m_Slots.size());
			m_Slots.push_back({ std::move(value), 1, true });
		}

		Handle<Tag> h;
		h.id		 = id;
		h.generation = m_Slots[id].generation;
		return h;
	}

	void Remove(Handle<Tag> handle)
	{
		if (!IsValid(handle)) return;

		auto& slot       = m_Slots[handle.id];
		++slot.generation;          // invalidates all existing handles to this slot
		slot.value    = {};
		slot.occupied = false;
		m_FreeList.push_back(handle.id);
	}

	T* Get(Handle<Tag> handle)
	{
		if (!IsValid(handle)) return nullptr;
		return &m_Slots[handle.id].value;
	}

	const T* Get(Handle<Tag> handle) const
	{
		if (!IsValid(handle)) return nullptr;
		return &m_Slots[handle.id].value;
	}

	bool IsValid(Handle<Tag> handle) const
	{
		if (handle.id == 0 || handle.id >= m_Slots.size()) return false;
		const auto& slot = m_Slots[handle.id];
		return slot.occupied && slot.generation == handle.generation;
	}

	// Iterate all occupied slots. F: void(Handle<Tag>, T&)
	template <typename F>
	void ForEach(F&& func)
	{
		for (uint32_t i = 1; i < static_cast<uint32_t>(m_Slots.size()); ++i)
		{
			if (m_Slots[i].occupied)
			{
				Handle<Tag> h;
				h.id		 = i;
				h.generation = m_Slots[i].generation;
				func(h, m_Slots[i].value);
			}
		}
	}

private:
	std::vector<Slot>	  m_Slots    = { Slot{} }; // index 0 = null sentinel
	std::vector<uint32_t> m_FreeList;
};

#endif // HANDLE_H
