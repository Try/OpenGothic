#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "materials_common.glsl"

#define PfxOrientationNone       0
#define PfxOrientationVelocity   1
#define PfxOrientationVelocity3d 2

struct Particle {
  vec3  pos;
  uint  color;
  vec3  size;
  uint  bits0;
  vec3  dir;
  uint  colorB;
  };

struct VaryingsPfx {
#if defined(MAT_UV)
  vec2 uv;
#endif
#if !defined(DEPTH_ONLY)
  vec3 normal;
#endif
#if defined(FORWARD)
  vec3 pos;
#endif
#if defined(MAT_COLOR)
  vec4 color;
#endif
  float d;
  };

layout(binding = L_Pfx, std430) readonly buffer PfxBuf {
  Particle pfx[];
  };

out gl_PerVertex {
  vec4 gl_Position;
  };
#if defined(MAT_VARYINGS)
layout(location = 0) out flat uint bucketId;
layout(location = 1) out Varyings  shOut;
#endif

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) out flat uint debugId;
#endif

struct Vertex {
  vec3  pos;
  vec3  normal;
  vec2  uv;
  vec3  size;
  uint  bits0;
  vec3  dir;
  uint  color;
  };

Vertex pullVertex(uint id) {
  Vertex ret;
  ret.pos    = pfx[id].pos;
  ret.color  = pfx[id].color;
  ret.size   = pfx[id].size;
  ret.bits0  = pfx[id].bits0;
  ret.dir    = pfx[id].dir;
  ret.uv     = vec2(0);
  ret.normal = vec3(0);
  return ret;
  }

void rotate(out vec3 rx, out vec3 ry, float a, in vec3 x, in vec3 y) {
  const float c = cos(a);
  const float s = sin(a);

  rx.x = x.x*c - y.x*s;
  rx.y = x.y*c - y.y*s;
  rx.z = x.z*c - y.z*s;

  ry.x = x.x*s + y.x*c;
  ry.y = x.y*s + y.y*c;
  ry.z = x.z*s + y.z*c;
  }

vec4 processVertex(out Varyings shOut, in Vertex v, uint instanceId, uint vboOffset) {
  // Bilboards
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
      const uint colorB = pfx[instanceId].colorB;
      v.normal = vec3(0,1,0);

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

#if defined(MAT_UV)
  shOut.uv     = v.uv;
#endif
#if defined(MAT_NORMAL)
  shOut.normal = v.normal;
#endif
#if defined(MAT_POSITION)
  shOut.pos    = v.pos;
#endif
#if defined(MAT_COLOR)
  shOut.color = unpackUnorm4x8(v.color);
#endif

  return scene.viewProject*vec4(v.pos,1.0);
  }

void main() {
  const Vertex vert = pullVertex(gl_InstanceIndex);

  Varyings var;
  gl_Position = processVertex(var, vert, gl_InstanceIndex, gl_VertexIndex);
#if defined(MAT_VARYINGS)
  bucketId = 0;
  shOut    = var;
#endif
  }
