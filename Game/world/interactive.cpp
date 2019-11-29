#include "interactive.h"
#include "npc.h"
#include "world.h"
#include "utils/fileext.h"

#include <Tempest/Painter>
#include <Tempest/Log>

Interactive::Interactive(World &owner, const ZenLoad::zCVobData &vob)
  :world(&owner),data(vob),skInst(std::make_unique<Pose>()) {
  if(!FileExt::hasExt(vob.visual,"3ds"))
    skeleton = Resources::loadSkeleton(vob.visual);
  mesh = Resources::loadMesh(vob.visual);

  if(mesh) {
    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));
    pos = Tempest::Matrix4x4(v);

    auto physicMesh = Resources::physicMesh(mesh); //FIXME: build physic model in Resources.cpp

    // view   = owner.getStaticView(vob.visual,0);
    view   = owner.getView(vob.visual);
    physic = owner.physic()->staticObj(physicMesh,pos);

    view  .setObjMatrix(pos);
    physic.setObjMatrix(pos);
    view  .setAttachPoint(skeleton);

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

  for(auto& i:data.oCMOB.owner)
    i = char(std::toupper(i));
  }

void Interactive::updateAnimation() {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world->tickCount();

  solver.update(tickCount);
  pose.update(solver,tickCount);
  view .setSkeleton(pose,pos);
  }

void Interactive::tick(uint64_t dt) {
  Pos* p = nullptr;
  for(auto& i:attPos) {
    if(i.user!=nullptr) {
      p = &i;
      }
    }

  if(p==nullptr)
    return;

  if(p->user==nullptr && (state==-1 && !p->attachMode))
    return;
  if(p->user==nullptr && (state==data.oCMobInter.stateNum && p->attachMode))
    return;
  implTick(*p,dt);
  }

void Interactive::implTick(Pos& p, uint64_t /*dt*/) {
  Npc* npc = p.user;
  if(p.attachMode && loopState){
    if(!setAnim(Anim::Active))
      return;
    if(npc!=nullptr)
      npc->setAnim(Npc::Anim::InteractIn);
    }
  else if(p.attachMode) {
    if(!loopState && npc!=nullptr && !npc->setAnim(Npc::Anim::InteractIn))
      return;
    if(!setAnim(Anim::In))
      return;
    } else {
    if(npc!=nullptr)
      npc->setAnim(Npc::Anim::InteractOut);
    if(!setAnim(Anim::Out))
      return;
    }

  if(p.userState==-1 && p.attachMode) {
    p.userState=0;
    if(npc!=nullptr)
      npc->world().sendPassivePerc(*npc,*npc,*npc,Npc::PERC_ASSESSUSEMOB);
    emitTriggerEvent();
    }

  if(npc!=nullptr && npc->isPlayer() && !loopState) {
    invokeStateFunc(*npc);
    }

  bool finalState = false;
  int  prev       = state;
  if(p.attachMode^reverseState) {
    state = std::min(data.oCMobInter.stateNum,state+1);
    if(state==data.oCMobInter.stateNum)
      finalState = true;
    } else {
    state = std::max(-1,state-1);
    if(state==-1)
      finalState = true;
    }
  p.userState = state;
  loopState   = (prev==state);

  if(!finalState && prev==state)
    return;

  if(state==-1) {
    implQuitInteract(p);
    return;
    }
  if(state==data.oCMobInter.stateNum) {
    //HACK: some beds in game is VT_oCMobDoor
    if((data.vobType==ZenLoad::zCVobData::VT_oCMobDoor && data.oCMobInter.onStateFunc.empty()) ||
        data.vobType==ZenLoad::zCVobData::VT_oCMobSwitch){
      implQuitInteract(p);
      return;
      }
    }
  }

void Interactive::implQuitInteract(Interactive::Pos &p) {
  Npc* npc = p.user;
  if(npc==nullptr || !npc->isPlayer() || npc->world().aiIsDlgFinished()) {
    if(npc!=nullptr) {
      if(!npc->setAnim(Npc::Anim::Idle))
        return;
      npc->quitIneraction();
      }
    p.user      = nullptr;
    p.userState = -1;
    }
  }

