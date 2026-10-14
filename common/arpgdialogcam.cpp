#include "arpgdialogcam.h"

#include <algorithm>
#include <cmath>

#include "world/objects/npc.h"
#include "world/world.h"
#include "physics/dynamicworld.h"

using namespace Tempest;

static Vec3 flat(const Vec3& v) {
  return Vec3(v.x, 0, v.z);
  }

static Vec3 norm2(const Vec3& v, const Vec3& fallback) {
  const Vec3  f = flat(v);
  const float l = f.length();
  if(l<1.f)
    return fallback;
  return f/l;
  }

// how far the point is from the line of sight (subject -> camera); far away when it is behind one of them
static float offLine(const Vec3& pt, const Vec3& from, const Vec3& to) {
  const Vec3  d  = to-from;
  const float l2 = Vec3::dotProduct(d, d);
  if(l2<1.f)
    return 1e6f;
  const float t = Vec3::dotProduct(pt-from, d)/l2;
  if(t<=0.02f || t>=0.98f)
    return 1e6f;
  return (pt-(from+d*t)).length();
  }

static Vec3 face(const Npc& n) {
  // the head bone is the base of the head: eyes a bit above
  return n.mapHeadBone()+Vec3(0, 8.f, 0);
  }

static Vec3 rotY(const Vec3& v, float deg) {
  const float a = deg*float(M_PI)/180.f;
  const float c = std::cos(a), s = std::sin(a);
  return Vec3(v.x*c - v.z*s, v.y, v.x*s + v.z*c);
  }

void ArpgDialogCam::reset() {
  *this = ArpgDialogCam();
  }

bool ArpgDialogCam::showsFace(const Npc* who) const {
  switch(take.kind) {
    case K_None:
      return false;
    case K_Wide:
    case K_Two:
      return who==nullptr || who!=thirdSeen; // a third one is not in the shots of the two
    case K_OverShoulder:
    case K_Medium:
    case K_Close:
      return take.subject==who;
    }
  return false;
  }

// the subject's face (x), the one he speaks to (y) and the side of their line the camera stays on (n)
void ArpgDialogCam::pair(const Npc* who, const Input& in, Vec3& x, Vec3& y, Vec3& n) const {
  const Vec3 fwdPl = norm2(faceOther-facePl, Vec3(1, 0, 0));
  n = Vec3(-fwdPl.z, 0, fwdPl.x)*side;
  if(who!=nullptr && who==in.third && who!=in.pl && who!=in.other) {
    // a third one: filmed against the hero, from the same side of the world as the main line
    x = faceThird;
    y = facePl;
    const Vec3 f  = norm2(x-y, fwdPl);
    Vec3       n3 = Vec3(-f.z, 0, f.x);
    if(Vec3::dotProduct(n3, n)<0.f)
      n3 = n3*-1.f;
    n = n3;
    return;
    }
  x = (who==in.pl ? facePl    : faceOther);
  y = (who==in.pl ? faceOther : facePl);
  }

ArpgDialogCam::Shot ArpgDialogCam::base(Kind k, const Npc* who, const Input& in) const {
  const Vec3  up    = Vec3(0, 1, 0);
  const Vec3  fwdPl = norm2(faceOther-facePl, Vec3(1, 0, 0));
  // the side of the line between the two where the camera stays (180 degree rule)
  const Vec3  nMain = Vec3(-fwdPl.z, 0, fwdPl.x)*side;
  const float sep   = flat(faceOther-facePl).length();
  const Vec3  mid   = (facePl+faceOther)*0.5f;

  Shot s;
  if(k==K_Wide) {
    s.origin = mid + nMain*(sep*1.6f+260.f) + up*55.f;
    s.target = mid - up*25.f;
    }
  else if(k==K_Two || who==nullptr) {
    s.origin = mid + nMain*(sep*1.15f+90.f) - up*12.f;
    s.target = mid - up*18.f;
    }
  else {
    Vec3 x, y, nn;
    pair(who, in, x, y, nn);                 // the subject, the other one, the side
    const Vec3 n = nn;
    const Vec3 u = norm2(y-x, who==in.pl ? fwdPl : fwdPl*-1.f); // from the subject towards the other
    switch(k) {
      case K_OverShoulder:
        s.origin = y + u*95.f + n*70.f + up*12.f;
        s.target = x - up*4.f;
        break;
      case K_Medium: {
        const float d = std::clamp(sep*0.9f, 110.f, 160.f);
        s.origin = x + u*d + n*(d*0.5f) - up*8.f;
        s.target = x - up*18.f;
        break;
        }
      case K_Close:
      default:
        s.origin = x + u*80.f + n*42.f;
        s.target = x - up*2.f;
        break;
      }
    }
  return s;
  }

