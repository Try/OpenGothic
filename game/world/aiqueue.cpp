#include "aiqueue.h"

#include <limits>
#include "game/serialize.h"

AiQueue::AiQueue() {  
  }

void AiQueue::save(Serialize& fout) const {
  fout.write(uint32_t(aiActions.size()));
  for(auto& i:aiActions){
    fout.write(uint32_t(i.act));
    fout.write(i.target,i.victum);
    fout.write(i.point,i.func,i.i0,i.i1,i.s0);
    if(i.act==AI_PrintScreen)
      fout.write(i.i2,i.s1);
    }
  }

void AiQueue::load(Serialize& fin) {
  uint32_t size = 0;
  fin.read(size);
  aiActions.resize(size);
  for(auto& i:aiActions){
    fin.read(reinterpret_cast<uint32_t&>(i.act));
    fin.read(i.target,i.victum);
    fin.read(i.point,i.func,i.i0,i.i1,i.s0);
    if(i.act==AI_PrintScreen)
      fin.read(i.i2,i.s1);
    }
  }

void AiQueue::clear() {
  aiActions.clear();
  }

void AiQueue::pushBack(AiAction&& a) {
  if(aiActions.size()>0) {
    if(aiActions.back().act==AI_LookAtNpc && a.act==AI_LookAtNpc) {
      aiActions.back() = a;
      return;
      }
    }
  aiActions.push_back(a);
  }

void AiQueue::pushFront(AiQueue::AiAction&& a) {
  if(a.act!=AI_PrintScreen) {
    assert(a.i2==0);
    assert(a.s1.empty());
    }
  aiActions.push_front(a);
  }

AiQueue::AiAction AiQueue::pop() {
  auto act = std::move(aiActions.front());
  aiActions.pop_front();
  return act;
  }

int AiQueue::aiOutputOrderId() const {
  int v = std::numeric_limits<int>::max();
  for(auto& i:aiActions)
    if(i.i0<v && (i.act==AI_Output || i.act==AI_OutputSvm || i.act==AI_OutputSvmOverlay || i.act==AI_StopProcessInfo))
      v = i.i0;
  return v;
  }

void AiQueue::onWldItemRemoved(const Item& itm) {
  for(auto& i:aiActions)
    if(i.item==&itm)
      i.item = nullptr;
  }

AiQueue::AiAction AiQueue::aiLookAt(const WayPoint* to) {
  AiAction a;
  a.act    = AI_LookAt;
  a.point = to;
  return a;
  }

AiQueue::AiAction AiQueue::aiLookAtNpc(Npc* other) {
  AiAction a;
  a.act    = AI_LookAtNpc;
  a.target = other;
  return a;
  }

AiQueue::AiAction AiQueue::aiStopLookAt() {
  AiAction a;
  a.act = AI_StopLookAt;
  return a;
  }

AiQueue::AiAction AiQueue::aiRemoveWeapon() {
  AiAction a;
  a.act = AI_RemoveWeapon;
  return a;
  }

AiQueue::AiAction AiQueue::aiTurnAway(Npc *other) {
  AiAction a;
  a.act    = AI_TurnAway;
  a.target = other;
  return a;
  }

AiQueue::AiAction AiQueue::aiTurnToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_TurnToNpc;
  a.target = other;
  return a;
  }

AiQueue::AiAction AiQueue::aiWhirlToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_WhirlToNpc;
  a.target = other;
  return a;
  }

AiQueue::AiAction AiQueue::aiGoToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_GoToNpc;
  a.target = other;
  return a;
  }

AiQueue::AiAction AiQueue::aiGoToNextFp(std::string_view fp) {
  AiAction a;
  a.act = AI_GoToNextFp;
  a.s0  = fp;
  return a;
  }

