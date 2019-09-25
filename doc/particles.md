## C_ParticleFx
### Emitter
| Type | Name | Description | Unit | Relevant if | |
---|---|---|---|---|---
| float | ppsValue | Particles per Second (Value)  | count/sec | | particles produced per second
| string | ppsScaleKeys_S | Particles per Second: Scale Keyframes   |  | |  set of factors by which ppsValue is scaled in the course
| bool | ppsIsLooping | Particles per Second: Is Looping? |  | |  
| bool | ppsIsSmooth | Particles per Second: Is Smooth?  |  | ppsScaleKeys_S.size > 1  |  
| float | ppsFPS | Particles per Second: Frames per Second | 1/sec | ppsScaleKeys_S.size > 1 | how fast should the list of factors in ppsScaleKeys_S be traversed 
| string | ppsCreateEm_S | Particles per Second: Create Emitter |  | | another PFX, which is additionally generated 
| float | ppsCreateEmDelay | Particles per Second: Create Emitter Delay | msec | ppsCreateEm_S not empty | this delay generates the effect called in ppsCreateEm_S

### Shape
| Type | Name | Description | Unit | Relevant if | |
---|---|---|---|---|---
| enum | shpType_S | Shape type |  | | On which body should the particles be produced. Possible values are "POINT" for a point, "LINE" for a vertical line, "CIRCLE" for a flat circle, "BOX" for a square and "SPHERE" for a sphere. The value "MESH" is also possible for any mesh. 
| enum | shpFOR_S | Shape: Frame of Reference  |  | | Should the effect be aligned with the world coordinate system "WORLD" or the effect "OBJECT"? If "WORLD" is selected, the rotation of the particle effect object has no influence on the start position of the particles
| string | shpOffsetVec_S | Offset Vector | (cm,cm,cm) | |  
| enum | shpDistribType_S | Distribution Type  |  | shpType_S=="LINE" or shpType_S=="CIRCLE"  | For lines or circles, "WALK" can be used to make the particle source "walk" evenly along the line / circle. Otherwise, there is only the random distribution "RAND". The value "UNIFORM" does not seem to work and "WALKW" behaves the same as "WALK"
| float | shpDistribWalkSpeed | Shape: Distribution Walk Speed | ~100 * loops/sec | shpDistribType_S == "WALK" |  The speed at which the circle or line will rotate
| bool | shpIsVolume | Shape: Is Volume? |  | shpType_S=="BOX" or shpType_S=="SPHERE"  | If set, particles are also generated within a sphere or cuboid rather than just at the boundary surfaces
| string | shpDim_S | Shape: Dimension | (cm,cm,cm) | shpType_S!="POINT" | The size of the emitter must be specified according to the type. For the circle or the sphere the diameter, for the line a length and for a cuboid three edge lengths
| string | shpMesh_S | Shape: Mesh |  | shpType_S=="MESH" | If the emitter is a mesh, it must also be specified which. This can happen here.
| bool | shpMeshRender_B | Shape: Render the Mesh? |  | shpType_S=="MESH" | If the emitter is a mesh (the particles are created on it) that does not automatically mean that the mesh is also rendered along. This is specified separately here. 
| string | shpScaleKeys_S | Shape: Scale: Keyframes |  | shpType_S!="POINT" | A number of factors by which the size of the emitter is scaled over time (so a sphere with the value "1 2" would be twice as large during the duration of the effect). 
| bool | shpScaleIsLooping | Shape: Scale: Is Looping? | | shpScaleKeys_S.size>1 | Should all the factors in shpScaleKeys_S be re-started from scratch in the list after going through all the factors?
| bool | shpScaleIsSmooth | Shape: Scale: Is Smooth? |  | shpScaleKeys_S.size>1 | Should the factors in shpScaleKeys_S be hard on each other or should they be superimposed linearly?
| float | shpScaleFPS | Shape: Scale: Frames per Second  | 1/sec | shpScaleKeys_S.size>1 | How fast should the list of factors in shpScaleKeys_S be traversed?

