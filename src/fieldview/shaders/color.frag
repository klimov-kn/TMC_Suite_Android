#version 440

layout(location = 0) in vec4 vCol;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(vCol.rgb * vCol.a, vCol.a);
}
