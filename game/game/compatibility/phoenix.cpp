#include "phoenix.h"

ZenLoad::zCMaterialData phoenix_compat::to_zenlib_material(const phoenix::material& px) {
  ZenLoad::zCMaterialData mat {};
  mat.matName = px.name;
  mat.matGroup = uint8_t(px.group);
  mat.color = px.color.r << 24 | px.color.g << 16 | px.color.b << 8 | px.color.a;
  mat.smoothAngle = px.smooth_angle;
  mat.texture = px.texture;
  mat.texScale = std::to_string(px.texture_scale.x) + " " + std::to_string(px.texture_scale.y);
  mat.texAniFPS = px.texture_anim_fps;
  mat.texAniMapMode = px.texture_anim_map_mode;
  mat.texAniMapDir = std::to_string(px.texture_anim_map_dir.x) + " " + std::to_string(px.texture_anim_map_dir.y);
  mat.noCollDet = px.disable_collision;
  mat.noLighmap = px.disable_lightmap;
  mat.loadDontCollapse = px.dont_collapse;
  mat.detailObject = px.detail_object;
  mat.detailTextureScale = px.texture_scale.x;
  mat.forceOccluder = px.force_occluder;
  mat.environmentMapping = px.environment_mapping;
  mat.environmentalMappingStrength = px.environment_mapping_strength;
  mat.waveMode = px.wave_mode;
  mat.waveSpeed = px.wave_speed;
  mat.waveMaxAmplitude = px.wave_max_amplitude;
  mat.waveGridSize = px.wave_grid_size;
  mat.ignoreSun = px.ignore_sun;

  switch(px.alpha_func) {
  case phoenix::alpha_function::test:
    mat.alphaFunc = 1;
    break;
  case phoenix::alpha_function::additive:
    mat.alphaFunc = 5;
    break;
  case phoenix::alpha_function::transparent:
    mat.alphaFunc = 4;
    break;
  case phoenix::alpha_function::multiply:
    mat.alphaFunc = 6;
    break;
  }

  return mat;
}
