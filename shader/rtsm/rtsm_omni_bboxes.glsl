
// visibility
shared uint  numPlanes;
shared uint  planeFace[6];
shared uvec4 planeBbox[6];

void rayBboxses(const vec3 ray, bool activeRay) {
  const uint laneID = gl_LocalInvocationIndex;

  numPlanes = 0;
  if(laneID<planeBbox.length())
    planeBbox[laneID] = uvec4(0xFFFFFFFF, 0xFFFFFFFF, 0, 0);
  barrier();

  if(activeRay) {
    const uint face = rayToFace(ray);
    const vec2 rf   = rayToFace(ray, face);
    atomicMin(planeBbox[face].x, floatToOrderedUint(rf.x));
    atomicMin(planeBbox[face].y, floatToOrderedUint(rf.y));
    atomicMax(planeBbox[face].z, floatToOrderedUint(rf.x));
    atomicMax(planeBbox[face].w, floatToOrderedUint(rf.y));
    }
  barrier();

  if(laneID<planeBbox.length()) {
    const uint  face  = laneID;
    const uvec4 iaabb = planeBbox[face];
    if(iaabb.x>=iaabb.z || iaabb.y>=iaabb.w) {
      // degenerated bbox
      return;
      }
    const vec4 aabb = orderedUintToFloat(iaabb);
    const uint id = atomicAdd(numPlanes, 1);
    planeBbox[id] = floatBitsToUint(aabb);
    planeFace[id] = laneID;
    }
  }

bool isPrimitiveVisible(vec3 a, vec3 b, vec3 c, const vec4 aabb, uint face) {
  a = vecToFace(a, face);
  b = vecToFace(b, face);
  c = vecToFace(c, face);
  if(a.x < aabb.x*a.z && b.x < aabb.x*b.z && c.x < aabb.x*c.z)
    return false;
  if(a.x > aabb.z*a.z && b.x > aabb.z*b.z && c.x > aabb.z*c.z)
    return false;
  if(a.y < aabb.y*a.z && b.y < aabb.y*b.z && c.y < aabb.y*c.z)
    return false;
  if(a.y > aabb.w*a.z && b.y > aabb.w*b.z && c.y > aabb.w*c.z)
    return false;
  return true;
  }

bool isPrimitiveVisible(const vec3 origin, vec3 a, vec3 b, vec3 c) {
  a -= origin;
  b -= origin;
  c -= origin;

  for(uint i=0; i<numPlanes; ++i) {
    if(isPrimitiveVisible(a, b, c, uintBitsToFloat(planeBbox[i]), planeFace[i]))
      return true;
    }
  return false;
  }
