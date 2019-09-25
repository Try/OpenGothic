#include "visualfx.h"

using namespace Tempest;

VisualFx::VisualFx(Daedalus::GEngineClasses::CFx_Base &&src):fx(std::move(src)) {
  if(src.visAlphaBlendFunc_S=="ADD") {
    rs.setBlendSource(RenderState::BlendMode::one);
    rs.setBlendDest  (RenderState::BlendMode::one);
    }
  else if(src.visAlphaBlendFunc_S=="BLEND") {
    rs.setBlendSource(RenderState::BlendMode::src_alpha);
    rs.setBlendDest  (RenderState::BlendMode::one_minus_src_alpha);
    }
  else if(src.visAlphaBlendFunc_S=="MUL") {
    //TODO
    }
  }
