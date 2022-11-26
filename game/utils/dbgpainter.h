#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>

class DbgPainter {
  public:
    DbgPainter(Tempest::Painter& painter, const Tempest::Matrix4x4& mvp, int w, int h);

    void setBrush(const Tempest::Brush& brush);
    void setPen  (const Tempest::Pen&   pen);

    void drawText(int x, int y, std::string_view txt);
    void drawLine(const Tempest::Vec3& a, const Tempest::Vec3& b);
    void drawPoint();

    Tempest::Painter&        painter;
    const Tempest::Matrix4x4 mvp;
    const int                w;
    const int                h;
  };

