#ifndef RENDER_VIEW_H
#define RENDER_VIEW_H

#include "GpuTypes.h"
#include "Math/Matrix4x4.h"
#include <vector>

struct RenderObject
{
    GpuBuffer VertexBuffer;
    GpuBuffer IndexBuffer;
    uint32_t  IndexCount;
    Matrix4x4 Model;
};

struct RenderView
{
    Matrix4x4                  View;
    Matrix4x4                  Projection;
    std::vector<RenderObject*> VisibleObjects;
};

#endif // RENDER_VIEW_H
