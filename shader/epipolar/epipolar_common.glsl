#ifndef EPIPOLAR_COMMON_GLSL
#define EPIPOLAR_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"

struct Epipole {
  vec2  rayOrig;
  vec2  rayDir;
  float tMin;
  float tMax;
  float dBegin;
  float dEnd;
  };

//TODO: remove/hide
vec4 dbgColor = vec4(0);

vec2 sunPosition2d(const SceneDesc scene) {
  vec3 sun = scene.sunDir;
  sun = (scene.viewProject*vec4(sun,0)).xyz;
  sun.xy /= sun.z;
  return sun.xy;
  }

vec2 rayPosition2d(uint rayId, ivec2 viewportSize, uint NumSlices) {
  const float ratio = float(viewportSize.x)/float(viewportSize.x+viewportSize.y);
  const uint  hor   = int(0.5*ratio*NumSlices);
  const uint  vert  = int(NumSlices)/2 - hor;

  if(rayId<hor) {
    // bottom
    dbgColor = vec4(0,1,0,0)*((rayId+0.5)/float(hor));
    return vec2((rayId+0.5)/float(hor), float(viewportSize.y-0.5)/float(viewportSize.y));
    }
  rayId -= hor;

  if(rayId<hor) {
    // top
    dbgColor = vec4(1,0,0,0)*((rayId+0.5)/float(hor));
    return vec2((rayId+0.5)/float(hor), float(0.5)/float(viewportSize.y));
    }
  rayId -= hor;

  if(rayId<vert) {
    // left
    dbgColor = vec4(0,0,1,0)*(rayId+0.5)/float(vert);
    return vec2(float(0.5)/float(viewportSize.x), (rayId+0.5)/float(vert));
    }
  rayId -= vert;

  // right
  dbgColor = vec4(1,0,1,0)*(rayId+0.5)/float(vert);
  return vec2(float(viewportSize.x-0.5)/float(viewportSize.x), (rayId+0.5)/float(vert));
  }

vec2 rayOrigin(const SceneDesc scene, ivec2 fragCoord, ivec2 viewportSize) {
  vec2 sun  = sunPosition2d(scene);
  vec2 rpos = vec2(fragCoord)/vec2(viewportSize);
  rpos  = rpos*2.0 - 1.0;

  vec2 dv  = rpos - sun;
  vec2 ret = vec2(0);
  {
    float d = (dv.x<0 ? -1 : +1) - sun.x;
    ret = sun + dv*abs(d/dv.x);
    if(-1<=ret.y && ret.y<=1)
      return ret;
  }
  {
    float d = (dv.y<0 ? -1 : +1) - sun.y;
    ret = sun + dv*abs(d/dv.y);
    if(-1<=ret.x && ret.x<=1)
      return ret;
  }
  return vec2(-1);
  }

uint sliceId(const SceneDesc scene, ivec2 fragCoord, ivec2 viewportSize, uint NumSlices) {
  const vec2  src   = rayOrigin(scene, fragCoord, viewportSize);
  const vec2  uv    = src*0.5+0.5;
  const float ratio = float(viewportSize.x)/float(viewportSize.x+viewportSize.y);
  const uint  hor   = int(0.5*ratio*NumSlices);
  const uint  vert  = int(NumSlices)/2 - hor;

  uint rayId = 0;
  if(src.x < src.y && src.x > -src.y) {
    // bottom
    dbgColor = vec4(0,1,0,0);
    return rayId + uint(uv.x * hor);
    }
  rayId += hor;

  if(src.x > src.y && src.x < -src.y) {
    // top
    dbgColor = vec4(1,0,0,0);
    return rayId + uint(uv.x * hor);
    }
  rayId += hor;

  if(src.x < src.y && src.x < -src.y) {
    // left
    dbgColor = vec4(0,0,1,0);
    return rayId + uint(uv.y * vert);
    }
  rayId += vert;

  // right
  dbgColor = vec4(1,0,1,0);
  return rayId + uint(uv.y * vert);
  }

#endif
