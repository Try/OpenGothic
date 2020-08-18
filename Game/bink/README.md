### This folder contains a Bink-video codec

Implementation is based on ffmepeg library source code.
Codec itself is completly independent unit, wich does not require any other OpenGothic parts in order to work - fell free to use it for your game remake.

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
