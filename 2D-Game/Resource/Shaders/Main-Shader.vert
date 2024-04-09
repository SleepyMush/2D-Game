#version 460 core
layout (location = 0) in vec2 Position;
layout (location = 1) in vec2 aTexCoord;

layout(std430, binding = 2) buffer Transform
{
    mat4 transforms[];
};

out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * transforms[int(floor(gl_VertexID / 6))] * vec4(Position, 0.0, 1.0);
    TexCoord = aTexCoord;
}