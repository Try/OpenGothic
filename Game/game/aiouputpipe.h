#pragma once

#include <string>

class Npc;

class AiOuputPipe {
  public:
    virtual ~AiOuputPipe()=default;

    virtual bool output   (Npc& npc, const std::string& text)=0;
    virtual bool outputSvm(Npc& npc, const std::string& text, int voice)=0;
    virtual bool outputOv (Npc& npc, const std::string& text, int voice)=0;

    virtual bool isFinished()=0;
    virtual bool close()=0;
  };
