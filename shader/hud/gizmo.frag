#version 460

#extension GL_EXT_samplerless_texture_functions : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in flat uint  axis;
layout(location = 1) in flat float scale;
layout(location = 2) in vec3       baseColor;

const float TMax = 1e30f;

layout(std140, push_constant) uniform Push {
  vec3 origin;
  uint axisBits;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform texture2D depth;

struct HitResult {
  vec3  norm;
  float hitT;
  int   axis;
  };

void handleHit(inout HitResult hit, vec3 n, float t, int axis) {
  if(t >= hit.hitT)
    return;
  hit.norm = n;
  hit.hitT = t;
  hit.axis = axis;
  }

float dot2(vec3 v) { return dot(v,v); }

vec4 cylIntersect(inout HitResult hit, int axis, vec3 ro, vec3 rd, vec3 a, vec3 b, float ra) {
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
  if( y>0.0 && y<baba ) {
    handleHit(hit, (oc+t*rd - ba*y/baba)/ra, t, axis);
    return vec4((oc+t*rd - ba*y/baba)/ra, t);
    }

  // caps
  t = ( ((y<0.0) ? 0.0 : baba) - baoc)/bard;
  if(abs(k1+k2*t) < h) {
    handleHit(hit, ba*sign(y)/sqrt(baba), t, axis);
    return vec4(ba*sign(y)/sqrt(baba), t);
    }

  return vec4(-1.0);//no intersection
  }

vec4 sphereIntersect(inout HitResult hit, int axis, vec3 ro, vec3 rd, vec3 center, float radius) {
  vec3 oc = ro - center;

  float b = dot(oc, rd);
  float c = dot(oc, oc) - radius * radius;

  float h = b*b - c;

  if(h < 0.0)
    return vec4(-1.0);

  h = sqrt(h);

  float t = -b - h;

  // Camera is inside sphere: use far intersection.
  if(t < 0.0)
    t = -b + h;

  if(t < 0.0)
    return vec4(-1.0);

  vec3 p = ro + rd * t;
  vec3 n = normalize(p - center);

  handleHit(hit, n, t, axis);
  return vec4(n, t);
  }

bool coneIntersect(inout HitResult hit, int axis, vec3 ro, vec3 rd, vec3 pa, vec3 pb, float ra, float rb) {
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
    if(dot2(oa*m2-rd*m1) < (ra*ra*m2*m2)) {
      // delayed division
      handleHit(hit, -ba*inversesqrt(m0), -m1/m2, axis);
      return true;
      }
    }
  else if(m9 > 0.0) {
    float t = -m9/m2;                     // NOTE delayed division
    if(dot2(ob+rd*t) < (rb*rb)) {
      handleHit(hit, ba*inversesqrt(m0), t, axis);
      return true;
      }
    }

  // body
  float rr = ra - rb;
  float hy = m0 + rr*rr;
  float k2 = m0*m0    - m2*m2*hy;
  float k1 = m0*m0*m3 - m1*m2*hy + m0*ra*(rr*m2*1.0        );
  float k0 = m0*m0*m5 - m1*m1*hy + m0*ra*(rr*m1*2.0 - m0*ra);
  float h  = k1*k1 - k2*k0;
  if(h < 0.0)
    return false; //no intersection
  float t = (-k1-sqrt(h))/k2;
  float y = m1 + t*m2;
  if(y < 0.0 || y > m0)
    return false; //no intersection
  handleHit(hit, normalize(m0*(m0*(oa+t*rd)+rr*ba*ra)-ba*hy*y), t, axis);
  return true;
  }

void arrowIntersect(inout HitResult hit, int axis, vec3 ro, vec3 rd) {
  vec3 off0 = vec3(0), off1 = vec3(0), off2 = vec3(0);
  off0[axis] = scale*10;
  off1[axis] = scale*160;
  off2[axis] = scale*200;

  cylIntersect (hit, axis, ro, rd, origin, origin + off1, 4.0 * scale);
  coneIntersect(hit, axis, ro, rd, origin + off1, origin + off2, 10.0 * scale, 0);
  }

