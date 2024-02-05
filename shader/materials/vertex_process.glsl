#ifndef VERTEX_PROCESS_GLSL
#define VERTEX_PROCESS_GLSL

#include "common.glsl"

struct Vertex {
#if (MESH_TYPE==T_LANDSCAPE) || (MESH_TYPE==T_OBJ) || (MESH_TYPE==T_MORPH)
  vec3  pos;
  uint  color;
  vec3  normal;
  vec2  uv;
#elif(MESH_TYPE==T_SKINING)
  vec3  pos0;
  vec3  pos1;
  vec3  pos2;
  vec3  pos3;
  uvec4 boneId;
  vec4  weight;
  vec3  normal;
  vec2  uv;
  uint  color;
#elif (MESH_TYPE==T_PFX)
  vec3  pos;
  vec3  normal;
  vec2  uv;
  vec3  size;
  uint  bits0;
  vec3  dir;
  uint  color;
#endif
  };

Vertex pullVertex(uint bucketId, uint id) {
#if defined(BINDLESS)
  nonuniformEXT uint vi = bucketId;
#else
  const         uint vi = 0;
#endif
  Vertex ret;
#if (MESH_TYPE==T_SKINING)
  id *=23;
  ret.normal = vec3(vbo[vi].vertices[id + 0], vbo[vi].vertices[id + 1], vbo[vi].vertices[id + 2]);
  ret.uv     = vec2(vbo[vi].vertices[id + 3], vbo[vi].vertices[id + 4]);
  ret.color  = floatBitsToUint(vbo[vi].vertices[id + 5]);
  ret.pos0   = vec3(vbo[vi].vertices[id +  6], vbo[vi].vertices[id +  7], vbo[vi].vertices[id +  8]);
  ret.pos1   = vec3(vbo[vi].vertices[id +  9], vbo[vi].vertices[id + 10], vbo[vi].vertices[id + 11]);
  ret.pos2   = vec3(vbo[vi].vertices[id + 12], vbo[vi].vertices[id + 13], vbo[vi].vertices[id + 14]);
  ret.pos3   = vec3(vbo[vi].vertices[id + 15], vbo[vi].vertices[id + 16], vbo[vi].vertices[id + 17]);
  ret.boneId = uvec4(unpackUnorm4x8(floatBitsToUint(vbo[vi].vertices[id + 18]))*255.0);
  ret.weight = vec4(vbo[vi].vertices[id + 19], vbo[vi].vertices[id + 20], vbo[vi].vertices[id + 21], vbo[vi].vertices[id + 22]);
#elif (MESH_TYPE==T_LANDSCAPE) || (MESH_TYPE==T_OBJ) || (MESH_TYPE==T_MORPH)
  id *= 9;
  ret.pos    = vec3(vbo[vi].vertices[id + 0], vbo[vi].vertices[id + 1], vbo[vi].vertices[id + 2]);
  ret.normal = vec3(vbo[vi].vertices[id + 3], vbo[vi].vertices[id + 4], vbo[vi].vertices[id + 5]);
  ret.uv     = vec2(vbo[vi].vertices[id + 6], vbo[vi].vertices[id + 7]);
  ret.color  = floatBitsToUint(vbo[vi].vertices[id + 8]);
#else
#error "unknown mesh type"
#endif
  return ret;
  }

#if (MESH_TYPE==T_MORPH)
vec3 morphOffset(uint bucketId, uint animPtr, uint vertexIndex) {
#if defined(BINDLESS)
  nonuniformEXT uint i = bucketId;
#else
  const         uint i = 0;
#endif
  MorphDesc md        = pullMorphDesc(animPtr);
  vec2      ai        = unpackUnorm2x16(md.alpha16_intensity16);
  float     alpha     = ai.x;
  float     intensity = ai.y;
  if(intensity<=0)
    return vec3(0);

  uint  vId   = vertexIndex + md.indexOffset;
  int   index = morphId[i].index[vId];
  if(index<0)
    return vec3(0);

  uint  f0 = md.sample0;
  uint  f1 = md.sample1;
  vec3  a  = morph[i].samples[f0 + index].xyz;
  vec3  b  = morph[i].samples[f1 + index].xyz;

  return mix(a,b,alpha) * intensity;
  }
#endif

#if (MESH_TYPE==T_PFX)
void rotate(out vec3 rx, out vec3 ry, float a, in vec3 x, in vec3 y){
  const float c = cos(a);
  const float s = sin(a);

  rx.x = x.x*c - y.x*s;
  rx.y = x.y*c - y.y*s;
  rx.z = x.z*c - y.z*s;

  ry.x = x.x*s + y.x*c;
  ry.y = x.y*s + y.y*c;
  ry.z = x.z*s + y.z*c;
  }
#endif

vec4 processVertex(out Varyings shOut, in Vertex v, uint bucketId, uint instanceId, uint vboOffset) {
#if defined(LVL_OBJECT)
  Instance obj = pullInstance(instanceId);
#endif

  // Position offsets
#if (MESH_TYPE==T_MORPH)
  for(int i=0; i<MAX_MORPH_LAYERS; ++i)
    v.pos += morphOffset(bucketId, obj.animPtr+i, vboOffset);
#endif

  // Normals
  vec3 normal = v.normal;
#if defined(LVL_OBJECT)
  normal = obj.mat*vec4(normal,0);
#endif

  // Position
#if (MESH_TYPE==T_SKINING)
  vec3 pos  = vec3(0);
  vec3 dpos = normal*obj.fatness;
  {
    const uvec4 boneId = v.boneId + uvec4(obj.animPtr);

    const vec3  t0 = (pullMatrix(boneId.x)*vec4(v.pos0,1.0)).xyz;
    const vec3  t1 = (pullMatrix(boneId.y)*vec4(v.pos1,1.0)).xyz;
    const vec3  t2 = (pullMatrix(boneId.z)*vec4(v.pos2,1.0)).xyz;
    const vec3  t3 = (pullMatrix(boneId.w)*vec4(v.pos3,1.0)).xyz;

    pos = (t0*v.weight.x + t1*v.weight.y + t2*v.weight.z + t3*v.weight.w) + dpos;
  }
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_MORPH)
  vec3 dpos = normal*obj.fatness;
  vec3 pos  = obj.mat*vec4(v.pos,1.0)  + dpos;
#else
  vec3 pos  = v.pos;
#endif

#if defined(MAT_UV)
  shOut.uv     = v.uv;
#endif

#if defined(MAT_NORMAL)
  shOut.normal = normal;
#endif

#if defined(MAT_POSITION)
  shOut.pos    = pos;
#endif

#if defined(MAT_COLOR)
  shOut.color = unpackUnorm4x8(v.color);
#endif

  return scene.viewProject*vec4(pos,1.0);
  }

#endif
