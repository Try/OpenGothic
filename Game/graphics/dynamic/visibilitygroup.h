#pragma once

#include <Tempest/Matrix4x4>
#include <cstdint>

#include "graphics/bounds.h"

class Frustrum;

class VisibilityGroup {
  public:
    VisibilityGroup();

    enum VisCamera : uint8_t {
      V_Shadow0    = 0,
      V_Shadow1    = 1,
      V_ShadowLast = 1,
      V_Main       = 2,
      V_Count
      };

    class Token {
      public:
        Token() = default;
        Token(Token&& t);
        Token& operator = (Token&& t);
        ~Token();

        void   setObjMatrix(const Tempest::Matrix4x4& at);
        void   setBounds   (const Bounds& bbox);
        bool   isVisible   (VisCamera c) const;

        const Bounds& bounds() const;

      private:
        Token(VisibilityGroup& ow, size_t id);

        VisibilityGroup* owner = nullptr;
        size_t           id    = 0;
      friend class VisibilityGroup;
      };

    Token get();
    void  pass(const Tempest::Matrix4x4& main, const Tempest::Matrix4x4* sh, size_t shCount);

  private:
    struct Tok {
      Tempest::Matrix4x4 pos;
      Bounds             bbox;
      bool               visible[V_Count] = {};
      };

    std::vector<Tok>    tokens;
    std::vector<size_t> freeList;
  };

inline bool VisibilityGroup::Token::isVisible(VisCamera c) const {
  return owner->tokens[id].visible[c];
  }
