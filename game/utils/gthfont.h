#pragma once
#include <phoenix/vdfs.hh>
#include <phoenix/font.hh>

#include <Tempest/Painter>

class GthFont final {
  public:
    GthFont(phoenix::buffer data, std::string_view ftex, const Tempest::Color &cl);
    GthFont(const GthFont&) = delete;

    int  pixelSize() const;

    void drawText(Tempest::Painter& p, int x, int y, int w, int h, std::string_view txt, Tempest::AlignFlag align, int firstLine=0) const;
    void drawText(Tempest::Painter& p, int x, int y, std::string_view txt) const;

    auto textSize(const std::string_view txt) const -> Tempest::Size;
    auto textSize(const char*    b, const char* e) const -> Tempest::Size;
    auto textSize(const uint8_t* b, const uint8_t* e) const -> Tempest::Size;
    auto textSize(int w, std::string_view txt) const -> Tempest::Size;
    auto lineCount(int w, std::string_view txt) const -> int32_t;

  private:
    phoenix::font             fnt;
    const Tempest::Texture2d* tex=nullptr;
    Tempest::Color            color;

    const uint8_t* getLine(const uint8_t* txt, int bw, int &width) const;
    const uint8_t* getWord(const uint8_t* txt, int &width, int &space) const;

    static bool    isSpace(uint8_t ch);
    Tempest::Size  processText(Tempest::Painter* p, int x, int y, int w, int h, std::string_view txt, Tempest::AlignFlag align, int firstLine) const;
  };

