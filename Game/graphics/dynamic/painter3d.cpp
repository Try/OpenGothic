#include "painter3d.h"

#include "graphics/bounds.h"
#include "graphics/lightsource.h"

using namespace Tempest;

Painter3d::Painter3d(Tempest::Encoder<CommandBuffer>& enc)
  :enc(enc) {
  }

Painter3d::~Painter3d() {
  }

void Painter3d::setFrustrum(const Matrix4x4& m) {
  frustrum.make(m);
  }

bool Painter3d::isVisible(const Bounds& b) const {
  return frustrum.testPoint(b.midTr.x,b.midTr.y,b.midTr.z, b.r);
  }

void Painter3d::setViewport(int x, int y, int w, int h) {
  enc.setViewport(x,y,w,h);
  }
