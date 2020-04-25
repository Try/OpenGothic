#include "interactive.h"
#include "npc.h"
#include "world.h"
#include "utils/fileext.h"
#include "game/serialize.h"
#include "graphics/pose.h"

#include <Tempest/Painter>
#include <Tempest/Log>


Interactive::Interactive(World &world, ZenLoad::zCVobData&& vob)
  :world(&world),skInst(std::make_unique<Pose>()) {
  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));

  vobType       = vob.vobType;
  vobName       = std::move(vob.vobName);
  focName       = std::move(vob.oCMOB.focusName);
  mdlVisual     = std::move(vob.visual);
  bbox[0]       = vob.bbox[0];
  bbox[1]       = vob.bbox[1];
  owner         = std::move(vob.oCMOB.owner);
  focOver       = vob.oCMOB.focusOverride;
  showVisual    = vob.showVisual;

  stateNum      = vob.oCMobInter.stateNum;
  triggerTarget = std::move(vob.oCMobInter.triggerTarget);
  useWithItem   = std::move(vob.oCMobInter.useWithItem);
  conditionFunc = std::move(vob.oCMobInter.conditionFunc);
  onStateFunc   = std::move(vob.oCMobInter.onStateFunc);
  rewind        = std::move(vob.oCMobInter.rewind);

  pos           = Tempest::Matrix4x4(v);

  for(auto& i:owner)
    i = char(std::toupper(i));

  if(isContainer()) {
    locked      = vob.oCMobContainer.locked;
    keyInstance = std::move(vob.oCMobContainer.keyInstance);
    pickLockStr = std::move(vob.oCMobContainer.pickLockStr);
    auto items  = std::move(vob.oCMobContainer.contains);
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
  setVisual(mdlVisual);
  }

Interactive::Interactive(World &world)
  :world(&world),skInst(std::make_unique<Pose>()) {
  }

void Interactive::load(Serialize &fin) {
  uint8_t vt=0;
  fin.read(vt,vobName,focName,mdlVisual);
  vobType = ZenLoad::zCVobData::EVobType(vt);
  fin.read(bbox[0].x,bbox[0].y,bbox[0].z,bbox[1].x,bbox[1].y,bbox[1].z,owner);
  if(fin.version()>=2)
    fin.read(focOver);
  if(fin.version()>=6)
    fin.read(showVisual);

  fin.read(stateNum,triggerTarget,useWithItem,conditionFunc,onStateFunc);
  fin.read(locked,keyInstance,pickLockStr);
  invent.load(*this,*world,fin);
  fin.read(pos,state,reverseState,loopState);

  setVisual(mdlVisual);

  uint32_t sz=0;
  fin.read(sz);
  for(size_t i=0;i<sz;++i){
    std::string name;
    Npc*        user=nullptr;
    int32_t     userState=0; // unused
    bool        attachMode=false;

    fin.read(name,user,userState,attachMode);
    for(auto& i:attPos)
      if(i.name==name) {
        i.user       = user;
        i.attachMode = attachMode;
        if(i.user!=nullptr)
          i.user->setInteraction(this,true);
        }
    }
  }

void Interactive::save(Serialize &fout) const {
  fout.write(uint8_t(vobType),vobName,focName,mdlVisual);
  fout.write(bbox[0].x,bbox[0].y,bbox[0].z,bbox[1].x,bbox[1].y,bbox[1].z);
  fout.write(owner,focOver,showVisual);
  fout.write(stateNum,triggerTarget,useWithItem,conditionFunc,onStateFunc);
  fout.write(locked,keyInstance,pickLockStr);
  invent.save(fout);
  fout.write(pos,state,reverseState,loopState);

  fout.write(uint32_t(attPos.size()));
  for(auto& i:attPos) {
    int32_t userState=0; // unused
    fout.write(i.name,i.user,userState,i.attachMode);
    }
  }