AiQueue::AiAction AiQueue::aiStartState(ScriptFn stateFn, int behavior, Npc* other, Npc* victum, std::string_view wp) {
  AiAction a;
  a.act    = AI_StartState;
  a.func   = stateFn;
  a.i0     = behavior;
  a.s0     = wp;
  a.target = other;
  a.victum = victum;
  return a;
  }

AiQueue::AiAction AiQueue::aiPlayAnim(std::string_view ani) {
  AiAction a;
  a.act  = AI_PlayAnim;
  a.s0   = ani;
  return a;
  }

AiQueue::AiAction AiQueue::aiPlayAnimBs(std::string_view ani, BodyState bs) {
  AiAction a;
  a.act  = AI_PlayAnimBs;
  a.s0   = ani;
  a.i0   = int(bs);
  return a;
  }

AiQueue::AiAction AiQueue::aiWait(uint64_t dt) {
  AiAction a;
  a.act  = AI_Wait;
  a.i0   = int(dt);
  return a;
  }

AiQueue::AiAction AiQueue::aiStandup() {
  AiAction a;
  a.act = AI_StandUp;
  return a;
  }

AiQueue::AiAction AiQueue::aiStandupQuick() {
  AiAction a;
  a.act = AI_StandUpQuick;
  return a;
  }

AiQueue::AiAction AiQueue::aiGoToPoint(const WayPoint& to) {
  AiAction a;
  a.act   = AI_GoToPoint;
  a.point = &to;
  return a;
  }

AiQueue::AiAction AiQueue::aiEquipArmor(int32_t id) {
  AiAction a;
  a.act = AI_EquipArmor;
  a.i0  = id;
  return a;
  }

AiQueue::AiAction AiQueue::aiEquipBestArmor() {
  AiAction a;
  a.act = AI_EquipBestArmor;
  return a;
  }

AiQueue::AiAction AiQueue::aiEquipBestMeleeWeapon() {
  AiAction a;
  a.act = AI_EquipMelee;
  return a;
  }

AiQueue::AiAction AiQueue::aiEquipBestRangeWeapon() {
  AiAction a;
  a.act = AI_EquipRange;
  return a;
  }

AiQueue::AiAction AiQueue::aiUseMob(std::string_view name, int st) {
  AiAction a;
  a.act = AI_UseMob;
  a.s0  = name;
  a.i0  = st;
  return a;
  }

AiQueue::AiAction AiQueue::aiUseItem(int32_t id) {
  AiAction a;
  a.act = AI_UseItem;
  a.i0  = id;
  return a;
  }

AiQueue::AiAction AiQueue::aiUseItemToState(int32_t id, int32_t state) {
  AiAction a;
  a.act  = AI_UseItemToState;
  a.i0   = id;
  a.i1   = state;
  return a;
  }

AiQueue::AiAction AiQueue::aiTeleport(const WayPoint &to) {
  AiAction a;
  a.act   = AI_Teleport;
  a.point = &to;
  return a;
  }

AiQueue::AiAction AiQueue::aiDrawWeapon() {
  AiAction a;
  a.act = AI_DrawWeapon;
  return a;
  }

AiQueue::AiAction AiQueue::aiReadyMeleeWeapon() {
  AiAction a;
  a.act = AI_DrawWeaponMelee;
  return a;
  }

AiQueue::AiAction AiQueue::aiReadyRangeWeapon() {
  AiAction a;
  a.act = AI_DrawWeaponRange;
  return a;
  }

AiQueue::AiAction AiQueue::aiReadySpell(int32_t spell,int32_t mana) {
  AiAction a;
  a.act = AI_DrawSpell;
  a.i0  = spell;
  a.i1  = mana;
  return a;
  }

AiQueue::AiAction AiQueue::aiAttack() {
  AiAction a;
  a.act = AI_Attack;
  return a;
  }

AiQueue::AiAction AiQueue::aiFlee() {
  AiAction a;
  a.act = AI_Flee;
  return a;
  }

AiQueue::AiAction AiQueue::aiDodge() {
  AiAction a;
  a.act = AI_Dodge;
  return a;
  }

