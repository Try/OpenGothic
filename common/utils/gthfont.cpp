#include "gthfont.h"

#include <Tempest/Size>
#include "resources.h"

using namespace Tempest;

GthFont::GthFont():pfnt(new zenkit::Font("", 16, {})) {
  }

GthFont::GthFont(zenkit::Read& data, std::string_view ftex, const Color &cl)
  :pfnt(std::make_shared<zenkit::Font>()), color(cl) {
  pfnt->load(&data);
  tex = Resources::loadTexture(ftex);
  setScale(1);
  }

int GthFont::pixelSize() const {
  return int(fntHeight);
  }

void GthFont::setScale(float s) {
  scale     = s;
  fntHeight = uint32_t(std::max(float(pfnt->height)*scale, 1.f)); // avoid division by zero
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
      if(p!=nullptr)
        x += drawGlyph(*p,x,y,*i);
      else
        x += glyphAdvance(*i);
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

  for(size_t i=0;txt[i];++i)
    x += drawGlyph(p,x,y,txt[i]);
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
  const auto& fnt = *pfnt;

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
    else if(id>=fnt.glyphs.size()) {
      x += 10;
      ++i;
      }
    else {
      x += glyphAdvance(id);
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
    space += glyphAdvance(id);
    ++txt;
    }

  while(true) {
    uint8_t id = *txt;
    if(id=='\0' || id=='\n' || id==' ')
      return txt;

    width += glyphAdvance(id);
    ++txt;
    }
  }

uint8_t GthFont::effectiveGlyph(uint8_t id) const {
  // CP1252 uses these bytes differently; only save-name copies opt into CP1251.
  if(!cyrillicFallback || (id!=0xa8 && id!=0xb8))
    return id;
  const uint8_t base = id==0xa8 ? 0xc5 : 0xe5;
  if(id<pfnt->glyphs.size() && pfnt->glyphs[id].width==0 &&
     base<pfnt->glyphs.size() && pfnt->glyphs[base].width!=0)
    return base;
  return id;
  }

int GthFont::glyphAdvance(uint8_t id) const {
  id = effectiveGlyph(id);
  if(id>=pfnt->glyphs.size())
    return 10;
  return int(pfnt->glyphs[id].width*scale);
  }

int GthFont::drawGlyph(Painter& p, int x, int y, uint8_t id) const {
  const auto glyph = effectiveGlyph(id);
  const int width = glyphAdvance(id);
  if(glyph>=pfnt->glyphs.size())
    return width;
  const auto& uv = pfnt->glyphs[glyph].uv;
  const int height = pixelSize();
  p.drawRect(x,y,width,height,
             float(tex->w())*uv[0].x,float(tex->h())*uv[0].y,
             float(tex->w())*uv[1].x,float(tex->h())*uv[1].y);
  if(glyph!=id && width>0) {
    // Two dots fit the existing E/e top padding. Solid color avoids dependence
    // on quotation-mark shapes and UV placement in different Gothic fonts.
    const int dot = std::max(1,std::min(height/12,width/3));
    const int top = y+(id==0xa8 ? 0 : height/5);
    auto brush = p.brush();
    p.setBrush(color);
    p.drawRect(x+std::max(0,width/3-dot/2),top,dot,dot);
    p.drawRect(x+std::min(width-dot,2*width/3-dot/2),top,dot,dot);
    p.setBrush(brush);
    }
  return width;
  }

bool GthFont::isSpace(uint8_t ch) {
  return ch==' ' || ch=='\n';
  }
