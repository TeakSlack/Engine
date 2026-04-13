#ifndef VERTEX_H
#define VERTEX_H

#include "Math/Vector2.h"
#include "Math/Vector3.h"

struct Vertex
{
    Vector3 Position;
    Vector3 Normal;
    Vector2 TexCoord;
    Vector3 Tangent;
    float   TangentSign; // +1 or -1 (glTF tangent.w) — determines bitangent handedness
};

#endif // VERTEX_H
