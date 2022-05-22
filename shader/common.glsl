#ifndef COMMON_GLSL
#define COMMON_GLSL

const float M_PI = 3.1415926535897932384626433832795;

float reconstructCSZ(float d, vec3 clipInfo) {
  // z_n * z_f,  z_n - z_f, z_f
  return (clipInfo[0] / (clipInfo[1] * d + clipInfo[2]));
  }

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta) {
  return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
  }

float fresnel(vec3 view, vec3 normal) {
  return clamp(0, 1, 1.0 - dot(view, normal));
  }

float cookTorrance(out float fresnel, vec3 normal, vec3 light, vec3 view, float roughness) {
  if(roughness<=0)
    return 0;

  vec3  hVec     = normalize(view+light);

  float NdotL    = max( dot(normal, light), 0.0 );
  float NdotV    = max( dot(normal, view ), 0.0 );
  float NdotH    = max( dot(normal, hVec ), 1.0e-7 );
  float VdotH    = max( dot(view,   hVec ), 0.0 );

  float geometric = 2.0 * NdotH / VdotH;
  geometric = min( 1.0, geometric * min(NdotV, NdotL) );

  float r_sq          = roughness * roughness;
  float NdotH_sq      = NdotH * NdotH;
  float NdotH_sq_r    = 1.0 / (NdotH_sq * r_sq);
  float roughness_exp = (NdotH_sq - 1.0) * ( NdotH_sq_r );
  float r     = exp(roughness_exp) * NdotH_sq_r / (4.0 * NdotH_sq );

  fresnel = 1.0/(1.0+NdotV);
  return min(1.0, (fresnel * geometric * r) / (NdotV * NdotL + 1.0e-7));
  }

#endif
