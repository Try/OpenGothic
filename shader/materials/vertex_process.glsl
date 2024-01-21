#ifndef VERTEX_PROCESS_GLSL
#define VERTEX_PROCESS_GLSL

#include "common.glsl"

#if defined(GL_VERTEX_SHADER) && !defined(CLUSTER)
#if (MESH_TYPE==T_SKINING)
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inColor;
layout(location = 3) in vec3 inPos0;
layout(location = 4) in vec3 inPos1;
layout(location = 5) in vec3 inPos2;
layout(location = 6) in vec3 inPos3;
layout(location = 7) in uint inId;
layout(location = 8) in vec4 inWeight;
#elif (MESH_TYPE==T_PFX)
// none
#else
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inColor;
#endif
#endif

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

#if defined(BINDLESS)
Vertex pullVertex(uint bucketId, uint id) {
  const uint vi = nonuniformEXT(bucketId);
#else
Vertex pullVertex(uint id) {
#endif

  Vertex ret;
#if   (MESH_TYPE==T_PFX)
  ret.pos    = pfx[id].pos;
  ret.color  = pfx[id].color;
  ret.size   = pfx[id].size;
  ret.bits0  = pfx[id].bits0;
  ret.dir    = pfx[id].dir;
  ret.uv     = vec2(0);
  ret.normal = vec3(0);
#elif (MESH_TYPE==T_SKINING) && defined(BINDLESS)
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
#elif (MESH_TYPE==T_SKINING) && defined(GL_VERTEX_SHADER) && !defined(CLUSTER)
  ret.normal = inNormal;
  ret.uv     = inUV;
  ret.color  = inColor;
  ret.pos0   = inPos0;
  ret.pos1   = inPos1;
  ret.pos2   = inPos2;
  ret.pos3   = inPos3;
  ret.boneId = uvec4(unpackUnorm4x8(inId)*255.0);
  ret.weight = inWeight;
#elif (MESH_TYPE==T_SKINING) && defined(MESH)
  id *=23;
  ret.normal = vec3(vbo.vertices[id + 0], vbo.vertices[id + 1], vbo.vertices[id + 2]);
  ret.uv     = vec2(vbo.vertices[id + 3], vbo.vertices[id + 4]);
  ret.color  = floatBitsToUint(vbo.vertices[id + 5]);
  ret.pos0   = vec3(vbo.vertices[id +  6], vbo.vertices[id +  7], vbo.vertices[id +  8]);
  ret.pos1   = vec3(vbo.vertices[id +  9], vbo.vertices[id + 10], vbo.vertices[id + 11]);
  ret.pos2   = vec3(vbo.vertices[id + 12], vbo.vertices[id + 13], vbo.vertices[id + 14]);
  ret.pos3   = vec3(vbo.vertices[id + 15], vbo.vertices[id + 16], vbo.vertices[id + 17]);
  ret.boneId = uvec4(unpackUnorm4x8(floatBitsToUint(vbo.vertices[id + 18]))*255.0);
  ret.weight = vec4(vbo.vertices[id + 19], vbo.vertices[id + 20], vbo.vertices[id + 21], vbo.vertices[id + 22]);
#elif defined(BINDLESS)
  id *= 9;
  ret.pos    = vec3(vbo[vi].vertices[id + 0], vbo[vi].vertices[id + 1], vbo[vi].vertices[id + 2]);
  ret.normal = vec3(vbo[vi].vertices[id + 3], vbo[vi].vertices[id + 4], vbo[vi].vertices[id + 5]);
  ret.uv     = vec2(vbo[vi].vertices[id + 6], vbo[vi].vertices[id + 7]);
  ret.color  = floatBitsToUint(vbo[vi].vertices[id + 8]);
#elif defined(GL_VERTEX_SHADER) && !defined(CLUSTER)
  ret.pos    = inPos;
  ret.normal = inNormal;
  ret.uv     = inUV;
  ret.color  = inColor;
#elif defined(MESH)
  id *= 9;
  ret.pos    = vec3(vbo.vertices[id + 0], vbo.vertices[id + 1], vbo.vertices[id + 2]);
  ret.normal = vec3(vbo.vertices[id + 3], vbo.vertices[id + 4], vbo.vertices[id + 5]);
  ret.uv     = vec2(vbo.vertices[id + 6], vbo.vertices[id + 7]);
  ret.color  = floatBitsToUint(vbo.vertices[id + 8]);
#endif
  return ret;
  }

#if (MESH_TYPE==T_MORPH) && defined(BINDLESS)
vec3 morphOffset(uint bucketId, uint animPtr, uint vertexIndex) {
  MorphDesc md        = pullMorphDesc(animPtr);
  vec2      ai        = unpackUnorm2x16(md.alpha16_intensity16);
  float     alpha     = ai.x;
  float     intensity = ai.y;
  if(intensity<=0)
    return vec3(0);

  uint  vId   = vertexIndex + md.indexOffset;
  int   index = morphId[nonuniformEXT(bucketId)].index[vId];
  if(index<0)
    return vec3(0);

  uint  f0 = md.sample0;
  uint  f1 = md.sample1;
  vec3  a  = morph[nonuniformEXT(bucketId)].samples[f0 + index].xyz;
  vec3  b  = morph[nonuniformEXT(bucketId)].samples[f1 + index].xyz;

  return mix(a,b,alpha) * intensity;
  }
#elif (MESH_TYPE==T_MORPH)
vec3 morphOffset(uint bucketId, uint animPtr, uint vertexIndex) {
  MorphDesc md        = pullMorphDesc(animPtr);
  vec2      ai        = unpackUnorm2x16(md.alpha16_intensity16);
  float     alpha     = ai.x;
  float     intensity = ai.y;
  if(intensity<=0)
    return vec3(0);

  uint  vId   = vertexIndex + md.indexOffset;
  int   index = morphId.index[vId];
  if(index<0)
    return vec3(0);

  uint  f0 = md.sample0;
  uint  f1 = md.sample1;
  vec3  a  = morph.samples[f0 + index].xyz;
  vec3  b  = morph.samples[f1 + index].xyz;

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
  Instance obj   = pullInstance(instanceId);
#elif (MESH_TYPE==T_PFX)
  uint     objId = instanceId;
#endif

  // Position offsets
#if (MESH_TYPE==T_MORPH)
  for(int i=0; i<MAX_MORPH_LAYERS; ++i)
    v.pos += morphOffset(bucketId, obj.animPtr+i, vboOffset);
#endif

  // Normals
  vec3 normal = vec3(0);
#if defined(LVL_OBJECT)
  normal = obj.mat*vec4(v.normal,0);
#else
  normal = v.normal;
#endif

  // Bilboards
#if (MESH_TYPE==T_PFX)
  {
    const float U[6]   = { 0, 1, 0,  0, 1, 1};
    const float V[6]   = { 1, 0, 0,  1, 1, 0};

    const float dxQ[6] = {-0.5, 0.5, -0.5, -0.5,  0.5,  0.5};
    const float dyQ[6] = { 0.5,-0.5, -0.5,  0.5,  0.5, -0.5};

    const float dxT[6] = {-0.3333,  1.5, -0.3333, 0,0,0};
    const float dyT[6] = { 1.5, -0.3333, -0.3333, 0,0,0};

    const bool  visZBias         = bitfieldExtract(v.bits0, 0, 1)!=0;
    const bool  visTexIsQuadPoly = bitfieldExtract(v.bits0, 1, 1)!=0;
    const bool  visYawAlign      = bitfieldExtract(v.bits0, 2, 1)!=0;
    const bool  isTrail          = bitfieldExtract(v.bits0, 3, 1)!=0;
    const uint  visOrientation   = bitfieldExtract(v.bits0, 4, 2);

    const mat4  m      = scene.viewProject;
    vec3        left   = scene.pfxLeft;
    vec3        top    = scene.pfxTop;
    vec3        depth  = scene.pfxDepth;

    if(visYawAlign) {
      left = vec3(left.x, 0, left.z);
      top  = vec3(0, -1, 0);
      }

    v.uv     = vec2(U[vboOffset],V[vboOffset]);
    v.normal = -depth;

    if(isTrail) {
      const uint colorB = pfx[objId].colorB;
      normal = vec3(0,1,0);

      if(dyQ[vboOffset]>0)
        v.color = colorB;

      float tA     = v.size.y;
      float tB     = v.size.z;

      vec3 n  = cross(depth,v.dir);
      n = normalize(n);
      n = n*v.size.x;

      v.pos += (dxQ[vboOffset])*n + (dyQ[vboOffset]+0.5)*v.dir;

      v.uv.x = dxQ[vboOffset]+0.5;
      v.uv.y = 1.0-(tA + (dyQ[vboOffset]+0.5)*(tB-tA));
      }
    else if(visOrientation==PfxOrientationVelocity3d) {
      float ldir = length(v.dir);
      if(ldir!=0.f)
        v.dir/=ldir;
      top  = -v.dir;
      left = -cross(top,depth);
      }
    else if(visOrientation==PfxOrientationVelocity) {
      vec3  dir  = (m * vec4(v.dir,0)).xyz;
      float ldir = length(dir);
      dir = (ldir>0) ? (dir/ldir) : vec3(0);

      float sVel = 1.5f*(1.f - abs(dir.z));
      float c    = -dir.x;
      float s    =  dir.y;
      float rot  = (s==0 && c==0) ? 0.f : atan(s,c);

      vec3 l,t;
      rotate(l,t,rot+float(M_PI/2),left,top);
      left = l*sVel;
      top  = t*sVel;
      }
    else {
      // nope
      }

    if(visTexIsQuadPoly)
      v.pos += left*dxQ[vboOffset]*v.size.x + top*dyQ[vboOffset]*v.size.y;
    else if(!isTrail)
      v.pos += left*dxT[vboOffset]*v.size.x + top*dyT[vboOffset]*v.size.y;

    if(visZBias)
      v.pos -= v.size.z*depth;
  }
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

#if !defined(DEPTH_ONLY)
  shOut.normal = normal;
#endif

#if defined(FORWARD) || (MESH_TYPE==T_LANDSCAPE)
  shOut.pos    = pos;
#endif

#if defined(MAT_COLOR)
  shOut.color = unpackUnorm4x8(v.color);
#endif

  vec4 ret = scene.viewProject*vec4(pos,1.0);
#ifdef MESH
  //ret.y = -ret.y;
#endif
  return ret;
  }

#endif
