#include "svmdefinitions.h"
#include "utils/string_frm.h"

SvmDefinitions::SvmDefinitions(phoenix::vm& vm):vm(vm) {
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
      if(i == nullptr)
        return "";
      svm[id] = vm.init_instance<phoenix::c_svm>(i);
      }

    speech = speech.substr(1);
    name = string_frm("C_SVM.",speech);

    auto* i = vm.find_symbol_by_name(name);
    if(i==nullptr)
      return "";
    return i->get_string(0,svm[size_t(id)]);
    }

  return "";
  }
