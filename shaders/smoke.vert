#version 450 

// layout (location = 0) out vec3 origin; //non_clip_pos
layout (location = 0) out vec2 outUV; //non_clip_pos

const float view_width  = 1920.0 / 10.0; //in block_diags
const float view_height = 1080.0 / 10.0; //in blocks

layout(push_constant) uniform readonly constants{
    vec3 camera_pos;
     int timeSeed;
     vec4 camera_direction;
} PushConstants;

vec3 cameraRayDirPlane;
vec3 horizline;
vec3 vertiline;

vec3 get_origin(vec2 clip_pos){
    const vec2 clip_pos_scaled = clip_pos*vec2(view_width, view_height);
    
    vec3 origin = PushConstants.camera_pos.xyz + 
        (horizline*clip_pos_scaled.x) + 
        (vertiline*clip_pos_scaled.y);

    return origin;
}

void main() 
{
    // cameraRayDirPlane = normalize(vec3(PushConstants.camera_direction.xy, 0));
    // horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
    // vertiline = normalize(cross(PushConstants.camera_direction.xyz, horizline));

    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

    vec2 clip = outUV * 2.0f + -1.0f;

    // vec3 origin = get_origin(clip);
    
    // gl_Position = vec4(clip, 0.0f, 1.0f);
    gl_Position = vec4(clip, 0.0f, 1.0f);
}