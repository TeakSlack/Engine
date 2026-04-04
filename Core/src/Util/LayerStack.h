#ifndef LAYER_STACK_H
#define LAYER_STACK_H
#include "Layer.h"
#include <vector>

// -------------------------------------------------------------------------
// LayerStack — ordered collection of Layer pointers.
//
// Memory layout (index 0 = front):
//   [ layer0, layer1, ..., layerN | overlay0, overlay1, ..., overlayM ]
//                                 ^
//                            m_LayerInsert  (= first overlay slot)
//
//   Normal layers are inserted at m_LayerInsert and push the boundary right.
//   Overlays are always appended past the boundary.
//
// Update iteration:  front → back  (base layers updated before overlays)
// Event iteration:   back  → front (overlays see events first)
//
// Ownership: the stack stores raw non-owning pointers.
//   Push triggers OnAttach; Pop triggers OnDetach.
//   The caller is responsible for the Layer object's lifetime.
// -------------------------------------------------------------------------
class LayerStack
{
public:
    LayerStack()  = default;
    ~LayerStack() = default;

    LayerStack(const LayerStack&)            = delete;
    LayerStack& operator=(const LayerStack&) = delete;
    LayerStack(LayerStack&&)                 = default;
    LayerStack& operator=(LayerStack&&)      = default;

    void PushLayer(Layer* layer);
    void PopLayer(Layer* layer);
    void PushOverlay(Layer* overlay);
    void PopOverlay(Layer* overlay);

    // Iterators for range-for in Engine
    std::vector<Layer*>::iterator       begin()  { return m_Layers.begin(); }
    std::vector<Layer*>::iterator       end()    { return m_Layers.end();   }
    std::vector<Layer*>::const_iterator begin()  const { return m_Layers.cbegin(); }
    std::vector<Layer*>::const_iterator end()    const { return m_Layers.cend();   }
    std::vector<Layer*>::reverse_iterator       rbegin() { return m_Layers.rbegin(); }
    std::vector<Layer*>::reverse_iterator       rend()   { return m_Layers.rend();   }

private:
    std::vector<Layer*> m_Layers;
    unsigned int        m_LayerInsert = 0; // index of first overlay slot
};

#endif // LAYER_STACK_H