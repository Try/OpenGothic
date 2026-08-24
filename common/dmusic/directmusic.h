#pragma once

#include "dlscollection.h"
#include "patternlist.h"
#include "segment.h"
#include "style.h"

#include <Tempest/File>
#include <vector>

namespace Dx8 {

/**
 * http://doc.51windows.net/Directx9_SDK/htm/directmusicfilestructures.htm
 */
class DirectMusic final {
  public:
    DirectMusic();

    using StyleList = std::vector<std::pair<std::u16string,Style>>;
    using DlsList   = std::vector<std::unique_ptr<std::pair<std::u16string,DlsCollection>>>;

    PatternList          load(const Segment& s);
    PatternList          load(const char16_t* fsgt);

    void addPath(std::u16string path);

    const Style&         style        (const Reference &id);
    const DlsCollection& dlsCollection(const Reference &id);
    const DlsCollection& dlsCollection(const std::u16string &file);

    const StyleList&     stlList() const { return styles; }
    const DlsList&       dlsCollection() { return dls;    }

  private:
    StyleList                   styles;
    DlsList                     dls;
    std::vector<std::u16string> path;

    Tempest::RFile              implOpen(const char16_t* file);
  };

}
