### This folder contains a Bink-video codec

Implementation is based on ffmpeg library source code (LGPLv2.1+).
Codec itself is completely independent unit, which does not require any other OpenGothic parts in order to work - fell free to use it for your open-source game remake.

Classes:
* Bink::Video - video codec
* Bink::Frame - frame image
* Bink::Video::Input - data input adapter
* Bink::Frame::Plane - one of YUV planes

Usage example:
```c++
#include <bink/video.h>

MyInput     fin(filename); // see Bink::Video::Input
Bink::Video vid(&fin);
for(size_t i=0; i<vid.frameCount(); ++i) {
  auto& f = vid.nextFrame();

  char buf[256]={};
  std::snprintf(buf,sizeof(buf),"%s_%d.tga",savename,i);
  saveImage(f,buf);
  }
```
