#pragma once

#include <zenload/zCFont.h>
#include <Tempest/Painter>

class GthFont final {
  public:
    GthFont(const char* name, const char *tex, const Tempest::Color& cl, const VDFS::FileIndex& fileIndex);
    GthFont(const GthFont&) = delete;

    int  pixelSize() const;

    void drawText(Tempest::Painter& p, int x, int y, int w, int h, const std::string& txt, Tempest::AlignFlag align) const;
    void drawText(Tempest::Painter& p, int x, int y, int w, int h, const char* txt, Tempest::AlignFlag align) const;
    void drawText(Tempest::Painter& p, int x, int y, const std::string& txt) const;
    void drawText(Tempest::Painter& p, int x, int y, const char* txt) const;

    auto textSize(const std::string& txt) const -> Tempest::Size;
    auto textSize(const char*        txt) const -> Tempest::Size;
    auto textSize(const char*    b, const char* e) const -> Tempest::Size;
    auto textSize(const uint8_t* b, const uint8_t* e) const -> Tempest::Size;
    auto textSize(int w, const char* txt) const -> Tempest::Size;

  private:
    ZenLoad::zCFont           fnt;
    const Tempest::Texture2d* tex=nullptr;
    Tempest::Color            color;

    const uint8_t* getLine(const uint8_t* txt, int bw, int &width) const;
    const uint8_t* getWord(const uint8_t* txt, int &width, int &space) const;

    static bool    isSpace(uint8_t ch);
    Tempest::Size  processText(Tempest::Painter* p, int x, int y, int w, int h, const char* txt, Tempest::AlignFlag align) const;
  };

