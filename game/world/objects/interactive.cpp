#include "interactive.h"

#include <Tempest/Painter>
#include <Tempest/Log>

#include <phoenix/vobs/mob.hh>

#include "game/serialize.h"
#include "graphics/mesh/skeleton.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "utils/dbgpainter.h"
#include "gothic.h"

static Npc::Anim toNpcAnim(Interactive::Anim dir) {
  switch(dir) {
    case Interactive::Anim::In:        return Npc::Anim::InteractIn;
    case Interactive::Anim::Active:    return Npc::Anim::InteractIn;
    case Interactive::Anim::Out:       return Npc::Anim::InteractOut;
    case Interactive::Anim::ToStand:   return Npc::Anim::InteractToStand;
    case Interactive::Anim::FromStand: return Npc::Anim::InteractFromStand;
    }
  return Npc::Anim::InteractIn;
  }

Interactive::Interactive(Vob* parent, World &world, const phoenix::vobs::mob& vob, Flags flags)
  : Vob(parent,world,vob,flags) {

  vobName       = vob.vob_name;
  focName       = vob.name;
  bbox[0]       = {vob.bbox.min.x, vob.bbox.min.y, vob.bbox.min.z};
  bbox[1]       = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  owner         = vob.owner;
  focOver       = vob.focus_override;
  showVisual    = vob.show_visual;

  auto p = position();
  displayOffset = Tempest::Vec3(0,bbox[1].y-p.y,0);

  if(vob.type != phoenix::vob_type::oCMOB) {
    // TODO: These might be movable
    auto& inter = reinterpret_cast<const phoenix::vobs::mob_inter&>(vob);
    stateNum      = inter.state;
    triggerTarget = inter.target;
    useWithItem   = inter.item;
    conditionFunc = inter.condition_function;
    onStateFunc   = inter.on_state_change_function;
    rewind        = inter.rewind;
    }

  for(auto& i:owner)
    i = char(std::toupper(i));

  if(vobType==phoenix::vob_type::oCMobDoor) {
    auto& door = reinterpret_cast<const phoenix::vobs::mob_door&>(vob);
    locked      = door.locked;
    keyInstance = door.key;
    pickLockStr = door.pick_string;
    }

  if(isContainer() && (flags&Flags::Startup)==Flags::Startup) {
    auto& container = reinterpret_cast<const phoenix::vobs::mob_container&>(vob);
    locked      = container.locked;
    keyInstance = container.key;
    pickLockStr = container.pick_string;

    auto items  = std::move(container.contents);
    if(items.size()>0) {
      char* it = &items[0];
      for(auto i=it;;++i) {
        if(*i==','){
          *i='\0';
          implAddItem(it);
          it=i+1;
          }
        else if(*i=='\0'){
          implAddItem(it);
          it=i+1;
          break;
          }
        }
      }
    }

  setVisual(vob);
  mdlVisual = std::move(vob.visual_name);

  if(isLadder() && !mdlVisual.empty()) {
    // NOTE: there must be else way to determinate steps count, nut for now - we parse filename
    size_t at = mdlVisual.size()-1;
    while(at>0 && !std::isdigit(mdlVisual[at]))
      --at;
    while(at>0 && std::isdigit(mdlVisual[at-1]))
      --at;
    stepsCount = std::atoi(mdlVisual.c_str()+at);
    stateNum   = stepsCount;

    transform().project(displayOffset);
    displayOffset -= position();
    }

  world.addInteractive(this);
  }

void Interactive::load(Serialize &fin) {
  Vob::load(fin);

  fin.read(vobName,focName,mdlVisual);
  fin.read(bbox[0],bbox[1],owner);
  fin.read(focOver,showVisual);

  fin.read(stateNum,triggerTarget,useWithItem,conditionFunc,onStateFunc);
  fin.read(locked,keyInstance,pickLockStr);
  fin.read(state,reverseState,loopState,isLockCracked);

  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0; i<sz; ++i) {
    std::string name;
    Npc*        user       = nullptr;
    bool        attachMode = false;
    Phase       started    = NonStarted;

    fin.read(name,user,attachMode,reinterpret_cast<uint8_t&>(started));

    for(auto& a:attPos)
      if(a.name==name) {
        a.user       = user;
        a.attachMode = attachMode;
        a.started    = started;
        }
    }

  if(fin.setEntry("worlds/",fin.worldName(),"/mobsi/",vobObjectID,"/inventory"))
    invent.load(fin,*this,world); else
    invent.clear(world.script(),*this,true);

  fin.setEntry("worlds/", fin.worldName(), "/mobsi/", vobObjectID, "/visual");
  visual.load(fin, *this);
  visual.setObjMatrix(transform());
  visual.syncPhysics();
  }

