#include "interactive.h"
#include "npc.h"
#include "world.h"

#include <Tempest/Painter>

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
  if(data.oCMobInter.onStateFunc.empty())
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

void Interactive::implAddItem(char *name) {
  char* sep = std::strchr(name,':');
  if(sep!=nullptr) {
    *sep='\0';++sep;
    long count = std::strtol(sep,nullptr,10);
    if(count>0)
      invent.addItem(name,uint32_t(count),*world->script());
    } else {
    invent.addItem(name,1,*world->script());
    }
  }

const Interactive::Pos *Interactive::findFreePos() const {
  for(auto& i:pos) {
    if(i.user==nullptr && (i.name=="ZS_POS0" || i.name=="ZS_POS0_FRONT" || i.name=="ZS_POS1_BACK" ||
                           i.name=="ZS_POS0_DIST")) {
      return &i;
      }
    }
  return nullptr;
  }

Interactive::Pos *Interactive::findFreePos() {
  for(auto& i:pos) {
    if(i.user==nullptr && (i.name=="ZS_POS0" || i.name=="ZS_POS0_FRONT" || i.name=="ZS_POS1_BACK" ||
                           i.name=="ZS_POS0_DIST")) {
      return &i;
      }
    }
  return nullptr;
  }

bool Interactive::isAvailable() const {
  return findFreePos()!=nullptr;
  }

bool Interactive::attach(Npc &npc) {
  if(auto p=findFreePos()){
    attach(npc,*p);
    return true;
    }
  return false;
  }

void Interactive::dettach(Npc &npc) {
  for(auto& i:pos)
    if(i.user==&npc) {
      i.user=nullptr;
      state=0;
      }
  }

void Interactive::setPos(Npc &npc,std::array<float,3> pos) {
  bool valid=false;
  auto ground = world->physic()->dropRay(pos[0],pos[1],pos[2],valid);
  if(valid) {
    pos[1]=ground.y();
    npc.setPosition(pos);
    }
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
  npc.setInteraction(this);

  state = (1)%std::max(data.oCMobInter.stateNum+1,1);
  }

const char* Interactive::anim(Interactive::Anim t) const {
  static const char* lab[]={
    "T_LAB_STAND_2_S0",
    "S_LAB_S1",
    "T_LAB_S0_2_STAND"
    };
  static const char* anv[]={
    "T_BSANVIL_STAND_2_S0",
    "S_BSANVIL_S1",
    "T_BSANVIL_S0_2_STAND"
    };
  static const char* grind[]={
    "T_BSSHARP_STAND_2_S0",
    "S_BSSHARP_S1",
    "T_BSSHARP_S0_2_STAND"
    };
  static const char* bench[]={
    "T_BENCH_STAND_2_S0",
    "S_BENCH_S1",
    "T_THRONE_S0_2_STAND"
    };
  static const char* chest[]={
    "T_CHESTSMALL_STAND_2_S0",
    "S_CHESTSMALL_S1",
    "T_CHESTSMALL_S1_2_S0"
    };
  static const char* forge[]={
    "T_BSFIRE_STAND_2_S0",
    "S_BSFIRE_S1",
    "T_BSFIRE_S0_2_STAND"
    };
  static const char* pray[]={
    "T_INNOS_STAND_2_S0",
    "S_INNOS_S1",
    "T_INNOS_S1_2_S0"
    };
  static const char* ore[]={
    "T_ORE_STAND_2_S0",
    "C_ORE_S1_1",
    "T_ORE_S0_2_STAND"
    };
  static const char* cauldron[]={
    "T_CAULDRON_STAND_2_S0",
    "S_CAULDRON_S1",
    "T_CAULDRON_S0_2_STAND"
    };
  static const char* throne[]={
    "T_THRONE_STAND_2_S0",
    "S_THRONE_S1",
    "T_THRONE_S0_2_STAND"
    };
  static const char* chair[]={
    "T_CHAIR_STAND_2_S0",
    "S_CHAIR_S1",
    "T_THRONE_S0_2_STAND"
    };
  static const char* rotSwitch[]={
    "T_TURNSWITCH_STAND_2_S0",
    "T_TURNSWITCH_S0_2_S1",
    "T_TOUCHPLATE_STAND_2_S0"
    };
  static const char* book[]={
    "T_BOOK_STAND_2_S0",
    "S_BOOK_S1",
    "T_BOOK_STAND_2_S0"
    };
  static const char* bbq[]={
    "T_BARBQ_STAND_2_S0",
    "S_BARBQ_S1",
    ""
    };

  if(data.oCMOB.focusName=="MOBNAME_INNOS" || data.oCMOB.focusName=="MOBNAME_ADDON_IDOL")
    return pray[t];
  if(data.oCMOB.focusName=="MOBNAME_LAB")
    return lab[t];
  if(data.oCMOB.focusName=="MOBNAME_ANVIL")
    return anv[t];
  if(data.oCMOB.focusName=="MOBNAME_GRINDSTONE")
    return grind[t];
  if(data.oCMOB.focusName=="MOBNAME_BENCH")
    return bench[t];
  if(data.oCMOB.focusName=="MOBNAME_CHEST")
    return chest[t];
  if(data.oCMOB.focusName=="MOBNAME_FORGE")
    return forge[t];
  if(data.oCMOB.focusName=="MOBNAME_ORE")
    return ore[t];
  if(data.oCMOB.focusName=="MOBNAME_CAULDRON")
    return cauldron[t];
  if(data.oCMOB.focusName=="MOBNAME_THRONE")
    return throne[t];
  if(data.oCMOB.focusName=="MOBNAME_CHAIR")
    return chair[t];
  if(data.oCMOB.focusName=="MOBNAME_SWITCH")
    return rotSwitch[t];
  if(data.oCMOB.focusName=="MOBNAME_BOOKSBOARD")
    return book[t];
  if(data.oCMOB.focusName=="MOBNAME_BBQ_SCAV" || data.oCMOB.focusName=="MOBNAME_BARBQ_SCAV")
    return bbq[t];
  return chair[t];
  }

void Interactive::marchInteractives(Tempest::Painter &p, const Tempest::Matrix4x4 &mvp, int w, int h) const {
  p.setBrush(Tempest::Color(1.0,0,0,1));

  for(auto& m:pos){
    auto mat = objMat;
    //mat.translate(mesh->rootTr[0],mesh->rootTr[1],mesh->rootTr[2]);
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