void Interactive::setVisual(const std::string &visual) {
  if(!FileExt::hasExt(visual,"3ds"))
    skeleton = Resources::loadSkeleton(visual.c_str());
  mesh = Resources::loadMesh(visual);

  if(mesh) {
    if(showVisual) {
      animChanged = true;
      view   = world->getView(visual.c_str());
      physic = PhysicMesh(*mesh,*world->physic());
      }

    view  .setSkeleton (skeleton);
    view  .setObjMatrix(pos);

    physic.setSkeleton (skeleton);
    physic.setObjMatrix(pos);

    attPos.resize(mesh->pos.size());
    for(size_t i=0;i<attPos.size();++i){
      attPos[i].name = mesh->pos[i].name;
      attPos[i].pos  = mesh->pos[i].transform;
      attPos[i].node = mesh->pos[i].node;
      }
    }

  if(mesh!=nullptr && skeleton!=nullptr) {
    solver.setSkeleton (skeleton);
    skInst->setFlags(Pose::NoTranslation);
    skInst->setSkeleton(skeleton);
    setAnim(Interactive::Active); // setup default anim
    }
  }

void Interactive::updateAnimation() {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world->tickCount();

  solver.update(tickCount);
  animChanged = pose.update(solver,0,tickCount);

  view.setPose(pose,pos);
  }

void Interactive::tick(uint64_t dt) {
  if(animChanged) {
    physic.setPose(*skInst,pos);
    animChanged = false;
    }

  Pos* p = nullptr;
  for(auto& i:attPos) {
    if(i.user!=nullptr) {
      p = &i;
      }
    }

  if(p==nullptr)
    return;

  if(world->tickCount()<waitAnim)
    return;

  if(p->user==nullptr && (state==-1 && !p->attachMode))
    return;
  if(p->user==nullptr && (state==stateNum && p->attachMode))
    return;
  implTick(*p,dt);
  }

void Interactive::implTick(Pos& p, uint64_t /*dt*/) {
  Npc* npc = p.user;

  if(!p.started) {
    // STAND -> S0
    if(npc!=nullptr) {
      auto sq = npc->setAnimAngGet(Npc::Anim::InteractFromStand,false);
      uint64_t t = sq==nullptr ? 0 : uint64_t(sq->totalTime());
      waitAnim = world->tickCount()+t;
      }
    p.started = true;
    return;
    }

  if(p.attachMode^reverseState) {
    if(state==stateNum) {
      //HACK: some beds in game are VT_oCMobDoor
      if(canQuitAtLastState()) {
        implQuitInteract(p);
        return;
        }
      }
    } else {
    if(state==0) {
      implQuitInteract(p);
      return;
      }
    }

  if((p.attachMode^reverseState) && state==stateNum){
    if(!setAnim(npc,Anim::Active))
      return;
    }
  else if(p.attachMode) {
    if(!setAnim(npc,Anim::In))
      return;
    }
  else {
    if(!setAnim(npc,Anim::Out))
      return;
    }

  if(state==0 && p.attachMode) {
    if(npc!=nullptr) // player only emitter?
      npc->world().sendPassivePerc(*npc,*npc,*npc,Npc::PERC_ASSESSUSEMOB);
    emitTriggerEvent();
    }

  if(npc!=nullptr && npc->isPlayer() && !loopState) {
    invokeStateFunc(*npc);
    }

  int  prev = state;
  if(p.attachMode^reverseState) {
    state = std::min(stateNum,state+1);
    } else {
    state = std::max(0,state-1);
    }
  loopState = (prev==state);
  }

void Interactive::implQuitInteract(Interactive::Pos &p) {
  Npc* npc = p.user;
  if(npc==nullptr || !npc->isPlayer() || npc->world().aiIsDlgFinished()) {
    if(npc!=nullptr) {
      const Animation::Sequence* sq = nullptr;
      if(state==0) {
        // S0 -> STAND
        sq = npc->setAnimAngGet(Npc::Anim::InteractToStand,false);
        }
      if(sq==nullptr && !npc->setAnim(Npc::Anim::Idle))
        return;
      npc->quitIneraction();
      }
    p.user      = nullptr;
    loopState   = false;
    }
  }

const std::string &Interactive::tag() const {
  return vobName;
  }

