#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(binding  = 0) uniform sampler2D diffuse;
layout(binding  = 1) uniform sampler2D normals;
layout(binding  = 2) uniform sampler2D depth;

layout(std140,binding = 3) uniform Ubo {
  mat4  mvp;
  mat4  mvpInv;
  vec4  fr[6];
  } ubo;

layout(location = 0) in vec4 scrPosition;
layout(location = 1) in vec4 cenPosition;
layout(location = 2) in vec3 color;

void main(void) {
  vec2 scr = scrPosition.xy/scrPosition.w;
  vec2 uv  = scr*0.5+vec2(0.5);
  vec4 z   = texture(depth,uv);

  vec4 pos = ubo.mvpInv*vec4(scr.x,scr.y,z.x,1.0);
  pos.xyz/=pos.w;
  vec3  ldir    = (pos.xyz-cenPosition.xyz);
  float qDist   = dot(ldir,ldir)/(cenPosition.w*cenPosition.w);

  if(qDist>1.0)
    discard;

  vec4  d       = texture(diffuse,uv);
  vec4  n       = texture(normals,uv);
  vec3  normal  = normalize(n.xyz*2.0-vec3(1.0));
  float lambert = max(0.0,-dot(normalize(ldir),normal));

  float light = (1.0-qDist)*lambert;
  //if(light<=0.001)
  //  discard;

  //outColor     = vec4(0.5,0.5,0.5,1);
  //outColor     = vec4(light,light,light,0.0);
  outColor     = vec4(d.rgb*color*vec3(light),0.0);
  }
