#ifndef LANCZOS_GLSL
#define LANCZOS_GLSL

float lanczosWeight(float x, float r) {
  const float PI_SQ = 9.8696044010893586188344910;
  if(x == 0.0)
    return 1.;
  return (r * sin(M_PI * x) * sin(M_PI * (x / r) )) / (PI_SQ * x*x);
  }

float lanczosWeight(vec2 x, float r) {
  return lanczosWeight(x.x, r) * lanczosWeight(x.y, r);
  }

vec3 lanczosUpscale(in sampler2D img, vec2 coord) {
  const int r = 2;

  vec2 res    = vec2(textureSize(img, 0));
  vec2 resInv = 1.0 / res;
  coord      += -0.5 * resInv;
  vec2 ccoord = floor(coord * res) * resInv;

  vec3 total = vec3(0);
  [[unroll]]
  for(int x = -r; x <= r; x++) {
    [[unroll]]
    for(int y = -r; y <= r; y++) {
      if(abs(x)==r && abs(y)==r)
        continue;

      vec2  offs   = vec2(x,y);
      vec2  sco    = offs * resInv + ccoord;
      vec2  d      = ((sco - coord) * res);

      vec3  val    = textureLod(img, sco+0.5*resInv, 0.0).rgb;
      float weight = lanczosWeight(d, float(r));

      total     += val * weight;
      }
    }

  return total;
  }

#endif