ArpgDialogCam::Shot ArpgDialogCam::film(const Take& tk, const Input& in, World& world, float* quality) const {
  const Vec3 up    = Vec3(0, 1, 0);
  const Vec3 fwdPl = norm2(faceOther-facePl, Vec3(1, 0, 0));
  const Vec3 n     = Vec3(-fwdPl.z, 0, fwdPl.x)*side;

  Shot s = base(tk.kind, tk.subject, in);
  {
    Vec3 x, y, ns;
    pair(tk.subject, in, x, y, ns);
    s.origin = s.origin + ns*tk.extraSide;
  }

  // every take moves a little in its own way; always away from the line between the two
  const float t = tk.t;
  switch(tk.move) {
    case M_PushIn:
      s.origin = s.origin + (s.target-s.origin)*std::min(0.10f, 0.02f*t);
      break;
    case M_Slide: {
      const Vec3 view = norm2(s.target-s.origin, fwdPl);
      Vec3       lat  = Vec3(-view.z, 0, view.x);
      if(Vec3::dotProduct(lat, n)<0.f)
        lat = lat*-1.f;
      s.origin = s.origin + lat*std::min(30.f, 5.f*t);
      break;
      }
    case M_Arc: {
      Vec3 off = s.origin-s.target;
      const Vec3 a = rotY(off,  std::min(6.f, 1.2f*t));
      const Vec3 b = rotY(off, -std::min(6.f, 1.2f*t));
      off = (Vec3::dotProduct(a-off, n)>=0.f) ? a : b;
      s.origin = s.target + off;
      break;
      }
    case M_Rise:
      s.origin = s.origin + up*std::min(14.f, 2.5f*t);
      s.target = s.target + up*std::min(4.f,  0.8f*t);
      break;
    }

  // in front of walls: the camera is pulled towards the subject
  float q = 1.f;
  if(auto physic = world.physic()) {
    const auto  r    = physic->ray(s.target, s.origin);
    const float full = (s.origin-s.target).length();
    if(r.hasCol && full>1.f) {
      const float hit = (r.v-s.target).length();
      const float nd  = std::max(20.f, hit-15.f);
      if(nd<full) {
        s.origin = s.target + (s.origin-s.target)*(nd/full);
        q        = nd/full;
        }
      }
    }
  s.focus = (s.target-s.origin).length();
  if(quality!=nullptr)
    *quality = q;
  return s;
  }

void ArpgDialogCam::pickSide(const Input& in, World& world) {
  // the side the ARPG camera was on (no jump in the picture), unless walls are closer there
  const Vec3  fwdPl = norm2(faceOther-facePl, Vec3(1, 0, 0));
  const Vec3  n0    = Vec3(-fwdPl.z, 0, fwdPl.x);
  const Vec3  mid   = (facePl+faceOther)*0.5f;
  const float pref  = (Vec3::dotProduct(flat(in.viewFrom-mid), n0)>=0.f) ? 1.f : -1.f;

  auto score = [&](float sd) {
    side = sd;
    float sum = 0;
    Take  tks[3];
    tks[0].kind = K_OverShoulder; tks[0].subject = in.pl;
    tks[1].kind = K_OverShoulder; tks[1].subject = in.other;
    tks[2].kind = K_Two;
    for(auto& tk:tks) {
      float q = 0;
      film(tk, in, world, &q);
      sum += q;
      }
    return sum;
    };
  const float sp = score(pref);
  const float so = score(-pref);
  side = (so>sp+0.3f) ? -pref : pref;
  }

void ArpgDialogCam::cut(Kind k, const Npc* who, const Input& in, World& world) {
  Take tk;
  tk.kind    = k;
  tk.subject = who;
  // a different movement than the last take
  tk.move    = Move((cuts*3u + uint32_t(k)) % 4u);
  if(k==K_Wide)
    tk.move = M_Rise;

  // a shot hidden by walls: the best other one
  float q = 1.f;
  film(tk, in, world, &q);
  if(q<0.5f && who!=nullptr) {
    float bestQ = q;
    for(Kind alt:{K_Close, K_Medium, K_OverShoulder}) {
      Take ta = tk;
      ta.kind = alt;
      float qa = 0;
      film(ta, in, world, &qa);
      if(qa>bestQ+0.05f) {
        tk.kind = alt;
        bestQ   = qa;
        }
      }
    }

  // the other one must not cover the face: big shoulder plates (necromancer robes, paladins), a bow on the back.
  // His head and shoulders keep 58 cm from the line of sight, else the camera goes further to his side.
  // Measured once at the cut: the take keeps its place (no jumps while it moves).
  if(who!=nullptr && tk.kind!=K_Two && tk.kind!=K_Wide) {
    const Vec3 up = Vec3(0, 1, 0);
    Vec3 x, y, n;
    pair(who, in, x, y, n);
    const Shot b  = base(tk.kind, who, in);
    for(int i=0; i<14; ++i) {
      const Vec3 o   = b.origin + n*tk.extraSide;
      const Vec3 eye = x+up*4.f;
      if(offLine(y, eye, o)>58.f && offLine(y-up*32.f, eye, o)>58.f && (o-y).length()>45.f)
        break;
      tk.extraSide += 12.f;
      }
    }

  take = tk;
  cuts++;
  }