const std::string &Interactive::tag() const {
  return data.vobName;
  }

const std::string& Interactive::focusName() const {
  return data.oCMOB.focusName;
  }

bool Interactive::checkMobName(const std::string &dest) const {
  const char* scheme=schemeName();
  if(scheme==dest)
    return true;
  return false;
  }

const std::string &Interactive::ownerName() const {
  return data.oCMOB.owner;
  }

std::array<float,3> Interactive::position() const {
  float x=data.position.x,
        y=data.position.y,
        z=data.position.z;
  return {x,y,z};
  }

std::array<float,3> Interactive::displayPosition() const {
  auto p = position();
  p[1]+=(data.bbox[1].y-data.bbox[0].y)-mesh->rootTr[1];
  return p;
  }

const char *Interactive::displayName() const {
  if(data.oCMOB.focusName.empty())
    return "";
  const char* strId=data.oCMOB.focusName.c_str();
  if(world->getSymbolIndex(strId)==size_t(-1)){
    return data.vobName.c_str();
    }
  auto& s=world->getSymbol(strId);
  const char* txt = s.getString(0).c_str();
  if(std::strlen(txt)==0)
    txt="";
  return txt;
  }

void Interactive::invokeStateFunc(Npc& npc) {
  if(data.oCMobInter.onStateFunc.empty() || state<0)
    return;
  char func[256]={};
  std::snprintf(func,sizeof(func),"%s_S%d",data.oCMobInter.onStateFunc.c_str(),state);

  auto& sc = npc.world().script();
  sc.useInteractive(npc.handle(),func);
  }

void Interactive::emitTriggerEvent() const {
  if(data.oCMobInter.triggerTarget.empty())
    return;
  const TriggerEvent evt(data.oCMobInter.triggerTarget,data.vobName);
  world->triggerEvent(evt);
  }

const char *Interactive::schemeName() const {
  const char* tag = "";
  if(data.oCMOB.focusName=="MOBNAME_BENCH")
    tag = "BENCH";
  else if(data.oCMOB.focusName=="MOBNAME_ANVIL")
    tag = "BSANVIL";
  else if(data.oCMOB.focusName=="MOBNAME_LAB")
    tag = "LAB";
  else if(data.oCMOB.focusName=="MOBNAME_CHEST" || data.oCMOB.focusName=="Chest")
    tag = "CHESTSMALL";
  else if(data.oCMOB.focusName=="MOBNAME_CHESTBIG")
    tag = "CHESTBIG";
  else if(data.oCMOB.focusName=="MOBNAME_FORGE")
    tag = "BSFIRE";
  else if(data.oCMOB.focusName=="MOBNAME_BOOKSBOARD")
    tag = "BOOK";
  else if(data.oCMOB.focusName=="MOBNAME_BBQ_SCAV" || data.oCMOB.focusName=="MOBNAME_BARBQ_SCAV")
    tag = "BARBQ";
  else if(data.oCMOB.focusName=="MOBNAME_SWITCH" || data.oCMOB.focusName=="MOBNAME_ADDON_ORNAMENTSWITCH")
    tag = "TURNSWITCH";
  else if(data.oCMOB.focusName=="MOBNAME_CHAIR")
    tag = "CHAIR";
  else if(data.oCMOB.focusName=="MOBNAME_THRONE" || data.oCMOB.focusName=="MOBNAME_SEAT" || data.oCMOB.focusName=="MOBNAME_ARMCHAIR")
    tag = "THRONE";
  else if(data.oCMOB.focusName=="MOBNAME_CAULDRON")
    tag = "CAULDRON";
  else if(data.oCMOB.focusName=="MOBNAME_ORE")
    tag = "ORE";
  else if(data.oCMOB.focusName=="MOBNAME_GRINDSTONE")
    tag = "BSSHARP";
  else if(data.oCMOB.focusName=="MOBNAME_INNOS")
    tag = "INNOS";
  else if(data.oCMOB.focusName=="MOBNAME_ADDON_IDOL")
    tag = "INNOS";//"IDOL";
  else if(data.oCMOB.focusName=="MOBNAME_STOVE")
    tag = "STOVE";
  else if(data.oCMOB.focusName=="MOBNAME_BED")
    tag = "BEDHIGH";
  else if(data.oCMOB.focusName=="MOBNAME_BUCKET")
    tag = "BSCOOL";
  else if(data.oCMOB.focusName=="MOBNAME_RUNEMAKER")
    tag = "RMAKER";
  else if(data.oCMOB.focusName=="MOBNAME_WATERPIPE")
    tag = "SMOKE";
  else if(data.oCMOB.focusName=="MOBNAME_SAW")
    tag = "BAUMSAEGE";
  else if(data.oCMOB.focusName=="MOBNAME_PAN")
    tag = "PAN";
  else if(data.oCMOB.focusName=="MOBNAME_DOOR" || data.oCMOB.focusName=="MOBNAME_Door")
    tag = "DOOR";
  else if(data.oCMOB.focusName=="MOBNAME_WINEMAKER")
    tag = "HERB";
  else if(data.oCMOB.focusName=="MOBNAME_BOOKSTAND")
    tag = "BOOK";
  else if(data.visual=="TREASURE_ADDON_01.ASC")
    tag = "TREASURE";
  else if(data.visual=="LEVER_1_OC.MDS")
    tag = "LEVER";
  else if(data.visual=="REPAIR_PLANK.ASC")
    tag = "REPAIR";
  else if(data.visual=="BENCH_NW_CITY_02.ASC")
    tag = "BENCH";
  else if(data.visual=="PAN_OC.MDS")
    tag = "PAN";
  else {
    // Tempest::Log::i("unable to recognize mobsi{",data.oCMOB.focusName,", ",data.visual,"}");
    }
  return tag;
  }

