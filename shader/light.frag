#version 460
#extension GL_ARB_separate_shader_objects : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

layout(location = 0) out vec4 outColor;

layout(binding  = 0) uniform sampler2D diffuse;
layout(binding  = 1) uniform sampler2D normals;
layout(binding  = 2) uniform sampler2D depth;

#if defined(RAY_QUERY)
layout(binding  = 5) uniform accelerationStructureEXT topLevelAS;
#endif

layout(std140,binding = 3) uniform Ubo {
  mat4  mvp;
  mat4  mvpInv;
  vec4  fr[6];
  } ubo;

layout(location = 0) in vec4 scrPosition;
layout(location = 1) in vec4 cenPosition;
layout(location = 2) in vec3 color;

bool isShadow(vec3 rayOrigin, vec3 direction) {
#if defined(RAY_QUERY)
  vec3  rayDirection = normalize(direction);
  float rayDistance  = length(direction)-3.0;
  float tMin         = 30;
  if(rayDistance<=tMin)
    return false;

  //uint flags = gl_RayFlagsCullBackFacingTrianglesEXT | gl_RayFlagsTerminateOnFirstHitEXT;
  uint flags = gl_RayFlagsTerminateOnFirstHitEXT;
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        rayOrigin, tMin, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery))
    ;

  return (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
#else
  return false;
#endif
  }

void main(void) {
  vec2 scr = scrPosition.xy/scrPosition.w;
  vec2 uv  = scr*0.5+vec2(0.5);
  vec4 z   = texture(depth,  uv);
  vec4 d   = texture(diffuse,uv);
  vec4 n   = texture(normals,uv);

  vec4 pos = ubo.mvpInv*vec4(scr.x,scr.y,z.x,1.0);
  pos.xyz/=pos.w;
  vec3  ldir  = (pos.xyz-cenPosition.xyz);
  float qDist = dot(ldir,ldir)/(cenPosition.w*cenPosition.w);

  if(qDist>1.0)
    discard;

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
  }
