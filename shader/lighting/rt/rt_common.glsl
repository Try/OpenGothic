#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query                   : enable
#extension GL_EXT_ray_flags_primitive_culling : enable
#endif

#if defined(RAY_QUERY_AT)
#extension GL_EXT_nonuniform_qualifier : enable
#endif

#if defined(RAY_QUERY)
layout(binding  = 6) uniform accelerationStructureEXT topLevelAS;
#endif

const uint CM_Opaque      = 0x1;
const uint CM_Transparent = 0x2;
const uint CM_Water       = 0x4;

struct HitDesc {
  uint instanceId;
  uint primitiveId;
  vec3 baryCoord;
  bool opaque;
  };

#if defined(RAY_QUERY_AT)
struct RtObjectDesc {
  uint instanceId; // "real" id
  uint firstPrimitive;
  };
layout(binding  = 7) uniform sampler   smp;
layout(binding  = 8) uniform texture2D textures[];
layout(binding  = 9,  std430) readonly buffer Vbo  { float vert[];   } vbo[];
layout(binding  = 10, std430) readonly buffer Ibo  { uint  index[];  } ibo[];
layout(binding  = 11, std430) readonly buffer Desc { RtObjectDesc rtDesc[]; };
#endif

#if defined(RAY_QUERY_AT)
vec2 pullTexcoord(uint id, uint vboOffset) {
  float u = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 6];
  float v = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 7];
  return vec2(u,v);
  }

vec3 pullNormal(uint id, uint vboOffset) {
  float x = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 3];
  float y = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 4];
  float z = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 5];
  return vec3(x,y,z);
  }

uvec3 pullTrinagleIds(uint id, uint primitiveID) {
  uvec3 index;
  index.x = ibo[nonuniformEXT(id)].index[primitiveID*3+0];
  index.y = ibo[nonuniformEXT(id)].index[primitiveID*3+1];
  index.z = ibo[nonuniformEXT(id)].index[primitiveID*3+2];
  return index;
  }

HitDesc pullHitDesc(in rayQueryEXT rayQuery) {
  const bool commited = false;
  uint descId = 0;
  descId += rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, commited);
  descId += rayQueryGetIntersectionGeometryIndexEXT(rayQuery, commited);

  const RtObjectDesc desc = rtDesc[descId];

  HitDesc d;
  d.instanceId = desc.instanceId;

  d.primitiveId  = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, commited);
  d.primitiveId += desc.firstPrimitive & 0x00FFFFFF;

  d.baryCoord    = vec3(0,rayQueryGetIntersectionBarycentricsEXT(rayQuery, commited));
  d.baryCoord.x  = (1-d.baryCoord.y-d.baryCoord.z);

  d.opaque       = (desc.firstPrimitive & 0x01000000)!=0;

  return d;
  }

HitDesc pullCommitedHitDesc(in rayQueryEXT rayQuery) {
  const bool commited = true;
  uint descId = 0;
  descId += rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, commited);
  descId += rayQueryGetIntersectionGeometryIndexEXT(rayQuery, commited);

  const RtObjectDesc desc = rtDesc[descId];

  HitDesc d;
  d.instanceId = desc.instanceId;

  d.primitiveId  = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, commited);
  d.primitiveId += desc.firstPrimitive & 0x00FFFFFF;

  d.baryCoord    = vec3(0,rayQueryGetIntersectionBarycentricsEXT(rayQuery, commited));
  d.baryCoord.x  = (1-d.baryCoord.y-d.baryCoord.z);

  d.opaque       = (desc.firstPrimitive & 0x01000000)!=0;

  return d;
  }

vec4 resolveHit(in HitDesc hit) {
  const uint   id    = hit.instanceId;
  const uvec3  index = pullTrinagleIds(id,hit.primitiveId);

  const vec2   uv0   = pullTexcoord(id,index.x);
  const vec2   uv1   = pullTexcoord(id,index.y);
  const vec2   uv2   = pullTexcoord(id,index.z);

  vec3 b  = hit.baryCoord;
  vec2 uv = (b.x*uv0 + b.y*uv1 + b.z*uv2);

  return textureLod(sampler2D(textures[nonuniformEXT(id)], smp),uv,0);
  }

vec4 resolveHit(in rayQueryEXT rayQuery) {
  HitDesc hit = pullHitDesc(rayQuery);
  return resolveHit(hit);
  }

vec4 resolveCommitedHit(in rayQueryEXT rayQuery) {
  HitDesc hit = pullCommitedHitDesc(rayQuery);
  return resolveHit(hit);
  }

bool isOpaqueHit(in rayQueryEXT rayQuery) {
  vec4 d = resolveHit(rayQuery);
  return (d.a>0.5);
  }
#endif

#if defined(RAY_QUERY_AT)
void rayQueryProceedShadow(in rayQueryEXT rayQuery) {
  while(rayQueryProceedEXT(rayQuery)) {
    const uint type = rayQueryGetIntersectionTypeEXT(rayQuery,false);
    if(type==gl_RayQueryCandidateIntersectionTriangleEXT) {
      const bool opaqueHit = isOpaqueHit(rayQuery);
      if(opaqueHit)
        rayQueryConfirmIntersectionEXT(rayQuery);
      }
    }
  }
#elif defined(RAY_QUERY)
void rayQueryProceedShadow(in rayQueryEXT rayQuery) {
  rayQueryProceedEXT(rayQuery);
  }
#endif

#endif
