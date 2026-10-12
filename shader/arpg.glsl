#ifndef ARPG_GLSL
#define ARPG_GLSL

// gothic-diablo: helpers for the isometric ARPG view.
// Requires `SceneDesc scene` (scene.glsl) to be declared before inclusion.

vec3 arpgWorldPos(vec2 ndc, float z) {
  vec4 p = scene.viewProjectInv*vec4(ndc, z, 1.0);
  return p.xyz/p.w;
  }

// Fog of war: the world fades to black beyond a distance from the hero
// (radius comes from the game's "sight range" option). 0 = visible, 1 = black.
float arpgFogOfWar(vec3 wpos, bool isSky) {
  const float radius = scene.arpgParams2.z;
  if(scene.arpgPlayer.w<=0.0 || radius<=0.0)
    return 0.0;
  if(isSky)
    return 1.0;
  const float dist = length(wpos.xz - scene.arpgPlayer.xz);
  return smoothstep(radius, radius+scene.arpgParams2.w, dist);
  }

#endif
