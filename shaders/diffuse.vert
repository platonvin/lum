#version 450 

// layout (location = 0) out vec2 outUV; //non_clip_pos
layout (location = 0) out vec2 outClip; //clip_pos

void main() 
{
    vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outClip = outUV * 2.0f + -1.0f;
    gl_Position = vec4(outClip, 0.0f, 1.0f);
}