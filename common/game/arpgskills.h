#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class Serialize;

// gothic-diablo: skill trees (like Last Epoch: every skill has its own tree) and the hero's skill points.
// Texts are UTF-8 (ArpgMenu::gtxt converts them for Gothic's fonts); icons are item instances of the game.
namespace ArpgSkills {
  struct Node   { const char* name; int max; const char* desc; const char* icon; };
  struct Branch { const char* name; std::vector<Node> nodes; };
  struct KeyReq {
    int                              treePoints = 0;  // this many points in the tree's branches ...
    std::vector<std::pair<int,int>>  anyMaxed;        // ... or one of these nodes (branch, node) at its maximum
    };
  struct Tree {
    const char*          name;
    const char*          key;
    const char*          root;
    const char*          icon;
    std::vector<Branch>  branches;
    Node                 keystone;
    const char*          keyReqText;
    std::vector<int>     keyFrom;   // branches drawn leading to the keystone (empty: all of them)
    KeyReq               keyReq;
    bool                 weapons = false; // Biegłość: branch b = Gothic's talent b+1 (1H, 2H, bow, crossbow), a rank = +5%,
                                          // the next node needs the one before it full; no keystone (keystone.name nullptr)
    };

  constexpr int32_t PctPerRank = 5;
  constexpr int32_t BasePct    = 10;  // Gothic's hero starts with 10% in every weapon
  constexpr int32_t FreeCap    = 30;  // without a teacher (like Gothic: further only after lessons)

  const std::vector<Tree>& trees();

  // the key a node's rank is saved under: "Tree/Node" (stays valid when the layout of a tree changes)
  std::string nodeKey(size_t tree, int branch, int node);   // branch -1: the keystone
  std::string branchKey(size_t tree, int branch);           // "Tree/Branch": points found for one branch

  // where a node is: branch -1 = the keystone (node is ignored)
  struct NodeId { size_t tree = 0; int branch = 0; int node = 0; };

  size_t      weaponsTree();             // index of Biegłość
  int         talentOf(const NodeId& n); // Gothic's talent of a Biegłość node, 0: not one
  const char* weaponName(int talent);    // "broń jednoręczna"...
  }

// gothic-diablo: what the points in the trees do, in numbers (made from the ranks, ArpgProgress::perks)
struct ArpgPerks {
  // Cios
  float meleeDmg    = 0;     // Ostra klinga: +damage of a normal hit (0.04 = +4%)
  int   critAdd     = 0;     // Słaby punkt: +% chance of Gothic's full damage
  float bleed       = 0;     // Głębokie rany: part of the hit as bleeding per second, 3 s
  float atkSpeed    = 0;     // Płynne ruchy: faster swings (0.07 = +7%)
  float comboWindow = 0;     // Wyczucie chwili: longer combo window (0.25 = +25%)
  bool  fourthHit   = false; // Czwarty cios: from the 4th hit of a series x1.5
  float arc         = 120;   // Szeroki łuk: degrees a swing hits
  float knockback   = 0;     // Odrzut: m small creatures are pushed back
  float vsDown      = 0;     // Dobicie: +damage against the knocked down
  float leech       = 0;     // Pijawka: part of the damage back as health
  int   hardSkin    = 0;     // Twarda skóra: protection for 3 s after a hit taken
  float shotSpeed   = 0;     // Szybki naciąg: faster bow / crossbow
  int   pierce      = 0;     // Przebicie: enemies an arrow flies through
  float farAim      = 0;     // Celne oko: +hit chance beyond 15 m
  bool  whirl       = false; // Wir: the last hit of a series hits all around
  // Atak ładowany
  float chargeTime   = 2.f;  // Skupienie: s to charge
  float chargeWindow = 0.2f; // Szersze okno: s of the gold window
  bool  sureHand     = false;// Pewna ręka: late release x2
  float chargeMul    = 3.f;  // Miażdżący cios
  float getUpDelay   = 0;    // Wstrząs: s the knocked down stay out of the fight longer
  bool  guardBreak   = false;// Łamacz gardy
  float waveRange    = 0;    // Fala uderzeniowa / Dalsza fala: cm of the wave (0 = none)
  float chargeCd     = 5.f;  // Opanowanie: s of the cooldown
  bool  battleRush   = false;// Pęd bitewny: a kill resets the cooldown
  bool  piercingShot = false;// Strzała przeszywająca
  int   volley       = 0;    // Grad strzał: more arrows of the charged shot
  int   heavyHead    = 0;    // Ciężki grot: 1 = pushes back, 2 = knocks down
  bool  innosWrath   = false;// Gniew Innosa: fire for 4 s
  // Fikołek
  float rollInvuln   = 0;    // Zwinność: + s without damage
  float rollCdCut    = 0;    // Lekki krok: - s between rolls
  bool  secondWind   = false;// Drugi oddech: two rolls in a row
  float rollDist     = 0;    // Dalszy przewrót: +distance (0.15 = +15%)
  bool  rollThrough  = false;// Przez szereg
  float counter      = 0;    // Kontratak: +damage for 2 s after a roll
  bool  rollStrike   = false;// Cios z przewrotu
  float sandEyes     = 0;    // Piasek w oczy: s the enemies passed miss
  bool  trip         = false;// Podcięcie
  bool  acrobat      = false;// Akrobata: no damage from falls
  float arrowDodge   = 0;    // Wyczucie strzał: chance an arrow misses for 1 s after a roll
  bool  shadow       = false;// Cień: 1 s unseen after a roll
  };

