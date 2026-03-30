#ifndef RENDER_VIEW_H
#define RENDER_VIEW_H

#include "glm/glm.hpp"
#include "RenderObject.h"
#include <vector>

struct RenderView
{
	glm::mat4 view;
	glm::mat4 proj;
	std::vector<RenderObject*> visibleObjects;
};

#endif // RENDER_VIEW_H