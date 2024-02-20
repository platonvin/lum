#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in uint MatID;

layout(location = 0) out vec3 posOut;
layout(location = 1) out vec3 normOut;
layout(location = 2) out float MatIDOut; //uint8 thing. But interpolated

vec3 cameraRayDir = normalize(vec3(12, 21, 7));
vec3 horizline = normalize(vec3(1,-1,0));
vec3 vertiline = normalize(cross(cameraRayDir, horizline));
vec3 cameraPos = vec3(-13, -22, -8)*1.5;
vec3 globalLightDir = normalize(vec3(10, 25, -6));

vec3 camera_unit_x = horizline / 2;
vec3 camera_unit_y = vertiline / 2;


void main() {
	float view_width  = 1920.0 / 64.0; //in block_diags
	float view_height = 1080.0 / 64.0; //in blocks

    vec3 vertexRelativeToCameraPos = pos - cameraPos;
    vec3 clip_coords;
    clip_coords.x = dot(vertexRelativeToCameraPos, horizline) / view_width ;
    clip_coords.y = dot(vertexRelativeToCameraPos, vertiline) / view_height;
    clip_coords.z = -dot(vertexRelativeToCameraPos, cameraRayDir) / 1000.0 + 0.5; //TODO:

    gl_Position  = vec4(clip_coords, 1.0);

    posOut = pos;
    normOut = -norm;
    MatIDOut = float(MatID);
}