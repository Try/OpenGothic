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
// Indoors also everything far below the hero's feet (cellars, the valley under a tower).
float arpgFogOfWar(vec3 wpos, bool isSky) {
  if(scene.arpgPlayer.w<=0.0)
    return 0.0;
  const float radius = scene.arpgParams2.z;
  float ret = 0.0;
  if(radius>0.0) {
    if(isSky)
      return 1.0;
    const float dist = length(wpos.xz - scene.arpgPlayer.xz);
    ret = smoothstep(radius, radius+scene.arpgParams2.w, dist);
    }
  const float indoor = scene.arpgParams3.x;
  if(indoor>0.0 && !isSky) {
    const float below = scene.arpgPlayer.y - wpos.y;
    ret = max(ret, indoor*smoothstep(scene.arpgParams3.y, scene.arpgParams3.z, below));
    }
  return ret;
  }

#endif