void Interactive::save(Serialize &fout) const {
  Vob::save(fout);

  fout.write(vobName,focName,mdlVisual);
  fout.write(bbox[0],bbox[1],owner);
  fout.write(focOver,showVisual);

  fout.write(stateNum,triggerTarget,useWithItem,conditionFunc,onStateFunc);
  fout.write(locked,keyInstance,pickLockStr);
  fout.write(state,reverseState,loopState,isLockCracked);

  fout.write(uint32_t(attPos.size()));
  for(auto& i:attPos) {
    fout.write(i.name,i.user,i.attachMode,i.started);
    }

  if(!invent.isEmpty()) {
    fout.setEntry("worlds/",fout.worldName(),"/mobsi/",vobObjectID,"/inventory");
    invent.save(fout);
    }

  fout.setEntry("worlds/",fout.worldName(),"/mobsi/",vobObjectID,"/visual");
  visual.save(fout,*this);
  }

void Interactive::postValidate() {
  for(auto& i:attPos)
    if(i.user!=nullptr && i.user->interactive()!=this)
      i.user = nullptr;
  }

void Interactive::resetPositionToTA(int32_t state) {
  for(auto& i:attPos)
    if(i.user!=nullptr && i.user->isPlayer())
      return;

  loopState = false;
  string_frm buf("S_S0");
  if(state>0)
    buf = string_frm("S_S",int(state));
  visual.startAnimAndGet(buf,world.tickCount(),true);
  setState(state);
  }

void Interactive::setVisual(const phoenix::vob& vob) {
  visual.setVisual(vob,world,true);
  visual.setObjMatrix(transform());
  visual.setInteractive(this);
  animChanged = true;
  if(auto mesh = visual.protoMesh()) {
    attPos.resize(mesh->pos.size());
    for(size_t i=0;i<attPos.size();++i){
      attPos[i].name = mesh->pos[i].name;
      attPos[i].pos  = mesh->pos[i].transform;
      attPos[i].node = mesh->pos[i].node;
      }
    }
  setAnim(Interactive::Active); // setup default anim
  }

void Interactive::updateAnimation(uint64_t dt) {
  if(visual.updateAnimation(nullptr,world,dt))
    animChanged = true;
  }

void Interactive::tick(uint64_t dt) {
  visual.processLayers(world);

  if(animChanged) {
    visual.syncPhysics();
    animChanged = false;
    }

  if(world.tickCount()<waitAnim)
    return;

  Pos* p = nullptr;
  for(auto& i:attPos) {
    if(i.user!=nullptr) {
      p = &i;
      }
    }

  if(p==nullptr) {
    // Note: oCMobInter::rewind, oCMobInter with killed user has to go back to state=-1
    // All other cases, oCMobFire, oCMobDoor in particular - preserve old state
    const int destSt = -1;
    if(destSt!=state && (vobType==phoenix::vob_type::oCMobInter || rewind)) {
      if(!setAnim(nullptr,Anim::Out))
        return;
      auto prev = state;
      setState(state-1);
      loopState = (prev==state);
      }
    return;
    }

  if(p->user==nullptr && (state==-1 && !p->attachMode))
    return;
  if(p->user==nullptr && (state==stateNum && p->attachMode))
    return;

  if(isLadder() && p->started==Started && p->user!=nullptr && p->user->isAiQueueEmpty())
    return;
  implTick(*p);
  }

void Interactive::onKeyInput(KeyCodec::Action act) {
  if(world.tickCount()<waitAnim)
    return;

  Pos* p = nullptr;
  for(auto& i:attPos) {
    if(i.user!=nullptr) {
      p = &i;
      }
    }
  if(p==nullptr)
    return;

  auto& npc = *p->user;
  assert(npc.isPlayer());

  if(act==KeyCodec::ActionGeneric) {
    npc.setInteraction(nullptr);
    npc.stopAnim("");
    p->user = nullptr;
    return;
    }

  if(act!=KeyCodec::Forward && act!=KeyCodec::Back)
    return;

  reverseState = (act==KeyCodec::Back);
  implTick(*p);
  }

