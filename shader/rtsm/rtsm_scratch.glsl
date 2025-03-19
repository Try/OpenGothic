// memory

#if !defined(CONST_SCRATCH)
uint allocScratch(uint size) {
  uint ptr = size==0 ? NULL : atomicAdd(pos.alloc, size);
  if(ptr+size>pos.data.length()) {
    // out of memory
    return NULL;
    }
  return ptr;
  }
#endif

// meshlets
const uint MeshletHeaderSize = 6;

vec4 pullMeshAabb(uint ptr) {
  vec4  aabb;
  aabb.x   = uintBitsToFloat(pos.data[ptr+0]);
  aabb.y   = uintBitsToFloat(pos.data[ptr+1]);
  aabb.z   = uintBitsToFloat(pos.data[ptr+2]);
  aabb.w   = uintBitsToFloat(pos.data[ptr+3]);
  return aabb;
  }

float pullMeshDepthMax(uint ptr) {
  return uintBitsToFloat(pos.data[ptr+4]);
  }

uint pullMeshBucketId(uint ptr) {
  return unpackBucketId(pos.data[ptr+5]);
  }

uint pullPrimitiveCount(uint ptr) {
  return unpackPrimitiveCount(pos.data[ptr+5]);
  }

// vertex/index
struct Vertex {
  vec3 pos;
  uint uv;
  };

uint pullPrimitivePkg(const uint ptr, const uint laneId) {
  uint bits = pos.data[ptr+laneId];
  return bits;
  }

uvec3 unpackPrimitive(uint bits) {
  uvec3 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  return prim;
  }

uvec4 unpackPrimitiveFull(uint bits) {
  uvec4 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  prim.w = ((bits >> 24) & 0xFF);
  return prim;
  }

uvec3 pullPrimitive(const uint ptr, const uint laneId) {
  uint bits = pos.data[ptr+laneId];

  uvec3 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  return prim;
  }

uvec4 pullPrimitiveFull(const uint ptr, const uint laneId) {
  uint bits = pos.data[ptr+laneId];

  uvec4 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  prim.w = ((bits >> 24) & 0xFF);
  return prim;
  }

vec3 pullVertex(uint ptr, const uint laneId) {
  ptr += 4*laneId;

  vec3 ret;
  ret.x = uintBitsToFloat(pos.data[ptr+0]);
  ret.y = uintBitsToFloat(pos.data[ptr+1]);
  ret.z = uintBitsToFloat(pos.data[ptr+2]);
  return ret;
  }

Vertex pullVertexFull(uint ptr, const uint laneId) {
  ptr += 4*laneId;

  Vertex ret;
  ret.pos.x = uintBitsToFloat(pos.data[ptr+0]);
  ret.pos.y = uintBitsToFloat(pos.data[ptr+1]);
  ret.pos.z = uintBitsToFloat(pos.data[ptr+2]);
  ret.uv    = pos.data[ptr+3];
  return ret;
  }
