#ifndef CLOUDS_GLSL
#define CLOUDS_GLSL

vec4 clouds(vec3 at, float nightPhase, vec3 highlight,
            vec2 dxy0, vec2 dxy1,
            in sampler2D dayL1,   in sampler2D dayL0,
            in sampler2D nightL1, in sampler2D nightL0) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

#if defined(SKY_LOD)
  vec4  cloudDL1 = textureLod(dayL1,   texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudDL0 = textureLod(dayL0,   texc*0.3 + dxy0, SKY_LOD);
  vec4  cloudNL1 = textureLod(nightL1, texc*0.3 + dxy1, SKY_LOD);
  vec4  cloudNL0 = textureLod(nightL0, texc*0.6 + vec2(0.5), SKY_LOD); // stars
#else
  vec4  cloudDL1 = texture(dayL1,   texc*0.3 + dxy1);
  vec4  cloudDL0 = texture(dayL0,   texc*0.3 + dxy0);
  vec4  cloudNL1 = texture(nightL1, texc*0.3 + dxy1);
  vec4  cloudNL0 = texture(nightL0, texc*0.6 + vec2(0.5)); // stars
#endif

  cloudDL0.a   = cloudDL0.a*0.2;
  cloudDL1.a   = cloudDL1.a*0.2;
  cloudNL0.a   = cloudNL0.a*1.0; // stars
  cloudNL1.a   = cloudNL1.a*0.1;

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb);
  night.rgb = srgbDecode(night.rgb);

  day.rgb   = day.rgb  *highlight*5.0;
  night.rgb = night.rgb*0.0004;

  //day  .a   = day  .a*0.2;
  night.a   = night.a*(nightPhase);

  vec4 color = mixClr(day,night);
  // color.rgb += hday;

  return color;
  }

#endif
