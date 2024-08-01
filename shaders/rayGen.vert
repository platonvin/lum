#version 450

#define varp highp

precision varp int;
precision varp float;

layout(location = 0) in varp uvec3 posIn;
layout(location = 1) in varp ivec3 normIn;
layout(location = 2) in varp uint MatIDIn;

// layout(location = 0)      out float depth;
// layout(location = 0)      out varp vec2 uv_shift;
layout(location = 0) flat out varp vec3 norm;
layout(location = 1) flat out varp uint mat;
// layout(location = 3)      out float depth;

const varp vec3 cameraRayDir = normalize(vec3(0.1, 1.0, -0.5));
const varp vec3 cameraPos = vec3(60, 0, 50);
// vec3 cameraRayDir = normalize(vec3(-1, 0, .1));
// vec3 cameraRayDir = normalize(vec3(12, 21, 7));
// vec3 cameraRayDir = normalize(vec3(1, 1, 0));
const varp vec3 globalLightDir = normalize(vec3(0.5, 1, -0.5));
// vec3 globalLightDir = cameraRayDir;
const varp vec3 cameraRayDirPlane = normalize(vec3(cameraRayDir.xy, 0));
const varp vec3 horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
const varp vec3 vertiline = normalize(cross(cameraRayDir, horizline));

// vec3 cameraPos = vec3(-13, -22, -8)*1.5/16.0;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
    mat4 trans_w2s_old;
} ubo;

//quatornions!
layout(push_constant) uniform constants{
    vec4 rot, old_rot;
    vec4 shift, old_shift;
} pco;

precision highp float;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);
    vec3 fnorm = vec3(normIn);

    vec4 world_pos     = vec4(qtransform(pco.rot    , fpos) + pco.shift.xyz ,1);
    // vec4 world_pos     = vec4(posIn + pco.shift.xyz ,1);
    vec4 world_pos_old = vec4(qtransform(pco.old_rot, fpos) + pco.old_shift.xyz ,1);
    // vec4 world_pos_old = vec4(posIn + pco.old_shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = -clip_coords.z;
    vec3 clip_coords_old = (ubo.trans_w2s_old*world_pos_old).xyz;

    // vec4 world_pos     = pco.trans_m2w     * vec4(posIn,1);
    // vec4 world_pos_old = pco.trans_m2w_old * vec4(posIn,1);
    // vec3 clip_coords =     ((pco.trans_w2s    *pco.trans_m2w)    * vec4(posIn,1)).xyz;
    //      clip_coords.z = -clip_coords.z;
    // vec3 clip_coords_old = ((pco.trans_w2s_old*pco.trans_m2w_old) * vec4(posIn,1)).xyz;
    // vec3 clip_coords =     ((pco.trans_m2w    )  * vec4(posIn,1)).xyz;
    //      clip_coords.z = -clip_coords.z;
    // vec3 clip_coords_old = ((pco.trans_m2w_old) * vec4(posIn,1)).xyz;

    gl_Position  = vec4(clip_coords, 1);    
    
    // mat3 m2w_normals = transpose(inverse(mat3(pco.trans_m2w))); 

    // uv_shift = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    
    norm = (qtransform(pco.rot,fnorm));
    // norm = normalize(cross(dFdx(world_pos.xyz), dFdy(world_pos.xyz)));
    mat = uint(MatIDIn);

    // if (ubo.trans_w2s != pco.trans_w2s){
    // gl_Position  = vec4(vec3(0), 1);    

    // }
}
/*
#version 450

layout(location = 0) in vec3 posIn;
layout(location = 1) in vec3 normIn;
layout(location = 2) in uint MatIDIn;

// layout(location = 0)      out float depth;
layout(location = 0) flat out vec2 old_uv;
layout(location = 1) flat out vec3 normal;
layout(location = 2) flat out float mat; //uint8 thing. But in float
layout(location = 3)      out float depth;

const vec3 cameraRayDir = normalize(vec3(0.1, 1.0, -0.5));
const vec3 cameraPos = vec3(60, 0, 50);
// vec3 cameraRayDir = normalize(vec3(-1, 0, .1));
// vec3 cameraRayDir = normalize(vec3(12, 21, 7));
// vec3 cameraRayDir = normalize(vec3(1, 1, 0));
const vec3 globalLightDir = normalize(vec3(0.5, 1, -0.5));
// vec3 globalLightDir = cameraRayDir;
const vec3 cameraRayDirPlane = normalize(vec3(cameraRayDir.xy, 0));
const vec3 horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
const vec3 vertiline = normalize(cross(cameraRayDir, horizline));

// vec3 cameraPos = vec3(-13, -22, -8)*1.5/16.0;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
} ubo;
layout(push_constant) uniform constants{
    mat4 trans_m2w; //model to world and than to screen
    mat4 trans_m2w_old; //old
} pco;

precision highp float;

// vec2 encode (in vec3 normal){
//     return normalize(normal.xy)*sqrt(normal.z*0.5+0.5);
// }
vec2 OctWrap(vec2 v)
{   
    vec2 res;
    res = (1.0 - abs(v.yx));
    res.x *=(v.x >= 0.0 ? 1.0 : -1.0);
    res.y *=(v.y >= 0.0 ? 1.0 : -1.0);
    return res;
}
 
vec2 encode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

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
        clip_coords    .z = -dot(relative_pos    , cameraRayDir) / 1000.0+0.5; //TODO:
    vec3 clip_coords_old;
        clip_coords_old.x =  dot(relative_pos_old, horizline) / view_width ;
        clip_coords_old.y =  dot(relative_pos_old, vertiline) / view_height;
        clip_coords_old.z = dot(relative_pos_old, cameraRayDir) / 1000.0+0.5; //TODO:

    gl_Position  = vec4(clip_coords, 1);

    //TODO: MERGE UNITS
    // pos = cameraPos + 
    //     horizline*clip_coords.x*view_width  + 
    //     vertiline*clip_coords.y*view_height + 
    //     depth*cameraRayDir;
    // vec2 pos = world_pos.xyz;
    // pos.x = depth;
    
    
    mat3 m2w_normals = transpose(inverse(mat3(pco.trans_m2w))); 

    //old uv = old_clip/2 + 0.5 - do in this shader
    // vec2 pos_old = clip_coords_old.xy; //-1 .. +1
    vec2 old_uv = clip_coords_old.xy/2 + 0.5; //0..1

    // packFloat2x16(pos_old.x, pos_old.y);
    // packHalf2x16(pos_old);
    // pack
    vec3 norm = normalize(m2w_normals*normIn);
    // vec2 norm_encoded = encode(norm);
    // float


      uv_encoded = packUnorm2x16(old_uv);
    norm_encoded = packSnorm2x16(encode(norm));
             mat = uint(MatIDIn);
            //  mat = floatBitsToUint(encode(norm).y);
           depth = dot(relative_pos, cameraRayDir);
    // MatID = float(66);
}
*/