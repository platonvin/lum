#version 450

// layout(location = 0) out vec3 fragColor;
layout(location = 0) out vec2 fragUV;

vec2 positions[3] = vec2[](
    vec2(-5, +5),
    vec2(+5, +5),
    vec2(+0, -5)
);

// vec3 colors[3] = vec3[](
//     vec3(1.0, 0.0, 0.0),
//     vec3(0.0, 1.0, 0.0),
//     vec3(0.0, 0.0, 1.0)
// );

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragUV = positions[gl_VertexIndex].xy;
    // fragColor = colors[gl_VertexIndex];
    // fragColor.x = sin(colors[gl_VertexIndex].y);
    // fragColor.y = sin(colors[gl_VertexIndex].x);
}