void Interactive::implTick(Pos& p) {
  if(p.user==nullptr)
    return;

  Npc&       npc    = *p.user;
  const bool attach = (p.attachMode^reverseState);

  if(p.started==NonStarted) {
    // STAND -> S0
    const bool omit = (!isLadder() && reverseState);
    if(!omit && !setAnim(&npc, Anim::FromStand)) {
      // some  mobsi have no animations in G2 - ignore them
      p.started    = Quit;
      p.attachMode = false;
      return;
      }

    if(attach)
      setState(std::min(stateNum,state+1));
    else if(!omit)
      setState(std::max(0,state-1));

    loopState = false;
    p.started = Started;
    }

  if(p.started==Quit) {
    npc.quitInteraction();
    loopState = false;
    p.user    = nullptr;
    p.started = NonStarted;
    return;
    }

  if(!loopState) {
    if(stateNum==state && attach) {
      invokeStateFunc(npc);
      loopState = true;
      }
    if(0==state && !p.attachMode) {
      invokeStateFunc(npc);
      loopState = true;
      }
    }

  if((state==0 && !attach) || (isLadder() && (attach && state>=stepsCount-1))) {
    implQuitInteract(p);
    return;
    }

  if(needToLockpick(npc)) {
    if(p.attachMode) {
      npc.world().sendPassivePerc(npc,npc,npc,PERC_ASSESSUSEMOB);
      return; // chest is locked - need to crack lock first
      }
    }

  Anim dir = (attach ? Anim::In : Anim::Out);
  if(state==stateNum && attach)
    dir = Anim::Active;

  if(!setAnim(&npc, dir))
    return;

  if(state==0 && p.attachMode) {
    npc.world().sendPassivePerc(npc,npc,npc,PERC_ASSESSUSEMOB);
    emitTriggerEvent();
    }

  if(npc.isPlayer() && !loopState) {
    invokeStateFunc(npc);
    }

  const int prev = state;
  if(attach)
    setState(std::min(stateNum,state+1)); else
    setState(std::max(0,state-1));
  loopState = (prev==state);
  }

void Interactive::implQuitInteract(Interactive::Pos &p) {
  if(p.user==nullptr)
    return;
  // S[i] -> STAND
  if(!setAnim(p.user, Anim::ToStand))
    return;
  // quit will be triggered with delay depend on animation
  p.started = Quit;
  }

std::string_view Interactive::tag() const {
  return vobName;
  }

std::string_view Interactive::focusName() const {
  return focName;
  }

bool Interactive::checkMobName(std::string_view dest) const {
  std::string_view scheme=schemeName();
  if(scheme==dest)
    return true;
  return false;
  }

std::string_view Interactive::ownerName() const {
  return owner;
  }

bool Interactive::overrideFocus() const {
  return focOver;
  }

Tempest::Vec3 Interactive::displayPosition() const {
  auto p = position();
  return p+displayOffset;
  }

std::string_view Interactive::displayName() const {
  if(focName.empty())
    return "";

  string_frm strId(focName);
  if(world.script().findSymbolIndex(strId)==size_t(-1))
    strId = string_frm("MOBNAME_",strId);

  if(world.script().findSymbolIndex(strId)==size_t(-1))
    return "";

  auto* s=world.script().findSymbol(strId);
  if(s==nullptr)
    return "";

  return s->get_string();
  }

const Tempest::Vec3* Interactive::bBox() const {
  return bbox;
  }

bool Interactive::setMobState(std::string_view scheme, int32_t st) {
  const bool ret = Vob::setMobState(scheme,st);
  if(state==st)
    return true;

  if(schemeName()!=scheme)
    return ret;

  string_frm name("S_S",st);
  if(visual.startAnimAndGet(name,world.tickCount())!=nullptr || !visual.isAnimExist(name)) {
    setState(st);
    return ret;
    }
  return false;
  }

void Interactive::invokeStateFunc(Npc& npc) {
  if(onStateFunc.empty() || state<0)
    return;
  if(loopState)
    return;

  string_frm func(onStateFunc,"_S",state);
  auto& sc = npc.world().script();
  sc.useInteractive(npc.handlePtr(), func);
  }

void Interactive::emitTriggerEvent() const {
  if(triggerTarget.empty())
    return;
  const TriggerEvent evt(triggerTarget,vobName,TriggerEvent::T_Activate);
  world.triggerEvent(evt);
  }