AiQueue::AiAction AiQueue::aiUnEquipWeapons() {
  AiAction a;
  a.act = AI_UnEquipWeapons;
  return a;
  }

AiQueue::AiAction AiQueue::aiUnEquipArmor() {
  AiAction a;
  a.act = AI_UnEquipArmor;
  return a;
  }

AiQueue::AiAction AiQueue::aiProcessInfo(Npc &other) {
  AiAction a;
  a.act    = AI_ProcessInfo;
  a.target = &other;
  return a;
  }

AiQueue::AiAction AiQueue::aiOutput(Npc& to, std::string_view  text, int order) {
  AiAction a;
  a.act    = AI_Output;
  a.s0     = text;
  a.target = &to;
  a.i0     = order;
  return a;
  }

AiQueue::AiAction AiQueue::aiOutputSvm(Npc &to, std::string_view  text, int order) {
  AiAction a;
  a.act    = AI_OutputSvm;
  a.s0     = text;
  a.target = &to;
  a.i0     = order;
  return a;
  }

AiQueue::AiAction AiQueue::aiOutputSvmOverlay(Npc &to, std::string_view  text, int order) {
  AiAction a;
  a.act    = AI_OutputSvmOverlay;
  a.s0     = text;
  a.target = &to;
  a.i0     = order;
  return a;
  }

AiQueue::AiAction AiQueue::aiStopProcessInfo(int order) {
  AiAction a;
  a.act = AI_StopProcessInfo;
  a.i0  = order;
  return a;
  }

AiQueue::AiAction AiQueue::aiContinueRoutine() {
  AiAction a;
  a.act = AI_ContinueRoutine;
  return a;
  }

AiQueue::AiAction AiQueue::aiAlignToFp() {
  AiAction a;
  a.act = AI_AlignToFp;
  return a;
  }

AiQueue::AiAction AiQueue::aiAlignToWp() {
  AiAction a;
  a.act = AI_AlignToWp;
  return a;
  }

AiQueue::AiAction AiQueue::aiSetNpcsToState(ScriptFn func, int32_t radius) {
  AiAction a;
  a.act  = AI_SetNpcsToState;
  a.func = func;
  a.i0   = radius;
  return a;
  }

AiQueue::AiAction AiQueue::aiSetWalkMode(WalkBit w) {
  AiAction a;
  a.act  = AI_SetWalkMode;
  a.i0   = int(w);
  return a;
  }

AiQueue::AiAction AiQueue::aiFinishingMove(Npc &other) {
  AiAction a;
  a.act    = AI_FinishingMove;
  a.target = &other;
  return a;
  }

AiQueue::AiAction AiQueue::aiTakeItem(Item& item) {
  AiAction a;
  a.act  = AI_TakeItem;
  a.item = &item;
  return a;
  }

AiQueue::AiAction AiQueue::aiGotoItem(Item& item) {
  AiAction a;
  a.act  = AI_GotoItem;
  a.item = &item;
  return a;
  }

AiQueue::AiAction AiQueue::aiPointAt(const WayPoint& to) {
  AiAction a;
  a.act   = AI_PointAt;
  a.point = &to;
  return a;
  }

AiQueue::AiAction AiQueue::aiPointAtNpc(Npc& other) {
  AiAction a;
  a.act    = AI_PointAtNpc;
  a.target = &other;
  return a;
  }

AiQueue::AiAction AiQueue::aiStopPointAt() {
  AiAction a;
  a.act    = AI_StopPointAt;
  return a;
  }

AiQueue::AiAction AiQueue::aiPrintScreen(int time, std::string_view font, int x,int y, std::string_view msg) {
  AiAction a;
  a.act    = AI_PrintScreen;
  a.i0     = x;
  a.i1     = y;
  a.s0     = msg;
  a.i2     = time;
  a.s1     = font;
  return a;
  }
