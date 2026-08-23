#pragma once

#include <Tempest/Color>
#include <Tempest/Font>
#include <Tempest/Icon>

class Assets {
  public:
    Assets();

    static const Assets& inst();

    struct {
      Tempest::Color panel      = {0.150f, 0.150f, 0.150f, 1.f};
      Tempest::Color workspace  = {0.260f, 0.260f, 0.260f, 1.f};
      Tempest::Color workspaceD = {0.100f, 0.100f, 0.100f, 1.f};
      Tempest::Color menu       = {0.250f, 0.250f, 0.250f, 1.f};
      Tempest::Color highlight  = {0.29f,  0.51f,  0.90f,  1.f};
      } colors;

    struct {
      Tempest::Icon close;
      Tempest::Icon close_save;
      Tempest::Icon more;
      Tempest::Icon add;
      Tempest::Icon down, up;
      Tempest::Icon tri_open, tri_close;

      Tempest::Icon file_project;
      Tempest::Icon folder_large;
      Tempest::Icon file_large;
      } ic;

    Tempest::Font fntSmall, fntApp;
  };
