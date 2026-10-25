#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "utils/string_frm.h"

namespace {

template<size_t storageSz>
void expectText(const string_frm<storageSz>& actual, std::string_view expected) {
  ASSERT_EQ(size_t(actual.end()-actual.begin()),expected.size());
  EXPECT_EQ(std::string_view(actual),expected);
  EXPECT_EQ(std::string_view(actual.begin(),expected.size()),expected);
  EXPECT_EQ(actual.begin()[expected.size()],'\0');
  EXPECT_EQ(actual.empty(),expected.empty());
  }

}

TEST(StringFrm, EmptyAndShortCString) {
  const char* empty = "";
  const char* shortText = "Apple";
  expectText(string_frm(empty),"");
  expectText(string_frm(shortText),"Apple");
  expectText(string_frm(empty,shortText,empty,"!"),"Apple!");
  }

TEST(StringFrm, CStringAtStackBoundary) {
  for(size_t length : {size_t(62),size_t(63),size_t(64)}) {
    SCOPED_TRACE(length);
    const std::string expected(length,'a');
    expectText(string_frm(expected.c_str()),expected);
    }
  }

TEST(StringFrm, LongSingleCString) {
  const std::string expected = std::string(512,'a')+"END";
  expectText(string_frm(expected.c_str()),expected);
  }

TEST(StringFrm, CStringStopsAtFirstNul) {
  const char text[] = {'A','p','p','l','e','\0','i','g','n','o','r','e','d','\0'};
  expectText(string_frm(text,"/END"),"Apple/END");
  }

TEST(StringFrm, CustomStorageKeepsTheSameCStringContract) {
  for(size_t length : {size_t(10),size_t(11),size_t(12),size_t(48)}) {
    SCOPED_TRACE(length);
    const std::string prefix(length,'p');
    expectText(string_frm<12>(prefix.c_str()),prefix);
    expectText(string_frm<12>(prefix.c_str(),"/END"),prefix+"/END");
    }
  }

TEST(StringFrm, CStringFragmentsContinueAfterStackLimit) {
  for(size_t length : {size_t(62),size_t(63),size_t(64),size_t(128)}) {
    SCOPED_TRACE(length);
    const std::string prefix(length,'p');
    const char* middle = "following fragment";
    expectText(string_frm(prefix.c_str(),"",middle,"/END"),
               prefix+"following fragment/END");
    }
  }

TEST(StringFrm, MixedArgumentsContinueAfterStackLimit) {
  const std::string prefix(63,'p');
  const std::string_view view = "view";
  expectText(string_frm(prefix.c_str(),view,'/',-12,'/',34u,'/',size_t(56),
                        '/',1.5f,'/',2.25,"/END"),
             prefix+"view/-12/34/56/1.500000/2.250000/END");
  }

TEST(StringFrm, EnglishQuantityAndSpellModalCompositions) {
  const char* controls = "Confirm: Cross / Enter\nCancel: Circle / Esc";
  const char* quantity = "Left/Right: 1   Up/Down: 10\nL1: 1   R1: All";
  const char* spell = "Arrows / L1 / R1: slot 3-10";
  expectText(string_frm("Drop items",": ",size_t(20),"\n",std::string_view("Apple"),
                        "\n",quantity,"\n",controls),
             "Drop items: 20\nApple\nLeft/Right: 1   Up/Down: 10\nL1: 1   R1: All\n"
             "Confirm: Cross / Enter\nCancel: Circle / Esc");
  expectText(string_frm("Assign slot",": ",size_t(10),"\n",std::string_view("Light"),
                        "\n",spell,"\n",controls),
             "Assign slot: 10\nLight\nArrows / L1 / R1: slot 3-10\n"
             "Confirm: Cross / Enter\nCancel: Circle / Esc");
  }

TEST(StringFrm, RussianGameCodepageQuantityAndSpellModalCompositions) {
  // Match the CP1251 bytes consumed by Gothic's bitmap fonts.
  const char* controls = "\xcf\xf0\xe8\xed\xff\xf2\xfc: Cross / Enter\n\xce\xf2\xec\xe5\xed\xe0: Circle / Esc";
  const char* quantity = "\xc2\xeb\xe5\xe2\xee/\xe2\xef\xf0\xe0\xe2\xee: 1   \xc2\xe2\xe5\xf0\xf5/\xe2\xed\xe8\xe7: 10\nL1: 1   R1: \xe2\xe5\xf1\xfc \xf1\xf2\xe5\xea";
  const char* spell = "\xd1\xf2\xf0\xe5\xeb\xea\xe8 / L1 / R1: \xf1\xeb\xee\xf2 3-10";
  expectText(string_frm("\xc2\xfb\xe1\xf0\xee\xf1\xe8\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb",": ",size_t(20),"\n",
                        std::string_view("\xdf\xe1\xeb\xee\xea\xee"),"\n",quantity,"\n",controls),
             "\xc2\xfb\xe1\xf0\xee\xf1\xe8\xf2\xfc \xef\xf0\xe5\xe4\xec\xe5\xf2\xfb: 20\n\xdf\xe1\xeb\xee\xea\xee\n"
             "\xc2\xeb\xe5\xe2\xee/\xe2\xef\xf0\xe0\xe2\xee: 1   \xc2\xe2\xe5\xf0\xf5/\xe2\xed\xe8\xe7: 10\nL1: 1   R1: \xe2\xe5\xf1\xfc \xf1\xf2\xe5\xea\n"
             "\xcf\xf0\xe8\xed\xff\xf2\xfc: Cross / Enter\n\xce\xf2\xec\xe5\xed\xe0: Circle / Esc");
  expectText(string_frm("\xcd\xe0\xe7\xed\xe0\xf7\xe8\xf2\xfc \xf1\xeb\xee\xf2",": ",size_t(10),"\n",
                        std::string_view("\xd1\xe2\xe5\xf2"),"\n",spell,"\n",controls),
             "\xcd\xe0\xe7\xed\xe0\xf7\xe8\xf2\xfc \xf1\xeb\xee\xf2: 10\n\xd1\xe2\xe5\xf2\n"
             "\xd1\xf2\xf0\xe5\xeb\xea\xe8 / L1 / R1: \xf1\xeb\xee\xf2 3-10\n"
             "\xcf\xf0\xe8\xed\xff\xf2\xfc: Cross / Enter\n\xce\xf2\xec\xe5\xed\xe0: Circle / Esc");
  }