bool Interactive::isContainer() const {
  return data.vobType==ZenLoad::zCVobData::VT_oCMobContainer;
  }

Inventory &Interactive::inventory()  {
  if(!isContainer())
    return invent;
  auto  items = std::move(data.oCMobContainer.contains);
  if(items.size()==0)
    return invent;
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
  return invent;
  }

uint32_t Interactive::stateMask(uint32_t orig) const {
  static const char* MOB_SIT[]   = {"BENCH","CHAIR","GROUND","THRONE"};
  static const char* MOB_LIE[]   = {"BED","BEDHIGH","BEDLOW"};
  static const char* MOB_CLIMB[] = {"CLIMB","LADDER","RANKE"};
  static const char* MOB_NOTINTERRUPTABLE[] =
     {"DOOR","LEVER","TOUCHPLATE","TURNSWITCH","VWHEEL","CHESTBIG","CHESTSMALL","HERB","IDOL","PAN","SMOKE","INNOS"};
  // TODO: fetch MOB_* from script
  const char* s = schemeName();
  for(auto i:MOB_SIT){
    if(std::strcmp(i,s)==0)
      return BS_SIT;
    }
  for(auto i:MOB_LIE){
    if(std::strcmp(i,s)==0)
      return BS_LIE;
    }
  for(auto i:MOB_CLIMB){
    if(std::strcmp(i,s)==0)
      return BS_CLIMB;
    }
  for(auto i:MOB_NOTINTERRUPTABLE){
    if(std::strcmp(i,s)==0)
      ;//return BS_MOBINTERACT_INTERRUPT;
    }
  return orig;
  }

