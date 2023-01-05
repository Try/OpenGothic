#ifndef VERTEX_PROCESS_GLSL
#define VERTEX_PROCESS_GLSL

#include "../common.glsl"

#if defined(VERTEX)
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

#if (MESH_TYPE==T_MORPH)
vec3 morphOffset(int i, uint vertexIndex) {
  vec2  ai        = unpackUnorm2x16(push.morph[i].alpha16_intensity16);
  float alpha     = ai.x;
  float intensity = ai.y;
  if(intensity<=0)
    return vec3(0);

  uint  vId   = vertexIndex + push.morph[i].indexOffset;
  int   index = morphId.index[vId];
  if(index<0)
    return vec3(0);

  uint  f0 = push.morph[i].sample0;
  uint  f1 = push.morph[i].sample1;
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

vec4 processVertex(out Varyings shOut, uint objId, uint vboOffset) {
#if   (MESH_TYPE==T_PFX)
  vec3  pos    = pfx[objId].pos;
  uint  color  = pfx[objId].color;
  vec3  size   = pfx[objId].size;
  uint  bits0  = pfx[objId].bits0;
  vec3  dir    = pfx[objId].dir;
  vec2  uv     = vec2(0);
  vec3  normal = vec3(0);
#elif   (MESH_TYPE==T_SKINING) && defined(VERTEX)
  vec3  normal = inNormal;
  vec2  uv     = inUV;
  uint  color  = inColor;
  vec3  pos0   = inPos0;
  vec3  pos1   = inPos1;
  vec3  pos2   = inPos2;
  vec3  pos3   = inPos3;
  uvec4 boneId = uvec4(objId) + uvec4(unpackUnorm4x8(inId)*255.0);
  vec4  weight = inWeight;
#elif (MESH_TYPE==T_SKINING) && defined(MESH)
  uint  id     = vboOffset*23;
  vec3  normal = vec3(vertices[id + 0], vertices[id + 1], vertices[id + 2]);
  vec2  uv     = vec2(vertices[id + 3], vertices[id + 4]);
  uint  color  = floatBitsToUint(vertices[id + 5]);
  vec3  pos0   = vec3(vertices[id +  6], vertices[id +  7], vertices[id +  8]);
  vec3  pos1   = vec3(vertices[id +  9], vertices[id + 10], vertices[id + 11]);
  vec3  pos2   = vec3(vertices[id + 12], vertices[id + 13], vertices[id + 14]);
  vec3  pos3   = vec3(vertices[id + 15], vertices[id + 16], vertices[id + 17]);
  uvec4 inId   = uvec4(unpackUnorm4x8(floatBitsToUint(vertices[id + 18]))*255.0);
  vec4  weight = vec4(vertices[id + 19], vertices[id + 20], vertices[id + 21], vertices[id + 22]);
  uvec4 boneId = uvec4(objId) + inId;
#elif  defined(VERTEX)
  vec3  pos    = inPos;
  vec3  normal = inNormal;
  vec2  uv     = inUV;
  uint  color  = inColor;
#elif  defined(MESH)
  uint  id     = vboOffset*9;
  vec3  pos    = vec3(vertices[id + 0], vertices[id + 1], vertices[id + 2]);
  vec3  normal = vec3(vertices[id + 3], vertices[id + 4], vertices[id + 5]);
  vec2  uv     = vec2(vertices[id + 6], vertices[id + 7]);
  uint  color  = floatBitsToUint(vertices[id + 8]);
#endif

  // Position offsets
  vec3 dpos   = vec3(0);
#if (MESH_TYPE==T_MORPH)
  for(int i=0; i<MAX_MORPH_LAYERS; ++i)
    dpos += morphOffset(i,vboOffset);
#endif
#if defined(LVL_OBJECT)
  dpos += normal*push.fatness;
#endif

  // Normals
#if (MESH_TYPE==T_SKINING)
  normal = (matrix[objId]*vec4(normal,  0.0)).xyz;
  normal = vec3(normal.z,normal.y,-normal.x);
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_MORPH)
  normal = (matrix[objId]*vec4(normal,  0.0)).xyz;
#else
  // normal = normal;
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

    const bool  visZBias         = bitfieldExtract(bits0, 0, 1)!=0;
    const bool  visTexIsQuadPoly = bitfieldExtract(bits0, 1, 1)!=0;
    const bool  visYawAlign      = bitfieldExtract(bits0, 2, 1)!=0;
    const bool  isTrail          = bitfieldExtract(bits0, 3, 1)!=0;
    const uint  visOrientation   = bitfieldExtract(bits0, 4, 2);

    const mat4  m      = scene.viewProject;
    vec3        left   = scene.pfxLeft;
    vec3        top    = scene.pfxTop;
    vec3        depth  = scene.pfxDepth;

    if(visYawAlign) {
      left = vec3(left.x, 0, left.z);
      top  = vec3(0, -1, 0);
      }

    uv     = vec2(U[vboOffset],V[vboOffset]);
    normal = -depth;

    if(isTrail) {
      const uint colorB = pfx[objId].colorB;
      normal = vec3(0,1,0);

      if(dyQ[vboOffset]>0)
        color = colorB;

      float tA     = size.y;
      float tB     = size.z;

      vec3 n  = cross(depth,dir);
      n = normalize(n);
      n = n*size.x;

      pos += (dxQ[vboOffset])*n + (dyQ[vboOffset]+0.5)*dir;

      uv.x = dxQ[vboOffset]+0.5;
      uv.y = 1.0-(tA + (dyQ[vboOffset]+0.5)*(tB-tA));
      }
    else if(visOrientation==PfxOrientationVelocity3d) {
      float ldir = length(dir);
      if(ldir!=0.f)
        dir/=ldir;
      top  = -dir;
      left = -cross(top,depth);
      }
    else if(visOrientation==PfxOrientationVelocity) {
      dir = (m * vec4(dir,0)).xyz;
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
      pos += left*dxQ[vboOffset]*size.x + top*dyQ[vboOffset]*size.y;
    else if(!isTrail)
      pos += left*dxT[vboOffset]*size.x + top*dyT[vboOffset]*size.y;

    if(visZBias)
      pos -= size.z*depth;
  }
#endif

  // Position
#if (MESH_TYPE==T_SKINING)
  vec3 pos = vec3(0);
  {
    dpos = (matrix[objId]*vec4(dpos,0)).xyz;
    dpos = vec3(dpos.z,dpos.y,-dpos.x);

    vec3 t0   = (matrix[boneId.x]*vec4(pos0,1.0)).xyz;
    vec3 t1   = (matrix[boneId.y]*vec4(pos1,1.0)).xyz;
    vec3 t2   = (matrix[boneId.z]*vec4(pos2,1.0)).xyz;
    vec3 t3   = (matrix[boneId.w]*vec4(pos3,1.0)).xyz;
    pos += (t0*weight.x + t1*weight.y + t2*weight.z + t3*weight.w) + dpos;
  }
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_MORPH)
  pos    = (matrix[objId]*vec4(pos+dpos,1.0)).xyz;
#else
  //pos = pos;
#endif

#if defined(MAT_UV)
  shOut.uv   = uv;
#endif

#if !defined(DEPTH_ONLY)
  shOut.normal = normal;
#endif

#if defined(FORWARD) || (MESH_TYPE==T_LANDSCAPE)
  shOut.pos    = pos;
#endif

#if defined(MAT_COLOR)
  shOut.color = unpackUnorm4x8(color);
#endif

  vec4 position = scene.viewProject*vec4(pos,1.0);
#if defined(SHADOW_MAP)
#  if defined(ATEST)
  position.z -= 16.0*(1.0/32768.0)*position.w;
#  else
  //position.z +=  4.0*(1.0/32768.0)*position.w;
#  endif
#endif
  return position;
  }

#endif
