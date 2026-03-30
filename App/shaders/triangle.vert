#version 450

// Positions and colors are baked in — no vertex buffer needed.
// gl_VertexIndex selects the correct vertex.
vec2 kPositions[3] = vec2[](
    vec2( 0.0, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

vec3 kColors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 outColor;

void main()
{
    gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    outColor    = kColors[gl_VertexIndex];
}