### Direction
| Type | Name | Description | Unit | Relevant if | |
---|---|---|---|---|---
| enum | dirMode_S | Direction: Mode  |  | |  Is the direction of the particles explicitly stated ("DIR"), randomly selected ("RAND") or are the particles heading for a target ("TARGET")?
| string | dirFOR_S | Direction: Frame of Reference |  | dirMode_S=="DIR" |  
| string | dirModeTargetFOR_S | Direction: Target: Frame of Reference |  | dirMode_S=="TARGET" | Should the position of the target be determined with the coordinate axes of the world "WORLD" or the effect "OBJECT"? If "WORLD" is selected, the rotation of the particle effect object has no influence on the target of the particles
| string | dirModeTargetPos_S | Direction: Target: Position | (cm, cm, cm) | dirMode_S=="TARGET" | Coordinates of the target to which the particles are to be heading (only has an influence on the direction of flight during production!) For example, particles can be deflected by gravity!) 
| float | dirAngleHead | Direction: Angle Head | degree | dirMode_S=="DIR" | Direction in which the particles fly, conceivable as the angle to "left" (+) or "right" (-).
| float | dirAngleHeadVar | Direction: Angle Head Variance  | degree | dirMode_S=="DIR" | The actual value of dirAngleHead is randomly scattered in the interval [dirAngleHead - dirAngleHeadVar, dirAngleHead + dirAngleHeadVar]
| float | dirAngleElev | Direction: Angle Elev | degree | dirMode_S=="DIR" | Direction in which the particles fly, conceivable as the angle to "up" (+) or "down" (-)
| float | dirAngleElevVar | Direction: Angle Elev Variance  | degree | dirMode_S=="DIR" | The actual value of dirAngleElev is randomly scattered in the interval [dirAngleElev - dirAngleElevVar, dirAngleElev + dirAngleElevVar].  
| float | velAvg | Direction: Velocity | cm/msec |  | Speed of the particles
| float | velVar | Direction: Velocity Variance | cm/msec |  | The actual velocity of the particles is randomly scattered in the interval [velAvg - velVar, velAvg + velVar].
| float | lspPartAvg | Lifespan of Particles: Average | msec |  |  
| float | lspPartVar | Lifespan of Particles: Variance | msec |  |  The actual lifetime of the particles is randomly scattered in the interval [lspPartAvg - lspPartVar, lspPartAvg + lspPartVar].
| string | flyGravity_S | gravity | (cm/msec^2, cm/msec^2, cm/msec^2)  |  | Gravity in the direction of the three coordinate axes of the world. There is no FrameOfReference value to make the gravitation in the coordinate system of the object work
| int | flyCollDet_B | Collision Detection policy |  |  | Collision detection knows different modes: 0 = no collision detection; 1 = the particles are decelerated and reflected; 2 = the particles are accelerated and reflected; 3 = the particles stop; 4 = the particles are removed