const std::string& Interactive::focusName() const {
  return focName;
  }

bool Interactive::checkMobName(const char* dest) const {
  const char* scheme=schemeName();
  if(strcmp(scheme,dest)==0)
    return true;
  return false;
  }

const std::string &Interactive::ownerName() const {
  return owner;
  }

bool Interactive::overrideFocus() const {
  return focOver;
  }

Tempest::Vec3 Interactive::position() const {
  float x=0,y=0,z=0;
  pos.project(x,y,z);
  return {x,y,z};
  }

Tempest::Vec3 Interactive::displayPosition() const {
  auto p = position();
  return {p.x,bbox[1].y,p.z};
  }

const char *Interactive::displayName() const {
  if(focName.empty())
    return "";
  const char* strId=focName.c_str();
  if(world->getSymbolIndex(strId)==size_t(-1)) {
    return vobName.c_str();
    }
  auto& s=world->getSymbol(strId);
  const char* txt = s.getString(0).c_str();
  if(std::strlen(txt)==0)
    txt="";
  return txt;
  }

void Interactive::invokeStateFunc(Npc& npc) {
  if(onStateFunc.empty() || state<0)
    return;
  char func[256]={};
  std::snprintf(func,sizeof(func),"%s_S%d",onStateFunc.c_str(),state);

  auto& sc = npc.world().script();
  sc.useInteractive(npc.handle(),func);
  }

void Interactive::emitTriggerEvent() const {
  if(triggerTarget.empty())
    return;
  const TriggerEvent evt(triggerTarget,vobName);
  world->triggerEvent(evt);
  }

const char* Interactive::schemeName() const {
  if(mesh!=nullptr)
    return mesh->scheme.c_str();
  Tempest::Log::i("unable to recognize mobsi{",focName,", ",mdlVisual,"}");
  return "";
  }

bool Interactive::isContainer() const {
  return vobType==ZenLoad::zCVobData::VT_oCMobContainer;
  }

Inventory &Interactive::inventory()  {
  return invent;
  }

uint32_t Interactive::stateMask() const {
  const char* s = schemeName();
  return world->script().schemeToBodystate(s);
  }

bool Interactive::canSeeNpc(const Npc& npc, bool freeLos) const {
  for(auto& i:attPos){
    auto pos = nodePosition(npc,i);
    if(npc.canSeeNpc(pos.x,pos.y,pos.z,freeLos))
      return true;
    }

  // graves
  if(attPos.size()==0){
    auto mat = pos;

    float x = mat.at(3,0);
    float y = mat.at(3,1);
    float z = mat.at(3,2);
    if(npc.canSeeNpc(x,y,z,freeLos))
      return true;
    }
  return false;
  }

void Interactive::implAddItem(char *name) {
  char* sep = std::strchr(name,':');
  if(sep!=nullptr) {
    *sep='\0';++sep;
    long count = std::strtol(sep,nullptr,10);
    if(count>0)
      invent.addItem(name,uint32_t(count),*world);
    } else {
    invent.addItem(name,1,*world);
    }
  }

void Interactive::autoDettachNpc() {
  for(auto& i:attPos)
    if(i.user!=nullptr) {
      if(i.user->world().aiIsDlgFinished())
        i.user->setInteraction(nullptr);
      }
  }

