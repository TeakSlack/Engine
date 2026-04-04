#ifndef VERTEX_H
#define VERTEX_H

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec3 TexCoord;
	glm::vec3 Tangent; // xyz = tangent, w = bitangent
};

#endif // VERTEX_H