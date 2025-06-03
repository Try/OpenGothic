
// visibility
shared uint  numPlanes;
shared uvec4 bbox[6];
shared vec3  frustum[6][4];

void rayBboxses(const vec3 ray, bool activeRay) {
  const uint laneID = gl_LocalInvocationIndex;

  numPlanes = 0;
  if(laneID<bbox.length())
    bbox[laneID] = uvec4(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
  barrier();

  if(activeRay) {
    const uint face = rayToFace(ray);
    const vec2 rf   = rayToFace(ray, face);
    atomicMin(bbox[face].x, floatToOrderedUint(rf.x));
    atomicMin(bbox[face].y, floatToOrderedUint(rf.y));
    atomicMax(bbox[face].z, floatToOrderedUint(rf.x));
    atomicMax(bbox[face].w, floatToOrderedUint(rf.y));
    }
  barrier();

  if(laneID<bbox.length()) {
    const uint  face  = laneID;
    const uvec4 iaabb = bbox[face];
    if(iaabb.x>=iaabb.z || iaabb.y>=iaabb.w) {
      // degenerated bbox
      return;
      }
    const vec4 aabb = orderedUintToFloat(iaabb);
    const vec3 fa   = faceToRay(vec2(aabb.xy), face);
    const vec3 fb   = faceToRay(vec2(aabb.zy), face);
    const vec3 fc   = faceToRay(vec2(aabb.zw), face);
    const vec3 fd   = faceToRay(vec2(aabb.xw), face);

    const uint id = atomicAdd(numPlanes, 1);
    frustum[id][0] = cross(fa, fb);
    frustum[id][1] = cross(fb, fc);
    frustum[id][2] = cross(fc, fd);
    frustum[id][3] = cross(fd, fa);
    }
  }

bool isPrimitiveVisible(vec3 a, vec3 b, vec3 c, uint face) {
  const vec3 p0 = frustum[face][0];
  const vec3 p1 = frustum[face][1];
  const vec3 p2 = frustum[face][2];
  const vec3 p3 = frustum[face][3];

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
  const vec3 p0 = frustum[face][0];
  const vec3 p1 = frustum[face][1];
  const vec3 p2 = frustum[face][2];
  const vec3 p3 = frustum[face][3];

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
  for(uint i=0; i<numPlanes; ++i) {
    if(isPrimitiveVisible(origin, sphere, i))
      return true;
    }
  return false;
  }

bool isPrimitiveVisible(const vec3 origin, vec3 a, vec3 b, vec3 c) {
  a -= origin;
  b -= origin;
  c -= origin;

  for(uint i=0; i<numPlanes; ++i) {
    if(isPrimitiveVisible(a, b, c, i))
      return true;
    }
  return false;
  }
