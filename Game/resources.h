#pragma once

#include <Tempest/Assets>
#include <Tempest/Font>
#include <Tempest/Texture2d>

#include <vdfs/fileIndex.h>

class Gothic;

class Resources {
  public:
    explicit Resources(Gothic& gothic,Tempest::Device& device);
    ~Resources();

    static Tempest::Font menuFont(){ return inst->menuFnt; }
    static Tempest::Font font(){ return inst->mainFnt; }

    static Tempest::Texture2d loadTexture(const char* name);
    static Tempest::Texture2d loadTexture(const std::string& name);

  private:
    static Resources* inst;

    void addVdf(const char* vdf);
    Tempest::Texture2d implLoadTexture(const std::string &name);

    Tempest::Device& device;
    Tempest::Font    menuFnt, mainFnt;
    Tempest::Assets  asset;
    Gothic&          gothic;
    VDFS::FileIndex  gothicAssets;
  };
