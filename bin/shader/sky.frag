#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D textureL0;
layout(binding = 2) uniform sampler2D textureL1;

layout(std140,binding = 0) uniform UniformBufferObject {
  mat4 mvp;
  vec4 color;
  vec2 dxy0;
  vec2 dxy1;
  vec3 sunDir;
  } ubo;

layout(location = 0) in vec2 inPos;
layout(location = 0) out vec4 outColor;

const int stepCount           = 16;

float rayleighBrightness      = 1.6;  //3.3;
float mieBrightness           = 0.1;
float spotBrightness          = 100.0;

float mieDistribution         = 0.1;  //0.63;
float surfaceHeight           = 0.99;
float intensity               = 1.8;

float scatterStrength         = 0.028;
float rayleighStrength        = 0.07; //0.139;
float mieStrength             = 0.3;  //0.264;

float rayleighCollectionPower = 0.81;
float mieCollectionPower      = 0.39;

vec3  skyColor                = ubo.color.rgb;

float phase(float alpha, float g) {
  float a = 3.0*(1.0-g*g);
  float b = 2.0*(2.0+g*g);
  float c = 1.0+alpha*alpha;
  float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
  d = max(d, 0.00001);
  return (a/b)*(c/d);
  }

float atmosphericDepth(vec3 position, vec3 dir){
  float a = dot(dir, dir);
  float b = 2.0*dot(dir, position);
  float c = dot(position, position)-1.0;
  float det = b*b-4.0*a*c;
  float detSqrt = sqrt(det);
  float q = (-b - detSqrt)/2.0;
  float t1 = c/q;
  return t1;
  }

float horizonExtinction(vec3 position, vec3 dir, float radius){
  float u = dot(dir, -position);
  if(u<0.0)
    return 1.0;

  vec3 near = position + u*dir;
  if(length(near) < radius)
    return 0.0;

  vec3  v2 = normalize(near)*radius - position;
  float diff = acos(dot(normalize(v2), dir));
  return smoothstep(0.0, 1.0, pow(diff*2.0, 3.0));
  }

vec3 absorb(float dist, vec3 color, float factor){
  return color-color*pow(skyColor, vec3(factor/dist));
  }

vec3 linToSrgb(vec3 c){
  float g = 1.0/2.2;
  return pow(c,vec3(g));
  }

void main() {
  vec4  v                = ubo.mvp*vec4(inPos,1.0,1.0);
  vec3  view             = normalize(v.xyz);
  vec3  lightdir         = ubo.sunDir;
  float alpha            = dot(view,lightdir);

  float rayleighFactor   = phase(alpha, -0.01)*rayleighBrightness;
  float mieFactor        = phase(alpha, mieDistribution)*mieBrightness;
  float spot             = smoothstep(0.0, 1000.0, phase(alpha, 0.9995))*spotBrightness;

  vec3  eyePosition      = vec3(0.0, surfaceHeight, 0.0);
  float eyeDepth         = atmosphericDepth(eyePosition, view);
  float stepLength       = eyeDepth/float(stepCount);
  float eyeExtinction    = horizonExtinction(eyePosition, view, surfaceHeight-0.3);

  vec3 rayleighCollected = vec3(0.0, 0.0, 0.0);
  vec3 mieCollected      = vec3(0.0, 0.0, 0.0);

  for(int i=0; i<stepCount; i++){
    float sampleDistance = stepLength*float(i);
    vec3  position       = eyePosition + view*sampleDistance;
    float extinction     = horizonExtinction(position, lightdir, surfaceHeight-0.35);
    float sample_depth   = atmosphericDepth(position, lightdir);
    vec3  influx         = absorb(sample_depth,   vec3(intensity), scatterStrength)*extinction;
    rayleighCollected   += absorb(sampleDistance, skyColor*influx, rayleighStrength);
    mieCollected        += absorb(sampleDistance, influx,          mieStrength);
    }

  rayleighCollected = (rayleighCollected*eyeExtinction*pow(eyeDepth, rayleighCollectionPower))/float(stepCount);
  mieCollected      = (mieCollected     *eyeExtinction*pow(eyeDepth, mieCollectionPower))     /float(stepCount);

  vec3 texc    = view.xyz/max(abs(view.y*2.0),0.001);
  vec4 cloudL1 = texture(textureL1,texc.zx*0.2+ubo.dxy1);
  vec4 cloudL0 = texture(textureL0,texc.zx*0.2+ubo.dxy0);
  vec4 c       = (cloudL0+cloudL1);

  vec3 color   = vec3(1.0*spot          *mieCollected +
                      1.0*mieFactor     *mieCollected +
                      1.0*rayleighFactor*rayleighCollected);
  vec3 cloud   = c.rgb*clamp(lightdir.y*2.0+0.1,0.1,1.0);

  color        = linToSrgb(color);
  color        = mix(color,cloud,min(1.0,c.a));
  outColor     = vec4(color,1.0);
  }
