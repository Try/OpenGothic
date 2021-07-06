#pragma once

#include <Tempest/Matrix4x4>
#include <cstdint>

#include "graphics/sceneglobals.h"
#include "graphics/bounds.h"

class Frustrum;
class VisibleSet;

class VisibilityGroup {
  public:
    VisibilityGroup();

    class Token {
      public:
        Token() = default;
        Token(Token&& t);
        Token& operator = (Token&& t);
        ~Token();

        void   setObject   (VisibleSet* b, size_t id);
        void   setObjMatrix(const Tempest::Matrix4x4& at);
        void   setAlwaysVis(bool v);
        void   setBounds   (const Bounds& bbox);

        const Bounds& bounds() const;

      private:
        Token(VisibilityGroup& ow, size_t id);

        VisibilityGroup* owner = nullptr;
        size_t           id    = 0;
      friend class VisibilityGroup;
      };

    Token get();
    void  pass(const Frustrum f[]);

  private:
    struct Tok {
      Tempest::Matrix4x4 pos;
      Bounds             bbox;
      VisibleSet*        vSet = nullptr;
      size_t             id     = 0;
      bool               updateBbox = false;
      bool               alwaysVis = false;
      };

    std::vector<Tok>    tokens;
    std::vector<size_t> freeList;

    static bool subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY);
  };