class ArpgProgress final {
  public:
    static constexpr int32_t PointsPerLevel = 1;

    using Ranks = std::map<std::string,int32_t>;

    // what of the hero matters for the points: his level and Gothic's weapon percents (index = talent 1..4)
    struct Hero {
      int32_t level  = 0;
      int32_t pct[5] = {};
      };

    void     clear();
    void     save(Serialize& fout) const;
    void     load(Serialize& fin);

    int32_t  rank      (const ArpgSkills::NodeId& n) const;
    int32_t  treePoints(size_t tree) const;              // spent in the branches of a tree (not the keystone)
    int32_t  freePoints(int32_t level) const;            // to spend in any tree
    int32_t  spentTotal() const;                         // all the points in the trees
    int32_t  branchLeft(size_t tree, int branch) const;  // found for this branch and not spent yet

    // false + why (UTF-8) when it is not possible
    bool     canAdd   (const ArpgSkills::NodeId& n, const Hero& h, std::string* why = nullptr) const;
    bool     add      (const ArpgSkills::NodeId& n, const Hero& h);
    // one point back; 'base': the ranks when the book was opened (a point can only be taken back the same visit)
    bool     canRemove(const ArpgSkills::NodeId& n, const Ranks& base) const;
    bool     remove   (const ArpgSkills::NodeId& n, const Ranks& base);

    const Ranks& ranks() const { return rk; }
    const ArpgPerks& perks() const;   // what the ranks do (kept until the ranks change)

    // Biegłość: how far a teacher let the hero learn (percent, at least FreeCap)
    int32_t  cap(int talent) const;
    void     setCap(int talent, int32_t pct);
    // the first time with Biegłość: Gothic's percents learned before become ranks (free: found for their branch)
    bool     weaponsReady() const { return weaponsInit; }
    void     initWeapons(const Hero& h);
    // all ranks away (a teacher's reset); out: the percents to take from the hero for each talent (index 1..4)
    void     resetRanks(int32_t pctBack[5]);

    int32_t  bought = 0;  // from teachers (learning points turned into skill points)
    Ranks    found;       // branchKey -> points from finds (stone tablets), only for that branch
    Ranks    caps;        // talent ("1".."4") -> percent a teacher unlocked
    bool     weaponsInit = false;

  private:
    Ranks    rk;          // nodeKey -> rank
    mutable ArpgPerks perksCache;
    mutable bool      perksDirty = true;

    int32_t  rankOf(const Ranks& r, const ArpgSkills::NodeId& n) const;
    bool     reqMet(const Ranks& r, const ArpgSkills::NodeId& n, std::string* why) const;
    bool     valid (const Ranks& r) const;
    int32_t  spentFree(const Ranks& r) const;
  };