bool Interactive::canSeeNpc(const Npc& npc, bool freeLos) const {
  for(auto& i:attPos){
    auto mat = pos;
    auto pos = mesh->mapToRoot(i.node);
    mat.mul(pos);

    float x = mat.at(3,0);
    float y = mat.at(3,1);
    float z = mat.at(3,2);
    if(npc.canSeeNpc(x,y,z,freeLos))
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

std::array<float,3> Interactive::worldPos(const Interactive::Pos &to) const {
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

bool Interactive::attach(Npc &npc, Interactive::Pos &to) {
  assert(to.user==nullptr);

  auto mat = pos;
  auto pos = mesh->mapToRoot(to.node);
  mat.mul(pos);

  float x=0, y=0, z=0;

  mat.project(x,y,z);

  std::array<float,3> mv = {x,y-npc.translateY(),z}, fallback={};
  if(!npc.testMove(mv,fallback,0))
    return false;

  setPos(npc,mv);
  setDir(npc,mat);

  if(state>0) {
    reverseState = (state>0);
    } else {
    reverseState = false;
    state = -1;
    }
  to.userState  = state;
  to.user       = &npc;
  to.attachMode = true;
  return true;
  }

bool Interactive::attach(Npc &npc) {
  float dist = 0;
  Pos*  p    = nullptr;
  for(auto& i:attPos) {
    if(i.user || !i.isAttachPoint())
      continue;
    float d = qDistanceTo(npc,i);
    if(d<dist || p==nullptr) {
      p    = &i;
      dist = d;
      }
    }

  if(p!=nullptr)
    return attach(npc,*p);
  return false;
  }

bool Interactive::dettach(Npc &npc) {
  for(auto& i:attPos)
    if(i.user==&npc) {
      i.attachMode = false;
      return true;
      }
  return true;
  }

void Interactive::setPos(Npc &npc,std::array<float,3> pos) {
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
  return npc.qDistTo(p[0],p[1]-npc.translateY(),p[2]);
  }

bool Interactive::setAnim(Interactive::Anim t) {
  int  st[]     = {state,state+(reverseState ? -t : t)};
  char ss[2][8] = {};

  st[1] = std::max(-1,std::min(st[1],data.oCMobInter.stateNum));

  char buf[256]={};
  for(int i=0;i<2;++i){
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"S0"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

  if(st[0]<0 || st[1]<0)
    std::snprintf(buf,sizeof(buf),"S_S0",ss[0]); else
  if(st[0]==st[1])
    std::snprintf(buf,sizeof(buf),"S_%s",ss[0]); else
    std::snprintf(buf,sizeof(buf),"T_%s_2_%s",ss[0],ss[1]);

  auto sq = solver.solveFrm(buf);
  if(sq)
    return skInst->startAnim(solver,sq,BS_NONE,false,world->tickCount());
  return true;
  }

const Animation::Sequence* Interactive::anim(const AnimationSolver &solver, Anim t) {
  const char* tag      = schemeName();
  int         st[]     = {state,state+(reverseState ? -t : t)};
  char        ss[2][8] = {};
  const char* point    = "";

  for(auto& i:attPos)
    if(i.user!=nullptr) {
      point = i.posTag();
      }

  st[1] = std::max(-1,std::min(st[1],data.oCMobInter.stateNum));

  char buf[256]={};
  for(int i=0;i<2;++i){
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"STAND"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

  if(st[0]==st[1])
    std::snprintf(buf,sizeof(buf),"S_%s%s_%s",tag,point,ss[0]); else
    std::snprintf(buf,sizeof(buf),"T_%s%s_%s_2_%s",tag,point,ss[0],ss[1]);
  return solver.solveFrm(buf);
  }

void Interactive::marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
  p.setBrush(Tempest::Color(1.0,0,0,1));

  for(auto& m:attPos){
    auto mat = pos;
    auto pos = mesh->mapToRoot(m.node);
    mat.mul(pos);

    float x = mat.at(3,0);
    float y = mat.at(3,1);
    float z = mat.at(3,2);
    mvp.project(x,y,z);

    x = (0.5f*x+0.5f)*w;
    y = (0.5f*y+0.5f)*h;

    p.drawRect(int(x),int(y),1,1);
    }
  }

const char *Interactive::Pos::posTag() const {
  if(name=="ZS_POS0_FRONT" || name=="ZS_POS1_FRONT")
    return "_FRONT";
  if(name=="ZS_POS0_BACK" || name=="ZS_POS1_BACK")
    return "_BACK";
  return "";
  }

bool Interactive::Pos::isAttachPoint() const {
  return name=="ZS_POS0" || name=="ZS_POS0_FRONT" || name=="ZS_POS0_BACK" || name=="ZS_POS0_DIST" ||
         name=="ZS_POS1" || name=="ZS_POS1_FRONT" || name=="ZS_POS1_BACK" || name=="ZS_POS1_DIST" ||
         name=="ZS_POS2" ||
         name=="ZS_POS3";
  }
