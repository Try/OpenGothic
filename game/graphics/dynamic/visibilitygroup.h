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
    TokList def, stat, alwaysVis;

    struct TreeItm {
      Tok*          self = nullptr;
      Tempest::Vec3 bbox[2];
      Tempest::Vec3 midTr;
      };
    struct Node {
      Bounds   bbox;
      bool     isLeaf = false;
      };
    struct TreeTask {
      TreeItm* begin = nullptr;
      TreeItm* end   = nullptr;
      size_t   node  = 0;
      };
    std::vector<Node>     treeNode;
    std::vector<TreeItm>  treeTok;
    std::vector<TreeTask> treeTasks;
    bool                  updateThree = false;

    void     buildTree();
    void     buildTree(size_t node, TreeItm* begin, TreeItm* end, size_t step);
    void     buildTreeTasks(size_t node, size_t depth, TreeItm* begin, TreeItm* end);
    TokList& group(Group gr);

    static void setVisible(SceneGlobals::VisCamera c, TreeItm* begin, TreeItm* end);
    void        testStaticObjectsThreaded(const Frustrum f[]);
    void        testStaticObjects(const Frustrum f[], SceneGlobals::VisCamera c,
                                  size_t node, TreeItm* begin, TreeItm* end);
    static void testVisibility(Tok& t, const Frustrum f[], float mX, float mY, float sh1X, float sh1Y);
    static bool subpixelMeshTest(const Tok& t, const Frustrum& f, float edgeX, float edgeY);
  };

