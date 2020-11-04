#version 450
#extension GL_ARB_separate_shader_objects : enable

#define PI 3.141592

layout(location = 0) in  vec2 scrPosition;
layout(location = 0) out vec4 outColor;

layout(std140,push_constant) uniform Ubo {
  mat4 mvp;
  mat4 mvpInv;
  } ubo;

layout(binding = 0) uniform sampler2D depth;

const float volFogHeightDensityAtViewer = 0.005;
const float globalDensity = 0.001;
const float heightFalloff = 0.0001;

float computeVolumetricFog(in vec3 cameraToWorldPos, in vec3 viewPos){
  // NOTE: cVolFogHeightDensityAtViewer = exp( -cHeightFalloff * cViewPos.z );
  // float volFogHeightDensityAtViewer = exp(-heightFalloff * viewPos.z);
  float fogInt = length(cameraToWorldPos) * volFogHeightDensityAtViewer;
  if(abs(cameraToWorldPos.y) > 0.01) {
    float t = heightFalloff*cameraToWorldPos.y;
    fogInt *= (1.0-exp(-t))/t;
    }
  return exp(-globalDensity*fogInt);
  }

void main() {
  vec2  uv = scrPosition*0.5+vec2(0.5);
  float z  = texture(depth,uv).r;

  vec4 pos1  = ubo.mvpInv*vec4(scrPosition,  z,1.0);
  pos1.xyz/=pos1.w;
  vec4 pos0  = ubo.mvpInv*vec4(scrPosition,0.0,1.0);
  pos0.xyz/=pos0.w;

  vec3  dpos = (pos1.xyz-pos0.xyz);
  float val  = 1.0-computeVolumetricFog(dpos,pos0.xyz);

  //outColor = vec4(0.7,0.7,0.7,val);
  outColor = vec4(0.45,0.49,0.53,val);
  //outColor = vec4(0.5,0.5,0.5,val);
  //outColor = vec4(val,val,val,1.0);
  }
