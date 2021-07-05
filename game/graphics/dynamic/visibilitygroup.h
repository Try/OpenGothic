#pragma once

#include <Tempest/Matrix4x4>
#include <cstdint>

#include "graphics/sceneglobals.h"
#include "graphics/bounds.h"

class Frustrum;

class VisibilityGroup {
  public:
    VisibilityGroup();

    class Token {
      public:
        Token() = default;
        Token(Token&& t);
        Token& operator = (Token&& t);
        ~Token();

        void   setObjMatrix(const Tempest::Matrix4x4& at);
        void   setBounds   (const Bounds& bbox);
        bool   isVisible   (SceneGlobals::VisCamera c) const;

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
      bool               updateBbox = false;
      bool               visible[SceneGlobals::V_Count] = {};
      };

    std::vector<Tok>    tokens;
    std::vector<size_t> freeList;

    static bool subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY);
  };

inline bool VisibilityGroup::Token::isVisible(SceneGlobals::VisCamera c) const {
  return owner->tokens[id].visible[c];
  }
