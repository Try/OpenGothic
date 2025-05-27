
// ray related
vec3 rayOrigin(ivec2 frag, float depth) {
  const vec2 fragCoord = ((frag.xy+0.5)*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec4 wpos      = scene.viewProjectLwcInv * scr;
  return wpos.xyz/wpos.w;
  }

// visibility
shared uint  cubeFaces;
shared uvec4 bbox[6];
shared vec3  frustum[6][4];

void rayBboxses(const vec3 ray, bool activeRay) {
  const uint laneID = gl_LocalInvocationIndex;

  cubeFaces = 0;
  if(laneID<bbox.length())
    bbox[laneID] = uvec4(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
  barrier();

  if(activeRay) {
    const uint face = rayToFace(ray);
    const vec3 rf   = rayToFace(ray, face);
    atomicOr(cubeFaces, 1u<<face);
    atomicMin(bbox[face].x, floatToOrderedUint(rf.x));
    atomicMin(bbox[face].y, floatToOrderedUint(rf.y));
    atomicMax(bbox[face].z, floatToOrderedUint(rf.x));
    atomicMax(bbox[face].w, floatToOrderedUint(rf.y));
    }
  barrier();

  if(laneID<bbox.length() && (cubeFaces & (1 << laneID))!=0) {
    uvec4 aabb = bbox[laneID];
    if(aabb.x==aabb.z || aabb.y==aabb.w) {
      // degenerated bbox
      atomicAnd(cubeFaces, ~(1 << laneID));
      } else {
      const vec4 aabb = orderedUintToFloat(aabb);
      bbox[laneID] = floatBitsToUint(aabb);
#if 1
      const uint face = laneID;

      const vec3 fa = faceToRay(vec2(aabb.xy), face);
      const vec3 fb = faceToRay(vec2(aabb.zy), face);
      const vec3 fc = faceToRay(vec2(aabb.zw), face);
      const vec3 fd = faceToRay(vec2(aabb.xw), face);

      frustum[face][0] = cross(fa, fb);
      frustum[face][1] = cross(fb, fc);
      frustum[face][2] = cross(fc, fd);
      frustum[face][3] = cross(fd, fa);
#endif
      }
    }
  }

bool isPrimitiveVisible(vec3 a, vec3 b, vec3 c, uint face) {
#if 0
  const vec4 aabb = uintBitsToFloat(bbox[face]);
  const vec3 fa = faceToRay(vec2(aabb.xy), face);
  const vec3 fb = faceToRay(vec2(aabb.zy), face);
  const vec3 fc = faceToRay(vec2(aabb.zw), face);
  const vec3 fd = faceToRay(vec2(aabb.xw), face);

  const vec3 p0 = cross(fa, fb);
  const vec3 p1 = cross(fb, fc);
  const vec3 p2 = cross(fc, fd);
  const vec3 p3 = cross(fd, fa);
#else
  const vec3 p0 = frustum[face][0];
  const vec3 p1 = frustum[face][1];
  const vec3 p2 = frustum[face][2];
  const vec3 p3 = frustum[face][3];
#endif

  if(dot(a, p0)<0 && dot(b, p0)<0 && dot(c, p0)<0)
    return false;
  if(dot(a, p1)<0 && dot(b, p1)<0 && dot(c, p1)<0)
    return false;
  if(dot(a, p2)<0 && dot(b, p2)<0 && dot(c, p2)<0)
    return false;
  if(dot(a, p3)<0 && dot(b, p3)<0 && dot(c, p3)<0)
    return false;

  return true;
  }

bool isPrimitiveVisible(const vec3 origin, const vec4 sphere, uint face) {
#if 0
  const vec4  aabb = uintBitsToFloat(bbox[face]);
  const vec3  fa = faceToRay(vec2(aabb.xy), face);
  const vec3  fb = faceToRay(vec2(aabb.zy), face);
  const vec3  fc = faceToRay(vec2(aabb.zw), face);
  const vec3  fd = faceToRay(vec2(aabb.xw), face);

  const vec3  p0 = cross(fa, fb);
  const vec3  p1 = cross(fb, fc);
  const vec3  p2 = cross(fc, fd);
  const vec3  p3 = cross(fd, fa);
#else
  const vec3 p0 = frustum[face][0];
  const vec3 p1 = frustum[face][1];
  const vec3 p2 = frustum[face][2];
  const vec3 p3 = frustum[face][3];
#endif
  const float R  = sphere.w;

  if(dot(sphere.xyz, p0) < -R)
    return false;
  if(dot(sphere.xyz, p1) < -R)
    return false;
  if(dot(sphere.xyz, p2) < -R)
    return false;
  if(dot(sphere.xyz, p3) < -R)
    return false;
  return true;
  }

bool isPrimitiveVisible(const vec3 origin, vec4 sphere) {
  sphere.xyz -= origin;

  for(uint face=0; face<bbox.length(); ++face) {
    if((cubeFaces & (1u << face))==0)
      continue;
    if(isPrimitiveVisible(origin, sphere, face))
      return true;
    }
  return false;
  }

bool isPrimitiveVisible(const vec3 origin, vec3 a, vec3 b, vec3 c) {
  a -= origin;
  b -= origin;
  c -= origin;

  for(uint face=0; face<bbox.length(); ++face) {
    if((cubeFaces & (1u << face))==0)
      continue;
    if(isPrimitiveVisible(a, b, c, face))
      return true;
    }
  return false;
  }
