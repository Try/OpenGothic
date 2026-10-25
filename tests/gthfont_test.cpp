// Built with -fno-access-control to supply font metadata without loading a GPU
// texture. The real GthFont implementation performs every measurement and wrap.
#include "utils/gthfont.h"

#include <Tempest/Texture2d>
#include <gtest/gtest.h>

namespace {
class FontFixture : public testing::Test {
  protected:
    void SetUp() override {
      font.pfnt = std::make_shared<zenkit::Font>("synthetic",16,std::vector<zenkit::FontGlyph>(256));
      font.pfnt->glyphs['a'].width = 5;
      font.pfnt->glyphs[' '].width = 3;
      font.pfnt->glyphs[0xc5].width = 9;
      font.pfnt->glyphs[0xe5].width = 7;
      font.setScale(1);
      // processText(nullptr) uses only metadata; no texture data or device exists.
      texture.texW = 512;
      texture.texH = 256;
      font.tex = &texture;
      }
    Tempest::Texture2d texture;
    GthFont font;
  };

TEST_F(FontFixture, DefaultDoesNotReinterpretMissingCp1252Bytes) {
  EXPECT_EQ(0,font.textSize("\xa8\xb8").w);
  EXPECT_EQ(16,font.textSize("\xa8\xb8").h);
  EXPECT_EQ(16,font.textSize("\xc5\xe5").w);
  }

TEST_F(FontFixture, MissingYoUsesCaseMatchedBaseMetricsInEveryTextSizeOverload) {
  font.enableCyrillicFallback();
  const std::string text = "a\xa8\xb8 a";
  const auto* begin = reinterpret_cast<const uint8_t*>(text.c_str());
  EXPECT_EQ(29,font.textSize(text).w);
  EXPECT_EQ(29,font.textSize(text.c_str(),text.c_str()+text.size()).w);
  EXPECT_EQ(29,font.textSize(begin,begin+text.size()).w);
  EXPECT_EQ(29,font.textSize(100,text).w);
  EXPECT_EQ(16,font.textSize(100,text).h);
  EXPECT_EQ(std::string("a\xa8\xb8 a"),text);
  }

TEST_F(FontFixture, ExplicitOptInIsIsolatedFromCachedFontAndOtherCopies) {
  auto nameFont = font;
  auto untouched = font;
  nameFont.enableCyrillicFallback();
  EXPECT_EQ(16,nameFont.textSize("\xa8\xb8").w);
  EXPECT_EQ(0,font.textSize("\xa8\xb8").w);
  EXPECT_EQ(0,untouched.textSize("\xa8\xb8").w);
  EXPECT_EQ(0u,font.pfnt->glyphs[0xa8].width);
  EXPECT_EQ(0u,font.pfnt->glyphs[0xb8].width);
  }

TEST_F(FontFixture, ExistingRealYoOrCp1252GlyphsKeepTheirOriginalMetrics) {
  font.pfnt->glyphs[0xa8].width = 4;
  font.pfnt->glyphs[0xb8].width = 6;
  font.enableCyrillicFallback();
  EXPECT_EQ(10,font.textSize("\xa8\xb8").w);
  EXPECT_EQ(4,font.textSize("\xa8").w);
  EXPECT_EQ(6,font.textSize("\xb8").w);
  }

TEST_F(FontFixture, AbsentBaseCannotProduceAnInvalidFallback) {
  font.pfnt->glyphs[0xc5].width = 0;
  font.pfnt->glyphs[0xe5].width = 0;
  font.enableCyrillicFallback();
  EXPECT_EQ(0,font.textSize("\xa8\xb8").w);
  }

TEST_F(FontFixture, ScaledAdvanceAndLineHeightAgreeWithBaseLetters) {
  font.enableCyrillicFallback();
  font.setScale(1.5f);
  EXPECT_EQ(font.textSize("\xc5\xe5").w,font.textSize("\xa8\xb8").w);
  EXPECT_EQ(23,font.textSize("\xa8\xb8").w);
  EXPECT_EQ(24,font.textSize("\xa8\xb8").h);
  }

TEST_F(FontFixture, WordWrappingUsesTheSameEffectiveAdvanceAsMeasurements) {
  const std::string text = "a \xb8 a";
  EXPECT_EQ(2,font.lineCount(10,text));
  font.enableCyrillicFallback();
  EXPECT_EQ(3,font.lineCount(10,text));
  EXPECT_EQ(7,font.textSize(10,text).w);
  EXPECT_EQ(48,font.textSize(10,text).h);
  EXPECT_EQ(font.textSize(10,"a \xe5 a").w,font.textSize(10,text).w);
  EXPECT_EQ(font.lineCount(10,"a \xe5 a"),font.lineCount(10,text));
  }
}
