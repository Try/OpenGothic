#pragma once
#include <string_view>

class Npc;

class AiOuputPipe {
  public:
    virtual ~AiOuputPipe()=default;

    virtual bool output   (Npc& npc, std::string_view text)=0;
    virtual bool outputSvm(Npc& npc, std::string_view text)=0;
    virtual bool outputOv (Npc& npc, std::string_view text)=0;
    virtual bool printScr (Npc& npc, int time, std::string_view msg, int x,int y, std::string_view font)=0;

    virtual bool isFinished()=0;
    virtual bool close()=0;
  };
