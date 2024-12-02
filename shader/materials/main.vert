#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_control_flow_attributes : enable

#if defined(VIRTUAL_SHADOW)
#include "virtual_shadow/vsm_common.glsl"
#endif

#include "materials_common.glsl"
#include "vertex_process.glsl"

#if !defined(GL_VERTEX_SHADER)
#extension GL_EXT_mesh_shader : enable
layout(local_size_x = 64) in;
layout(triangles, max_vertices = MaxVert, max_primitives = MaxPrim) out;
#endif

#if defined(VIRTUAL_SHADOW)
layout(push_constant, std430) uniform Push {
  uint      commandId;
  } push;
#else
layout(push_constant, std430) uniform Push {
  uint      firstMeshlet;
  int       meshletCount;
  } push;
#endif

#if defined(GL_VERTEX_SHADER)
out gl_PerVertex {
  vec4  gl_Position;
#if defined(VIRTUAL_SHADOW)
  float gl_ClipDistance[4];
#endif
  };
#else
out gl_MeshPerVertexEXT {
  vec4  gl_Position;
#if defined(VIRTUAL_SHADOW)
  float gl_ClipDistance[4];
#endif
  } gl_MeshVerticesEXT[];
#endif

#if defined(GL_VERTEX_SHADER) && defined(MAT_VARYINGS)
layout(location = 0) out flat uint bucketIdOut;
layout(location = 1) out Varyings  shOut;
#elif defined(MAT_VARYINGS)
layout(location = 0) out flat uint bucketIdOut[]; //TODO: per-primitive
layout(location = 1) out Varyings  shOut[];
#endif

#if defined(GL_VERTEX_SHADER) && defined(VIRTUAL_SHADOW)
layout(location = 3) out flat uint vsmMipIdOut;
#elif defined(VIRTUAL_SHADOW)
layout(location = 3) out flat uint vsmMipIdOut[];
#endif

#if defined(VIRTUAL_SHADOW)
uint shadowPageId = 0;
#endif

#if defined(VIRTUAL_SHADOW)
vec4 mapViewportProj(const uint data, vec4 pos, out float clipDistance[4]) {
#if 0
  clipDistance[0] = 1;
  clipDistance[1] = 1;
  clipDistance[2] = 1;
  clipDistance[3] = 1;
  return pos;
#endif

  const uvec2 lightId = unpackLightId(data);
  const ivec2 page    = ivec2(0); // unpackVsmPageInfoProj(data);
  const ivec2 sz      = unpackVsmPageSize(data);

  pos.xy = (pos.xy*0.5+0.5*pos.w); // [0..1]
  pos.xy = (pos.xy*VSM_PAGE_TBL_SIZE - page.xy*pos.w);

  {
    clipDistance[0] = 0         +pos.x;
    clipDistance[1] = sz.x*pos.w-pos.x;
    clipDistance[2] = 0         +pos.y;
    clipDistance[3] = sz.y*pos.w-pos.y;
  }

  const vec2 pageId = vec2(unpackVsmPageId(shadowPageId));
  pos.xy = (pos.xy + pageId*pos.w)/VSM_PAGE_PER_ROW;

  pos.xy = pos.xy*2.0-1.0*pos.w; // [-1..1]
  return pos;
  }

vec4 mapViewportOrtho(const uint data, vec4 pos, out float clipDistance[4]) {
  const ivec3 page = unpackVsmPageInfo(data);
  const ivec2 sz   = unpackVsmPageSize(data);
  pos.xy /= float(1u << page.z);

  pos.xy = (pos.xy*0.5+0.5); // [0..1]
  pos.xy = (pos.xy*VSM_PAGE_TBL_SIZE - page.xy);

  {
    clipDistance[0] = 0   +pos.x;
    clipDistance[1] = sz.x-pos.x;
    clipDistance[2] = 0   +pos.y;
    clipDistance[3] = sz.y-pos.y;
  }

  const vec2 pageId = vec2(unpackVsmPageId(shadowPageId));
  pos.xy = (pos.xy + pageId)/VSM_PAGE_PER_ROW;

  pos.xy = pos.xy*2.0-1.0; // [-1..1]
  return pos;
  }

vec4 mapViewport(vec4 pos, out float clipDistance[4]) {
  const uint  data = vsm.pageList[shadowPageId];
  if(vsmPageIsOmni(data))
    return mapViewportProj(data, pos, clipDistance);
  return mapViewportOrtho(data, pos, clipDistance);
  }

