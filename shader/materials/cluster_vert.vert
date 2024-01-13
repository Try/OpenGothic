#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

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

uvec2 processMeshlet(const uint meshletId) {
  const uint iboOffset = meshletId * MaxPrim + MaxPrim - 1;
  const uint bits      = indexes[iboOffset];
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

uvec3 processPrimitive(const uint meshletId, const uint outId) {
  const uint iboOffset = meshletId * MaxPrim + outId;
  const uint bits      = indexes[iboOffset];
  uvec3 prim;
  prim.x = ((bits >>  0) & 0xFF);
  prim.y = ((bits >>  8) & 0xFF);
  prim.z = ((bits >> 16) & 0xFF);
  return prim;
  }

vec4  processVertex(out Varyings var, uint instanceOffset, const uint meshletId, const uint laneID) {
  const uint vboOffset = meshletId * MaxVert + laneID;

#if (MESH_TYPE==T_PFX)
  const Vertex vert = pullVertex(instanceOffset);
#else
  const Vertex vert = pullVertex(vboOffset);
#endif

  textureId = vert.textureId;

  vec4 position = processVertex(var, vert, instanceOffset, vboOffset);
  // position.y = -position.y;
  return position;
  }

uvec2 pullPayload() {
#if defined(LVL_OBJECT)
  ///
#elif (MESH_TYPE==T_LANDSCAPE)
  const uint instanceOffset = 0;
  const uint meshletId      = globalPayload.meshlets[gl_InstanceIndex + push.firstMeshlet];
#else
  ///
#endif
  return uvec2(instanceOffset, meshletId);
  }

#if defined(GL_VERTEX_SHADER)
void vertexShader() {
  /*
  if(gl_InstanceIndex>=globalPayload.instanceCount) {
    gl_Position = vec4(-1);
    return;
    }
  */
  const uvec2 pl         = pullPayload();
  const uint  instanceId = pl.x;
  const uint  meshletId  = pl.y;

  const uvec2 mesh      = processMeshlet(meshletId);
  const uint  vertCount = mesh.x;
  const uint  primCount = mesh.y;

  const uint  laneID    = gl_VertexIndex/3;

  if(laneID>=primCount) {
    gl_Position = vec4(-1);
    return;
    }

  Varyings var;
  uint idx    = processPrimitive(meshletId, laneID)[gl_VertexIndex%3];
  gl_Position = processVertex(var, instanceId, meshletId, idx);
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
