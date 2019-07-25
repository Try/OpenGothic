glslangValidator -V sky.vert -o sky.vert.sprv
glslangValidator -V sky.frag -o sky.frag.sprv

glslangValidator -V shadow_compose.vert -o shadow_compose.vert.sprv
glslangValidator -V shadow_compose.frag -o shadow_compose.frag.sprv


glslangValidator -V                              anim.vert   -o land.vert.sprv
glslangValidator -V                              anim.frag   -o land.frag.sprv

glslangValidator -V -DOBJ                        anim.vert   -o object.vert.sprv
glslangValidator -V -DOBJ                        anim.frag   -o object.frag.sprv

glslangValidator -V -DOBJ -DSKINING              anim.vert   -o anim.vert.sprv
glslangValidator -V -DOBJ -DSKINING              anim.frag   -o anim.frag.sprv


glslangValidator -V -DSHADOW_MAP                 anim.vert   -o land_shadow.vert.sprv
glslangValidator -V -DSHADOW_MAP                 anim.frag   -o land_shadow.frag.sprv

glslangValidator -V -DOBJ -DSHADOW_MAP           anim.vert   -o object_shadow.vert.sprv
glslangValidator -V -DOBJ -DSHADOW_MAP           anim.frag   -o object_shadow.frag.sprv

glslangValidator -V -DOBJ -DSKINING -DSHADOW_MAP anim.vert   -o anim_shadow.vert.sprv
glslangValidator -V -DOBJ -DSKINING -DSHADOW_MAP anim.frag   -o anim_shadow.frag.sprv
