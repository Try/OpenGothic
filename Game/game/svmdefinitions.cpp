#include "svmdefinitions.h"

#include <daedalus/DaedalusVM.h>

SvmDefinitions::SvmDefinitions(Daedalus::DaedalusVM &vm):vm(vm) {
  }

const std::string &SvmDefinitions::find(const char *speech, const int intId) {
  if(speech!=nullptr && speech[0]=='$' && intId>=0){
    const size_t id=size_t(intId);

    char name[128]={};
    std::snprintf(name,sizeof(name),"SVM_%d",id);

    if(svm.size()<=id)
      svm.resize(id+1);
    if(svm[id].instanceSymbol==0){
      size_t i = vm.getDATFile().getSymbolIndexByName(name);
      vm.initializeInstance(&svm[id], i, Daedalus::IC_Svm);
      }

    std::snprintf(name,sizeof(name),"C_SVM.%s",speech+1);

    auto& i = vm.getDATFile().getSymbolByName(name);
    return i.getString(0,&svm[size_t(id)]);
    }

  static const std::string empty;
  return empty;
  }
