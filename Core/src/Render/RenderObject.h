#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include "glm/glm.hpp"
#include "nvrhi/nvrhi.h"

struct RenderObject
{
	nvrhi::BufferHandle vertexBuffer;
	nvrhi::BufferHandle indexBuffer;
	uint32_t indexCount;
	glm::mat4 model;
};

#endif // RENDER_OBJECT_H