std::string_view Interactive::schemeName() const {
  if(auto mesh = visual.protoMesh())
    return mesh->scheme;
  Tempest::Log::i("unable to recognize mobsi{",focName,", ",mdlVisual,"}");
  return "";
  }

std::string_view Interactive::posSchemeName() const {
  for(auto& i:attPos)
    if(i.user!=nullptr) {
      return i.posTag();
      }
  return "";
  }

bool Interactive::isContainer() const {
  return vobType==phoenix::vob_type::oCMobContainer;
  }

bool Interactive::isLadder() const {
  return vobType==phoenix::vob_type::oCMobLadder;
  }

bool Interactive::needToLockpick(const Npc& pl) const {
  const size_t keyInst = keyInstance.empty() ? size_t(-1) : world.script().findSymbolIndex(keyInstance);
  if(keyInst!=size_t(-1) && pl.inventory().itemCount(keyInst)>0)
    return false;
  return !(pickLockStr.empty() || isLockCracked);
  }

Inventory &Interactive::inventory()  {
  return invent;
  }

uint32_t Interactive::stateMask() const {
  std::string_view s = schemeName();
  return world.script().schemeToBodystate(s);
  }

bool Interactive::canSeeNpc(const Npc& npc, bool freeLos) const {
  for(auto& i:attPos){
    auto pos = nodePosition(npc,i);
    if(npc.canSeeNpc(pos.x,pos.y,pos.z,freeLos))
      return true;
    }

  // graves
  if(attPos.size()==0){
    auto pos = displayPosition();

    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    if(npc.canSeeNpc(x,y,z,freeLos))
      return true;
    }
  return false;
  }

Tempest::Vec3 Interactive::nearestPoint(const Npc& to) const {
  if(auto p = findNearest(to))
    return worldPos(*p);
  return displayPosition();
  }

template<class P, class Inter>
P* Interactive::findNearest(Inter& in, const Npc& to) {
  float dist = 0;
  P*    p    = nullptr;
  for(auto& i:in.attPos) {
    if(i.user || !i.isAttachPoint())
      continue;
    float d = in.qDistanceTo(to,i);
    if(d<dist || p==nullptr) {
      p    = &i;
      dist = d;
      }
    }
  return p;
  }

const Interactive::Pos* Interactive::findNearest(const Npc& to) const {
  return findNearest<const Interactive::Pos, const Interactive>(*this,to);
  }

Interactive::Pos* Interactive::findNearest(const Npc& to) {
  return findNearest<Interactive::Pos, Interactive>(*this,to);
  }

void Interactive::implAddItem(std::string_view name) {
  size_t sep = name.find(':');
  if(sep!=std::string::npos) {
    auto itm = name.substr(0,sep);
    long count = std::strtol(name.data()+sep+1,nullptr,10);
    if(count>0)
      invent.addItem(itm,size_t(count),world);
    } else {
    invent.addItem(name,1,world);
    }
  }

void Interactive::autoDetachNpc() {
  for(auto& i:attPos)
    if(i.user!=nullptr) {
      if(i.user->world().aiIsDlgFinished())
        i.user->setInteraction(nullptr);
      }
  }

bool Interactive::checkUseConditions(Npc& npc) {
  const bool isPlayer = npc.isPlayer();

  auto& sc = npc.world().script();

  if(isPlayer) {
    const bool             g1             = Gothic::inst().version().game==1;
    const size_t           ItKE_lockpick  = world.script().lockPickId();
    const size_t           lockPickCnt    = npc.inventory().itemCount(ItKE_lockpick);
    const bool             canLockPick    = ((g1 || npc.talentSkill(TALENT_PICKLOCK)!=0) && lockPickCnt>0);

    const size_t           keyInst        = keyInstance.empty() ? size_t(-1) : world.script().findSymbolIndex(keyInstance);
    const bool             needToPicklock = (pickLockStr.size()>0);

    if(keyInst!=size_t(-1) && npc.itemCount(keyInst)>0)
      return true;
    if((canLockPick || isLockCracked) && needToPicklock)
      return true;

    if(keyInst!=size_t(-1) && needToPicklock) { // key+lockpick
      sc.printMobMissingKeyOrLockpick(npc);
      return false;
      }
    else if(keyInst!=size_t(-1)) { // key-only
      sc.printMobMissingKey(npc);
      return false;
      }
    else if(needToPicklock) { // lockpick only
      sc.printMobMissingLockpick(npc);
      return false;
      }

    if(!conditionFunc.empty()) {
      const int check = sc.invokeCond(npc,conditionFunc);
      if(check==0)
        return false;
      }

    if(!useWithItem.empty()) {
      size_t it = world.script().findSymbolIndex(useWithItem);
      if(it!=size_t(-1) && npc.itemCount(it)==0) {
        sc.printMobMissingItem(npc);
        return false;
        }
      }
    }
  return true;
  }

