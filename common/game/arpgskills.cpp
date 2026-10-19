#include "arpgskills.h"
#include "arpgkeys.h"

#include <algorithm>

#include "game/serialize.h"

using namespace ArpgSkills;

// ---------------------------------------------------------------------------------------------------------------
// the trees. Icons: the game's item models.

const std::vector<Tree>& ArpgSkills::trees() {
  static const std::vector<Tree> t = {
    {"Cios", "LPM", "Zwykły atak bronią białą lub strzał z łuku", "ITMW_1H_SLD_SWORD",
      {
        {"Ostrze",       {{"Ostra klinga",5,"+4% obrażeń ciosu za punkt","ITMW_SCHWERT1"},
                          {"Słaby punkt",3,"+5% szansy na pełne obrażenia (krytyk Gothica)","ITMW_1H_VLK_DAGGER"},
                          {"Głębokie rany",3,"Trafienie: krwawienie 5% obrażeń na sekundę przez 3 s, za punkt","ITAT_CLAW"}}},
        {"Rytm",         {{"Płynne ruchy",5,"+7% szybkości ataku za punkt","ITMW_RAPIER"},
                          {"Wyczucie chwili",2,"Okno combo dłuższe o 25% za punkt","ITMI_SEXTANT"},
                          {"Czwarty cios",1,"Seria kończy się dodatkowym ciosem x1,5","ITMW_MEISTERDEGEN"}}},
        {"Zamach",       {{"Szeroki łuk",3,"Łuk cięcia 120 stopni, +20 za punkt (do 180)","ITMW_SENSE"},
                          {"Odrzut",2,"Cios odpycha małe stworzenia o 0,5 m za punkt","ITMW_KRIEGSHAMMER1"},
                          {"Dobicie",3,"+15% obrażeń wobec powalonych za punkt","ITMW_SCHLACHTAXT"}}},
        {"Krew za krew", {{"Pijawka",3,"2% zadanych obrażeń wraca jako zdrowie, za punkt","ITPO_HEALTH_01"},
                          {"Twarda skóra",3,"Po trafieniu +5 ochrony przed bronią na 3 s, za punkt","ITAT_CRAWLERPLATE"}}},
      },
      {"Wir",1,"Ostatni cios serii: obrót dookoła, trafia wszystkich wokół","ITRU_WHIRLWIND"},
      "3/3 w Dobiciu albo Czwarty cios", {1,2}, {0, {{2,2},{1,2}}}},
    {"Atak ładowany", "PPM", "Ładuj 2 s, puść w złotym oknie: x3, zamach 180 stopni", "ITMW_ZWEIHAENDER1",
      {
        {"Ładowanie",    {{"Skupienie",4,"Ładowanie krótsze o 0,2 s za punkt (do 1,2 s)","ITMI_FOCUS"},
                          {"Szersze okno",2,"Złote okno puszczenia +0,1 s za punkt","ITMI_ROCKCRYSTAL"},
                          {"Pewna ręka",1,"Spóźnione puszczenie daje x2, a nie zwykły cios","ITRI_STR_01"}}},
        {"Siła",         {{"Miażdżący cios",5,"Mnożnik obrażeń +0,2 za punkt (do x4)","ITMW_STEINBRECHER"},
                          {"Wstrząs",3,"Powaleni wstają o 0,5 s później, za punkt","ITMW_MORGENSTERN"},
                          {"Łamacz gardy",1,"Atak ładowany przebija blok przeciwnika","ITMW_2H_ORCAXE_01"}}},
        {"Fala",         {{"Fala uderzeniowa",1,"Zamach puszcza falę na 4 m przed postać (50% obrażeń)","ITRU_WINDFIST"},
                          {"Dalsza fala",3,"+1 m zasięgu fali za punkt","ITSC_WINDFIST"}}},
        {"Odnowienie",   {{"Opanowanie",4,"Odnowienie krótsze o 0,5 s za punkt","ITPL_SPEED_HERB_01"},
                          {"Pęd bitewny",1,"Zabicie atakiem ładowanym zeruje odnowienie","ITPO_SPEED"}}},
      },
      {"Gniew Innosa",1,"Atak ładowany podpala wrogów: ogień przez 4 s","ITMI_INNOSSTATUE"},
      "10 punktów w drzewku", {}, {10, {}}},
    {"Fikołek", "Spacja", "Przewrót w stronę kursora, chwila bez obrażeń", "ITBE_ADDON_DEX_10",
      {
        {"Zwinność",     {{"Zwinność",3,"Bez obrażeń dłużej o 0,1 s za punkt","ITRI_DEX_01"},
                          {"Lekki krok",3,"Przerwa między fikołkami krótsza o 0,05 s, za punkt","ITAT_WING"},
                          {"Drugi oddech",1,"Dwa fikołki pod rząd, ładunek wraca po 3 s","ITFO_ADDON_SCHNELLERHERING"}}},
        {"Dystans",      {{"Dalszy przewrót",3,"+15% dystansu za punkt","ITRI_DEX_02"},
                          {"Przez szereg",1,"Fikołek przez wrogów, bez zatrzymania na nich","ITAT_SHADOWHORN"}}},
        {"Kontra",       {{"Kontratak",3,"Przez 2 s po fikołku cios +15% obrażeń, za punkt","ITMW_ELBASTARDO"},
                          {"Cios z przewrotu",1,"LPM w trakcie fikołka: cios zaraz po nim","ITMW_SHORTSWORD1"}}},
        {"Kurz",         {{"Piasek w oczy",2,"Mijani wrogowie chybiają przez 1 s, za punkt","ITMI_SULFUR"},
                          {"Podcięcie",1,"Wrogowie na drodze fikołka przewracają się","ITMW_STABKEULE"}}},
        {"Akrobatyka",   {{"Akrobata",1,"Fikołek przy upadku z wysokości: bez obrażeń","ITAR_LEATHER_L"},
                          {"Wyczucie strzał",3,"+10% szans, że strzała chybi, przez 1 s po fikołku","ITAM_PROT_POINT_01"}}},
      },
      {"Cień",1,"Po fikołku 1 s niewidzialności: wrogowie gubią cel","ITAT_SHADOWFUR"},
      "3/3 w Kontrataku", {2}, {0, {{2,0}}}},
    {"Blok", "", "Blok (środkowy przycisk) bronią albo pięściami; później także tarczą. Umiejętność pod klawiszem bloku",
     "ITMW_1H_SPECIAL_04",
      {
        {"Uderzenie",    {{"Uderzenie",1,"Zryw 1,5 m w stronę kursora, cel zachwiany (odnowienie 6 s)","ITMW_STREITKOLBEN"},
                          {"Ciężkie uderzenie",3,"Uderzenie: +25% obrażeń i +0,25 s zachwiania za punkt","ITMW_NAGELKEULE"},
                          {"Szybsze uderzenie",2,"Odnowienie uderzenia krótsze o 1 s za punkt","ITPO_SPEED"}}, 0, ArpgKeys::SkillBlock},
        {"Szarża",       {{"Szarża",1,"Uderzenie z rozbiegu: do 6 m, przewraca wszystkich na drodze","ITAT_SHADOWHORN"},
                          {"Taran",2,"Szarża dalej o 1 m za punkt","ITMW_KRIEGSHAMMER1"}}, 0, -1, 0},
        {"Garda",        {{"Twarda garda",2,"Blok: +1 ładunek za punkt","ITAT_CRAWLERPLATE"},
                          {"Oddech",3,"Ładunki bloku wracają o 20% szybciej, za punkt","ITPL_SPEED_HERB_01"}}},
        {"Osłona",       {{"Osłona",1,"Blok zatrzymuje strzały i bełty z przodu","ITRW_ARROW"},
                          {"Osłona przed magią",3,"W bloku -15% obrażeń od czarów za punkt","ITAM_PROT_POINT_01"}}},
        {"Odwet",        {{"Odwet",3,"Zablokowany cios: 10% obrażeń wraca do atakującego, za punkt","ITAT_CLAW"}}},
      },
      {"Bastion",1,"Przez 3 s po parowaniu blok z każdej strony i bez zużycia ładunków","ITMI_INNOSSTATUE"},
      "8 punktów w drzewku", {}, {8, {}}},
    {"Jednoręczna", "", "Umiejętności broni jednoręcznej. Druga umiejętność od 30% biegłości, węzeł kluczowy od 60%",
     "ITMW_1H_MIL_SWORD",
      {
        {"Pchnięcie",    {{"Pchnięcie",1,"Wypad 3 m w stronę kursora, trafia wrogów na drodze (x1,5), krwawienie (odnowienie 5 s)","ITMW_RAPIER"},
                          {"Głębokie pchnięcie",3,"Pchnięcie: +25% obrażeń za punkt","ITMW_1H_VLK_DAGGER"},
                          {"Dalszy wypad",2,"Pchnięcie dalej o 1 m za punkt","ITRI_DEX_02"}}, 0, ArpgKeys::SkillA},
        {"Taniec ostrzy",{{"Taniec ostrzy",1,"Trzy szybkie cięcia, każde w stronę kursora (odnowienie 8 s)","ITMW_MEISTERDEGEN"},
                          {"Szybkie ostrza",3,"Odnowienie tańca krótsze o 1 s za punkt","ITPO_SPEED"},
                          {"Czwarte cięcie",1,"Taniec ostrzy: cztery cięcia","ITMW_1H_SPECIAL_04"}}, 30, ArpgKeys::SkillB},
        {"Szermierka",   {{"Wyczucie ostrza",3,"Okno parowania dłuższe o 50 ms za punkt (broń jednoręczna)","ITMI_SEXTANT"},
                          {"Dłuższe zachwianie",2,"Wróg zachwiany o 0,25 s dłużej, za punkt","ITMW_ELBASTARDO"}}},
        {"Precyzja",     {{"Celny sztych",3,"+5% szansy na pełne obrażenia bronią jednoręczną, za punkt","ITMW_SCHWERT1"},
                          {"Rozcięcie",2,"Pełne obrażenia: krwawienie 5% na sekundę przez 3 s, za punkt","ITAT_CLAW"}}},
        {"Wolna ręka",   {{"Lekka broń",3,"+5% szybkości ataku bronią jednoręczną, za punkt","ITMW_SHORTSWORD1"},
                          {"Zwinny krok",2,"Z bronią jednoręczną przerwa między fikołkami krótsza o 0,1 s, za punkt","ITAT_WING"}}},
      },
      {"Riposta",1,"Każde parowanie: od razu kontratak w zachwianego wroga (bez klikania)","ITMW_1H_PAL_SWORD"},
      "60% biegłości i 8 punktów w drzewku", {}, {8, {}}, false, 1, 60},
    {"Dwuręczna", "", "Umiejętności broni dwuręcznej. Druga umiejętność od 30% biegłości, węzeł kluczowy od 60%",
     "ITMW_ZWEIHAENDER1",
      {
        {"Skok",         {{"Skok",1,"Skok do 5 m w punkt kursora, cios znad głowy przewraca wrogów w promieniu 2,5 m (odnowienie 8 s)","ITMW_SCHLACHTAXT"},
                          {"Twarde lądowanie",3,"Skok: +25% obrażeń za punkt","ITMW_STEINBRECHER"},
                          {"Dalszy skok",2,"Skok dalej o 1 m za punkt","ITBE_ADDON_DEX_10"}}, 0, ArpgKeys::SkillA},
        {"Młyn",         {{"Młyn",1,"Trzymany: obrót z bronią, trafia wszystkich wokół co 0,4 s, do 2 s (odnowienie 8 s)","ITRU_WHIRLWIND"},
                          {"Wicher",3,"Młyn: +15% obrażeń za punkt","ITSC_WINDFIST"},
                          {"Długi młyn",2,"Młyn dłużej o 0,5 s za punkt","ITMW_2H_SLD_SWORD"}}, 30, ArpgKeys::SkillB},
        {"Ciężar",       {{"Miażdżenie",3,"+5% obrażeń bronią dwuręczną za punkt","ITMW_MORGENSTERN"},
                          {"Ogłuszenie",2,"Cios dwuręczny: 10% szansy na zachwianie, za punkt","ITMW_2H_ORCAXE_01"}}},
        {"Zawziętość",   {{"Niewzruszony",1,"Trafienie nie przerywa zamachu bronią dwuręczną","ITAR_LEATHER_L"},
                          {"Twardy jak skała",3,"Z bronią dwuręczną -5% otrzymanych obrażeń, za punkt","ITAT_CRAWLERPLATE"}}},
        {"Rzeźnik",      {{"Rzeźnik",3,"Zabicie bronią dwuręczną: +10% szybkości ataku na 3 s, za punkt","ITAT_TEETH"}}},
      },
      {"Szał najemnika",1,"Poniżej 30% życia: +30% obrażeń i 3% pijawki","ITMW_2H_PAL_SWORD"},
      "60% biegłości i 8 punktów w drzewku", {}, {8, {}}, false, 2, 60},
    {"Łuk i kusza", "", "Umiejętności łuku i kuszy (liczy się lepsza biegłość z dwóch). Druga umiejętność od 30%, węzeł kluczowy od 60%",
     "ITRW_BOW_M_04",
      {
        {"Deszcz strzał",{{"Deszcz strzał",1,"Strzały z nieba przez 2 s na obszar 4 m w punkcie kursora (odnowienie 10 s)","ITRU_FIRERAIN"},
                          {"Ulewa",3,"Deszcz strzał dłużej o 0,5 s za punkt","ITRW_ARROW"},
                          {"Szeroki deszcz",2,"Deszcz strzał: +1 m promienia za punkt","ITRW_BOW_M_01"}}, 0, ArpgKeys::SkillA},
        {"Odskok",       {{"Odskok",1,"Skok w tył (od kursora) i strzał w wroga pod kursorem albo najbliższego (odnowienie 6 s)","ITAT_WING"},
                          {"Dalszy odskok",2,"Odskok dalej o 0,5 m za punkt","ITRI_DEX_01"},
                          {"Podwójny strzał",1,"Odskok: dwie strzały","ITRW_BOW_L_04"}}, 30, ArpgKeys::SkillB},
        {"Naciąg",       {{"Szybki naciąg",5,"+4% szybkości strzałów za punkt","ITRW_BOW_L_01"},
                          {"Przebicie",2,"Strzała przelatuje przez 1 wroga za punkt","ITRW_ARROW"},
                          {"Celne oko",3,"+5% szansy trafienia powyżej 15 m, za punkt","ITRW_BOW_H_01"}}},
        {"Strzał ładowany",{{"Strzała przeszywająca",1,"Strzał ładowany przebija wszystkich w linii","ITRW_ADDON_MAGICARROW"},
                          {"Grad strzał",3,"+1 strzała w wachlarzu za punkt","ITRW_BOW_M_01"},
                          {"Ciężki grot",2,"Strzał ładowany odpycha, przy 2/2 przewraca","ITRW_BOLT"}}},
        {"Myśliwy",      {{"Myśliwy",3,"+10% obrażeń strzał wobec zwierząt i potworów, za punkt","ITAT_WOLFFUR"},
                          {"Kołczan",3,"10% szansy, że strzała nie ubędzie, za punkt","ITRW_CROSSBOW_L_01"}}},
      },
      {"Łowca",1,"Pierwszy strzał we wroga, który bohatera nie zauważył: obrażenia x3","ITRW_BOW_H_04"},
      "60% biegłości i 8 punktów w drzewku", {}, {8, {}}, false, 3, 60},
    {"Biegłość", "", "Procent Gothica w broni: szansa na pełne obrażenia i lepsze ruchy w walce (od 30% wojownik, od 60% mistrz). "
                     "Bez nauczyciela do 30%, dalej do tylu procent, do ilu nauczyciel uczy", "ITMW_1H_SPECIAL_04",
      {
        {"Jednoręczna",  {{"Nowicjusz",4,"+5% biegłości w broni jednoręcznej za punkt (do 30%)","ITMW_SHORTSWORD1"},
                          {"Wojownik",6,"+5% za punkt (do 60%); od 30% ruchy wojownika. Wymaga nauczyciela","ITMW_1H_MIL_SWORD"},
                          {"Mistrz",8,"+5% za punkt (do 100%); od 60% ruchy mistrza. Wymaga nauczyciela","ITMW_1H_PAL_SWORD"}}},
        {"Dwuręczna",    {{"Nowicjusz",4,"+5% biegłości w broni dwuręcznej za punkt (do 30%)","ITMW_ZWEIHAENDER1"},
                          {"Wojownik",6,"+5% za punkt (do 60%); od 30% ruchy wojownika. Wymaga nauczyciela","ITMW_2H_SLD_SWORD"},
                          {"Mistrz",8,"+5% za punkt (do 100%); od 60% ruchy mistrza. Wymaga nauczyciela","ITMW_2H_PAL_SWORD"}}},
        {"Łuk",          {{"Nowicjusz",4,"+5% biegłości w łuku za punkt (do 30%)","ITRW_BOW_L_04"},
                          {"Strzelec",6,"+5% za punkt (do 60%). Wymaga nauczyciela","ITRW_BOW_M_04"},
                          {"Mistrz",8,"+5% za punkt (do 100%). Wymaga nauczyciela","ITRW_BOW_H_04"}}},
        {"Kusza",        {{"Nowicjusz",4,"+5% biegłości w kuszy za punkt (do 30%)","ITRW_CROSSBOW_L_01"},
                          {"Strzelec",6,"+5% za punkt (do 60%). Wymaga nauczyciela","ITRW_CROSSBOW_M_01"},
                          {"Mistrz",8,"+5% za punkt (do 100%). Wymaga nauczyciela","ITRW_CROSSBOW_H_01"}}},
      },
      {nullptr,0,nullptr,nullptr},
      "", {}, {}, true},
    };
  return t;
  }

