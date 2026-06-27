#include "svmdefinitions.h"
#include "utils/string_frm.h"

SvmDefinitions::SvmDefinitions(zenkit::DaedalusVm& vm):vm(vm) {
  }

SvmDefinitions::~SvmDefinitions() {
  }

std::string_view SvmDefinitions::find(std::string_view speech, int intId) {
  if(!speech.empty() && speech[0]=='$' && intId>=0) {
    const size_t id=size_t(intId);

    string_frm name("SVM_",int(id));
    if(svm.size()<=id)
      svm.resize(id+1);

    if(svm[id] == nullptr) {
      auto* i = vm.find_symbol_by_name(name);
      if (i == nullptr)
          return "";
      svm[id] = vm.init_opaque_instance(i);
      }

    // NOTE: in original-game oCSVMManager::GetOU @0x00779e50 the '$'-stripped SVM key is run
    // through Upper()+TrimRight(' ')+TrimLeft(' ') before the C_SVM member lookup.
    // find_symbol_by_name already uppercases, but the surrounding-space trim was missing, so a
    // key like "$DEAD " (stray space) resolved to a line in Gothic2.exe yet to a null symbol here.
    speech = speech.substr(1);
    while(!speech.empty() && speech.front()==' ')
      speech.remove_prefix(1);
    while(!speech.empty() && speech.back()==' ')
      speech.remove_suffix(1);
    name = string_frm("C_SVM.",speech);

    auto* i = vm.find_symbol_by_name(name);
    if(i==nullptr)
      return "";
    return i->get_string(0,svm[size_t(id)].get());
    }

  return "";
  }
