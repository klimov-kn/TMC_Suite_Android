#version 440

layout(location = 0) in float vT;
layout(location = 1) in vec3 vPos;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 grid;
    vec4 map;
    vec4 color;
    vec4 misc;
};

layout(binding = 1) uniform sampler2D scaleTex;

void main()
{
    vec4 c = misc.x > 0.5 ? texture(scaleTex, vec2(vT, 0.5)) : color;
    if (misc.y > 0.0) {
        // Мягкое затенение склонов рельефа (нормаль треугольника по производным):
        // плоское поле не темнеет, светлота шкалы на ровных местах не меняется.
        vec3 n = normalize(cross(dFdx(vPos), dFdy(vPos)));
        float k = 1.0 - misc.y * (1.0 - abs(n.z));
        c.rgb *= k;
    }
    fragColor = vec4(c.rgb * c.a, c.a);
}
