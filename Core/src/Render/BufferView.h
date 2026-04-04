#ifndef BUFFER_VIEW_H
#define BUFFER_VIEW_H

#include "GpuTypes.h"

struct VertexBufferView
{
    GpuBuffer buffer;
    uint32_t  stride;
    uint32_t  offset;
};

struct IndexBufferView
{
    GpuBuffer buffer;
    uint32_t  offset;
    GpuFormat format;
};

#endif // BUFFER_VIEW_H
