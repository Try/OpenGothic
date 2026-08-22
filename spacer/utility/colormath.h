#pragma once

class ColorMath {
  public:
    static void rgb2hsv(double  r, double  g, double  b, double& h, double& s, double& v);
    static void hsv2rgb(double& r, double& g, double& b, double  h, double  s, double  v);

    static void rgb2hsv(float   r, float   g, float   b, float&  h, float&  s, float&  v);
    static void hsv2rgb(float&  r, float&  g, float&  b, float   h, float   s, float   v);
  };