std::string ArpgSkills::nodeKey(size_t tree, int branch, int node) {
  auto& t = trees()[tree];
  if(branch<0)
    return std::string(t.name)+"/"+(t.keystone.name!=nullptr ? t.keystone.name : "");
  auto& br = t.branches[size_t(branch)];
  if(t.weapons) // the same node names in every branch
    return std::string(t.name)+"/"+br.name+"/"+br.nodes[size_t(node)].name;
  return std::string(t.name)+"/"+br.nodes[size_t(node)].name;
  }

size_t ArpgSkills::treeOf(std::string_view name) {
  auto& tr = trees();
  for(size_t i=0; i<tr.size(); ++i)
    if(name==tr[i].name)
      return i;
  return size_t(-1);
  }

int32_t ArpgSkills::treePct(const Tree& t, const int32_t pct[5]) {
  if(t.talent<=0 || t.talent>4)
    return 0;
  if(t.talent==3)
    return std::max(pct[3], pct[4]); // bow or crossbow
  return pct[t.talent];
  }

size_t ArpgSkills::weaponsTree() {
  auto& tr = trees();
  for(size_t i=0; i<tr.size(); ++i)
    if(tr[i].weapons)
      return i;
  return size_t(-1);
  }

int ArpgSkills::talentOf(const NodeId& n) {
  if(n.tree!=weaponsTree() || n.branch<0 || n.branch>3)
    return 0;
  return n.branch+1;
  }