bool rotationRingIntersect(inout HitResult hit, int axis, vec3 ro, vec3 rd, float rIn, float rOut) {
  ro -= origin;
  if(axis == 0) {
    ro = ro.zyx;
    rd = rd.zyx;
    }
  else if(axis == 1) {
    ro = ro.xzy;
    rd = rd.xzy;
    }
  else {
    ro = ro.xyz;
    rd = rd.xyz;
    }

  float po  = 1.0;
  float Ra2 = 0.5*(rIn + rOut); Ra2*=Ra2;
  float ra2 = 0.5*(rIn - rOut); ra2*=ra2;

  float m = dot(ro, ro);
  float n = dot(ro, rd);
  float k = (m + Ra2 - ra2)/2.0;
  float k3 = n;
  float k2 = n*n - Ra2*dot(rd.xy,rd.xy) + k;
  float k1 = n*k - Ra2*dot(rd.xy,ro.xy);
  float k0 = k*k - Ra2*dot(ro.xy,ro.xy);

  if(abs(k3*(k3*k3-k2)+k1) < 0.01) {
    po = -1.0;
    float tmp=k1; k1=k3; k3=tmp;
    k0 = 1.0/k0;
    k1 = k1*k0;
    k2 = k2*k0;
    k3 = k3*k0;
    }

  float c2 = k2*2.0 - 3.0*k3*k3;
  float c1 = k3*(k3*k3-k2)+k1;
  float c0 = k3*(k3*(c2+2.0*k2)-8.0*k1)+4.0*k0;
  c2 /= 3.0;
  c1 *= 2.0;
  c0 /= 3.0;
  float Q = c2*c2 + c0;
  float R = c2*c2*c2 - 3.0*c2*c0 + c1*c1;
  float h = R*R - Q*Q*Q;

  if( h>=0.0 ) {
    h = sqrt(h);
    float v = sign(R+h)*pow(abs(R+h),1.0/3.0); // cube root
    float u = sign(R-h)*pow(abs(R-h),1.0/3.0); // cube root
    vec2 s = vec2( (v+u)+4.0*c2, (v-u)*sqrt(3.0));
    float y = sqrt(0.5*(length(s)+s.x));
    float x = 0.5*s.y/y;
    float r = 2.0*c1/(x*x+y*y);
    float t1 =  x - r - k3; t1 = (po<0.0)?2.0/t1:t1;
    float t2 = -x - r - k3; t2 = (po<0.0)?2.0/t2:t2;
    float t = TMax;
    if( t1>0.0 ) t=t1;
    if( t2>0.0 ) t=min(t,t2);

    vec3 pos  = ro + t*rd;
    vec3 norm = normalize(pos*(dot(pos,pos)-ra2 - Ra2*vec3(1.0,1.0,-1.0)));
    if(axis==0)
      norm = norm.zyx;
    else if(axis==1)
      norm = norm.xzy;
    handleHit(hit, norm, t, axis);
    return true;
    }

  float sQ = sqrt(Q);
  float w = sQ*cos( acos(-R/(sQ*Q)) / 3.0 );
  float d2 = -(w+c2);
  if( d2<0.0 )
    return false;
  float d1 = sqrt(d2);
  float h1 = sqrt(w - 2.0*c2 + c1/d1);
  float h2 = sqrt(w - 2.0*c2 - c1/d1);
  float t1 = -d1 - h1 - k3; t1 = (po<0.0)?2.0/t1:t1;
  float t2 = -d1 + h1 - k3; t2 = (po<0.0)?2.0/t2:t2;
  float t3 =  d1 - h2 - k3; t3 = (po<0.0)?2.0/t3:t3;
  float t4 =  d1 + h2 - k3; t4 = (po<0.0)?2.0/t4:t4;
  float t = TMax;
  if( t1>0.0 ) t=t1;
  if( t2>0.0 ) t=min(t,t2);
  if( t3>0.0 ) t=min(t,t3);
  if( t4>0.0 ) t=min(t,t4);

  vec3 pos  = ro + t*rd;
  vec3 norm = normalize(pos*(dot(pos,pos)-ra2 - Ra2*vec3(1.0,1.0,-1.0)));
  if(axis==0)
    norm = norm.zyx;
  else if(axis==1)
    norm = norm.xzy;
  handleHit(hit, norm, t, axis);
  return true;
  }

vec4 gizmoIntersect(vec3 ro, vec3 rd, int axis) {
  HitResult hit;
  hit.hitT = TMax;
  if(axis < 4) {
    arrowIntersect (hit, 0, ro, rd);
    arrowIntersect (hit, 1, ro, rd);
    arrowIntersect (hit, 2, ro, rd);
    sphereIntersect(hit, 3, ro, rd, origin, 10.0 * scale);
    if(hit.axis!=axis)
      return vec4(-1);
    return vec4(hit.norm, hit.hitT);
    }

  axis -= 4;
  if(axis < 3) {
    rotationRingIntersect(hit, 0, ro, rd, 145.0 * scale, 155.0 * scale);
    rotationRingIntersect(hit, 1, ro, rd, 145.0 * scale, 155.0 * scale);
    rotationRingIntersect(hit, 2, ro, rd, 145.0 * scale, 155.0 * scale);
    sphereIntersect(hit, -1, ro, rd, origin, 145.0 * scale);

    if(hit.axis!=axis)
      return vec4(-1);
    return vec4(hit.norm, hit.hitT);
    }

  return vec4(-1);
  }

bool depthTest(vec3 ro, vec3 rd, float t) {
  vec3  hitPos  = ro + rd * t;
  vec4  hitClip = scene.viewProject * vec4(hitPos, 1.0);
  float hitNdcZ = hitClip.z / hitClip.w;

  float d  = texelFetch(depth, ivec2(gl_FragCoord.xy), 0).x;
  return hitNdcZ > d;
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  start4    = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, 1.0, 1.0);
  const vec3  start     = start4.xyz/start4.w;

  const vec3  camPos    = scene.camPos;
  const vec3  view      = normalize(start - camPos);

  const vec4 nort = gizmoIntersect(camPos, view, int(axis));
  if(nort.w>0 && nort.w!=TMax) {
    vec3 clr = baseColor;
    clr *= (max(0.0, dot(scene.sunDir, nort.xyz))*0.8 + 0.2);
    if(depthTest(camPos, view, nort.w)) {
      ivec2 id = ivec2(gl_FragCoord.xy)/4;
      clr *= (id.x+id.y)%2==0 ? 0.1 : 0.9;
      }
    outColor = vec4(clr,1);
    return;
    }
  outColor = vec4(0,0,0,1);
  discard;
  }
