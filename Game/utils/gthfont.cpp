#include "gthfont.h"

#include <Tempest/Size>
#include "resources.h"

using namespace Tempest;

GthFont::GthFont(const char *name, const char *ftex, const Color &cl, const VDFS::FileIndex &fileIndex)
  :fnt(name,fileIndex), color(cl) {
  tex = Resources::loadTexture(ftex);
  }

int GthFont::pixelSize() const {
  return int(fnt.getFontInfo().fontHeight);
  }

void GthFont::drawText(Painter &p, int bx, int by, int bw, int bh, const std::string& txtChar) const {
  drawText(p,bx,by,bw,bh,txtChar.c_str());
  }

void GthFont::drawText(Painter &p, int bx, int by, int bw, int /*bh*/, const char *txtChar) const {
  if(tex==nullptr || txtChar==nullptr)
    return;

  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtChar);

  auto b = p.brush();
  p.setBrush(Brush(*tex,color));

  int   h  = pixelSize();
  int   x  = bx, y=by-h;
  float tw = tex->w();
  float th = tex->h();

  int   lwidth = 0;

  while(*txt) {
    auto t = getLine(txt,bw,lwidth);

    for(auto i=txt;i!=t;++i) {
      uint8_t id  = *i;
      auto&   uv1 = fnt.getFontInfo().fontUV1[id];
      auto&   uv2 = fnt.getFontInfo().fontUV2[id];
      int     w   = fnt.getFontInfo().glyphWidth[id];

      p.drawRect(x,y, w,h,
                 tw*uv1.x,th*uv1.y, tw*uv2.x,th*uv2.y);
      x += w;
      }

    txt = t;
    x = bx;
    y+= h;
    }

  p.setBrush(b);
  }

void GthFont::drawText(Painter &p, int x, int y, const std::string &txt) const {
  drawText(p,x,y,txt.c_str());
  }

void GthFont::drawText(Tempest::Painter &p, int bx, int by, const char *txtChar) const {
  if(tex==nullptr || txtChar==nullptr)
    return;

  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtChar);

  auto b = p.brush();
  p.setBrush(Brush(*tex,color));

  int   h  = pixelSize();
  int   x  = bx, y=by-h;
  float tw = tex->w();
  float th = tex->h();

  for(size_t i=0;txt[i];++i) {
    uint8_t id  = txt[i];
    auto&   uv1 = fnt.getFontInfo().fontUV1[id];
    auto&   uv2 = fnt.getFontInfo().fontUV2[id];
    int     w   = fnt.getFontInfo().glyphWidth[id];

    p.drawRect(x,y, w,h,
               tw*uv1.x,th*uv1.y, tw*uv2.x,th*uv2.y);
    x += w;
    }
  p.setBrush(b);
  }

Size GthFont::textSize(const std::string &txt) const {
  return textSize(txt.c_str());
  }

Size GthFont::textSize(const char *txtChar) const {
  if(txtChar==nullptr)
    return Size();
  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtChar);

  int   h  = pixelSize();
  int   x  = 0, y=h;

  for(size_t i=0;txt[i];++i) {
    uint8_t id  = txt[i];
    int     w   = fnt.getFontInfo().glyphWidth[id];
    x += w;
    }

  return Size(x,y);
  }

const uint8_t* GthFont::getLine(const uint8_t *txt, int bw, int& width) const {
  width = 0;
  int x = 0;
  int wwidth = 0, wspace = 0;

  // always draw at least one word
  txt = getWord(txt,wwidth,wspace);
  x += wwidth+wspace;

  while(*txt!='\0' && *txt!='\n') {
    auto t = getWord(txt,wwidth,wspace);
    x += wwidth+wspace;
    if(x>bw)
      return txt;
    width = x;
    txt   = t;
    }

  if(*txt=='\0')
    return txt;
  return txt+1;
  }

const uint8_t* GthFont::getWord(const uint8_t *txt, int& width, int& space) const {
  width = 0;
  space = 0;

  while(true) {
    uint8_t id = *txt;
    if(id=='\0' || id=='\n')
      return txt;
    if(id!=' ')
      break;
    space += fnt.getFontInfo().glyphWidth[id];
    ++txt;
    }

  while(true) {
    uint8_t id = *txt;
    if(id=='\0' || id=='\n' || id==' ')
      return txt;

    width += fnt.getFontInfo().glyphWidth[id];
    ++txt;
    }
  }

bool GthFont::isSpace(uint8_t ch) {
  return ch==' ' || ch=='\n';
  }
