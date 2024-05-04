#version 450

layout(location = 0) in vec3 posIn;
layout(location = 1) in vec3 normIn;
layout(location = 2) in uint MatIDIn;

layout(location = 0)      out vec3 pos;
layout(location = 1)      out vec3 norm;
layout(location = 2)      out vec3 pos_old;
layout(location = 3) flat out float MatID; //uint8 thing. But in float

vec3 cameraRayDir = normalize(vec3(12, 21, 7));
// vec3 cameraRayDir = normalize(vec3(1, 1, 0));
vec3 horizline = normalize(vec3(1,-1,0));
vec3 vertiline = normalize(cross(cameraRayDir, horizline));
vec3 cameraPos = vec3(-13, -22, -8)*1.5;
vec3 globalLightDir = normalize(vec3(10, 25, -6));

vec3 camera_unit_x = horizline / 2;
vec3 camera_unit_y = vertiline / 2;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
} ubo;
layout(push_constant) uniform constants{
    mat4 trans_m2w; //model to world and than to screen
    mat4 trans_m2w_old; //old
} pco;

precision highp float;

void main() {
    mat4 m2w = pco.trans_m2w;
        m2w[3][0] /= 16.0;
        m2w[3][1] /= 16.0;
        m2w[3][2] /= 16.0;
    mat4 m2w_old = pco.trans_m2w_old;
        m2w_old[3][0] /= 16.0;
        m2w_old[3][1] /= 16.0;
        m2w_old[3][2] /= 16.0;
    // vec4 clip_coords = w2s * world_pos;
    // clip_coords /= 1280.0;
    // clip_coords.z = clip_coords.z/10.0+0.5;
    // clip_coords.z += 0.5;
    // gl_Position = vec4(pos,1);
    
    float view_width  = 1920.0 / 32.0; //in block_diags
	float view_height = 1080.0 / 32.0; //in blocks


    vec4 world_pos     = m2w     * vec4(posIn,1);
    vec4 world_pos_old = m2w_old * vec4(posIn,1);
    vec3 relative_pos     = world_pos    .xyz - cameraPos;
    vec3 relative_pos_old = world_pos_old.xyz - cameraPos;
    vec3 clip_coords;
        clip_coords    .x =  dot(relative_pos    , horizline) / view_width ;
        clip_coords    .y =  dot(relative_pos    , vertiline) / view_height;
        clip_coords    .z = -dot(relative_pos    , cameraRayDir) / 1000.0 + 0.5; //TODO:
    vec3 clip_coords_old;
        clip_coords_old.x =  dot(relative_pos_old, horizline) / view_width ;
        clip_coords_old.y =  dot(relative_pos_old, vertiline) / view_height;
        clip_coords_old.z = -dot(relative_pos_old, cameraRayDir) / 1000.0 + 0.5; //TODO:

    pos_old      =      clip_coords_old;     
    gl_Position  = vec4(clip_coords, 1);

    //TODO: MERGE UNITS
    pos = (pco.trans_m2w * vec4(posIn,1)).xyz;
    
    mat3 m2w_normals = transpose(inverse(mat3(pco.trans_m2w))); 

    norm = normalize(m2w_normals*normIn);
    MatID = float(MatIDIn);
    // MatID = float(66);
}