void ArpgDialogCam::cutToSpeaker(const Input& in, World& world, bool newSpeaker) {
  Kind k = K_OverShoulder;
  if(newSpeaker) {
    // the other one speaks: mostly over the shoulder, every few cuts closer or wider
    static const Kind seq[] = {K_OverShoulder, K_OverShoulder, K_Close, K_OverShoulder, K_Medium};
    k = seq[cuts%5];
    } else {
    // the same one goes on: another size
    k = (take.kind==K_OverShoulder ? K_Close : (take.kind==K_Close ? K_Medium : K_OverShoulder));
    }
  cut(k, in.speaker, in, world);
  }

ArpgDialogCam::Shot ArpgDialogCam::tick(float dt, const Input& in, World& world) {
  if(in.pl==nullptr || in.other==nullptr)
    return last;

  // heads: smoothed (gestures move them), snapped on the first frame
  {
    const Vec3 a = face(*in.pl);
    const Vec3 b = face(*in.other);
    if(!faceInit) {
      facePl    = a;
      faceOther = b;
      faceInit  = true;
      } else {
      const float f = std::min(1.f, 5.f*dt);
      facePl    = facePl    + (a-facePl)*f;
      faceOther = faceOther + (b-faceOther)*f;
      }
  }
  if(in.third!=nullptr) {
    const Vec3 c = face(*in.third);
    if(in.third!=thirdSeen) {
      faceThird = c;
      thirdSeen = in.third;
      } else {
      faceThird = faceThird + (c-faceThird)*std::min(1.f, 5.f*dt);
      }
    }
  if(side==0.f) {
    pickSide(in, world);
    cut(K_Wide, nullptr, in, world); // the talk opens with the place
    }

  static constexpr float WideHold  = 1.6f;  // the opening shot stays at least this long
  static constexpr float MinHold   = 1.3f;  // no cut sooner while the face of the one who speaks is seen
  static constexpr float ShortLine = 2.0f;

  const Npc* listener = (in.speaker==in.pl ? in.other : in.pl);
  const bool opening  = (take.kind==K_Wide && take.t<WideHold);

  if(in.line!=line) {
    // a new line
    line        = in.line;
    lineT       = 0;
    prevLineLen = lineLen;
    lineLen     = in.lineLen;
    reacting    = false;
    reactDone   = false;
    if(in.speaker!=nullptr) {
      const bool newSpeaker = (in.speaker!=lineSpeaker);
      const bool thirdIn    = (in.third!=nullptr && (in.speaker==in.third || lineSpeaker==in.third));
      const bool fast       = newSpeaker && lineSpeaker!=nullptr && lineLen<ShortLine && prevLineLen<ShortLine && !thirdIn;
      if(opening) {
        pendingSpeaker = in.speaker;  // after the opening shot
        }
      else if(fast) {
        // short lines back and forth: one shot of both, no ping-pong
        if(!fastTalk || take.kind!=K_Two)
          cut(K_Two, nullptr, in, world);
        fastTalk = true;
        }
      else {
        fastTalk = false;
        if(newSpeaker || take.kind==K_Two || take.kind==K_Wide || take.subject!=in.speaker) {
          if(!(showsFace(in.speaker) && take.t<MinHold && take.kind!=K_Wide && take.kind!=K_Two))
            cutToSpeaker(in, world, true);
          }
        else if(take.t>3.5f && lineLen>=1.f) {
          // the same one goes on after a long take: another size
          cutToSpeaker(in, world, false);
          }
        }
      lineSpeaker = in.speaker;
      }
    } else {
    lineT += dt;
    }

  if(pendingSpeaker!=nullptr && !(take.kind==K_Wide && take.t<WideHold)) {
    if(in.speaker==pendingSpeaker)
      cutToSpeaker(in, world, true);
    pendingSpeaker = nullptr;
    }

  if(in.speaker!=nullptr && !fastTalk && take.kind!=K_Wide) {
    // long lines: a look at the listener in the middle, then back with another size
    if(lineLen>6.f && !reacting && !reactDone && lineT>lineLen*0.45f) {
      beforeReact = take.kind;
      reacting    = true;
      cut((cuts%2)==0 ? K_Close : K_Medium, listener, in, world);
      }
    else if(reacting && take.t>std::clamp(lineLen*0.18f, 1.4f, 2.2f)) {
      reacting  = false;
      reactDone = true;
      cut(beforeReact==K_Close ? K_Medium : K_Close, in.speaker, in, world);
      }
    }
  else if(in.speaker==nullptr) {
    reacting = false;
    if(in.choosing && !(take.kind==K_Wide && take.t<WideHold)) {
      // the player picks: over the hero's shoulder on the other one
      fastTalk = false;
      if(!(take.kind==K_OverShoulder && take.subject==in.other))
        cut(K_OverShoulder, in.other, in, world);
      }
    }

  take.t += dt;
  last = film(take, in, world, nullptr);
  return last;
  }