const char* ArpgSkills::weaponName(int talent) {
  switch(talent) {
    case 1: return "broń jednoręczna";
    case 2: return "broń dwuręczna";
    case 3: return "łuk";
    case 4: return "kusza";
    }
  return "";
  }

std::string ArpgSkills::branchKey(size_t tree, int branch) {
  auto& t = trees()[tree];
  return std::string(t.name)+"/"+t.branches[size_t(branch)].name;
  }

static const Node& nodeOf(const NodeId& n) {
  auto& t = trees()[n.tree];
  if(n.branch<0)
    return t.keystone;
  return t.branches[size_t(n.branch)].nodes[size_t(n.node)];
  }

static bool exists(const NodeId& n) {
  auto& tr = trees();
  if(n.tree>=tr.size())
    return false;
  auto& t = tr[n.tree];
  if(n.branch<0)
    return t.keystone.name!=nullptr;
  if(size_t(n.branch)>=t.branches.size())
    return false;
  return n.node>=0 && size_t(n.node)<t.branches[size_t(n.branch)].nodes.size();
  }

// ---------------------------------------------------------------------------------------------------------------

void ArpgProgress::clear() {
  perksDirty = true;
  rk.clear();
  found.clear();
  caps.clear();
  bought      = 0;
  weaponsInit = false;
  }

