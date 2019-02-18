#include "interactive.h"
#include "npc.h"
#include "world.h"

#include <Tempest/Painter>

Interactive::Interactive(World &owner, const ZenLoad::zCVobData &vob)
  :world(&owner){
  mesh = Resources::loadMesh(vob.visual);
  data = vob;

  if(mesh) {
    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));
    objMat = Tempest::Matrix4x4(v);
    physic = Resources::physicMesh(mesh);

    pos.resize(mesh->pos.size());
    for(size_t i=0;i<pos.size();++i){
      pos[i].name = mesh->pos[i].name;
      pos[i].pos  = mesh->pos[i].transform;
      pos[i].node = mesh->pos[i].node;
      }
    }
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
  auto& s=world->getSymbol(data.oCMOB.focusName.c_str());
  const char* txt = s.getString(0).c_str();
  if(std::strlen(txt)==0)
    txt=data.vobName.c_str();
  return txt;
  }

std::string Interactive::stateFunc() const {
  if(data.oCMobInter.onStateFunc.empty())
    return std::string();
  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s_S%d",data.oCMobInter.onStateFunc.c_str(),state);
  return buf;
  }

const Trigger* Interactive::triggerTarget() const {
  return world->findTrigger(data.oCMobInter.triggerTarget);
  }

bool Interactive::attach(Npc &npc) {
  for(auto& i:pos) {
    if(i.name=="ZS_POS0" && i.user==nullptr) {
      attach(npc,i);
      return true;
      }
    }
  for(auto& i:pos) {
    if(i.name=="ZS_POS0_DIST" && i.user==nullptr) {
      attach(npc,i);
      return true;
      }
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
    pos[1]=ground;
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
  return lab[t];
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
