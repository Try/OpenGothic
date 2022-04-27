#pragma once

#include <Tempest/Matrix4x4>
#include <cstdint>

#include "graphics/sceneglobals.h"
#include "graphics/bounds.h"

class Frustrum;
class VisibleSet;

class VisibilityGroup {
  private:
    struct TokList;

  public:
    VisibilityGroup(const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox);

    enum Group : uint8_t {
      G_Default,
      G_Static,
      G_AlwaysVis,
      };

    class Token {
      public:
        Token() = default;
        Token(Token&& t);
        Token& operator = (Token&& t);
        ~Token();

        void   setObject   (VisibleSet* b, size_t id);
        void   setObjMatrix(const Tempest::Matrix4x4& at);
        void   setGroup    (Group gr);
        void   setBounds   (const Bounds& bbox);

        const Bounds& bounds() const;

      private:
        Token(VisibilityGroup& owner, TokList& group, size_t id);

        VisibilityGroup* owner = nullptr;
        TokList*         group = nullptr;
        size_t           id    = 0;
      friend class VisibilityGroup;
      };

    Token get(Group g);
    void  pass(const Frustrum f[]);

  private:
    struct Tok {
      Tempest::Matrix4x4 pos;
      Bounds             bbox;
      VisibleSet*        vSet = nullptr;
      size_t             id     = 0;
      bool               updateBbox = false;
      };

    struct TokList {
      std::vector<Tok>    tokens;
      std::vector<size_t> freeList;
      };
    TokList def, alwaysVis;

    TokList& group(Group gr);
    static bool subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY);
  };

