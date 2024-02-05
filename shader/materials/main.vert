#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#include "materials_common.glsl"
#include "vertex_process.glsl"

#if !defined(GL_VERTEX_SHADER)
#extension GL_EXT_mesh_shader : enable
layout(local_size_x = 64) in;
layout(triangles, max_vertices = MaxVert, max_primitives = MaxPrim) out;
#endif

layout(push_constant, std430) uniform Push {
  uint      firstMeshlet;
  int       meshletCount;
  } push;

#if defined(GL_VERTEX_SHADER)
out gl_PerVertex {
  vec4 gl_Position;
  };
#else
out gl_MeshPerVertexEXT {
  vec4 gl_Position;
  } gl_MeshVerticesEXT[];
#endif

#if defined(GL_VERTEX_SHADER) && defined(BINDLESS) && defined(MAT_VARYINGS)
layout(location = 0) out flat uint bucketIdOut;
layout(location = 1) out Varyings  shOut;
#elif defined(GL_VERTEX_SHADER) && defined(MAT_VARYINGS)
layout(location = 0) out Varyings  shOut;
#elif defined(BINDLESS) && defined(MAT_VARYINGS)
layout(location = 0) out flat uint bucketIdOut[]; //TODO: per-primitive
layout(location = 1) out Varyings  shOut[];
#endif

uvec2 processMeshlet(const uint meshletId, const uint bucketId) {
  const uint iboOffset = meshletId * MaxPrim + MaxPrim - 1;
  const uint bits      = ibo[nonuniformEXT(bucketId)].indexes[iboOffset];
  uvec4 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);

  uint vertCount = MaxVert;
  uint primCount = MaxPrim;
  if(prim.x==prim.y) {
    // last dummy triangle encodes primitive count
    prim.z = ((bits >> 16) & 0xFF);
    prim.w = ((bits >> 24) & 0xFF);

    primCount = prim.z;
    vertCount = prim.w;
    }
  return uvec2(vertCount, primCount);
  }

uvec3 processPrimitive(const uint meshletId, const uint bucketId, const uint outId) {
  const uint iboOffset = meshletId * MaxPrim + outId;
  const uint bits      = ibo[nonuniformEXT(bucketId)].indexes[iboOffset];
  uvec3 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  return prim;
  }

vec4  processVertex(out Varyings var, uint instanceOffset, const uint meshletId, const uint bucketId, const uint laneID) {
  const uint vboOffset = meshletId * MaxVert + laneID;
#if (MESH_TYPE==T_PFX)
  const Vertex vert = pullVertex(instanceOffset);
#else
  const Vertex vert = pullVertex(bucketId, vboOffset);
#endif
  return processVertex(var, vert, bucketId, instanceOffset, vboOffset);
  }

#if defined(GL_VERTEX_SHADER)
void vertexShader(const uvec4 task) {
  const uint  instanceId = task.x;
  const uint  meshletId  = task.y;
  const uint  bucketId   = task.z;

  const uvec2 mesh       = processMeshlet(meshletId, bucketId);
  const uint  vertCount  = mesh.x;
  const uint  primCount  = mesh.y;

  const uint  laneID     = gl_VertexIndex/3;
  if(laneID>=primCount) {
    gl_Position = vec4(uintBitsToFloat(0x7fc00000));
    return;
    }

#if defined(BINDLESS) && defined(MAT_VARYINGS)
  bucketIdOut = bucketId;
#endif

  Varyings var;
  uint idx    = processPrimitive(meshletId, bucketId, laneID)[gl_VertexIndex%3];
  gl_Position = processVertex(var, instanceId, meshletId, bucketId, idx);
#if defined(MAT_VARYINGS)
  shOut       = var;
#endif
  }
#else
void meshShader(const uvec4 task) {
  const uint  instanceId = task.x;
  const uint  meshletId  = task.y;
  const uint  bucketId   = task.z;

  const uvec2 mesh       = processMeshlet(meshletId, bucketId);
  const uint  vertCount  = mesh.x;
  const uint  primCount  = mesh.y;

  const uint  laneID     = gl_LocalInvocationIndex;

  // Alloc outputs
  SetMeshOutputsEXT(vertCount, primCount);

#if defined(BINDLESS) && defined(MAT_VARYINGS)
  bucketIdOut[laneID] = bucketId;
#endif

  Varyings var;
  if(laneID<primCount)
    gl_PrimitiveTriangleIndicesEXT[laneID] = processPrimitive(meshletId, bucketId, laneID);
  if(laneID<vertCount)
    gl_MeshVerticesEXT[laneID].gl_Position = processVertex(var, instanceId, meshletId, bucketId, laneID);
#if defined(MAT_VARYINGS)
  if(laneID<vertCount)
    shOut[laneID]                          = var;
#endif
  }
#endif

void main() {
#if defined(GL_VERTEX_SHADER)
  const uint workIndex = gl_InstanceIndex;
#else
  const uint workIndex = gl_WorkGroupID.x;
#endif

  const uvec4 task       = payload[workIndex + push.firstMeshlet];

#if defined(GL_VERTEX_SHADER)
  vertexShader(task);
#else
  meshShader(task);
#endif
  }
