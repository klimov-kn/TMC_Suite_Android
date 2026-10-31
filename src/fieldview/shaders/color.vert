#version 440
// Топология: четырёхугольники блоков, цвет — у вершины (материал, style-guide).

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 col;

layout(location = 0) out vec4 vCol;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
};

void main()
{
    vCol = col;
    gl_Position = mvp * vec4(pos, 1.0);
}
