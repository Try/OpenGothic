#include "interactive.h"
#include "npc.h"
#include "world.h"

#include <Tempest/Painter>
#include <Tempest/Log>

Interactive::Interactive(World &owner, const ZenLoad::zCVobData &vob)
  :world(&owner),data(vob) {
  mesh = Resources::loadMesh(vob.visual);
  if(mesh) {
    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));
    objMat = Tempest::Matrix4x4(v);

    auto physicMesh = Resources::physicMesh(mesh); //FIXME: build physic model in Resources.cpp

    view   = owner.getStaticView(vob.visual,0);
    physic = owner.physic()->staticObj(physicMesh,objMat);

    view  .setObjMatrix(objMat);
    physic.setObjMatrix(objMat);

    pos.resize(mesh->pos.size());
    for(size_t i=0;i<pos.size();++i){
      pos[i].name = mesh->pos[i].name;
      pos[i].pos  = mesh->pos[i].transform;
      pos[i].node = mesh->pos[i].node;
      }
    }
  }

const std::string &Interactive::tag() const {
  return data.vobName;
  }

const std::string& Interactive::focusName() const {
  return data.oCMOB.focusName;
  }

bool Interactive::checkMobName(const std::string &dest) const {
  const auto& name=focusName();
  if(name==dest)
    return true;
  if(dest=="RAPAIR")
    return false; //TODO
  if(name=="MOBNAME_LAB" && dest=="LAB")
    return true;
  if(name=="MOBNAME_ANVIL" && dest=="BSANVIL")
    return true;
  if(name=="MOBNAME_BOOKSBOARD" && dest=="BOOK")
    return true;
  if(name=="MOBNAME_GRINDSTONE" && dest=="BSSHARP")
    return true;
  if(name=="MOBNAME_BUCKET" && dest=="BSCOOL")
    return true;
  if(name=="MOBNAME_FORGE" && dest=="BSFIRE")
    return true;
  if((name=="MOBNAME_BBQ_SCAV" || name=="MOBNAME_BARBQ_SCAV") && dest=="BARBQ")
    return true;
  if(name=="MOBNAME_STOVE" && dest=="STOVE")
    return true;
  if(name=="MOBNAME_ARMCHAIR" && dest=="THRONE")
    return true;
  if(name=="MOBNAME_SAW" && dest=="BAUMSAEGE")
    return true;
  if(name=="MOBNAME_WATERPIPE" && dest=="SMOKE")
    return true;
  if(name=="MOBNAME_BENCH" && dest=="BENCH")
    return true;
  if(data.visual=="BENCH_NW_CITY_02.ASC" && dest=="BENCH")
    return true; // bug in knoris
  if(name=="MOBNAME_CHAIR" && dest=="CHAIR")
    return true;
  if(name=="MOBNAME_INNOS" && dest=="INNOS")
    return true;
  if(name=="MOBNAME_CAULDRON" && dest=="CAULDRON")
    return true;
  if(name=="MOBNAME_WINEMAKER" && dest=="HERB")
    return true;
  if(name=="MOBNAME_ORE" && dest=="ORE")
    return true;
  if(name=="MOBNAME_BED" && dest=="BEDHIGH")
    return true;
  if(name=="MOBNAME_THRONE" && dest=="THRONE")
    return true;
  if(name.find("MOBNAME_")==0 && dest==name.c_str()+8)
    return true;
  return false;
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

std::string Interactive::stateFunc() const {
  if(data.oCMobInter.onStateFunc.empty() || state<0)
    return std::string();
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s_S%d",data.oCMobInter.onStateFunc.c_str(),state);
  return buf;
  }

Trigger* Interactive::triggerTarget() const {
  return world->findTrigger(data.oCMobInter.triggerTarget);
  }

bool Interactive::isContainer() const {
  return data.objectClass.find("oCMobContainer")!=std::string::npos;
  //return data.vobType==ZenLoad::zCVobData::VT_oCMobContainer;
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
  //orig |= Npc::BS_MOBINTERACT;

  if(data.oCMOB.focusName=="MOBNAME_BENCH" ||
     data.oCMOB.focusName=="MOBNAME_THRONE" ||
     data.oCMOB.focusName=="MOBNAME_CHAIR")
    orig = Npc::BS_SIT;
  return orig;
  }

bool Interactive::canSeeNpc(const Npc& npc, bool freeLos) const {
  for(auto& i:pos){
    auto mat = objMat;
    auto pos = mesh->mapToRoot(i.node);
    mat.mul(pos);

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

const Interactive::Pos *Interactive::findFreePos() const {
  for(auto& i:pos)
    if(i.user==nullptr && i.isAttachPoint()) {
      return &i;
      }
  return nullptr;
  }

Interactive::Pos *Interactive::findFreePos() {
  for(auto& i:pos)
    if(i.user==nullptr && i.isAttachPoint()) {
      return &i;
      }
  return nullptr;
  }

std::array<float,3> Interactive::worldPos(const Interactive::Pos &to) const {
  auto mat = objMat;
  auto pos = mesh->mapToRoot(to.node);
  mat.mul(pos);

  float x=0, y=0, z=0;

  mat.project(x,y,z);
  return {x,y,z};
  }

bool Interactive::isAvailable() const {
  return findFreePos()!=nullptr;
  }

bool Interactive::attach(Npc &npc) {
  float dist = 0;
  Pos*  p    = nullptr;
  for(auto& i:pos){
    if(i.user || !i.isAttachPoint())
      continue;
    float d = qDistanceTo(npc,i);
    if(d<dist || p==nullptr) {
      p    = &i;
      dist = d;
      }
    }

  if(p!=nullptr){
    attach(npc,*p);
    return true;
    }
  return false;
  }

void Interactive::dettach(Npc &npc) {
  for(auto& i:pos)
    if(i.user==&npc) {
      i.user=nullptr;
      state=-1;
      }
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

void Interactive::attach(Npc &npc, Interactive::Pos &to) {
  assert(to.user==nullptr);

  auto mat = objMat;
  auto pos = mesh->mapToRoot(to.node);
  mat.mul(pos);

  to.user = &npc;
  float x=0, y=0, z=0;

  mat.project(x,y,z);
  setPos(npc,{x,y-npc.translateY(),z});

  setDir(npc,mat);
  //npc.setInteraction(this);
  state = -1;
  }

float Interactive::qDistanceTo(const Npc &npc, const Interactive::Pos &to) {
  auto p = worldPos(to);
  return npc.qDistTo(p[0],p[1]-npc.translateY(),p[2]);
  }

void Interactive::nextState() {
  state = std::min(data.oCMobInter.stateNum,state+1);
  }

void Interactive::prevState() {
  state = std::max(-1,state-1);
  }

AnimationSolver::Sequence Interactive::anim(const AnimationSolver &solver, Anim t) {
  int         st[]     = {state,state+t};
  char        ss[2][8] = {};
  const char* tag      = "";

  st[1] = std::max(-1,std::min(st[1],data.oCMobInter.stateNum));

  char buf[256]={};

  for(int i=0;i<2;++i){
    if(st[i]<0)
      std::snprintf(ss[i],sizeof(ss[i]),"STAND"); else
      std::snprintf(ss[i],sizeof(ss[i]),"S%d",st[i]);
    }

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
    tag = "BEDHIGH_BACK";
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
  else if(data.oCMOB.focusName=="MOBNAME_DOOR")
    tag = "DOOR_BACK";
  else if(data.oCMOB.focusName=="MOBNAME_WINEMAKER")
    tag = "HERB";
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
    Tempest::Log::i("unable to recognize mobsi{",data.oCMOB.focusName,", ",data.visual,"}");
    }

  loopState = (st[0]==st[1]);
  if(loopState)
    std::snprintf(buf,sizeof(buf),"S_%s_%s",tag,ss[0]); else
    std::snprintf(buf,sizeof(buf),"T_%s_%s_2_%s",tag,ss[0],ss[1]);
  return solver.animSequence(buf);
  }

void Interactive::marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
  p.setBrush(Tempest::Color(1.0,0,0,1));

  for(auto& m:pos){
    auto mat = objMat;
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

bool Interactive::Pos::isAttachPoint() const {
  return name=="ZS_POS0" || name=="ZS_POS0_FRONT" || name=="ZS_POS0_DIST" ||
         name=="ZS_POS1" || name=="ZS_POS1_BACK" ||
         name=="ZS_POS2" ||
         name=="ZS_POS3";
  }
