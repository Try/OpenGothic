#include "gthfont.h"

#include <Tempest/Size>
#include "resources.h"

using namespace Tempest;


GthFont::GthFont(phoenix::buffer data, std::string_view ftex, const Color &cl)
  :fnt(phoenix::font::parse(data)), color(cl) {
  tex = Resources::loadTexture(ftex);
  }

int GthFont::pixelSize() const {
  return int(fnt.height);
  }

void GthFont::drawText(Painter &p, int bx, int by, int bw, int bh,
                       std::string_view txt, Tempest::AlignFlag align, int firstLine) const {
  if(tex==nullptr || txt.empty())
    return;

  auto b = p.brush();
  p.setBrush(Brush(*tex,color));
  processText(&p,bx,by,bw,bh,txt,align,firstLine);
  p.setBrush(b);
  }

Size GthFont::processText(Painter* p, int bx, int by, int bw, int bh,
                          std::string_view txtView, AlignFlag align, int firstLine) const {
  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtView.data());

  int   h  = pixelSize();
  int   x  = bx, y=by-h;
  float tw = float(tex->w());
  float th = float(tex->h());

  int   lwidth = 0;

  Size ret = {0,0};
  while(*txt) {
    auto t    = getLine(txt,bw,lwidth);
    auto sz   = textSize(txt,t);
    auto next = t;

    if(*next=='\n')
      ++next;
    while(*next==' ')
      ++next; // lead spaces of next line

    if(firstLine>0) {
      txt = next;
      --firstLine;
      continue;
      }
    if(ret.h+sz.h>bh && bh>0) {
      break;
      }

    ret.w  = std::max(ret.w,sz.w);
    ret.h += sz.h;

    if(align!=NoAlign && align!=AlignLeft) {
      int x1 = x;
      if(align & AlignHCenter)
        x1 = x + (bw-sz.w)/2;
      if(align & AlignRight)
        x1 = x + (bw-sz.w);
      x = x1;
      }

    for(auto i=txt; i!=t; ++i) {
      uint8_t id  = *i;
      auto&   uv1 = fnt.glyphs[id].uv[0];
      auto&   uv2 = fnt.glyphs[id].uv[1];
      int     w   = fnt.glyphs[id].width;

      if(p!=nullptr) {
        p->drawRect(x,y, w,h,
                    tw*uv1.x,th*uv1.y, tw*uv2.x,th*uv2.y);
        }
      x += w;
      }

    txt = next;
    x   = bx;
    y  += sz.h;
    }

  return ret;
  }

void GthFont::drawText(Tempest::Painter &p, int bx, int by, std::string_view txtChar) const {
  if(tex==nullptr || txtChar.empty())
    return;

  const uint8_t* txt = reinterpret_cast<const uint8_t*>(txtChar.data());

  auto b = p.brush();
  p.setBrush(Brush(*tex,color));

  int   h  = pixelSize();
  int   x  = bx, y=by-h;
  float tw = float(tex->w());
  float th = float(tex->h());

  for(size_t i=0;txt[i];++i) {
    uint8_t id  = txt[i];
    auto&   uv1 = fnt.glyphs[id].uv[0];
    auto&   uv2 = fnt.glyphs[id].uv[1];
    int     w   = fnt.glyphs[id].width;

    p.drawRect(x,y, w,h,
               tw*uv1.x,th*uv1.y, tw*uv2.x,th*uv2.y);
    x += w;
    }
  p.setBrush(b);
  }

Size GthFont::textSize(std::string_view txt) const {
  if(txt.empty())
    return Size();
  return textSize(txt.data(),txt.data()+txt.size());
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
      }
    else {
      int w = fnt.glyphs[id].width;
      x += w;
      ++i;
      }
    }

  return Size(totalW,y);
  }

Size GthFont::textSize(int bw, std::string_view txt) const {
  if(tex==nullptr || txt.empty())
    return Size();
  return processText(nullptr,0,0,bw,0,txt,NoAlign,0);
  }

int32_t GthFont::lineCount(int bw, std::string_view txt) const {
  if(tex==nullptr || txt.empty())
    return 0;
  auto ret = processText(nullptr,0,0,bw,0,txt,NoAlign,0);
  return ret.h/pixelSize();
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

  // if(*txt=='\0')
  //   return txt;
  // return txt+1;
  return txt;
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
    space += fnt.glyphs[id].width;
    ++txt;
    }

  while(true) {
    uint8_t id = *txt;
    if(id=='\0' || id=='\n' || id==' ')
      return txt;

    width += fnt.glyphs[id].width;
    ++txt;
    }
  }

bool GthFont::isSpace(uint8_t ch) {
  return ch==' ' || ch=='\n';
  }