static void writeMap(Serialize& fout, const ArpgProgress::Ranks& m) {
  fout.write(uint32_t(m.size()));
  for(auto& i:m)
    fout.write(i.first, i.second);
  }

static void readMap(Serialize& fin, ArpgProgress::Ranks& m) {
  uint32_t n = 0;
  fin.read(n);
  for(uint32_t i=0; i<n; ++i) {
    std::string k;
    int32_t     v = 0;
    fin.read(k, v);
    m[k] = v;
    }
  }

void ArpgProgress::save(Serialize& fout) const {
  const uint8_t ver = 2;
  fout.write(ver, bought);
  writeMap(fout, rk);
  writeMap(fout, found);
  // v2: Biegłość
  writeMap(fout, caps);
  fout.write(weaponsInit);
  }

void ArpgProgress::load(Serialize& fin) {
  clear(); // perks made again
  uint8_t ver = 0;
  fin.read(ver, bought);
  readMap(fin, rk);
  readMap(fin, found);
  if(ver>=2) {
    readMap(fin, caps);
    fin.read(weaponsInit);
    }
  // Diathic: the bow nodes of Cios and Atak ładowany moved to the tree Łuk i kusza (2026-10-16)
  static const char* moved[][2] = {
    {"Cios/Szybki naciąg",                   "Łuk i kusza/Szybki naciąg"},
    {"Cios/Przebicie",                       "Łuk i kusza/Przebicie"},
    {"Cios/Celne oko",                       "Łuk i kusza/Celne oko"},
    {"Atak ładowany/Strzała przeszywająca",  "Łuk i kusza/Strzała przeszywająca"},
    {"Atak ładowany/Grad strzał",            "Łuk i kusza/Grad strzał"},
    {"Atak ładowany/Ciężki grot",            "Łuk i kusza/Ciężki grot"},
    };
  for(auto& m:moved) {
    auto it = rk.find(m[0]);
    if(it==rk.end())
      continue;
    rk[m[1]] += it->second;
    rk.erase(it);
    }
  }

