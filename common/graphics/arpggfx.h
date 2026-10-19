#pragma once

#include <string_view>
#include <vector>

#include <Tempest/AbstractGraphicsApi>

namespace ArpgGfx {
struct Option {
  std::string_view              ini;
  std::string_view              name;
  std::string_view              info;
  std::vector<std::string_view> levels;
  int                           def = 0;
  bool                          restart = false;
  };

const std::vector<Option>& options();
int value(std::string_view ini);
int levelsSupported(const Option& o);
void refresh();
Tempest::Sampler worldSampler(bool arpgEnabled);
constexpr float colorGradeStrength      = 0.35f;
constexpr float colorGradeShadowStart   = 0.08f;
constexpr float colorGradeShadowEnd     = 0.48f;
constexpr float colorGradeHighlightStart = 0.42f;
constexpr float colorGradeHighlightEnd = 0.88f;
constexpr float colorGradeShadowR       = 0.985f;
constexpr float colorGradeShadowG       = 1.0f;
constexpr float colorGradeShadowB       = 1.025f;
constexpr float colorGradeHighlightR    = 1.025f;
constexpr float colorGradeHighlightG    = 1.008f;
constexpr float colorGradeHighlightB    = 0.985f;
constexpr float colorGradeVignette      = 0.035f;
constexpr float colorGradeVignetteStart = 0.55f;
constexpr float colorGradeVignetteEnd   = 1.45f;
}
