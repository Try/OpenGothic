#include "cameradefinitions.h"

#include <Tempest/Log>
#include <zenkit/vobs/Camera.hh>

#include "gothic.h"
#include "utils/string_frm.h"

using namespace Tempest;

CameraDefinitions::CameraDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Camera.dat");

  vm->enumerate_instances_by_class_name("CCAMSYS", [this, &vm](zenkit::DaedalusSymbol& s) {
    try {
      auto cam = vm->init_instance<zenkit::ICamera>(&s);
      cameras.emplace_back(s.name(), *cam);
      }
    catch(const zenkit::DaedalusScriptError&) {
      // There was an error initializing the ICamera. Ignore it.
      }
    });

  camModDialog    = getCam("CAMMODDIALOG");
  camModInventory = getCam("CAMMODINVENTORY");
  camModNormal    = getCam("CAMMODNORMAL");
  camModBack      = getCam("CAMMODLOOKBACK");
  camModFp        = getCam("CAMMODFIRSTPERSON");
  camModDeath     = getCam("CAMMODDEATH");
  camModMelee     = getCam("CAMMODMELEE");
  camModRange     = getCam("CAMMODRANGED");
  camModMage      = getCam("CAMMODMAGIC");
  camModSwim      = getCam("CAMMODSWIM");
  camModDive      = getCam("CAMMODDIVE");
  camModFall      = getCam("CAMMODFALL");

  // dialog presets: unused yet
  std::vector<zenkit::VCutsceneCamera> cameras;
  try {
    std::unique_ptr<zenkit::Read> read;
    auto zen = Resources::openReader("DialogCams.ZEN", read);

    while(!read->eof()) {
      zenkit::ArchiveObject   obj {};
      zenkit::VCutsceneCamera preset {};
      zen->read_object_begin(obj);
      preset.load(*zen, Gothic::inst().version().game == 1 ? zenkit::GameVersion::GOTHIC_1
                                                           : zenkit::GameVersion::GOTHIC_2);
      cameras.emplace_back(std::move(preset));
      if(!zen->read_object_end()) {
        zen->skip_object(true);
        }
      }
    }
  catch(...) {
    Log::e("unable to load Zen-file: \"DialogCams.ZEN\"");
    }
  }

const zenkit::ICamera& CameraDefinitions::mobsiCam(std::string_view tag, std::string_view pos) const {
  if(!pos.empty()) {
    string_frm name("CAMMODMOB",tag,'_',pos);
    if(auto* c = find(name))
      return *c;
    }

  string_frm name("CAMMODMOB",tag);
  if(auto* c = find(name))
    return *c;
  if(auto* c = find("CAMMODMOBDEFAULT"))
    return *c;
  return camModNormal;
  }

zenkit::ICamera CameraDefinitions::getCam(std::string_view name) {
  for(auto& i:cameras)
    if(i.first==name)
      return i.second;

  return {};
  }

const zenkit::ICamera* CameraDefinitions::find(std::string_view name) const {
  for(auto& i:cameras)
    if(i.first==name)
      return &i.second;
  return nullptr;
  }