int32_t ArpgProgress::cap(int talent) const {
  auto it = caps.find(std::to_string(talent));
  const int32_t c = (it!=caps.end()) ? it->second : 0;
  return std::clamp(std::max(c, FreeCap), 0, 100);
  }

void ArpgProgress::setCap(int talent, int32_t pct) {
  caps[std::to_string(talent)] = std::clamp(pct, 0, 100);
  }

void ArpgProgress::initWeapons(const Hero& h) {
  weaponsInit = true;
  perksDirty  = true;
  const size_t wt = weaponsTree();
  if(wt==size_t(-1))
    return;
  auto& t = trees()[wt];
  for(int b=0; b<int(t.branches.size()) && b<4; ++b) {
    const int     tal  = b+1;
    const int32_t pct  = h.pct[tal];
    int32_t       left = std::clamp((pct-BasePct)/PctPerRank, 0, 18);
    // a teacher let him learn that far
    if(pct>cap(tal))
      setCap(tal, pct);
    if(left<=0)
      continue;
    found[branchKey(wt, b)] += left;
    auto& br = t.branches[size_t(b)];
    for(int n=0; n<int(br.nodes.size()) && left>0; ++n) {
      const int32_t r = std::min(left, int32_t(br.nodes[size_t(n)].max));
      rk[nodeKey(wt, b, n)] = r;
      left -= r;
      }
    }
  }

void ArpgProgress::resetRanks(int32_t pctBack[5]) {
  for(int i=0; i<5; ++i)
    pctBack[i] = 0;
  const size_t wt = weaponsTree();
  if(wt!=size_t(-1)) {
    for(int b=0; b<4 && b<int(trees()[wt].branches.size()); ++b) {
      int32_t r = 0;
      for(int n=0; n<int(trees()[wt].branches[size_t(b)].nodes.size()); ++n)
        r += rank(NodeId{wt,b,n});
      pctBack[b+1] = r*PctPerRank;
      }
    }
  rk.clear(); // points found for a branch stay there (to be spent again)
  perksDirty = true;
  }

int32_t ArpgProgress::spentTotal() const {
  int32_t sum = 0;
  for(auto& i:rk)
    sum += i.second;
  return sum;
  }

