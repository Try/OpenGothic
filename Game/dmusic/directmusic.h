#pragma once

#include "dlscollection.h"
#include "music.h"
#include "segment.h"
#include "style.h"
#include <vector>

namespace Dx8 {

/**
 * http://doc.51windows.net/Directx9_SDK/htm/directmusicfilestructures.htm
 */
class DirectMusic final {
  public:
    DirectMusic();

    using StyleList = std::vector<std::pair<std::u16string,Style>>;
    using DlsList   = std::vector<std::pair<std::u16string,DlsCollection>>;

    Music                load(const Segment& s);

    const Style&         style        (const Reference &id);
    const DlsCollection& dlsCollection(const Reference &id);

    const StyleList&     stlList() const { return styles; }
    const DlsList&       dlsCollection() { return dls;    }

  private:
    StyleList styles;
    DlsList   dls;
  };

}
