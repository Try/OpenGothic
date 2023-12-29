#pragma once
#include <phoenix/font.hh>

#include <Tempest/Painter>

class GthFont final {
  public:
    GthFont();
    GthFont(zenkit::Read* data, std::string_view ftex, const Tempest::Color &cl);
    GthFont(const GthFont&) = default;
    GthFont(GthFont&&)      = default;

    GthFont& operator = (const GthFont&) = default;
    GthFont& operator = (GthFont&&)      = default;

    int  pixelSize() const;
    void setScale(float s);

    void drawText(Tempest::Painter& p, int x, int y, int w, int h, std::string_view txt, Tempest::AlignFlag align, int firstLine=0) const;
    void drawText(Tempest::Painter& p, int x, int y, std::string_view txt) const;

    auto textSize(const std::string_view txt) const -> Tempest::Size;
    auto textSize(const char*    b, const char* e) const -> Tempest::Size;
    auto textSize(const uint8_t* b, const uint8_t* e) const -> Tempest::Size;
    auto textSize(int w, std::string_view txt) const -> Tempest::Size;
    auto lineCount(int w, std::string_view txt) const -> int32_t;

  private:
    std::shared_ptr<phoenix::font> pfnt;
    const Tempest::Texture2d*      tex       = nullptr;
    uint32_t                       fntHeight = 0;
    float                          scale     = 0;
    Tempest::Color                 color;

    const uint8_t* getLine(const uint8_t* txt, int bw, int &width) const;
    const uint8_t* getWord(const uint8_t* txt, int &width, int &space) const;

    static bool    isSpace(uint8_t ch);
    Tempest::Size  processText(Tempest::Painter* p, int x, int y, int w, int h, std::string_view txt, Tempest::AlignFlag align, int firstLine) const;
  };