const Interactive::Pos *Interactive::findFreePos() const {
  for(auto& i:attPos)
    if(i.user==nullptr && i.isAttachPoint()) {
      return &i;
      }
  return nullptr;
  }

Interactive::Pos *Interactive::findFreePos() {
  for(auto& i:attPos)
    if(i.user==nullptr && i.isAttachPoint()) {
      return &i;
      }
  return nullptr;
  }

Tempest::Vec3 Interactive::worldPos(const Interactive::Pos &to) const {
  auto mesh = visual.protoMesh();
  if(mesh==nullptr)
    return Tempest::Vec3();

  auto mat  = transform();
  auto pos  = mesh->mapToRoot(to.node);
  mat.mul(pos);

  Tempest::Vec3 ret = {};
  mat.project(ret);
  return ret;
  }

bool Interactive::isAvailable() const {
  for(auto& i:attPos)
    if(i.user!=nullptr)
      return false;
  return findFreePos()!=nullptr;
  }

bool Interactive::isStaticState() const {
  for(auto& i:attPos)
    if(i.user!=nullptr) {
      if(i.attachMode && i.started==Started)
        return loopState;
      return false;
      }
  return loopState;
  }

bool Interactive::isDetachState(const Npc& npc) const {
  for(auto& i:attPos)
    if(i.user==&npc)
      return !(i.attachMode ^ reverseState);
  return !reverseState;
  }

bool Interactive::canQuitAtState(Npc& npc, int32_t state) const {
  if(state<0)
    return true;

  auto scheme = schemeName();
  auto pos    = posSchemeName();

  string_frm anim;
  if(pos.empty())
    anim = string_frm("T_",scheme,"_S",state,"_2_STAND"); else
    anim = string_frm("T_",scheme,"_",pos,"_S",state,"_2_STAND");

  if(npc.hasAnim(anim))
    return true;
  return state==stateNum && reverseState;
  }

bool Interactive::attach(Npc& npc, Interactive::Pos& to) {
  assert(to.user==nullptr);

  auto mat = nodeTranform(npc,to);
  float x=0, y=0, z=0;
  mat.project(x,y,z);

  Tempest::Vec3 mv = {x,y-npc.translateY(),z};
  if(!npc.testMove(mv)) {
    // FIXME: switches on stone-arks
    // return false;
    }

  if((npc.position()-mv).quadLength()>MAX_AI_USE_DISTANCE*MAX_AI_USE_DISTANCE) {
    if(npc.isPlayer()) {
      auto& sc = npc.world().script();
      sc.printMobTooFar(npc);
      }
    if(npc.isPlayer())
      return false; // TODO: same for npc
    }

  if(!checkUseConditions(npc))
    return false;

  if(!useWithItem.empty()) {
    size_t it = world.script().findSymbolIndex(useWithItem);
    npc.setCurrentItem(it);
    }

  setPos(npc,mv);
  setDir(npc,mat);

  if(vobType==phoenix::vob_type::oCMobLadder) {
    if(&to!=&attPos[0])
      state = -1; else
      state = stepsCount;
    loopState = false;
    }

  if(state>0) {
    reverseState = (state>0);
    } else {
    reverseState = false;
    state        = -1;
    }

  to.user       = &npc;
  to.started    = NonStarted;
  to.attachMode = true;
  return true;
  }

bool Interactive::attach(Npc &npc) {
  for(auto& i:attPos)
    if(i.user==&npc)
      return true;

  if(!isAvailable()) {
    if(npc.isPlayer() && !attPos.empty())
      world.script().printMobAnotherIsUsing(npc);
    return false;
    }

  auto p = findNearest(npc);
  if(p!=nullptr)
    return attach(npc,*p);

  if(npc.isPlayer() && !attPos.empty())
    world.script().printMobAnotherIsUsing(npc);
  return false;
  }

