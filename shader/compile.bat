# Ubershader flags:
#   OBJ        - enable object matrix
#   SKINING    - animation skeleton
#   SHADOW_MAP - output is shadowmap
#   ATEST      - use alpha test
#   PFX        - use color modulation

SET BIN=../bin/shader/

glslangValidator -V -DATEST                              main.vert   -o %BIN%land.vert.sprv
glslangValidator -V -DATEST                              main.frag   -o %BIN%land.frag.sprv
                    -DATEST
glslangValidator -V -DATEST -DOBJ                        main.vert   -o %BIN%object.vert.sprv
glslangValidator -V -DATEST -DOBJ                        main.frag   -o %BIN%object.frag.sprv
                    -DATEST
glslangValidator -V -DATEST -DOBJ -DSKINING              main.vert   -o %BIN%anim.vert.sprv
glslangValidator -V -DATEST -DOBJ -DSKINING              main.frag   -o %BIN%anim.frag.sprv
                    -DATEST
glslangValidator -V -DPFX                                main.vert   -o %BIN%pfx.vert.sprv
glslangValidator -V -DPFX                                main.frag   -o %BIN%pfx.frag.sprv


glslangValidator -V -DATEST -DSHADOW_MAP                 main.vert   -o %BIN%land_shadow.vert.sprv
glslangValidator -V -DATEST -DSHADOW_MAP                 main.frag   -o %BIN%land_shadow.frag.sprv
                    -DATEST
glslangValidator -V -DATEST -DOBJ -DSHADOW_MAP           main.vert   -o %BIN%object_shadow.vert.sprv
glslangValidator -V -DATEST -DOBJ -DSHADOW_MAP           main.frag   -o %BIN%object_shadow.frag.sprv
                    -DATEST
glslangValidator -V -DATEST -DOBJ -DSKINING -DSHADOW_MAP main.vert   -o %BIN%anim_shadow.vert.sprv
glslangValidator -V -DATEST -DOBJ -DSKINING -DSHADOW_MAP main.frag   -o %BIN%anim_shadow.frag.sprv

glslangValidator -V -DSHADOW_MAP                         main.vert   -o %BIN%pfx_shadow.vert.sprv
glslangValidator -V -DSHADOW_MAP                         main.frag   -o %BIN%pfx_shadow.frag.sprv


glslangValidator -V sky.vert -o %BIN%sky.vert.sprv
glslangValidator -V sky.frag -o %BIN%sky.frag.sprv

glslangValidator -V shadow_compose.vert -o %BIN%shadow_compose.vert.sprv
glslangValidator -V shadow_compose.frag -o %BIN%shadow_compose.frag.sprv