### Visual
| Type | Name | Description | Unit | Relevant if | |
---|---|---|---|---|---
| string | visName_S | Visual: Name |  |  | Name of the texture that each particle has
| string | visOrientation_S | Visual: Orientation |  |  | Orientation of the particles to the camera: "NONE" = The particles always look exactly to the camera; "VELO" = the particles appear smaller, the more the flight direction and the direction of view coincide. The particles seem to "point" in the direction of flight (but are still aligned with the camera); "VELO3D" = The particles are aligned in the direction of flight, they are aligned with the camera only with the remaining degree of freedom
| bool | visTexIsQuadPoly | Visual: Is Texture a Quad Poly?  |  |  | When set, not only the top right corner of the texture is rendered, but a whole rectangle. This beats double the alpha-poly boundary, but looks much better depending on the texture.
| float | visTexAniFPS | Visual: Texture Animation: Frames per Second | 1/sec | Texture has multiple frames | If the texture has multiple frames ("* .A0.TGA", "* .A1.TGA", ...) then the number of texture changes per second can be selected.
| bool | visTexAniIsLooping | Visual: Texture Animation: Is Looping?  |  | Texture has multiple frames | As a rule, an animated texture should loop, that is, when the last frame is reached, it should start again at the first. This can be stated here
| string | visTexColorStart_S | Visual: Texture Color: Start | (R,G,B) |  | Factors ("[R] [G] [B]") for each color channel. "255 255 255" (white) would take the texture as it is, whereas "255 128 0" (orange) scales the green channel to half and the blue channel to 0. With the given example factors, a white smoke could be colored to an orange smoke.
| string | visTexColorEnd_S | Visual: Texture Color: End | (R,G,B) |  | Like visTexColorStart_S only the value for the end of the lifetime of the particle. Between birth and death, visTexColorStart_S merges linearly with visTexColorEnd_S.
| string | visSizeStart_S | Visual: Size: Start | (cm, cm) |  | Size of the particles. For (visOrientation_S == "NONE"), the first value for the X direction is on the screen, the second for the Y direction. For other values of visOrientation_S, the second value is the size in the direction of flight
| float | visSizeEndScale | Visual: Size: End Scale |  |  | The size of the particles changes linearly during the lifetime, so that they are at times visSizeEndScale so big
| string | visAlphaFunc_S | Visual: Alpha-Function |  |  | Different fade modes. "NONE" and "BLEND" have a masking crossfade, such as when multiple painted slides are overlaid. "ADD" adds the colors of each face to those of the background, so there would be red (1, 0, 0) + green (0, 1, 0) ⇒ yellow (1, 1, 0). However, a fully saturated color channel remains full: red + red ⇒ red; So there is a risk of overexposure. "MUL" multiplies the color channels of the surface with the background, for example, gives orange (1,0,5,0) times green (0, 1, 0) ⇒ dark green (0, 0.5, 0); in other words, if multiplied by green, nothing is allowed to pass green. Usually, particle effects use the "ADD" mode. "MUL" is hardly used for PFX because of the peculiarity that only the background is edited without the effect itself having an appearance.
| float | visAlphaStart | Visual: Alpha: Start | 0-255 | visAlphaFunc_S=="ADD" or visAlphaFunc_S=="BLEND" | When the blending mode is "BLEND" or "ADD", the alpha channel of the particle texture is scaled. visAlphaStart is the starting value for this factor and is reduced or increased linearly to visAlphaEnd until the dead of the particle. visAlphaEnd is often 0, so that the particles do not disappear abruptly
| float | visAlphaEnd | Visual: Alpha: End | 0-255 | visAlphaFunc_S=="ADD" or visAlphaFunc_S=="BLEND" | see visAlphaStart
| float | trlFadeSpeed | Trail: Fade Speed | unit/msec |  | Particle effects can create a trail in their trajectory that slowly fades. trlFadeSpeed is the speed with which this track disappears
| string | trlTexture_S | Trail: Texture |  | trlFadeSpeed>0 | Texture of the trail
| float | trlWidth | Trail: Width | cm | trlFadeSpeed>0 | Width of the trail
| float | mrkFadeSpeed | Mark: Fade Speed | unit/msec | flyCollDet_B!=0 | "Marks" are patches created when a particle effect collides with the level mesh on the level mesh. mrkFadeSpeed is the speed with which these spots disappear
| string | mrkTexture_S | Mark: Texture |  | mrkFadeSpeed>0 | Texture of mark
| float | mrkSize | Mark: Size |  | mrkFadeSpeed>0 | Size of mark
| string | flockMode |  |  |  | unknown
| bool | useEmittersFOR | use Emitters Frame of Reference? |  | Effect is moving | When set, the particles are "children" of the emitter. So they move as the emitter moves (rather than just following their own movement)
| string | timeStartEnd_S | Start- and Endtime of Rendering | (time,time) |  | Time interval in which the effect is rendered. For example, fireflies could be given the interval "22 3" so that they can only be seen between 10:00 pm and 3:00 am during the night.
| bool | m_bIsAmbientPFX | Is Ambient PFX? |  | Player has deactivated Ambiet-PFX in the options | Ambient PFX can be disabled in game options
