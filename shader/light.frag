#version 460
#extension GL_ARB_separate_shader_objects : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#if defined(RAY_QUERY_AT)
#extension GL_EXT_nonuniform_qualifier : enable
#endif
layout(early_fragment_tests) in;

layout(location = 0) out vec4 outColor;

layout(binding  = 0) uniform sampler2D diffuse;
layout(binding  = 1) uniform sampler2D normals;
layout(binding  = 2) uniform sampler2D depth;

layout(binding  = 3, std140) uniform Ubo {
  mat4  mvp;
  mat4  mvpInv;
  vec4  fr[6];
  } ubo;

#if defined(RAY_QUERY)
layout(binding  = 5) uniform accelerationStructureEXT topLevelAS;
#endif

#if defined(RAY_QUERY_AT)
layout(binding  = 6) uniform sampler2D textures[];
layout(binding  = 7, std430) readonly buffer Vbo { float vert[];   } vbo[];
layout(binding  = 8, std430) readonly buffer Ibo { uint  index[];  } ibo[];
layout(binding  = 9, std430) readonly buffer Off { uint  offset[]; } iboOff;
#endif

layout(location = 0) in vec4 scrPosition;
layout(location = 1) in vec4 cenPosition;
layout(location = 2) in vec3 color;

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

bool alphaTest(in rayQueryEXT rayQuery, uint id) {
  const bool commited   = false;

  if(id==0)
    return true; // landscape
  //if(id!=62)
  //  return true; // debug

  const uint  primOffset  = iboOff.offset[id];
  const uint  primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, commited) + primOffset;
  const uvec3 index       = pullTrinagleIds(id,primitiveID);

  const vec2  uv0         = pullTexcoord(id,index.x);
  const vec2  uv1         = pullTexcoord(id,index.y);
  const vec2  uv2         = pullTexcoord(id,index.z);

  vec3 b = vec3(0,rayQueryGetIntersectionBarycentricsEXT(rayQuery,commited));
  b.x = (1-b.y-b.z);
  vec2 uv = (b.x*uv0 + b.y*uv1 + b.z*uv2);

  vec4 d = textureLod(textures[nonuniformEXT(id)],uv,0);
  return (d.a>0.5);
  }
#endif

bool isShadow(vec3 rayOrigin, vec3 direction) {
#if defined(RAY_QUERY)
  vec3  rayDirection = normalize(direction);
  float rayDistance  = length(direction)-3.0;
  float tMin         = 30;
  if(rayDistance<=tMin)
    return false;

  //uint flags = gl_RayFlagsCullBackFacingTrianglesEXT | gl_RayFlagsTerminateOnFirstHitEXT;
  uint flags = gl_RayFlagsTerminateOnFirstHitEXT;
#if defined(RAY_QUERY_AT)
  flags |= gl_RayFlagsNoOpaqueEXT;
#endif

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        rayOrigin, tMin, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery)) {
#if defined(RAY_QUERY_AT)
    const uint type = rayQueryGetIntersectionTypeEXT(rayQuery,false);
    const uint id   = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery,false);
    if(type==gl_RayQueryCandidateIntersectionTriangleEXT) {
      const bool opaqueHit = alphaTest(rayQuery,id);
      if(opaqueHit)
        rayQueryConfirmIntersectionEXT(rayQuery);
      }
#endif
    }

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  return true;
#else
  return false;
#endif
  }

void main(void) {
  vec2 scr = scrPosition.xy/scrPosition.w;
  vec2 uv  = scr*0.5+vec2(0.5);
  vec4 z   = textureLod(depth,  uv,0);

  vec4 pos = ubo.mvpInv*vec4(scr.x,scr.y,z.x,1.0);
  pos.xyz/=pos.w;
  vec3  ldir  = (pos.xyz-cenPosition.xyz);
  float qDist = dot(ldir,ldir)/(cenPosition.w*cenPosition.w);

  if(qDist>1.0)
    discard;

  vec4 d   = textureLod(diffuse,uv,0);
  vec4 n   = textureLod(normals,uv,0);

  vec3  normal  = normalize(n.xyz*2.0-vec3(1.0));
  float lambert = max(0.0,-dot(normalize(ldir),normal));

  float light = (1.0-qDist)*lambert;
  //if(light<=0.001)
  //  discard;

  pos.xyz  = pos.xyz+5.0*normal; //bias
  ldir = (pos.xyz-cenPosition.xyz);
  if(light>0 && isShadow(cenPosition.xyz,ldir))
    discard;

  //outColor     = vec4(0.5,0.5,0.5,1);
  //outColor     = vec4(light,light,light,0.0);
  outColor     = vec4(d.rgb*color*vec3(light),0.0);
  //if(dbg!=vec4(0))
  //  outColor = dbg;
  }