bool Interactive::checkUseConditions(Npc& npc) {
  const bool isPlayer = npc.isPlayer();

  auto& sc = npc.world().script();
  if(isPlayer && !conditionFunc.empty()) {
    const int check = sc.invokeCond(npc,conditionFunc.c_str());
    if(check==0) {
      // FIXME: proper message
      sc.printNothingToGet();
      return false;
      }
    }
  if(!useWithItem.empty()) {
    size_t it = world->getSymbolIndex(useWithItem.c_str());
    if(it!=size_t(-1)) {
      if(isPlayer && npc.hasItem(it)==0) {
        sc.printMobMissingItem(npc);
        return false;
        }
      // forward here, if this is not a player
      npc.delItem(it,1);
      npc.setCurrentItem(it);
      }
    }
  if(isPlayer && !keyInstance.empty()) {
    size_t it = world->getSymbolIndex(keyInstance.c_str());
    if(it!=size_t(-1) && npc.hasItem(it)==0) {
      sc.printMobMissingKey(npc);
      return false;
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
  auto mat = pos;
  auto pos = mesh->mapToRoot(to.node);
  mat.mul(pos);

  float x=0, y=0, z=0;

  mat.project(x,y,z);
  return {x,y,z};
  }

bool Interactive::isAvailable() const {
  return findFreePos()!=nullptr;
  }

bool Interactive::isStaticState() const {
  for(auto& i:attPos)
    if(i.user!=nullptr) {
      if(i.attachMode)
        return loopState;
      return false;
      }
  return loopState;
  }

bool Interactive::canQuitAtLastState() const {
  return (vobType==ZenLoad::zCVobData::VT_oCMobDoor && onStateFunc.empty()) ||
          vobType==ZenLoad::zCVobData::VT_oCMobSwitch ||
          reverseState;
  }

bool Interactive::attach(Npc &npc, Interactive::Pos &to) {
  assert(to.user==nullptr);

  if(!checkUseConditions(npc))
    return false;

  auto mat = nodeTranform(npc,to);

  float x=0, y=0, z=0;
  mat.project(x,y,z);

  Tempest::Vec3 mv = {x,y-npc.translateY(),z}, fallback={};
  if(!npc.testMove(mv,fallback,0)) {
    // FIXME: switches on stone-arks
    // return false;
    }

  setPos(npc,mv);
  setDir(npc,mat);

  if(state>0) {
    reverseState = (state>0);
    } else {
    reverseState = false;
    state = 0;
    }

  to.user       = &npc;
  to.attachMode = true;
  to.started    = false;
  return true;
  }

bool Interactive::attach(Npc &npc) {
  float dist = 0;
  Pos*  p    = nullptr;

  for(auto& i:attPos)
    if(i.user==&npc)
      return true;

  for(auto& i:attPos) {
    if(i.user || !i.isAttachPoint())
      continue;
    float d = qDistanceTo(npc,i);
    if(d<dist || p==nullptr) {
      p    = &i;
      dist = d;
      }
    }

  if(p!=nullptr) {
    return attach(npc,*p);
    } else {
    if(npc.isPlayer() && attPos.size()>0)
      world->script().printMobAnotherIsUsing(npc);
    }
  return false;
  }

bool Interactive::dettach(Npc &npc, bool quick) {
  for(auto& i:attPos) {
    if(i.user==&npc) {
      if(quick) {
        i.user = nullptr;
        i.attachMode = false;
        if(reverseState)
          state = stateNum; else
          state = 0;
        npc.quitIneraction();
        } else {
        i.attachMode = false;
        return true;
        }
      }
    }
  return true;
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

float Interactive::qDistanceTo(const Npc &npc, const Interactive::Pos &to) {
  auto p = worldPos(to);
  return npc.qDistTo(p.x,p.y-npc.translateY(),p.z);
  }

Tempest::Matrix4x4 Interactive::nodeTranform(const Npc &npc, const Pos& p) const {
  auto npos = mesh->mapToRoot(p.node);

  if(p.isDistPos()) {
    float nodeX = npos.at(3,0);
    float nodeY = npos.at(3,1);
    float nodeZ = npos.at(3,2);
    float dist  = std::sqrt(nodeX*nodeX + nodeZ*nodeZ);

    float npcX  = npc.position().x - pos.at(3,0);
    float npcZ  = npc.position().z - pos.at(3,2);
    float npcA  = 180.f*std::atan2(npcZ,npcX)/float(M_PI);

    npos.identity();
    npos.rotateOY(-npcA);
    npos.translate(dist,nodeY,0);
    npos.rotateOY(-90);

    float x = pos.at(3,0)+npos.at(3,0);
    float y = pos.at(3,1)+npos.at(3,1);
    float z = pos.at(3,2)+npos.at(3,2);
    npos.set(3,0,x);
    npos.set(3,1,y);
    npos.set(3,2,z);
    return npos;
    }

  auto mat = pos;
  mat.mul(npos);
  return mat;
  }

Tempest::Vec3 Interactive::nodePosition(const Npc &npc, const Pos &p) const {
  auto  mat = nodeTranform(npc,p);
  float x   = mat.at(3,0);
  float y   = mat.at(3,1);
  float z   = mat.at(3,2);
  return {x,y,z};
  }

const Animation::Sequence* Interactive::setAnim(Interactive::Anim t) {
  int  st[]     = {state,state+(reverseState ? -t : t)};
  char ss[2][12] = {};

  st[1] = std::max(0,std::min(st[1],stateNum));

  char buf[256]={};
  for(int i=0;i<2;++i){
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"S0"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

  if(st[0]<0 || st[1]<0)
    std::snprintf(buf,sizeof(buf),"S_S0"); else
  if(st[0]==st[1])
    std::snprintf(buf,sizeof(buf),"S_%s",ss[0]); else
    std::snprintf(buf,sizeof(buf),"T_%s_2_%s",ss[0],ss[1]);

  auto sq = solver.solveFrm(buf);
  if(sq) {
    if(skInst->startAnim(solver,sq,BS_NONE,Pose::NoHint,world->tickCount()))
      return sq;
    }
  return nullptr;
  }

bool Interactive::setAnim(Npc* npc,Anim dir) {
  Npc::Anim                  dest = dir==Anim::Out ? Npc::Anim::InteractOut : Npc::Anim::InteractIn;
  const Animation::Sequence* sqNpc=nullptr;
  const Animation::Sequence* sqMob=nullptr;
  if(npc!=nullptr) {
    sqNpc = npc->setAnimAngGet(dest,false);
    if(sqNpc==nullptr && dir!=Anim::Out)
      return false;
    }
  sqMob = setAnim(dir);
  if(sqMob==nullptr && sqNpc==nullptr && dir!=Anim::Out)
    return false;

  uint64_t t0 = sqNpc==nullptr ? 0 : uint64_t(sqNpc->totalTime());
  uint64_t t1 = sqMob==nullptr ? 0 : uint64_t(sqMob->totalTime());
  if(dir!=Anim::Active)
    waitAnim = world->tickCount()+std::max(t0,t1);
  return true;
  }

const Animation::Sequence* Interactive::animNpc(const AnimationSolver &solver, Anim t) {
  const char* tag      = schemeName();
  int         st[]     = {state,state+(reverseState ? -t : t)};
  char        ss[2][12] = {};
  const char* point    = "";

  if(t==Anim::FromStand) {
    st[0] = -1;
    st[1] = 0;
    }
  else if(t==Anim::ToStand) {
    st[0] = 0;
    st[1] = -1;
    }

  for(auto& i:attPos)
    if(i.user!=nullptr) {
      point = i.posTag();
      }

  st[1] = std::max(-1,std::min(st[1],stateNum));

  char buf[256]={};
  for(int i=0;i<2;++i){
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"STAND"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

  for(auto pt:{point,""}) {
    if(st[0]==st[1])
      std::snprintf(buf,sizeof(buf),"S_%s%s_%s",tag,pt,ss[0]); else
      std::snprintf(buf,sizeof(buf),"T_%s%s_%s_2_%s",tag,pt,ss[0],ss[1]);
    if(auto ret = solver.solveFrm(buf))
      return ret;
    }
  return nullptr;
  }

void Interactive::marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
  p.setBrush(Tempest::Color(1.0,0,0,1));

  for(auto& m:attPos){
    auto pos = worldPos(m);

    float x = pos.x;
    float y = pos.y;
    float z = pos.z;
    mvp.project(x,y,z);

    x = (0.5f*x+0.5f)*float(w);
    y = (0.5f*y+0.5f)*float(h);

    p.drawRect(int(x),int(y),1,1);
    }
  }

const char *Interactive::Pos::posTag() const {
  if(name.rfind("_FRONT")==name.size()-6)
    return "_FRONT";
  if(name.rfind("_BACK")==name.size()-5)
    return "_BACK";
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
