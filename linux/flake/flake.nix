{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {self, nixpkgs, flake-utils, ...}:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };
      in {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            git
            cmake
            glslang
            addDriverRunpath
          ];
          buildInputs = with pkgs; [
            alsa-lib
            libX11
            libXcursor
            vulkan-headers
            vulkan-loader
            vulkan-validation-layers
          ];
        };
      });
}