int32_t ArpgProgress::rankOf(const Ranks& r, const NodeId& n) const {
  if(!exists(n))
    return 0;
  auto it = r.find(nodeKey(n.tree, n.branch, n.node));
  return it!=r.end() ? it->second : 0;
  }

int32_t ArpgProgress::rank(const NodeId& n) const {
  return rankOf(rk, n);
  }

static int32_t treePointsOf(const ArpgProgress::Ranks& r, size_t tree) {
  auto&   t   = trees()[tree];
  int32_t sum = 0;
  for(int b=0; b<int(t.branches.size()); ++b)
    for(int n=0; n<int(t.branches[size_t(b)].nodes.size()); ++n) {
      auto it = r.find(nodeKey(tree, b, n));
      if(it!=r.end())
        sum += it->second;
      }
  return sum;
  }

int32_t ArpgProgress::treePoints(size_t tree) const {
  if(tree>=trees().size())
    return 0;
  return treePointsOf(rk, tree);
  }

int32_t ArpgProgress::spentFree(const Ranks& r) const {
  // points found for a branch are spent there first; everything else comes from the free points
  int32_t sum = 0;
  auto&   tr  = trees();
  for(size_t t=0; t<tr.size(); ++t) {
    for(int b=0; b<int(tr[t].branches.size()); ++b) {
      int32_t inBranch = 0;
      for(int n=0; n<int(tr[t].branches[size_t(b)].nodes.size()); ++n)
        inBranch += rankOf(r, NodeId{t,b,n});
      auto    f   = found.find(branchKey(t, b));
      int32_t use = (f!=found.end()) ? std::min(std::max(0, f->second), inBranch) : 0;
      sum += inBranch-use;
      }
    sum += rankOf(r, NodeId{t,-1,0});
    }
  return sum;
  }

int32_t ArpgProgress::freePoints(int32_t level) const {
  return std::max(0, level)*PointsPerLevel + bought - spentFree(rk);
  }

int32_t ArpgProgress::branchLeft(size_t tree, int branch) const {
  if(tree>=trees().size() || branch<0 || size_t(branch)>=trees()[tree].branches.size())
    return 0;
  auto f = found.find(branchKey(tree, branch));
  if(f==found.end())
    return 0;
  int32_t inBranch = 0;
  for(int n=0; n<int(trees()[tree].branches[size_t(branch)].nodes.size()); ++n)
    inBranch += rank(NodeId{tree,branch,n});
  return std::max(0, f->second-inBranch);
  }

bool ArpgProgress::reqMet(const Ranks& r, const NodeId& n, std::string* why) const {
  auto& t = trees()[n.tree];
  if(n.branch>=0) {
    // the first node of a branch is open (or needs the skill it changes); the next one needs a point in the one
    // before it (Biegłość: that one full)
    if(n.node==0) {
      const int nb = t.branches[size_t(n.branch)].needs;
      if(nb<0 || size_t(nb)>=t.branches.size() || rankOf(r, NodeId{n.tree, nb, 0})>0)
        return true;
      if(why!=nullptr)
        *why = std::string("Wymaga punktu w: ")+t.branches[size_t(nb)].nodes[0].name;
      return false;
      }
    const NodeId prev{n.tree, n.branch, n.node-1};
    const auto&  pn   = nodeOf(prev);
    if(t.weapons ? rankOf(r, prev)>=pn.max : rankOf(r, prev)>0)
      return true;
    if(why!=nullptr)
      *why = std::string(t.weapons ? "Wymaga pełnego węzła: " : "Wymaga punktu w: ")+pn.name;
    return false;
    }
  auto& kr = t.keyReq;
  if(kr.treePoints>0 && treePointsOf(r, n.tree)>=kr.treePoints)
    return true;
  for(auto& m:kr.anyMaxed) {
    const NodeId id{n.tree, m.first, m.second};
    if(exists(id) && rankOf(r, id)>=nodeOf(id).max)
      return true;
    }
  if(why!=nullptr)
    *why = std::string("Wymaga: ")+t.keyReqText;
  return false;
  }

bool ArpgProgress::valid(const Ranks& r) const {
  auto& tr = trees();
  for(size_t t=0; t<tr.size(); ++t) {
    for(int b=0; b<int(tr[t].branches.size()); ++b)
      for(int n=0; n<int(tr[t].branches[size_t(b)].nodes.size()); ++n) {
        const NodeId id{t,b,n};
        if(rankOf(r, id)>0 && !reqMet(r, id, nullptr))
          return false;
        }
    const NodeId key{t,-1,0};
    if(rankOf(r, key)>0 && !reqMet(r, key, nullptr))
      return false;
    }
  return true;
  }

