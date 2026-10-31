#version 440
// Рельеф и каркас поля: координаты узла (u, v) лежат в видеопамяти один раз,
// значение узла приходит каждый кадр. Высота и цвет — как ZtoGLWinCoord и SetColor
// Windows-FieldView (fieldscene.h).

layout(location = 0) in vec2 uv;
layout(location = 1) in float value;

layout(location = 0) out float vT;
layout(location = 1) out vec3 vPos;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 grid;   // x, y — размеры поля в сцене; z — множитель высоты; w — подъём над рельефом
    vec4 map;    // x — k, y — off (h = v·k + off); z — режим цвета (0: (h+1)/2, 1: |h|); w — уровней (0 — плавно)
    vec4 color;  // цвет каркаса (если не по шкале)
    vec4 misc;   // x — 1: цвет по шкале, 0: color; y — сила затенения рельефа
};

void main()
{
    float h = value * map.x + map.y;
    float t = map.z > 0.5 ? abs(h) : (h + 1.0) * 0.5;
    t = clamp(t, 0.0, 1.0);
    if (map.w > 1.5) {
        // Дискретные уровни, как 20 цветов Windows: i = (int)(t·19), 0…19.
        float i = min(floor(t * (map.w - 1.0)), map.w - 1.0);
        t = i / (map.w - 1.0);
    }
    vT = t;
    vec3 p = vec3((uv.x - 0.5) * grid.x, (uv.y - 0.5) * grid.y, h * grid.z + grid.w);
    vPos = p;
    gl_Position = mvp * vec4(p, 1.0);
}
