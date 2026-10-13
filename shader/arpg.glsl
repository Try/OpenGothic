#ifndef ARPG_GLSL
#define ARPG_GLSL

// gothic-diablo: helpers for the isometric ARPG view.
// Requires `SceneDesc scene` (scene.glsl) to be declared before inclusion.

vec3 arpgWorldPos(vec2 ndc, float z) {
  vec4 p = scene.viewProjectInv*vec4(ndc, z, 1.0);
  return p.xyz/p.w;
  }

// gbuffer hint bit 4: inner side of the world mesh drawn as a black cross-section (see main.frag)
const int ARPG_CAP_BIT = (1 << 4);

bool arpgIsCap(float gbufAlpha) {
  const int x = int(gbufAlpha*255.0+0.5);
  return (x & ARPG_CAP_BIT)!=0;
  }

float arpgLosAt(int i) {
  i = i & 127;
  return scene.arpgLos[i>>2][i&3];
  }

// Indoors: 1 for what the hero cannot see (beyond the cave walls around him), 0 for what he can
float arpgOutOfSight(vec3 wpos) {
  const vec2  d    = wpos.xz - scene.arpgPlayer.xz;
  const float dist = length(d);
  if(dist<1.0)
    return 0.0;
  float f = atan(d.y, d.x)*(128.0/(2.0*3.14159265));
  if(f<0.0)
    f += 128.0;
  const int   i0  = int(floor(f));
  const float los = mix(arpgLosAt(i0), arpgLosAt(i0+1), fract(f));
  // walls themselves (and a bit behind them) stay visible
  return smoothstep(los+150.0, los+350.0, dist);
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
    ret = max(ret, indoor*arpgOutOfSight(wpos));
    }
  return ret;
  }

#endif