bool Interactive::detach(Npc &npc, bool quick) {
  for(auto& i:attPos) {
    if(i.user==&npc && i.attachMode) {
      if(quick || canQuitAtState(*i.user,state)) {
        if(!quick) {
          auto sq = npc.setAnimAngGet(Npc::Anim::InteractToStand);
          if(sq==nullptr)
            return false;
          }
        i.user       = nullptr;
        i.attachMode = false;
        npc.quitInteraction();
        return true;
        }
      else {
        i.attachMode = false;
        return false;
        }
      return false;
      }
    }

  return npc.interactive()==nullptr;
  }

bool Interactive::isAttached(const Npc& to) {
  for(auto& i:attPos)
    if(i.user==&to)
      return true;
  return false;
  }

void Interactive::setPos(Npc &npc,const Tempest::Vec3& pos) {
  npc.setPosition(pos);
  }

void Interactive::setDir(Npc &npc, const Tempest::Matrix4x4 &mat) {
  float x0=0,y0=0,z0=0;
  float x1=0,y1=0,z1=1;

  mat.project(x0,y0,z0);
  mat.project(x1,y1,z1);

  npc.setDirection(x1-x0,y1-y0,z1-z0);
  }

float Interactive::qDistanceTo(const Npc &npc, const Interactive::Pos &to) const {
  auto p = worldPos(to);
  return npc.qDistTo(p.x,p.y-npc.translateY(),p.z);
  }

Tempest::Matrix4x4 Interactive::nodeTranform(const Npc &npc, const Pos& p) const {
  auto mesh = visual.protoMesh();
  if(mesh==nullptr)
    return Tempest::Matrix4x4();

  auto nodeId = mesh->findNode(p.name);
  if(p.isDistPos()) {
    auto pos = position();
    Tempest::Matrix4x4 npos;
    if(nodeId!=size_t(-1)) {
      npos = visual.bone(nodeId);
      } else {
      npos.identity();
      }
    float nodeX = npos.at(3,0) - pos.x;
    float nodeY = npos.at(3,1) - pos.y;
    float nodeZ = npos.at(3,2) - pos.z;
    float dist  = std::sqrt(nodeX*nodeX + nodeZ*nodeZ);

    float npcX  = npc.position().x - pos.x;
    float npcZ  = npc.position().z - pos.z;
    float npcA  = 180.f*std::atan2(npcZ,npcX)/float(M_PI);

    npos.identity();
    npos.rotateOY(-npcA);
    npos.translate(dist,nodeY,0);
    npos.rotateOY(-90);

    float x = pos.x+npos.at(3,0);
    float y = pos.y+npos.at(3,1);
    float z = pos.z+npos.at(3,2);
    npos.set(3,0,x);
    npos.set(3,1,y);
    npos.set(3,2,z);
    return npos;
    }

  if(nodeId!=size_t(-1))
    return visual.bone(nodeId);

  return transform();
  }

Tempest::Vec3 Interactive::nodePosition(const Npc &npc, const Pos &p) const {
  auto  mat = nodeTranform(npc,p);
  float x   = mat.at(3,0);
  float y   = mat.at(3,1);
  float z   = mat.at(3,2);
  return {x,y,z};
  }

Tempest::Matrix4x4 Interactive::nodeTranform(std::string_view nodeName) const {
  auto mesh = visual.protoMesh();
  if(mesh==nullptr || mesh->skeleton==nullptr)
    return Tempest::Matrix4x4();

  auto id  = mesh->skeleton->findNode(nodeName);
  auto ret = transform();
  if(id!=size_t(-1))
    ret = visual.bone(id);
  return ret;
  }

