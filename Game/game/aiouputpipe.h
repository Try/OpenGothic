#pragma once

#include <daedalus/ZString.h>

class Npc;

class AiOuputPipe {
  public:
    virtual ~AiOuputPipe()=default;

    virtual bool output   (Npc& npc, const Daedalus::ZString& text)=0;
    virtual bool outputSvm(Npc& npc, const Daedalus::ZString& text, int voice)=0;
    virtual bool outputOv (Npc& npc, const Daedalus::ZString& text, int voice)=0;

    virtual bool isFinished()=0;
    virtual bool close()=0;
  };
