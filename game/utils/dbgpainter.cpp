#include "dbgpainter.h"

#include "utils/gthfont.h"
#include "resources.h"

using namespace Tempest;

DbgPainter::DbgPainter(Painter& painter, const Tempest::Matrix4x4& mvp, int w, int h)
  :painter(painter), mvp(mvp), w(w), h(h) {
  }

void DbgPainter::setBrush(const Brush& brush) {
  painter.setBrush(brush);
  }

void DbgPainter::setPen(const Pen& pen) {
  painter.setPen(pen);
  }

void DbgPainter::drawText(int x, int y, std::string_view txt) {
  auto& fnt = Resources::font(1.0);
  fnt.drawText(painter,x,y,txt);
  }

void DbgPainter::drawText(const Tempest::Vec3& a, std::string_view txt) {
  Vec3 pa = a; float wa = 1;
  mvp.project(pa.x,pa.y,pa.z, wa);
  if(wa<0.001f)
    return;
  pa /= wa;
  int x0 = int((pa.x+1.f)*0.5f*float(w));
  int y0 = int((pa.y+1.f)*0.5f*float(h));
  drawText(x0, y0, txt);
  }

void DbgPainter::drawLine(const Vec3& a, const Vec3& b) {
  Vec3  pa = a, pb = b;
  float wa = 1, wb = 1;
  mvp.project(pa.x,pa.y,pa.z,wa);
  mvp.project(pb.x,pb.y,pb.z,wb);

  if(wa<0.001f || wb<0.001f)
    return;

  pa /= wa;
  pb /= wb;

  int x0 = int((pa.x+1.f)*0.5f*float(w));
  int y0 = int((pa.y+1.f)*0.5f*float(h));

  int x1 = int((pb.x+1.f)*0.5f*float(w));
  int y1 = int((pb.y+1.f)*0.5f*float(h));

  painter.drawLine(x0,y0,x1,y1);
  }
