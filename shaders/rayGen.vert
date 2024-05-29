#version 450

layout(location = 0) in vec3 posIn;
layout(location = 1) in vec3 normIn;
layout(location = 2) in uint MatIDIn;

// layout(location = 0)      out float depth;
layout(location = 0)      out vec3 pos;
layout(location = 1) flat out vec3 norm;
layout(location = 2)      out vec3 pos_old;
layout(location = 3) flat out float MatID; //uint8 thing. But in float


const vec3 cameraRayDir = normalize(vec3(1, 0.1, -0.5));
// vec3 cameraRayDir = normalize(vec3(-1, 0, .1));
// vec3 cameraRayDir = normalize(vec3(12, 21, 7));
// vec3 cameraRayDir = normalize(vec3(1, 1, 0));
const vec3 globalLightDir = normalize(vec3(1, -1, 0));
// vec3 globalLightDir = cameraRayDir;
const vec3 cameraRayDirPlane = normalize(vec3(cameraRayDir.xy, 0));
const vec3 horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
const vec3 vertiline = normalize(cross(cameraRayDir, horizline));
const vec3 cameraPos = vec3(-40, +40, 100);
// vec3 cameraPos = vec3(-13, -22, -8)*1.5/16.0;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
} ubo;
layout(push_constant) uniform constants{
    mat4 trans_m2w; //model to world and than to screen
    mat4 trans_m2w_old; //old
} pco;

precision highp float;

void main() {
    float view_width  = 1920.0 / 42.0; //in block_diags
	float view_height = 1080.0 / 42.0; //in blocks

    vec4 world_pos     = pco.trans_m2w     * vec4(posIn,1);
    vec4 world_pos_old = pco.trans_m2w_old * vec4(posIn,1);
    vec3 relative_pos     = world_pos    .xyz - cameraPos;
    vec3 relative_pos_old = world_pos_old.xyz - cameraPos;
    vec3 clip_coords;
        clip_coords    .x =  dot(relative_pos    , horizline) / view_width ;
        clip_coords    .y =  dot(relative_pos    , vertiline) / view_height;
        clip_coords    .z = -dot(relative_pos    , cameraRayDir) / 1000.0 + 0.5; //TODO:
    vec3 clip_coords_old;
        clip_coords_old.x =  dot(relative_pos_old, horizline) / view_width ;
        clip_coords_old.y =  dot(relative_pos_old, vertiline) / view_height;
        clip_coords_old.z = dot(relative_pos_old, cameraRayDir) / 1000.0 + 0.5; //TODO:

    pos_old      =      clip_coords_old;     
    gl_Position  = vec4(clip_coords, 1);

    float depth = dot(relative_pos, cameraRayDir);
    //TODO: MERGE UNITS
    // pos = cameraPos + 
    //     horizline*clip_coords.x*view_width  + 
    //     vertiline*clip_coords.y*view_height + 
    //     depth*cameraRayDir;
    pos = world_pos.xyz;
    pos.x = depth;
    
    
    mat3 m2w_normals = transpose(inverse(mat3(pco.trans_m2w))); 

    norm = normalize(m2w_normals*normIn);
    MatID = float(MatIDIn);
    // MatID = float(66);
}