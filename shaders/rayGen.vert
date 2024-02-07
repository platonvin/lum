#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in uint MatID;

layout(location = 0) out vec3 posOut;
layout(location = 1) out vec3 normOut;
layout(location = 2) out float MatIDOut; //uint8 thing. But interpolated

vec3 ray_dir = normalize(vec3(-5, -3.4, 2));
vec3 horizline = normalize(vec3(1,-1,0));
vec3 vertiline = normalize(cross(ray_dir, horizline));
vec3 camera_pos = vec3 (5, 3.4, -2);

void main() {
	float view_width  = 1920 / 32.0 / 4; //in block_diags
	float view_height = 1080 / 32.0 / 4; //in blocks

    vec3 vertexRelativeToCameraPos = pos - camera_pos;
    vec3 clip_coords;
    clip_coords.x = dot(vertexRelativeToCameraPos, horizline) / view_width  / 3;
    clip_coords.y = dot(vertexRelativeToCameraPos, vertiline) / view_height / 3;
    clip_coords.z = dot(vertexRelativeToCameraPos, ray_dir) / 3;

    gl_Position  = vec4(clip_coords, 1.0);


    posOut = pos;
    normOut = norm;
    MatIDOut = float(MatID);
}