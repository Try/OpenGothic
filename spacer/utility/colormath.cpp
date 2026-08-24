#include "colormath.h"

#include <algorithm>

void ColorMath::rgb2hsv(double r, double g, double b,
                        double& h,double& s,double& v) {
  double rgb_max = std::max(r, std::max(g, b));
  double rgb_min = std::min(r, std::min(g, b));
  double delta = rgb_max - rgb_min;
  s = delta / (rgb_max + 1e-20);
  v = rgb_max;

  double hue;
  if(r == rgb_max)
    hue = (g - b) / (delta + 1e-20);
  else if (g == rgb_max)
    hue = 2 + (b - r) / (delta + 1e-20);
  else
    hue = 4 + (r - g) / (delta + 1e-20);
  if(hue < 0)
    hue += 6.0;
  h = hue / 6.0;
  }

void ColorMath::hsv2rgb(double& r, double& g, double& b,
                        double  h,double   s, double  v) {
  double hh, p, q, t, ff;
  long   i;

  if(s <= 0.0){
    r = v;
    g = v;
    b = v;
    return;
    }

  hh = h*6.0;
  i  = (long)hh;
  ff = hh - i;
  if(i>=6){
    i  = 0;
    ff = 0;
    }
  p  = v * (1.0 - s);
  q  = v * (1.0 - (s * ff));
  t  = v * (1.0 - (s * (1.0 - ff)));

  switch( i ){
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    case 5:
    default:
      r = v;
      g = p;
      b = q;
      break;
    }
  }

void ColorMath::rgb2hsv(float r, float g, float b, float& h, float& s, float& v) {
  double dh = h, ds =s, dv = v;
  rgb2hsv(double(r),double(g),double(b),dh,ds,dv);
  h = float(dh);
  s = float(ds);
  v = float(dv);
  }

void ColorMath::hsv2rgb(float& r, float& g, float& b, float h, float s, float v) {
  double dr = r, dg = g, db = b;
  hsv2rgb(dr,dg,db,double(h),double(s),double(v));
  r = float(dr);
  g = float(dg);
  b = float(db);
  }
