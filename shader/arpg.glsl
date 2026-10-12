#ifndef ARPG_GLSL
#define ARPG_GLSL

// gothic-diablo: helpers for the isometric ARPG view.
// Requires `SceneDesc scene` (scene.glsl) to be declared before inclusion.

const float ARPG_TWO_PI = 6.28318530718;

vec3 arpgWorldPos(vec2 ndc, float z) {
  vec4 p = scene.viewProjectInv*vec4(ndc, z, 1.0);
  return p.xyz/p.w;
  }

float arpgRoomRadius(int i) {
  return scene.arpgRoom[i>>2][i&3];
  }

// Indoors only: how much a pixel lies outside of the room around the hero.
// 0 = visible, 1 = outside (rendered black, like unexplored space in Diablo).
float arpgOutside(vec3 wpos, bool isSky) {
  if(scene.arpgPlayer.w<=0.0 || scene.arpgParams2.z<=0.0)
    return 0.0;
  if(isSky)
    return 1.0;

  // far below the floor (valley below a tower, holes in cut walls)
  const float drop  = (scene.arpgPlayer.y - scene.arpgParams2.w) - wpos.y;
  const float below = smoothstep(0.0, 100.0, drop);

  // beyond the walls: 16 horizontal rays cast from the hero (line of sight)
  const vec2  d    = wpos.xz - scene.arpgPlayer.xz;
  const float dist = length(d);
  float a = atan(d.y, d.x);
  if(a<0.0)
    a += ARPG_TWO_PI;
  const float f      = a*(16.0/ARPG_TWO_PI);
  const int   i0     = int(floor(f)) & 15;
  const int   i1     = (i0+1) & 15;
  const float r      = mix(arpgRoomRadius(i0), arpgRoomRadius(i1), fract(f));
  const float beyond = smoothstep(r, r+150.0, dist);

  return max(beyond, below);
  }

#endif
