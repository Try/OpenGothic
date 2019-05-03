#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform sampler2D textureD;
layout(binding = 3) uniform sampler2D textureSm;

#ifdef SHADOW_MAP
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
#else
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inShadowPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inLight;
#endif

layout(location = 0) out vec4 outColor;

void main() {
  vec4 t = texture(textureD,inUV);
  if(t.a<0.5)
    discard;
#ifdef SHADOW_MAP
  outColor = vec4(inShadowPos.xyz,0.0);
#else
  float lambert = max(0.0,dot(inLight,normalize(inNormal)));
  vec3  shPos   = inShadowPos.xyz/inShadowPos.w;
  vec3  shMap   = texture(textureSm,shPos.xy*vec2(0.5,0.5)+vec2(0.5)).xyz;
  float shZ     = min(0.99,shPos.z);
  float light   = lambert*smoothstep(shZ-0.01,shZ,shMap.z);

  vec3  ambient = vec3(0.25);//*inColor.xyz
  vec3  diffuse = vec3(1.0)*inColor.xyz;
  vec3  color   = mix(ambient,diffuse,clamp(light,0.0,1.0));
  outColor      = vec4(t.rgb*color,t.a);

  //outColor = vec4(vec3(shMap),1.0);
#endif
  }
