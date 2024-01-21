#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#if defined(BINDLESS)
#extension GL_EXT_nonuniform_qualifier : enable
#endif

#define CLUSTER
#define MESH
#include "materials_common.glsl"
#include "vertex_process.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

#if defined(BINDLESS) && defined(MAT_VARYINGS)
layout(location = 0) out flat uint textureId;
layout(location = 1) out Varyings  shOut;
#elif defined(MAT_VARYINGS)
layout(location = 0) out Varyings shOut;
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
void vertexShader() {
  const uvec4 task       = payload[gl_InstanceIndex + push.firstMeshlet];

  const uint  instanceId = task.x;
  const uint  meshletId  = task.y;
  const uint  bucketId   = task.z;

  const uvec2 mesh       = processMeshlet(meshletId, bucketId);
  const uint  vertCount  = mesh.x;
  const uint  primCount  = mesh.y;

  const uint  laneID     = gl_VertexIndex/3;
  if(laneID>=primCount) {
    gl_Position = vec4(-1,-1,0,1);
    return;
    }

#if defined(BINDLESS) && defined(MAT_VARYINGS)
  textureId = bucketId;
#endif

  Varyings var;
  uint idx    = processPrimitive(meshletId, bucketId, laneID)[gl_VertexIndex%3];
  gl_Position = processVertex(var, instanceId, meshletId, bucketId, idx);
#if defined(MAT_VARYINGS)
  shOut       = var;
#endif
  }
#endif

void main() {
#if defined(GL_VERTEX_SHADER)
  vertexShader();
#endif
  }
