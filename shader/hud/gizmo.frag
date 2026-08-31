#version 460

#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in flat uint  axis;
layout(location = 1) in flat float scale;

layout(std140, push_constant) uniform Push {
  vec3 origin;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform texture2D depth;


float dot2(vec3 v) { return dot(v,v); }

vec4 cylIntersect(vec3 ro, vec3 rd, vec3 a, vec3 b, float ra ) {
  vec3  ba = b  - a;
  vec3  oc = ro - a;

  float baba = dot(ba,ba);
  float bard = dot(ba,rd);
  float baoc = dot(ba,oc);
  float k2   = baba            - bard*bard;
  float k1   = baba*dot(oc,rd) - baoc*bard;
  float k0   = baba*dot(oc,oc) - baoc*baoc - ra*ra*baba;
  float h    = k1*k1 - k2*k0;

  if( h<0.0 )
    return vec4(-1.0);//no intersection
  h = sqrt(h);
  float t = (-k1-h)/k2;
  // body
  float y = baoc + t*bard;
  if( y>0.0 && y<baba )
    return vec4((oc+t*rd - ba*y/baba)/ra, t);

  // caps
  t = ( ((y<0.0) ? 0.0 : baba) - baoc)/bard;
  if(abs(k1+k2*t) < h)
    return vec4(ba*sign(y)/sqrt(baba), t);

  return vec4(-1.0);//no intersection
  }

vec4 coneIntersect(vec3 ro, vec3 rd, vec3 pa, vec3 pb, float ra, float rb) {
  vec3  ba = pb - pa;
  vec3  oa = ro - pa;
  vec3  ob = ro - pb;
  float m0 = dot(ba,ba);
  float m1 = dot(oa,ba);
  float m2 = dot(rd,ba);
  float m3 = dot(rd,oa);
  float m5 = dot(oa,oa);
  float m9 = dot(ob,ba);

  // caps
  if(m1 < 0.0) {
    if(dot2(oa*m2-rd*m1) < (ra*ra*m2*m2)) // delayed division
      return vec4(-ba*inversesqrt(m0), -m1/m2);
    }
  else if(m9 > 0.0) {
    float t = -m9/m2;                     // NOTE delayed division
    if(dot2(ob+rd*t) < (rb*rb))
      return vec4(ba*inversesqrt(m0), t);
    }

  // body
  float rr = ra - rb;
  float hy = m0 + rr*rr;
  float k2 = m0*m0    - m2*m2*hy;
  float k1 = m0*m0*m3 - m1*m2*hy + m0*ra*(rr*m2*1.0        );
  float k0 = m0*m0*m5 - m1*m1*hy + m0*ra*(rr*m1*2.0 - m0*ra);
  float h  = k1*k1 - k2*k0;
  if(h < 0.0)
    return vec4(-1.0); //no intersection
  float t = (-k1-sqrt(h))/k2;
  float y = m1 + t*m2;
  if(y < 0.0 || y > m0)
    return vec4(-1.0); //no intersection
  return vec4(normalize(m0*(m0*(oa+t*rd)+rr*ba*ra)-ba*hy*y), t);
  }

vec4 arrowIntersect(vec3 ro, vec3 rd) {
  vec3 off0 = vec3(0), off1 = vec3(0), off2 = vec3(0);
  off0[axis] = scale*10;
  off1[axis] = scale*160;
  off2[axis] = scale*200;

  vec4 body = cylIntersect (ro, rd, origin, origin + off1, 4.0 * scale);
  vec4 cap  = coneIntersect(ro, rd, origin + off1, origin + off2, 10.0 * scale, 0);
  if(body.w>=0 && (body.w<cap.w || cap.w<=0))
    return body;
  return cap;
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  start4    = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, 1.0, 1.0);
  const vec3  start     = start4.xyz/start4.w;

  const vec3  camPos    = scene.camPos;
  const vec3  view      = normalize(start - camPos);

  const vec4 nort = arrowIntersect(camPos, view);
  if(nort.w>0) {
    vec3 clr = vec3(0);
    clr[axis] = 1;
    clr *= (max(0.0, dot(scene.sunDir, nort.xyz))*0.8 + 0.2);
    float d  = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;
    float dl = linearDepth(d, scene.clipInfo);
    if(nort.w > dl) {
      ivec2 id = ivec2(gl_FragCoord.xy)/4;
      clr *= (id.x+id.y)%2==0 ? 0.1 : 0.9;
      }
    outColor = vec4(clr,1);
    return;
    }

  discard;
  }
