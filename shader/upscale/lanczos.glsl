#ifndef LANCZOS_GLSL
#define LANCZOS_GLSL

float lanczosWeight(float x, float r) {
  const float PI_SQ = 9.8696044010893586188344910;
  // Use the analytic limit near zero to avoid unstable GPU sine/division results.
  if(abs(x) < 0.0001)
    return 1.;
  return (r * sin(M_PI * x) * sin(M_PI * (x / r) )) / (PI_SQ * x*x);
  }

vec3 lanczosUpscale(in sampler2D img, vec2 coord) {
  const int r = 2;

  vec2 res    = vec2(textureSize(img, 0));
  vec2 resInv = 1.0 / res;
  coord      += -0.5 * resInv;
  vec2 ccoord = floor(coord * res) * resInv;

  // Evaluate each one-dimensional weight once.
  // Keep the original footprint, including its omitted four corner samples.
  vec2 weights[5];
  [[unroll]]
  for(int i = -r; i <= r; ++i) {
    vec2 d = ((vec2(i) * resInv + ccoord - coord) * res);
    weights[i+r] = vec2(lanczosWeight(d.x, float(r)), lanczosWeight(d.y, float(r)));
    }

  // The weights at offsets zero and one have the same sign.
  // A bilinear read combines them without replacing the Lanczos kernel.
  // The caller must use a linear, clamp-to-edge sampler.
  vec2 centerWeight = weights[2] + weights[3];
  vec2 centerOffset = clamp(weights[3] / centerWeight, vec2(0), vec2(1));
  vec2 offsets[4] = vec2[4](vec2(-2), vec2(-1), centerOffset, vec2(2));
  vec2 merged[4] = vec2[4](weights[0], weights[1], centerWeight, weights[4]);

  vec3 total = vec3(0);
  [[unroll]]
  for(int x = 0; x < 4; x++) {
    [[unroll]]
    for(int y = 0; y < 4; y++) {
      if((x==0 || x==3) && (y==0 || y==3))
        continue;

      vec2  sco    = vec2(offsets[x].x, offsets[y].y) * resInv + ccoord;
      vec3  val    = textureLod(img, sco+0.5*resInv, 0.0).rgb;
      float weight = merged[x].x * merged[y].y;

      total     += val * weight;
      }
    }

  return total;
  }

#endif