bool ArpgProgress::canAdd(const NodeId& n, const Hero& h, std::string* why) const {
  if(!exists(n))
    return false;
  if(rank(n)>=nodeOf(n).max) {
    if(why!=nullptr)
      *why = "Pełne";
    return false;
    }
  if(!reqMet(rk, n, why))
    return false;
  {
    // a weapon tree: a skill / the keystone needs Gothic's percent in the weapon
    auto& t    = trees()[n.tree];
    int   need = 0;
    if(n.branch<0)
      need = t.keyPct;
    else if(n.node==0)
      need = t.branches[size_t(n.branch)].pct;
    if(t.talent>0 && need>0 && treePct(t, h.pct)<need) {
      if(why!=nullptr)
        *why = "Wymaga "+std::to_string(need)+"% biegłości: "+(t.talent==3 ? std::string("łuk lub kusza") : std::string(weaponName(t.talent)));
      return false;
      }
  }
  if(const int tal = talentOf(n); tal>0 && h.pct[tal]+PctPerRank>cap(tal)) {
    if(why!=nullptr)
      *why = "Dalej tylko u nauczyciela: teraz możesz do "+std::to_string(cap(tal))+"%";
    return false;
    }
  if(n.branch>=0 && branchLeft(n.tree, n.branch)>0)
    return true;
  if(freePoints(h.level)>0)
    return true;
  if(why!=nullptr)
    *why = "Brak wolnych punktów umiejętności";
  return false;
  }

bool ArpgProgress::add(const NodeId& n, const Hero& h) {
  if(!canAdd(n, h))
    return false;
  rk[nodeKey(n.tree, n.branch, n.node)] = rank(n)+1;
  perksDirty = true;
  return true;
  }

bool ArpgProgress::canRemove(const NodeId& n, const Ranks& base) const {
  if(!exists(n))
    return false;
  const int32_t r = rank(n);
  if(r<=0 || r<=rankOf(base, n))
    return false; // only points added since the book was opened (a reset is done by a teacher)
  Ranks test = rk;
  test[nodeKey(n.tree, n.branch, n.node)] = r-1;
  return valid(test);
  }

bool ArpgProgress::remove(const NodeId& n, const Ranks& base) {
  if(!canRemove(n, base))
    return false;
  const auto k = nodeKey(n.tree, n.branch, n.node);
  if(--rk[k]<=0)
    rk.erase(k);
  perksDirty = true;
  return true;
  }

