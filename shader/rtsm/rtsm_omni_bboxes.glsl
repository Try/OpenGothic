
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

void rayBboxses(const vec3 ray, bool activeRay) {
  const uint laneID = gl_LocalInvocationIndex;

  cubeFaces = 0;
  if(laneID<bbox.length())
    bbox[laneID] = uvec4(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
  barrier();

  const uint face = rayToFace(ray);
  if(activeRay)
    atomicOr(cubeFaces, 1u<<face);
  barrier();

  if(activeRay) {
    const uint id = face;
    const vec3 rf = rayToFace(ray, face);
    atomicMin(bbox[id].x, floatToOrderedUint(rf.x));
    atomicMin(bbox[id].y, floatToOrderedUint(rf.y));
    atomicMax(bbox[id].z, floatToOrderedUint(rf.x));
    atomicMax(bbox[id].w, floatToOrderedUint(rf.y));
    }
  barrier();

  if(laneID<bbox.length() && (cubeFaces & (1 << laneID))!=0) {
    uvec4 aabb = bbox[laneID];
    if(aabb.x==aabb.z || aabb.y==aabb.w) {
      // degenerated bbox
      atomicAnd(cubeFaces, ~(1 << laneID));
      } else {
      bbox[laneID] = floatBitsToUint(orderedUintToFloat(aabb));
      }
    }
  }

bool isPrimitiveVisible(const vec3 origin, vec3 a, vec3 b, vec3 c, uint face) {
  const vec4 aabb = uintBitsToFloat(bbox[face]);
  const vec3 fa = faceToRay(vec2(aabb.xy), face);
  const vec3 fb = faceToRay(vec2(aabb.zy), face);
  const vec3 fc = faceToRay(vec2(aabb.zw), face);
  const vec3 fd = faceToRay(vec2(aabb.xw), face);

  const vec3 p0 = cross(fa, fb);
  const vec3 p1 = cross(fb, fc);
  const vec3 p2 = cross(fc, fd);
  const vec3 p3 = cross(fd, fa);

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
  const vec4 aabb = uintBitsToFloat(bbox[face]);
  const vec3 fa = faceToRay(vec2(aabb.xy), face);
  const vec3 fb = faceToRay(vec2(aabb.zy), face);
  const vec3 fc = faceToRay(vec2(aabb.zw), face);
  const vec3 fd = faceToRay(vec2(aabb.xw), face);

  const vec3 p0 = cross(fa, fb);
  const vec3 p1 = cross(fb, fc);
  const vec3 p2 = cross(fc, fd);
  const vec3 p3 = cross(fd, fa);
  const float R = sphere.w;

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
    if(isPrimitiveVisible(origin, a, b, c, face))
      return true;
    }
  return false;
  }
