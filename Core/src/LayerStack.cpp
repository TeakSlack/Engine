#include "LayerStack.h"
#include <algorithm>

void LayerStack::PushLayer(Layer* layer)
{
    m_Layers.emplace(m_Layers.begin() + m_LayerInsert, layer);
    ++m_LayerInsert;
}

void LayerStack::PopLayer(Layer* layer)
{
    // Search only the layers region (before m_LayerInsert).
    auto it = std::find(m_Layers.begin(),
                        m_Layers.begin() + m_LayerInsert,
                        layer);
    if (it != m_Layers.begin() + m_LayerInsert)
    {
        layer->OnDetach();
        m_Layers.erase(it);
        --m_LayerInsert;
    }
}

void LayerStack::PushOverlay(Layer* overlay)
{
    m_Layers.emplace_back(overlay);
}

void LayerStack::PopOverlay(Layer* overlay)
{
    // Search only the overlays region (from m_LayerInsert onwards).
    auto it = std::find(m_Layers.begin() + m_LayerInsert,
                        m_Layers.end(),
                        overlay);
    if (it != m_Layers.end())
    {
        overlay->OnDetach();
        m_Layers.erase(it);
    }
}
