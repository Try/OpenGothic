#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#if defined(RAY_QUERY)
layout(binding  = 6) uniform accelerationStructureEXT topLevelAS;
#endif

#if defined(RAY_QUERY_AT)
struct RtObjectDesc {
  uint instanceId; // "real" id
  uint firstPrimitive;
  };
layout(binding  = 7) uniform sampler   smp;
layout(binding  = 8) uniform texture2D textures[];
layout(binding  = 9,  std430) readonly buffer Vbo { float vert[];   } vbo[];
layout(binding  = 10, std430) readonly buffer Ibo { uint  index[];  } ibo[];
layout(binding  = 11, std430) readonly buffer Off { RtObjectDesc rtDesc[]; };
#endif

#if defined(RAY_QUERY_AT)
vec2 pullTexcoord(uint id, uint vboOffset) {
  float u = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 6];
  float v = vbo[nonuniformEXT(id)].vert[vboOffset*9 + 7];
  return vec2(u,v);
  }

uvec3 pullTrinagleIds(uint id, uint primitiveID) {
  uvec3 index;
  index.x = ibo[nonuniformEXT(id)].index[primitiveID*3+0];
  index.y = ibo[nonuniformEXT(id)].index[primitiveID*3+1];
  index.z = ibo[nonuniformEXT(id)].index[primitiveID*3+2];
  return index;
  }

RtObjectDesc pullRtDesc(in rayQueryEXT rayQuery) {
  const bool commited = false;
  uint id = 0;
  id += rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, commited);
  id += rayQueryGetIntersectionGeometryIndexEXT(rayQuery, commited);
  return rtDesc[id];
  }

bool isOpaqueHit(in rayQueryEXT rayQuery) {
  const bool  commited     = false;

  RtObjectDesc desc        = pullRtDesc(rayQuery);
  const uint   id          = desc.instanceId;
  const uint   primOffset  = desc.firstPrimitive;

  const uint   primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, commited) + primOffset;
  const uvec3  index       = pullTrinagleIds(id,primitiveID);

  const vec2   uv0         = pullTexcoord(id,index.x);
  const vec2   uv1         = pullTexcoord(id,index.y);
  const vec2   uv2         = pullTexcoord(id,index.z);

  vec3 b = vec3(0,rayQueryGetIntersectionBarycentricsEXT(rayQuery, commited));
  b.x = (1-b.y-b.z);
  vec2 uv = (b.x*uv0 + b.y*uv1 + b.z*uv2);

  vec4 d = textureLod(sampler2D(textures[nonuniformEXT(id)], smp),uv,0);
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
