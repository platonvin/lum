#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in uint MatID;

layout(location = 0) out vec3 posOut;
layout(location = 1) out vec3 normOut;
layout(location = 2) flat out float MatIDOut; //uint8 thing. But in float

vec3 cameraRayDir = normalize(vec3(12, 21, 7));
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
} pco;

void main() {
    mat4 m2w = pco.trans_m2w;
        m2w[3][0] /= 16.0;
        m2w[3][1] /= 16.0;
        m2w[3][2] /= 16.0;
    // mat4 w2s = ubo.trans_w2s;

    vec4 world_pos   = m2w * vec4(pos,1);
    // vec4 clip_coords = w2s * world_pos;
    // clip_coords /= 1280.0;
    // clip_coords.z = clip_coords.z/10.0+0.5;
    // clip_coords.z += 0.5;
    // gl_Position = vec4(pos,1);
    
    float view_width  = 1920.0 / 64.0; //in block_diags
	float view_height = 1080.0 / 64.0; //in blocks

    vec3 vertexRelativeToCameraPos = world_pos.xyz - cameraPos;
    vec4 clip_coords;
    clip_coords.x =  dot(vertexRelativeToCameraPos, horizline) / view_width ;
    clip_coords.y =  dot(vertexRelativeToCameraPos, vertiline) / view_height;
    clip_coords.z = -dot(vertexRelativeToCameraPos, cameraRayDir) / 1000.0 + 0.5; //TODO:

    // clip_coords = vertices[gl_VertexIndex];

    gl_Position  = clip_coords;

    //TODO: MERGE UNITS
    posOut = (pco.trans_m2w * vec4(pos,1)).xyz;
    
    mat4 m2w_normals = transpose(inverse(pco.trans_m2w)); 

    normOut = normalize(mat3(m2w_normals)*norm);
    MatIDOut = float(MatID);
    // MatIDOut = float(66);
}