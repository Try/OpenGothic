#include "reference.h"

using namespace Dx8;

Reference::Reference(Riff &input) {
  input.read([&](Riff& ch){
    if(ch.is("refh"))
      ch.read(&header,sizeof(header));
    else if(ch.is("guid"))
      ch.read(&guid,sizeof(guid));
    else if(ch.is("name"))
      ch.read(name);
    else if(ch.is("file"))
      ch.read(file);
    else if(ch.is("catg"))
      ch.read(category);
    else if(ch.is("vers"))
      ch.read(&version,sizeof(version));
    });
  }