const Animation::Sequence* Interactive::setAnim(Interactive::Anim t) {
  int  dir       = (t==Anim::Out || t==Anim::ToStand) ? -1 : 1;
  int  st[]      = {state,state+dir};
  char ss[2][12] = {};

  st[1] = std::max(0,std::min(st[1],stateNum));

  char buf[256]={};
  for(int i=0;i<2;++i) {
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"S0"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

  if(st[0]<0 || st[1]<0)
    std::snprintf(buf,sizeof(buf),"S_S0"); else
  if(st[0]==st[1])
    std::snprintf(buf,sizeof(buf),"S_%s",ss[0]); else
    std::snprintf(buf,sizeof(buf),"T_%s_2_%s",ss[0],ss[1]);

  return visual.startAnimAndGet(buf,world.tickCount());
  }

bool Interactive::setAnim(Npc* npc, Anim dir) {
  const Npc::Anim            dest = toNpcAnim(dir);
  const Animation::Sequence* sqNpc=nullptr;
  const Animation::Sequence* sqMob=nullptr;

  if(npc!=nullptr) {
    sqNpc = npc->setAnimAngGet(dest);
    // NOTE: Book-stand has no 'out' animation
    if(sqNpc==nullptr && !(vobType==phoenix::vob_type::oCMobInter && dir==Anim::Out))
      return false;
    }
  sqMob = setAnim(dir);
  if(sqMob==nullptr && sqNpc==nullptr && dir!=Anim::Out)
    return false;

  uint64_t aniT = sqNpc==nullptr ? 0 : uint64_t(sqNpc->totalTime());
  if(aniT==0) {
    /* Note: testing shows that in vanilla only npc animation maters.
     * testcase: chest animation
     * modsi timings only here for completness
     */
    aniT = sqMob==nullptr ? 0 : uint64_t(sqMob->totalTime());
    }

  if(dir!=Anim::Active)
    waitAnim = world.tickCount()+aniT;
  return true;
  }

void Interactive::setState(int st) {
  state = st;
  onStateChanged();
  }

const Animation::Sequence* Interactive::animNpc(const AnimationSolver &solver, Anim t) {
  std::string_view tag      = schemeName();
  int              st[]     = {state,state+t};
  string_frm<12>   ss[2]    = {};
  string_frm       pointBuf = {};
  string_frm       point    = {};

  if(t==Anim::FromStand) {
    st[0] = -1;
    st[1] = state<1 ? 0 : stateNum - 1;
    }
  else if(t==Anim::ToStand) {
    st[0] = state<1 ? 0 : stateNum - 1;
    st[1] = -1;
    }

  for(auto& i:attPos)
    if(i.user!=nullptr) {
      point = string_frm("_",i.posTag());
      }

  st[1] = std::max(-1,std::min(st[1],stateNum));

  for(int i=0;i<2;++i) {
    if(st[i]<0)
      ss[i] = "STAND"; else
      ss[i] = string_frm("S",st[i]);
    }

  string_frm buf = {};
  for(auto pt:{std::string_view(point),std::string_view()}) {
    if(st[0]==st[1])
      buf = string_frm("S_",tag,pt,ss[0]); else
      buf = string_frm("T_",tag,pt,"_",ss[0],"_2_",ss[1]);
    if(auto ret = solver.solveFrm(buf))
      return ret;
    }
  return nullptr;
  }

void Interactive::marchInteractives(DbgPainter &p) const {
  p.setBrush(Tempest::Color(1.0,0,0,1));

  for(auto& m:attPos) {
    auto pos = worldPos(m);

    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    p.mvp.project(x,y,z);

    x = (0.5f*x+0.5f)*float(p.w);
    y = (0.5f*y+0.5f)*float(p.h);

    p.painter.drawRect(int(x),int(y),1,1);
    p.drawText(int(x), int(y), schemeName().data());
    }

  if(attPos.size()==0) {
    auto pos = displayPosition();

    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    p.mvp.project(x,y,z);

    x = (0.5f*x+0.5f)*float(p.w);
    y = (0.5f*y+0.5f)*float(p.h);

    p.painter.drawRect(int(x),int(y),1,1);
    p.drawText(int(x), int(y), schemeName().data());
    }
  }

void Interactive::moveEvent() {
  Vob::moveEvent();
  visual.setObjMatrix(transform());
  }

float Interactive::extendedSearchRadius() const {
  float x = bbox[1].x-bbox[0].x;
  float y = bbox[1].y-bbox[0].y;
  float z = bbox[1].z-bbox[0].z;

  return std::max(x,std::max(y,z));
  }

std::string_view Interactive::Pos::posTag() const {
  if(name.rfind("_FRONT")==name.size()-6)
    return "FRONT";
  if(name.rfind("_BACK")==name.size()-5)
    return "BACK";
  return "";
  }

bool Interactive::Pos::isAttachPoint() const {
  /*
    ZS_POS0, ZS_POS0_FRONT, ZS_POS0_BACK, ZS_POS0_DIST
    ZS_POS1, ZS_POS1_FRONT, ZS_POS1_BACK,
    ZS_POS2, ZS_POS3, ...
  */
  return name.find("ZS_POS")==0;
  }

bool Interactive::Pos::isDistPos() const {
  return name.rfind("_DIST")==name.size()-5;
  }
