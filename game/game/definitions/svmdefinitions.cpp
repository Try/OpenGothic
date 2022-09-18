#include "svmdefinitions.h"

SvmDefinitions::SvmDefinitions(phoenix::vm& vm):vm(vm) {
  }

SvmDefinitions::~SvmDefinitions() {
  }

const std::string& SvmDefinitions::find(std::string_view speech, int intId) {
  if(!speech.empty() && speech[0]=='$' && intId>=0){
    const size_t id=size_t(intId);

    char name[128]={};
    std::snprintf(name,sizeof(name),"SVM_%d",int(id));

    if(svm.size()<=id)
      svm.resize(id+1);

    if(svm[id] == nullptr){
      auto i = vm.find_symbol_by_name(name);
      svm[id] = vm.init_instance<phoenix::c_svm>(i);
      }

    speech = speech.substr(1);
    std::snprintf(name,sizeof(name),"C_SVM.%.*s",int(speech.size()),speech.data());

    auto* i = vm.find_symbol_by_name(name); //TODO: optimize
    return i->get_string(0,svm[size_t(id)]);
    }

  static std::string empty;
  return empty;
  }