vec4 processVertexVsm(out Varyings var, const Vertex vert, const uint bucketId, uint instanceId, const uint vboOffset, const uint laneID) {
  const vec3 wpos = processVertexCommon(var, vert, bucketId, instanceId, vboOffset);
  const uint data = vsm.pageList[shadowPageId];

  vec4 pos4;
  if(vsmPageIsOmni(data)) {
    const uvec2       page = unpackLightId(data);
    const LightSource lx   = lights[page.x];

    vec3 pos = (wpos - lx.pos)/lx.range;
    // cubemap-face
    switch(page.y) {
      case 0: pos = vec3(pos.yz, +pos.x); break;
      case 1: pos = vec3(pos.yz, -pos.x); break;
      case 2: pos = vec3(pos.xz, +pos.y); break;
      case 3: pos = vec3(pos.xz, -pos.y); break;
      case 4: pos = vec3(pos.xy, +pos.z); break;
      case 5: pos = vec3(pos.xy, -pos.z); break;
      }

    // projection
    const float zNear = 0.0001;
    const float zFar  = 1.0;
    const float k     = zFar / (zFar - zNear);
    const float kw    = (zNear * zFar) / (zNear - zFar);
    //pos4 = vec4(pos,1); //vec4(pos.xy, pos.z*k+kw, pos.z);
    pos4 = vec4(pos.xy, (1-pos.z)*k+kw, pos.z);
    } else {
    pos4 = scene.viewProject*vec4(wpos,1.0);
    }
#if defined(GL_VERTEX_SHADER)
  return mapViewport(pos4, gl_ClipDistance);
#else
  return mapViewport(pos4, gl_MeshVerticesEXT[laneID].gl_ClipDistance);
#endif
  }
#endif

#if defined(VIRTUAL_SHADOW)
void initVsm(uvec4 task) {
  shadowPageId = task.w & 0xFFFF;
  }
#endif

uvec2 processMeshlet(const uint meshletId, const uint bucketId) {
#if defined(BINDLESS)
  nonuniformEXT uint bId = bucketId;
#else
  const         uint bId = 0;
#endif

  const uint iboOffset = meshletId * MaxPrim + MaxPrim - 1;
  const uint bits      = ibo[bId].indexes[iboOffset];
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
#if defined(BINDLESS)
  nonuniformEXT uint bId = bucketId;
#else
  const         uint bId = 0;
#endif

  const uint iboOffset = meshletId * MaxPrim + outId;
  const uint bits      = ibo[bId].indexes[iboOffset];
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
#if defined(VIRTUAL_SHADOW)
  return processVertexVsm(var, vert, bucketId, instanceOffset, vboOffset, laneID);
#else
  return processVertex(var, vert, bucketId, instanceOffset, vboOffset);
#endif
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

#if defined(MAT_VARYINGS)
  bucketIdOut = bucketId;
#endif

  Varyings var;
  uint idx = processPrimitive(meshletId, bucketId, laneID)[gl_VertexIndex%3];
  vec4 pos = processVertex(var, instanceId, meshletId, bucketId, idx);
  gl_Position = pos;
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

#if defined(MAT_VARYINGS)
  bucketIdOut[laneID] = bucketId;
#endif

  Varyings var;
  if(laneID<primCount)
    gl_PrimitiveTriangleIndicesEXT[laneID] = processPrimitive(meshletId, bucketId, laneID);
  if(laneID<vertCount) {
    vec4 pos = processVertex(var, instanceId, meshletId, bucketId, laneID);
    gl_MeshVerticesEXT[laneID].gl_Position = pos;

    /*
        Workaround the AMD driver issue where assigning to a vec3 output results in a driver crash.
        Here we unroll the shOut[laneID] = var assignment into individual fields and apply the workaround.
        Once AMD fixes the issue the original code can be restored.
        Original code: 
            #if defined(MAT_VARYINGS)
                shOut[laneID] = var;
            #endif
        Link to the discussion on AMD forums: https://community.amd.com/t5/newcomers-start-here/driver-crash-when-using-vk-ext-mesh-shader/m-p/667490
        Link to the github issue: https://github.com/Try/OpenGothic/issues/577
    */
#if defined(MAT_COLOR)
    shOut[laneID].color = var.color;
#endif
#if defined(MAT_UV)
    shOut[laneID].uv = var.uv;
#endif
#if defined(MAT_POSITION)
    shOut[laneID].pos.xyz = var.pos; // pos is vec3, applying the workaround
#endif
#if defined(MAT_NORMAL)
    shOut[laneID].normal.xyz = var.normal; // normal is vec3, applying the workaround
#endif
    }
  }
#endif

void main() {
#if defined(GL_VERTEX_SHADER)
  const uint workIndex = gl_InstanceIndex;
#else
  const uint workIndex = gl_WorkGroupID.x;
#endif

#if defined(VIRTUAL_SHADOW)
  const uint firstMeshlet = cmd[push.commandId].writeOffset;
#else
  const uint firstMeshlet = push.firstMeshlet;
#endif

  const uvec4 task = payload[workIndex + firstMeshlet];

#if defined(VIRTUAL_SHADOW)
  initVsm(task);
#endif

#if defined(GL_VERTEX_SHADER)
  vertexShader(task);
#else
  meshShader(task);
#endif
  }
