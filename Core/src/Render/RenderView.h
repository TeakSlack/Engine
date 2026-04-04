#ifndef RENDER_VIEW_H
#define RENDER_VIEW_H

#include "GpuTypes.h"
#include <glm/glm.hpp>
#include <vector>

struct RenderObject
{
    GpuBuffer vertexBuffer;
    GpuBuffer indexBuffer;
    uint32_t  indexCount;
    glm::mat4 model;
};

struct RenderView
{
    glm::mat4 view;
    glm::mat4 proj;
    std::vector<RenderObject*> visibleObjects;
};

#endif // RENDER_VIEW_H