const ArpgPerks& ArpgProgress::perks() const {
  if(!perksDirty)
    return perksCache;
  perksDirty = false;
  // rank of a node by its tree and name (keystones too)
  auto r = [&](const char* tree, const char* node) -> float {
    auto& tr = trees();
    for(size_t t=0; t<tr.size(); ++t) {
      if(std::string_view(tr[t].name)!=tree)
        continue;
      if(tr[t].keystone.name!=nullptr && std::string_view(tr[t].keystone.name)==node)
        return float(rank(NodeId{t,-1,0}));
      for(int b=0; b<int(tr[t].branches.size()); ++b)
        for(int n=0; n<int(tr[t].branches[size_t(b)].nodes.size()); ++n)
          if(std::string_view(tr[t].branches[size_t(b)].nodes[size_t(n)].name)==node)
            return float(rank(NodeId{t,b,n}));
      }
    return 0.f;
    };
  ArpgPerks p;
  p.meleeDmg    = 0.04f*r("Cios","Ostra klinga");
  p.critAdd     = int(5.f*r("Cios","Słaby punkt"));
  p.bleed       = 0.05f*r("Cios","Głębokie rany");
  p.atkSpeed    = 0.07f*r("Cios","Płynne ruchy"); // Diathic: from the standard speed of Gothic (no base bonus)
  p.comboWindow = 0.25f*r("Cios","Wyczucie chwili");
  p.fourthHit   = r("Cios","Czwarty cios")>0;
  p.arc         = std::min(180.f, 120.f+20.f*r("Cios","Szeroki łuk"));
  p.knockback   = 0.5f*r("Cios","Odrzut");
  p.vsDown      = 0.15f*r("Cios","Dobicie");
  p.leech       = 0.02f*r("Cios","Pijawka");
  p.hardSkin    = int(5.f*r("Cios","Twarda skóra"));
  p.shotSpeed   = 0.04f*r("Łuk i kusza","Szybki naciąg");
  p.pierce      = int(r("Łuk i kusza","Przebicie"));
  p.farAim      = 0.05f*r("Łuk i kusza","Celne oko");
  p.whirl       = r("Cios","Wir")>0;

  p.chargeTime   = std::max(1.2f, 2.f-0.2f*r("Atak ładowany","Skupienie"));
  p.chargeWindow = 0.2f+0.1f*r("Atak ładowany","Szersze okno");
  p.sureHand     = r("Atak ładowany","Pewna ręka")>0;
  p.chargeMul    = 3.f+0.2f*r("Atak ładowany","Miażdżący cios");
  p.getUpDelay   = 0.5f*r("Atak ładowany","Wstrząs");
  p.guardBreak   = r("Atak ładowany","Łamacz gardy")>0;
  p.waveRange    = r("Atak ładowany","Fala uderzeniowa")>0 ? 400.f+100.f*r("Atak ładowany","Dalsza fala") : 0.f;
  p.chargeCd     = 5.f-0.5f*r("Atak ładowany","Opanowanie");
  p.battleRush   = r("Atak ładowany","Pęd bitewny")>0;
  p.piercingShot = r("Łuk i kusza","Strzała przeszywająca")>0;
  p.volley       = int(r("Łuk i kusza","Grad strzał"));
  p.heavyHead    = int(r("Łuk i kusza","Ciężki grot"));
  p.innosWrath   = r("Atak ładowany","Gniew Innosa")>0;

  p.rollInvuln   = 0.1f*r("Fikołek","Zwinność");
  p.rollCdCut    = 0.05f*r("Fikołek","Lekki krok");
  p.secondWind   = r("Fikołek","Drugi oddech")>0;
  p.rollDist     = 0.15f*r("Fikołek","Dalszy przewrót");
  p.rollThrough  = r("Fikołek","Przez szereg")>0;
  p.counter      = 0.15f*r("Fikołek","Kontratak");
  p.rollStrike   = r("Fikołek","Cios z przewrotu")>0;
  p.sandEyes     = 1.f*r("Fikołek","Piasek w oczy");
  p.trip         = r("Fikołek","Podcięcie")>0;
  p.acrobat      = r("Fikołek","Akrobata")>0;
  p.arrowDodge   = 0.1f*r("Fikołek","Wyczucie strzał");
  p.shadow       = r("Fikołek","Cień")>0;

  p.bash          = r("Blok","Uderzenie")>0;
  p.bashDmg       = 0.25f*r("Blok","Ciężkie uderzenie");
  p.bashStagger   = 0.25f*r("Blok","Ciężkie uderzenie");
  p.bashCdCut     = 1.f*r("Blok","Szybsze uderzenie");
  p.bashRun       = p.bash && r("Blok","Szarża")>0;
  p.bashRunDist   = 100.f*r("Blok","Taran");
  p.blockCharges  = int(r("Blok","Twarda garda"));
  p.blockRegen    = 0.2f*r("Blok","Oddech");
  p.arrowBlock    = r("Blok","Osłona")>0;
  p.spellBlock    = 0.15f*r("Blok","Osłona przed magią");
  p.retaliate     = 0.1f*r("Blok","Odwet");
  p.bastion       = r("Blok","Bastion")>0;

  p.thrust        = r("Jednoręczna","Pchnięcie")>0;
  p.thrustDmg     = 0.25f*r("Jednoręczna","Głębokie pchnięcie");
  p.thrustDist    = 100.f*r("Jednoręczna","Dalszy wypad");
  p.bladeDance    = r("Jednoręczna","Taniec ostrzy")>0;
  p.danceCdCut    = 1.f*r("Jednoręczna","Szybkie ostrza");
  p.danceHits     = r("Jednoręczna","Czwarte cięcie")>0 ? 4 : 3;
  p.parry1H       = 50.f*r("Jednoręczna","Wyczucie ostrza");
  p.staggerAdd    = 0.25f*r("Jednoręczna","Dłuższe zachwianie");
  p.crit1H        = int(5.f*r("Jednoręczna","Celny sztych"));
  p.critBleed     = 0.05f*r("Jednoręczna","Rozcięcie");
  p.atkSpeed1H    = 0.05f*r("Jednoręczna","Lekka broń");
  p.rollCd1H      = 0.1f*r("Jednoręczna","Zwinny krok");
  p.riposte       = r("Jednoręczna","Riposta")>0;

  p.leap          = r("Dwuręczna","Skok")>0;
  p.leapDmg       = 0.25f*r("Dwuręczna","Twarde lądowanie");
  p.leapDist      = 100.f*r("Dwuręczna","Dalszy skok");
  p.mill          = r("Dwuręczna","Młyn")>0;
  p.millDmg       = 0.15f*r("Dwuręczna","Wicher");
  p.millTime      = 0.5f*r("Dwuręczna","Długi młyn");
  p.dmg2H         = 0.05f*r("Dwuręczna","Miażdżenie");
  p.stun2H        = 0.1f*r("Dwuręczna","Ogłuszenie");
  p.unstoppable   = r("Dwuręczna","Niewzruszony")>0;
  p.guard2H       = 0.05f*r("Dwuręczna","Twardy jak skała");
  p.butcher       = 0.1f*r("Dwuręczna","Rzeźnik");
  p.mercRage      = r("Dwuręczna","Szał najemnika")>0;

  p.rain          = r("Łuk i kusza","Deszcz strzał")>0;
  p.rainTime      = 0.5f*r("Łuk i kusza","Ulewa");
  p.rainRadius    = 100.f*r("Łuk i kusza","Szeroki deszcz");
  p.disengage     = r("Łuk i kusza","Odskok")>0;
  p.disengageDist = 50.f*r("Łuk i kusza","Dalszy odskok");
  p.doubleShot    = r("Łuk i kusza","Podwójny strzał")>0;
  p.hunter        = 0.1f*r("Łuk i kusza","Myśliwy");
  p.quiver        = 0.1f*r("Łuk i kusza","Kołczan");
  p.stalker       = r("Łuk i kusza","Łowca")>0;
  perksCache = p;
  return perksCache;
  }
