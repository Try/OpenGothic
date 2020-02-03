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

void GthFont::drawText(Painter &p, int bx, int by, int bw, int bh, const std::string& txtChar, AlignFlag align) const {
  drawText(p,bx,by,bw,bh,txtChar.c_str(),align);
  }

void GthFont::drawText(Painter &p, int bx, int by, int bw, int /*bh*/,
                       const char *txtChar, Tempest::AlignFlag align) const {
  if(tex==nullptr || txtChar==nullptr)
    return;

  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtChar);

  auto b = p.brush();
  p.setBrush(Brush(*tex,color));

  int   h  = pixelSize();
  int   x  = bx, y=by-h;
  float tw = float(tex->w());
  float th = float(tex->h());

  int   lwidth = 0;

  while(*txt) {
    auto t = getLine(txt,bw,lwidth);

    if(align!=NoAlign && align!=AlignLeft) {
      auto sz = textSize(txt,t);
      int x1 = x;
      if(align & AlignHCenter)
        x1 = x + (bw-sz.w)/2;
      if(align & AlignRight)
        x1 = x + (bw-sz.w);
      x = x1;
      }

    for(auto i=txt;i!=t;++i) {
      uint8_t id  = *i;
      auto&   uv1 = fnt.getFontInfo().fontUV1[id];
      auto&   uv2 = fnt.getFontInfo().fontUV2[id];
      int     w   = fnt.getFontInfo().glyphWidth[id];

      p.drawRect(x,y, w,h,
                 tw*uv1.x,th*uv1.y, tw*uv2.x,th*uv2.y);
      x += w;
      }

    while(*t==' ')
      ++t;

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
  float tw = float(tex->w());
  float th = float(tex->h());

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
  return textSize(txtChar,txtChar+std::strlen(txtChar));
  }

Size GthFont::textSize(const char* cb, const char* ce) const {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(cb);
  const uint8_t* e = reinterpret_cast<const uint8_t*>(ce);
  return textSize(b,e);
  }

Size GthFont::textSize(const uint8_t* b, const uint8_t* e) const {
  int h  = pixelSize();
  int x  = 0, y = h;
  int totalW = 0;

  for(size_t i=0;;) {
    uint8_t id = b[i];
    if(b+i==e) {
      totalW = std::max(totalW,x);
      break;
      }
    else if(id=='\n') {
      totalW = std::max(totalW,x);
      ++i;
      while(b[i]==' ' && b+i!=e)
        ++i;
      y+=h;
      x=0;
      } else {
      int w = fnt.getFontInfo().glyphWidth[id];
      x += w;
      ++i;
      }
    }

  return Size(totalW,y);
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
