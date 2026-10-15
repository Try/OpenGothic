#include "mainwindow.h"

#include <Tempest/MemReader>
#include <diathicassets.h>

#include "game/arpgkeys.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "ui/dialogmenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "utils/mouseutil.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "game/gamesession.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"

#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),
#if defined(__MOBILE_PLATFORM__)
    mobileUi(player),
#endif
    player(dialogs,inventory) {
  arpgMenu.setInventoryMenu(&inventory); // gothic-diablo: trade / chests / looting in the ARPG book
  Gothic::inst().onSettingsChanged.bind(this,&MainWindow::onSettings);
  onSettings();

  setWindowTitle("Diathic"); // Diathic: the name of the project (gothic-diablo is the internal code name)

  if(!CommandLine::inst().isWindowMode()) {
    setFullscreen(true);
    }

  //renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  Gothic::inst().onStartGame   .bind(this,&MainWindow::startGame);
  Gothic::inst().onLoadGame    .bind(this,&MainWindow::loadGame);
  Gothic::inst().onSaveGame    .bind(this,&MainWindow::saveGame);

  Gothic::inst().onStartLoading.bind(this,&MainWindow::onStartLoading);
  Gothic::inst().onWorldLoaded .bind(this,&MainWindow::onWorldLoaded);
  Gothic::inst().onSessionExit .bind(this,&MainWindow::onSessionExit);

  Gothic::inst().onVideo       .bind(this,&MainWindow::onVideo);
  Gothic::inst().onArpgSpeech  .bind(this,&MainWindow::onArpgSpeech);

  Gothic::inst().onBenchmarkFinished.bind(this,&MainWindow::onBenchmarkFinished);

  if(!Gothic::inst().defaultSave().empty()){
    Gothic::inst().load(Gothic::inst().defaultSave());
    rootMenu.popMenu();
    }
  else if(!CommandLine::inst().doStartMenu()) {
    startGame(Gothic::inst().defaultWorld());
    rootMenu.popMenu();
    }
  else {
    rootMenu.processMusicTheme();
    }

  funcKey[2] = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  funcKey[2].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F2>);

  funcKey[3] = Shortcut(*this,Event::M_NoModifier,Event::K_F3);
  funcKey[3].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F3>);

  funcKey[4] = Shortcut(*this,Event::M_NoModifier,Event::K_F4);
  funcKey[4].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F4>);

  funcKey[5] = Shortcut(*this,Event::M_NoModifier,Event::K_F5);
  funcKey[5].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F5>);

  funcKey[6] = Shortcut(*this,Event::M_NoModifier,Event::K_F6);
  funcKey[6].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F6>);

  funcKey[7] = Shortcut(*this,Event::M_NoModifier,Event::K_F7);
  funcKey[7].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F7>);

  funcKey[9] = Shortcut(*this,Event::M_NoModifier,Event::K_F9);
  funcKey[9].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F9>);

  funcKey[10] = Shortcut(*this,Event::M_NoModifier,Event::K_F10);
  funcKey[10].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F10>);

  displayPos = Shortcut(*this,Event::M_Alt,Event::K_P);
  displayPos.onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_P>);
  }

MainWindow::~MainWindow() {
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  takeWidget(&mobileUi);
#endif
  removeAllWidgets();
  dropArpgDialogLight(); // gothic-diablo: before its world goes
  // unload
  Gothic::inst().setGame(std::unique_ptr<GameSession>());
  }

float MainWindow::uiScale() const {
  return SystemApi::uiScale(hwnd());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  addWidget(&mobileUi);
#endif

  rootMenu.setMainMenu();

  Gothic::inst().onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  Gothic::inst().isNpcInDialogFn = std::bind(&DialogMenu::isNpcInDialog, &dialogs, std::placeholders::_1);

  Gothic::inst().onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  Gothic::inst().onPrint       .bind(&dialogs,&DialogMenu::print);

  Gothic::inst().onIntroChapter.bind(&chapter, &ChapterScreen::show);
  Gothic::inst().onShowDocument.bind(&document,&DocumentMenu::show);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);
  arpgBarItems.reset(); // gothic-diablo: skill bar models, added again below when the bar is shown
  arpgCursorItem.reset();
  auto world = Gothic::inst().world();
  auto st    = Gothic::inst().checkLoading();

  if(!Gothic::inst().isInGame() && st==Gothic::LoadState::Idle && background.isEmpty()) {
    // Diathic: our picture behind the main menu (the game's STARTSCREEN when it is missing)
    auto bg = DiathicAsset::get("menu_bg.jpg");
    if(!bg.empty()) {
      try {
        Tempest::MemReader rd(bg.data, bg.len);
        Tempest::Pixmap    pm(rd);
        background = Resources::loadTexturePm(pm);
        }
      catch(...) {
        Log::e("unable to load the Diathic menu background");
        }
      }
    if(background.isEmpty())
      background = Resources::loadTextureUncached("STARTSCREEN.TGA");
    }

  if(world==nullptr && !background.isEmpty()) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle)
      paintMenuBackground(p);
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  if(st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(auto back = Gothic::inst().loadingBanner()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        }
      if(loadBox!=nullptr && !loadBox->isEmpty()) {
        if(Gothic::inst().version().game==1) {
          int lw = int(w()*0.5);
          int lh = int(h()*0.05);
          drawLoading(p,(w()-lw)/2, int(h()*0.75), lw, lh);
          } else {
          drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
          }
        }
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *Gothic::inst().camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      drawMsg(p);

      auto focus = world->validateFocus(player.focus());
      auto pl0   = Gothic::inst().player();
      if(arpgWorldMap.isOpen() && pl0!=nullptr) {
        // Diathic: the big map over the whole screen
        arpgLabels.clear();
        ArpgWorldMap::Ctx ctx{*world, *pl0, camera, player, w(), h(), Gothic::interfaceScale(this)};
        arpgWorldMap.paint(p, ctx);
        }
      else if(arpgMenu.isOpen()) {
        // gothic-diablo: equipment / character window; the hero stands in the middle (camera in front of him)
        arpgLabels.clear();
        if(auto pl = Gothic::inst().player())
          arpgMenu.paint(p, w(), h(), Gothic::interfaceScale(this), arpgBarItems, *pl);
        }
      else if(isArpgMouse()) {
        // Diathic: Tab (second time) = the explored map over the game, like Diablo II
        if(arpgOverlay && pl0!=nullptr) {
          ArpgWorldMap::Ctx ctx{*world, *pl0, camera, player, w(), h(), Gothic::interfaceScale(this)};
          arpgWorldMap.paintOverlay(p, ctx);
          }
        // gothic-diablo: labels like in Diablo; Z = labels of everything in sight
        if(arpgShowAll)
          paintArpgLabels(p,vp,focus); else
          arpgLabels.clear();
        if(!arpgShowAll || focus.npc!=nullptr)
          paintArpgFocus(p,focus,vp);
        paintArpgCombat(p,vp);
        paintArpgRemarks(p,vp);
        paintArpgCombo(p,vp);
        paintArpgMinimap(p);
        paintArpgBar(p);
        } else {
        arpgLabels.clear();
        if(!dialogs.isArpgCinema())
          paintFocus(p,focus,vp); // gothic-diablo: nothing over the film of a dialog
        }

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop() && !arpgMenu.isOpen() && !dialogs.isArpgCinema() && !arpgWorldMap.isOpen()) {
          auto& opt = Gothic::options();
          float hp  = float(pl->attribute(ATR_HITPOINTS))/float(pl->attribute(ATR_HITPOINTSMAX));
          float mp  = float(pl->attribute(ATR_MANA))     /float(pl->attribute(ATR_MANAMAX));

          bool showHealthBar = opt.showHealthBar;
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

          if(arpgBarVisible()) {
            paintArpgVitals(p); // Diathic: life and mana next to the skill bar, experience over it
            showHealthBar = false;
            showManaBar   = false;
            }
          if(showHealthBar)
            drawBar(p,barHp, 10, h()-10, hp, AlignLeft | AlignBottom);
          if(showManaBar)
            drawBar(p,barMana, w()-10, h()-10, mp, AlignRight | AlignBottom);
          if(showSwimBar) {
            uint32_t gl = pl->guild();
            auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
            if(v>0) {
              auto t = float(pl->diveTime())/1000.f;
              drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
              }
            }
          }
        }
      }
    }

  if(auto c = Gothic::inst().camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      world->view()->dbgLights(dbg);
      world->marchCsCameras(dbg);
      }
    }

  renderer.dbgDraw(p);

  const float scale = Gothic::interfaceScale(this);
  if(Gothic::inst().doFrate() && !Gothic::inst().isDesktop()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

    auto& fnt = Resources::font(scale);
    fnt.drawText(p,5,fnt.pixelSize()+5,fpsT);
    }

  if(!Gothic::inst().isDesktop() && world!=nullptr) {
    if(Gothic::inst().doClock()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
      }

    auto c = Gothic::inst().camera();
    if(Gothic::inst().doVobBox() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      world->drawVobBoxNpcNear(dbg);
      }

    if(Gothic::inst().doVobRays() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      player.drawVobRay(dbg);
      }
    }

  if(auto wx = Gothic::inst().worldView()) {
    wx->dbgClusters(p, Vec2(float(w()), float(h())));
    }

  paintArpgCursor();
  }

// Diathic: the picture of the main menu fills the screen (cut, not stretched), very slowly drifting closer and back,
// with embers rising over it
void MainWindow::paintMenuBackground(Painter& p) {
  const float    W = float(w()), H = float(h());
  const float    tw = float(background.w()), th = float(background.h());
  const uint64_t now = Application::tickCount();
  const float    t   = float(now%120000)/120000.f;               // 2 minutes there and back
  const float    zoom = 1.03f + 0.04f*(0.5f-0.5f*std::cos(t*2.f*3.14159265f));

  float sw = tw, sh = tw*H/W;
  if(sh>th) {
    sh = th;
    sw = th*W/H;
    }
  sw /= zoom;
  sh /= zoom;
  const float sx = (tw-sw)*0.55f, sy = (th-sh)*0.45f;
  p.setBrush(Brush(background, Painter::NoBlend));
  p.drawRect(0, 0, w(), h(), int(sx), int(sy), int(sx+sw), int(sy+sh));

  // embers: small sparks rising and fading (additive)
  const float dt = menuEmbersAt==0 ? 0.f : float(std::min<uint64_t>(now-menuEmbersAt, 100))/1000.f;
  menuEmbersAt = now;
  while(menuEmbers.size()<70) {
    MenuEmber e;
    e.x    = float(std::rand()%1000)/1000.f;
    e.y    = 1.02f + float(std::rand()%300)/1000.f;
    e.vy   = 0.03f + float(std::rand()%100)/1000.f*0.06f;
    e.vx   = (float(std::rand()%100)/100.f-0.5f)*0.02f;
    e.size = 1.5f + float(std::rand()%100)/100.f*2.5f;
    e.life = 0.f;
    menuEmbers.push_back(e);
    }
  const float k = H/1080.f;
  for(auto& e:menuEmbers) {
    e.life += dt;
    e.y    -= e.vy*dt;
    e.x    += (e.vx + 0.01f*std::sin(e.life*1.7f + e.size))*dt;
    const float a = std::clamp(1.f-(1.02f-e.y)/0.9f, 0.f, 1.f) * std::clamp(e.life*2.f, 0.f, 1.f);
    const int   s = std::max(1, int(e.size*k));
    p.setBrush(Brush(Color(1.f, 0.55f, 0.18f, 0.85f*a), Painter::Add));
    p.drawRect(int(e.x*W), int(e.y*H), s, s);
    }
  menuEmbers.erase(std::remove_if(menuEmbers.begin(), menuEmbers.end(),
                                  [](const MenuEmber& e){ return e.y<0.08f || e.x<-0.05f || e.x>1.05f; }),
                   menuEmbers.end());
  }

// Diathic: in the ARPG mode the cursor is a sword from the game (Miecz paladyna), red over an enemy;
// the tip is the hot spot, the hilt goes down to the right; the system cursor is hidden meanwhile
void MainWindow::paintArpgCursor() {
  const bool show = Camera::isArpgEnabled() && arpgCursorValid && Gothic::inst().world()!=nullptr &&
                    !video.isActive() && Gothic::inst().checkLoading()==Gothic::LoadState::Idle;
  const CursorShape want = show ? CursorShape::Hidden
                                : ((SystemApi::isFullscreen(hwnd()) && !Camera::isArpgEnabled()) ? CursorShape::Hidden : CursorShape::Arrow);
  if(cursorShape()!=want)
    setCursorShape(want);
  if(!show)
    return;

  const float k     = float(h())/1080.f;
  const int   sz    = std::max(24, int(56.f*k)); // the blade is about as long as the box
  const bool  enemy = isArpgMouse() && !rootMenu.isActive() && player.arpgCursorOnEnemy();
  const Vec4  tint  = enemy ? Vec4(1.35f, 0.32f, 0.28f, 1.f) : Vec4(1.f, 1.f, 1.f, 0.f);
  arpgCursorItem.drawPointer(arpgCursor.x, arpgCursor.y, sz, ArpgCursorModel, tint, ArpgCursorFlip);
  }

void MainWindow::resizeEvent(SizeEvent&) {
  for(auto& i:fence)
    i.wait();
  swapchain.reset();
  if(auto camera = Gothic::inst().camera())
    camera->setViewport(swapchain.w(),swapchain.h());

  if(SystemApi::isFullscreen(hwnd()) && !Camera::isArpgEnabled()) {
    setCursorPosition(Point(w()/2,h()/2));
    }

  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(arpgWorldMap.isOpen()) {
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    arpgWorldMap.mouseDown(event); // Diathic: the big map takes the mouse
    return;
    }
  if(arpgMenu.isOpen()) {
    // gothic-diablo: the ARPG window takes the mouse
    if(auto pl = Gothic::inst().player())
      arpgMenu.mouseDown(event.pos(), event.button==Event::ButtonRight, (event.modifier & Event::M_Shift)!=0, *pl);
    return;
    }
  if(event.button==Event::ButtonLeft && isArpgMouse()) {
    // gothic-diablo: left button = move / interact / attack under the cursor
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    arpgLeftDown    = true;
    updateArpgAim();
    player.tickFocus();
    if(arpgShowAll) {
      // click on a label picks that object even if something else is under the cursor
      auto w = Gothic::inst().world();
      auto f = arpgLabelAt(event.pos());
      if(w!=nullptr && f)
        f = w->validateFocus(f);
      if(f) {
        player.arpgMouseDown(f);
        return;
        }
      }
    player.arpgMouseDown();
    return;
    }
  if(event.button==Event::ButtonMid && isArpgMouse() && player.arpgCanBlock()) {
    // gothic-diablo: melee weapon in hand: middle button held = block
    arpgMidBlock = true;
    player.arpgBlock(true);
    return;
    }
  if(event.button==Event::ButtonMid && isArpgMouse() && player.arpgCanPush()) {
    // gothic-diablo: bow / crossbow in hand: middle button = push the enemies in front away
    arpgMidPush = true;
    updateArpgAim();
    player.tickFocus();
    player.arpgPush();
    return;
    }
  if(event.button==Event::ButtonMid && isArpgMouse()) {
    // gothic-diablo: middle button drag = rotate the camera; a plain click keeps its binding
    arpgMidDown     = true;
    arpgMidRotating = false;
    arpgMidStart    = event.pos();
    arpgMidLast     = event.pos();
    return;
    }
  if(event.button==Event::ButtonRight && isArpgMouse()) {
    // gothic-diablo: right button held = charge the special attack, release = strike (dodge is on Space)
    arpgRightDown = true;
    updateArpgAim();
    player.tickFocus();
    player.arpgChargeStart();
    return;
    }
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  if(arpgWorldMap.isOpen()) {
    arpgWorldMap.mouseUp(event);
    return;
    }
  if(arpgMenu.isOpen()) {
    // gothic-diablo: end of a click or of dragging an item in the ARPG window
    if(auto pl = Gothic::inst().player())
      arpgMenu.mouseUp(event.pos(), event.button==Event::ButtonRight, *pl);
    }
  if(arpgMenu.isOpen() && !arpgLeftDown && !arpgRightDown && !arpgMidDown && !arpgMidBlock && !arpgMidPush)
    return;
  if(event.button==Event::ButtonLeft && arpgLeftDown) {
    arpgLeftDown = false;
    player.arpgMouseUp();
    return;
    }
  if(event.button==Event::ButtonRight && arpgRightDown) {
    arpgRightDown = false;
    player.arpgChargeRelease();
    return;
    }
  if(event.button==Event::ButtonMid && arpgMidBlock) {
    arpgMidBlock = false;
    player.arpgBlock(false);
    return;
    }
  if(event.button==Event::ButtonMid && arpgMidPush) {
    arpgMidPush = false;
    return;
    }
  if(event.button==Event::ButtonMid && arpgMidDown) {
    arpgMidDown = false;
    if(!arpgMidRotating) {
      auto act     = keycodec.tr(event);
      auto mapping = keycodec.mapping(event);
      player.onKeyPressed (act,KeyEvent::K_NoKey,mapping);
      player.onKeyReleased(act,mapping);
      }
    arpgMidRotating = false;
    return;
    }
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  if(arpgWorldMap.isOpen()) {
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    arpgWorldMap.mouseMove(event);
    return;
    }
  if(Camera::isArpgEnabled()) {
    if(arpgMidDown) {
      if(!arpgMidRotating && std::abs(event.x-arpgMidStart.x)>6)
        arpgMidRotating = true;
      if(arpgMidRotating) {
        if(auto c = Gothic::inst().camera())
          c->arpgRotate(-float(event.x-arpgMidLast.x)*0.3f);
        }
      arpgMidLast = event.pos();
      }
    arpgMenu.mouseMove(event.pos());
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    return;
    }
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
  if(Camera::isArpgEnabled()) {
    // gothic-diablo: free, visible cursor; the hero turns towards it
    arpgWorldMap.mouseMove(event);
    arpgMenu.mouseMove(event.pos());
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    return;
    }
  auto center = Point(w()/2,h()/2);
  if(enable && event.pos()!=center && hasFocus()) {
    dMouse += (event.pos()-center);
    setCursorPosition(center);
    }
  }

void MainWindow::tickMouse(uint64_t dt) {
  auto camera = Gothic::inst().camera();
  if(dialogs.hasContent() || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = Gothic::inst().settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  if(dMouse==Point())
    return;

  const bool  camLookaroundInverse = Gothic::inst().settingsGetI("GAME","camLookaroundInverse");
  const float mouseSensitivity     = Gothic::inst().settingsGetF("GAME","mouseSensitivity")/MouseUtil::mouseSysSpeed();
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());
  if(camLookaroundInverse)
    dpScaled.y *= -1.f;

  static float mul = 270.f;
  dpScaled *= mul;

  static float psMax = 720.f;
  const float  dtF   = float(dt)/1000.f;
  dpScaled.x = std::clamp(dpScaled.x, -(psMax*dtF), psMax*dtF);
  dpScaled.y = std::clamp(dpScaled.y, -(psMax*dtF), psMax*dtF);

  // Log::d("mouse dMouse   = ", dMouse.x,   ", ", dMouse.y);
  // Log::d("mouse dpScaled = ", dpScaled.x, ", ", dpScaled.y);

  camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse(-dpScaled.x, -dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::updateArpgAim() {
  // gothic-diablo: world ray under the mouse cursor, for picking and click-to-move
  PlayerControl::ArpgCursor cur;
  auto camera = Gothic::inst().camera();
  auto pl     = Gothic::inst().player();
  if(!Camera::isArpgEnabled() || !arpgCursorValid || camera==nullptr || pl==nullptr ||
     !camera->isArpgActive() || w()<=0 || h()<=0) {
    player.setArpgCursor(cur);
    return;
    }

  const float x = (float(arpgCursor.x)+0.5f)/float(w())*2.f - 1.f;
  const float y = (float(arpgCursor.y)+0.5f)/float(h())*2.f - 1.f;

  const auto vp  = camera->viewProj();
  auto       inv = vp;
  inv.inverse();
  Vec4 p0 = {x, y, 0.f, 1.f};
  Vec4 p1 = {x, y, 1.f, 1.f};
  inv.project(p0);
  inv.project(p1);
  if(p0.w==0.f || p1.w==0.f) {
    player.setArpgCursor(cur);
    return;
    }

  cur.valid    = true;
  cur.rayFrom  = Vec3(p0.x, p0.y, p0.z)/p0.w;
  cur.rayTo    = Vec3(p1.x, p1.y, p1.z)/p1.w;
  cur.viewProj = vp;
  cur.pixel    = Vec2(float(arpgCursor.x), float(arpgCursor.y));
  cur.screen   = Vec2(float(w()), float(h()));
  player.setArpgCursor(cur);
  }

bool MainWindow::isArpgMouse() const {
  auto camera = Gothic::inst().camera();
  return Camera::isArpgEnabled() && camera!=nullptr && camera->isArpgActive() &&
         Gothic::inst().player()!=nullptr &&
         !dialogs.isActive() && !inventory.isActive() && !document.isActive() && !arpgMenu.isOpen() &&
         !arpgWorldMap.isOpen();
  }

void MainWindow::onSettings() {
  auto zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(arpgWorldMap.isOpen()) {
    arpgWorldMap.mouseWheel(event);
    return;
    }
  if(arpgMenu.isOpen()) {
    arpgMenu.mouseWheel(event.delta); // backpack rows
    return;
    }
  if(arpgMinimap && arpgMapRect.w>0 && arpgMapRect.contains(event.pos()) && isArpgMouse()) {
    // gothic-diablo: wheel over the minimap zooms the minimap (20..100 m across)
    const float k = event.delta>0 ? 1.f/1.2f : 1.2f;
    arpgMapHalf = std::clamp(arpgMapHalf*k, 1000.f, 5000.f);
    return;
    }
  if(auto camera = Gothic::inst().camera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(video.isActive()){
    event.accept();
    video.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&video;
      return;
      }
    }

  if(rootMenu.isActive()) {
    event.accept();
    rootMenu.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&rootMenu;
      return;
      }
    }

  if(chapter.isActive()){
    event.accept();
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(document.isActive()){
    event.accept();
    document.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&document;
      return;
      }
    }

  if(dialogs.isActive()){
    event.accept();
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    event.accept();
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;

  if(arpgWorldMap.isOpen()) {
    // Diathic: the big map: the note of a pin, M / Esc close it (on key-up), nothing else reaches the game
    event.accept();
    if(arpgWorldMap.keyDown(event))
      return;
    if(event.key==Event::K_ESCAPE || ArpgKeys::is(ArpgKeys::Map, event.key))
      arpgMapKey = event.key;
    return;
    }

  // Diathic: the keys of the ARPG mode (ArpgKeys; "Sterowanie" in the main menu)
  const int ak = Camera::isArpgEnabled() ? ArpgKeys::actionOf(event.key) : -1;
  if(ak==ArpgKeys::Labels)
    arpgShowAll = true; // gothic-diablo: hold to show all items / usable objects in sight

  if(arpgMenu.isOpen()) {
    // gothic-diablo: the ARPG window is open: I / C / T switch or close it, Esc closes it,
    // 3-0 put the rune / scroll under the cursor on that key, the camera turn keys still turn the camera
    event.accept();
    if(event.key==Event::K_ESCAPE || ak==ArpgKeys::Equipment || ak==ArpgKeys::Character || ak==ArpgKeys::Skills) {
      arpgMenuKey = event.key;
      return;
      }
    if((event.key>=Event::K_3 && event.key<=Event::K_9) || event.key==Event::K_0) {
      if(auto pl = Gothic::inst().player())
        arpgMenu.spellKey(event.key==Event::K_0 ? uint8_t(10) : uint8_t(event.key-Event::K_0), *pl);
      return;
      }
    auto act = keycodec.tr(event);
    if(act==KeyCodec::RotateL || act==KeyCodec::RotateR)
      player.onKeyPressed(act,event.key,keycodec.mapping(event));
    return;
    }

  if(Camera::isArpgEnabled() && Gothic::inst().player()!=nullptr) {
    // gothic-diablo: character / skill trees (with the equipment: ArpgMenu, acted on key-up)
    if((ak==ArpgKeys::Character || ak==ArpgKeys::Skills) && isArpgMouse()) {
      arpgMenuKey = event.key;
      event.accept();
      return;
      }
    // gothic-diablo: minimap -> map over the game (Diathic) -> nothing; equipment window (Backspace = Gothic's inventory)
    if(ak==ArpgKeys::Minimap) {
      if(!arpgMinimap && !arpgOverlay)
        arpgMinimap = true;
      else if(arpgMinimap) {
        arpgMinimap = false;
        arpgOverlay = true;
        }
      else
        arpgOverlay = false;
      arpgTabDown = true;
      event.accept();
      return;
      }
    // Diathic: the big map (instead of Gothic's map document)
    if(ak==ArpgKeys::Map && isArpgMouse()) {
      arpgMapKey = event.key;
      event.accept();
      return;
      }
    if(ak==ArpgKeys::Equipment) {
      arpgIDown = true;
      event.accept();
      return;
      }
    if(ak==ArpgKeys::Log && isArpgMouse()) {
      arpgLogDown = true; // the diary (Gothic's MENU_LOG) on key-up
      event.accept();
      return;
      }
    if(ak==ArpgKeys::Labels) {
      event.accept();
      return;
      }
    }

  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto tex = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
    auto pm  = device.readPixels(textureCast<const Texture2d&>(tex));
    pm.save("dbg.png");
    }
  event.accept();
  }

void MainWindow::keyRepeatEvent(KeyEvent& event) {
  if(uiKeyUp==&video){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    rootMenu.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  const int ak = Camera::isArpgEnabled() ? ArpgKeys::actionOf(event.key) : -1;
  if(ak==ArpgKeys::Labels)
    arpgShowAll = false;
  if(arpgMapKey!=Event::K_NoKey && event.key==arpgMapKey) {
    // Diathic: M opens / closes the big map, Esc closes it
    arpgMapKey = Event::K_NoKey;
    arpgWorldMap.setOpen(event.key!=Event::K_ESCAPE ? !arpgWorldMap.isOpen() : false);
    if(arpgWorldMap.isOpen()) {
      player.arpgStop();
      clearInput();
      }
    return;
    }
  if(arpgWorldMap.isOpen())
    return;
  if(ak==ArpgKeys::Minimap && arpgTabDown) {
    arpgTabDown = false;
    return;
    }
  if(ak==ArpgKeys::Log && arpgLogDown) {
    arpgLogDown = false;
    if(!dialogs.isActive() && !inventory.isActive()) {
      rootMenu.setMenu("MENU_LOG", KeyCodec::Log);
      if(auto pl = Gothic::inst().player())
        rootMenu.setPlayer(*pl);
      clearInput();
      }
    return;
    }
  if(ak==ArpgKeys::Equipment && arpgIDown) {
    arpgIDown = false;
    if(!dialogs.isActive() && !inventory.isActive() && Gothic::inst().player()!=nullptr) {
      arpgMenu.toggle(ArpgMenu::P_Equip); // gothic-diablo: the ARPG equipment window
      player.arpgStop();
      clearInput();
      }
    return;
    }
  if(arpgMenuKey!=Event::K_NoKey && event.key==arpgMenuKey) {
    arpgMenuKey = Event::K_NoKey;
    if(event.key==Event::K_ESCAPE)
      arpgMenu.close();
    else if(ak==ArpgKeys::Equipment)
      arpgMenu.toggle(ArpgMenu::P_Equip);
    else if(ak==ArpgKeys::Character)
      arpgMenu.toggle(ArpgMenu::P_Character);
    else if(ak==ArpgKeys::Skills)
      arpgMenu.toggle(ArpgMenu::P_Skills);
    if(arpgMenu.isOpen()) {
      player.arpgStop();
      clearInput();
      }
    return;
    }
  if(uiKeyUp==&video){
    video.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    document.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }


  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);

  if(ak>=0 && act!=KeyCodec::Escape) {
    // Diathic: a key of the ARPG mode does not also do what Gothic binds to it (status, log, inventory...)
    player.onKeyReleased(act, mapping);
    return;
    }

  std::string_view menuEv;
  if(act==KeyCodec::Escape)
    menuEv = Gothic::inst().menuMain();
  else if(act==KeyCodec::Log)
    menuEv = "MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv = "MENU_STATUS";

  if(!menuEv.empty()) {
    rootMenu.setMenu(menuEv,act);
    rootMenu.showVersion(act==KeyCodec::Escape);
    if(auto pl = Gothic::inst().player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory && !dialogs.isActive()) {
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = Gothic::inst().player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  player.onKeyReleased(act, mapping);
  }

void MainWindow::focusEvent(FocusEvent &event) {
  if(!event.in) {
    // gothic-diablo: key-ups are lost while the window is in the background
    player.clearInput();
    player.arpgBlock(false);
    player.arpgChargeRelease();
    arpgLeftDown = arpgRightDown = arpgMidDown = arpgShowAll = arpgMidBlock = false;
    return;
    }
  dMouse = Point();

  if(SystemApi::isFullscreen(hwnd()) && !Camera::isArpgEnabled())
    setCursorPosition(Point(w()/2,h()/2));
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto        world = Gothic::inst().world();
  auto        pl    = world==nullptr ? nullptr : world->player();
  if(pl==nullptr)
    return;

  auto pw  = 1.f;
  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);

  if(pw<=0.f)
    return;

  pos /= pw;

  int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font(scale);

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  fnt.drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }

  const int foc = Gothic::settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto tr = vp;
    tr.mul(focus.npc->transform());

    const auto b    = focus.npc->bounds();
    const auto bbox = b.bbox; //focus.npc->bBox();

    Vec3 bx[] = {
      {bbox[0].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[1].y, bbox[1].z},
      {bbox[0].x, bbox[1].y, bbox[1].z},
      };

    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      tr.project(bx[i]);
      int x = int((bx[i].x*0.5f+0.5f)*float(w()));
      int y = int((bx[i].y*0.5f+0.5f)*float(h()));
      min[0] = std::min(x,min[0]);
      min[1] = std::min(y,min[1]);
      max[0] = std::max(x,max[0]);
      max[1] = std::max(y,max[1]);
      }

    paintFocus(p,Rect(min[0],min[1],max[0]-min[0],max[1]-min[1]));
    }

  // focusImg
  /*
  if(auto pl = focus.interactive){
    pl->marchInteractives(p,vp,w(),h());
    }*/
  }

bool MainWindow::arpgLabelRect(const Focus& f, const Matrix4x4& vp, Rect& out) const {
  auto name = f.displayName();
  if(name.empty())
    return false;
  auto pw  = 1.f;
  auto pos = f.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);
  if(pw<=0.f)
    return false;
  pos /= pw;
  if(pos.x<-1.f || pos.x>1.f || pos.y<-1.f || pos.y>1.f)
    return false;
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);
  const int   pad   = int(4.f*scale);
  const int   ix    = int((0.5f*pos.x+0.5f)*float(w()));
  const int   iy    = int((0.5f*pos.y+0.5f)*float(h()));
  const auto  ts    = fnt.textSize(name);
  out = Rect(ix-ts.w/2-pad, iy-ts.h-pad, ts.w+2*pad, ts.h+2*pad);
  out.y = std::clamp(out.y, 0, std::max(0, h()-out.h));
  return true;
  }

void MainWindow::drawArpgLabel(Painter& p, const Rect& r, const Focus& f, bool hi) {
  const float scale = Gothic::interfaceScale(this);
  const int   pad   = int(4.f*scale);
  const bool  enemy = (f.npc!=nullptr && player.arpgIsEnemy(*f.npc));

  p.setBrush(Brush(hi ? Color(0.25f,0.2f,0.05f,0.8f) : Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
  p.drawRect(r.x, r.y, r.w, r.h);

  auto type = Resources::FontType::Normal;
  if(enemy)
    type = Resources::FontType::Red;
  else if(hi)
    type = Resources::FontType::Hi;
  else if(f.interactive!=nullptr)
    type = Resources::FontType::Yellow;
  auto& fnt = Resources::font(type, scale);
  p.setBrush(Color(1.0));
  fnt.drawText(p, r.x+pad, r.y+r.h-pad, f.displayName());
  }

void MainWindow::paintArpgFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;
  Rect r;
  if(arpgLabelRect(focus, vp, r))
    drawArpgLabel(p, r, focus, true);
  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(std::max(1,focus.npc->attribute(ATR_HITPOINTSMAX)));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }
  }

void MainWindow::paintArpgCombo(Painter& p, const Matrix4x4& vp) {
  // small bar above the hero: fills up until the moment for the next combo hit, gold = now
  auto pl = Gothic::inst().player();
  if(pl==nullptr || dialogs.isActive() || inventory.isActive())
    return;
  const auto ws = pl->weaponState();
  if(ws!=WeaponState::Fist && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return;
  if(player.arpgChargeTime()>=0.f)
    return; // the charge bar is shown instead
  const auto ct = pl->comboTiming();
  if(!ct.attack || ct.last || ct.missed)
    return;

  auto pw  = 1.f;
  auto pos = pl->position() + Vec3(0,210.f,0);
  vp.project(pos.x,pos.y,pos.z,pw);
  if(pw<=0.f)
    return;
  pos /= pw;

  const float scale  = Gothic::interfaceScale(this);
  const bool  queued = player.arpgAttackQueued();
  const int   bw     = int(90.f*scale);
  const int   bh     = std::max(4, int(8.f*scale));
  const int   x      = int((0.5f*pos.x+0.5f)*float(w())) - bw/2;
  const int   y      = int((0.5f*pos.y+0.5f)*float(h())) - bh/2;

  // next hit already clicked (queued): white frame, it goes off by itself when the window opens
  if(queued) {
    p.setBrush(Brush(Color(1.f,1.f,1.f,0.85f), Painter::Alpha));
    p.drawRect(x-3, y-3, bw+6, bh+6);
    }
  p.setBrush(Brush(Color(0.f,0.f,0.f,0.75f), Painter::Alpha));
  p.drawRect(x-1, y-1, bw+2, bh+2);
  if(ct.broken) {
    p.setBrush(Brush(Color(0.8f,0.15f,0.1f,0.85f), Painter::Alpha));
    p.drawRect(x, y, bw, bh);
    }
  else if(ct.open) {
    const int fw = std::max(2, int(float(bw)*ct.windowLeft));
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.95f), Painter::Alpha));
    p.drawRect(x+(bw-fw)/2, y, fw, bh);
    }
  else {
    p.setBrush(Brush(Color(0.8f,0.8f,0.8f,0.6f), Painter::Alpha));
    p.drawRect(x, y, int(float(bw)*ct.progress), bh);
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.95f), Painter::Alpha));
    p.drawRect(x+bw-2, y, 2, bh); // where the window opens
    }

  // hits chained so far: one gold pip per hit under the bar
  const int pip = std::max(4, int(7.f*scale));
  for(int i=0; i<=int(ct.len) && i<6; ++i) {
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.75f), Painter::Alpha));
    p.drawRect(x + i*(pip+3)-1, y+bh+4-1, pip+2, pip+2);
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.95f), Painter::Alpha));
    p.drawRect(x + i*(pip+3), y+bh+4, pip, pip);
    }
  }

void MainWindow::paintArpgLabels(Painter& p, const Matrix4x4& vp, const Focus& focus) {
  arpgLabels.clear();
  player.arpgVisibleObjects(arpgLabelObjs);
  if(arpgLabelObjs.empty())
    return;

  for(auto& f:arpgLabelObjs) {
    ArpgLabel l;
    if(!arpgLabelRect(f, vp, l.rect))
      continue;
    l.focus = f;
    arpgLabels.push_back(l);
    }

  // lowest on screen (usually closest) first, then push overlapping labels upwards
  std::sort(arpgLabels.begin(), arpgLabels.end(), [](const ArpgLabel& a, const ArpgLabel& b){
    return a.rect.y>b.rect.y;
    });
  auto overlaps = [](const Rect& a, const Rect& b) {
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
    };
  for(size_t i=0; i<arpgLabels.size(); ++i) {
    for(int iter=0; iter<12; ++iter) {
      bool hit = false;
      for(size_t r=0; r<i; ++r)
        if(overlaps(arpgLabels[i].rect, arpgLabels[r].rect)) {
          arpgLabels[i].rect.y = arpgLabels[r].rect.y - arpgLabels[i].rect.h - 1;
          hit = true;
          }
      if(!hit)
        break;
      }
    }

  // highlighted: label under the cursor, otherwise the object picked under the cursor
  Focus hover = arpgLabelAt(arpgCursor);
  if(!hover && focus.npc==nullptr)
    hover = focus;
  for(auto& l:arpgLabels) {
    const bool hi = bool(hover) && l.focus.item==hover.item && l.focus.interactive==hover.interactive;
    drawArpgLabel(p, l.rect, l.focus, hi);
    }
  }

Focus MainWindow::arpgLabelAt(const Point& pos) const {
  // labels drawn last are on top
  for(size_t i=arpgLabels.size(); i>0; --i) {
    auto& l = arpgLabels[i-1];
    if(l.rect.contains(pos))
      return l.focus;
    }
  return Focus();
  }

void MainWindow::paintFocus(Painter& p, Rect rect) {
  if(focusImg==nullptr)
    return;
  const int w2 = focusImg->w();
  const int h2 = focusImg->h();
  const int w  = w2/2;
  const int h  = h2/2;

  if(rect.w<w) {
    int dw = w-rect.w;
    rect.x -= dw/2;
    rect.w += dw;
    }
  if(rect.h<h) {
    int dh = h-rect.h;
    rect.y -= dh/2;
    rect.h += dh;
    }

  p.setBrush(Brush(*focusImg,Painter::Add));
  p.drawRect(rect.x,         rect.y,         w,h, 0,0, w, h);
  p.drawRect(rect.x+rect.w-w,rect.y,         w,h, w,0, w2,h);
  p.drawRect(rect.x,         rect.y+rect.h-h,w,h, 0,h, w, h2);
  p.drawRect(rect.x+rect.w-w,rect.y+rect.h-h,w,h, w,h, w2,h2);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*24.f/32.f;
  //const float destHin = 20;//float(destH)*24.f/32.f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*(destH-destHin));
  float pd = 9.f*k;
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(destHin),
             0,0,bar->w(),bar->h());
  }

void MainWindow::drawMsg(Tempest::Painter& p) {
  if(barBack==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;

  const int y = 10 + int(destH) + 10;
  dialogs.drawMsg(p, y);
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  int paddL = int((float(w)*75.f)/float(loadBox->w()));
  int paddT = int((float(h)*10.f)/float(loadBox->h()));

  if(Gothic::inst().version().game==1) {
    paddL = int((float(w)*30.f)/float(loadBox->w()));
    paddT = int((float(h)* 5.f)/float(loadBox->h()));
    }

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = Gothic::inst().loadingBanner()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  int         szX   = Gothic::options().saveGameImageSize.w;
  int         szY   = Gothic::options().saveGameImageSize.h;

  if(szX<=460 || szY<=300) {
    // way too small otherwise
    szX = 460;
    szY = 300;
    }
  szX = int(float(szX)*scale);
  szY = int(float(szY)*scale);
  drawSaving(p,*saveback,szX,szY,scale);
  }

void MainWindow::drawSaving(Painter& p, const Tempest::Texture2d& back, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(back);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh, 0,0,back.w(),back.h());
    }

  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive());
  }

template<Tempest::KeyEvent::KeyType k>
void MainWindow::onMarvinKey() {
  switch(k) {
    case Event::K_F2:
      if(Gothic::inst().isMarvinEnabled()) {
        console.resize(w(),h());
        console.setFocus(true);
        console.exec();
        }
      break;
    case Event::K_F3:
      setFullscreen(!SystemApi::isFullscreen(hwnd()));
      break;
    case Event::K_F4:
      if(Gothic::inst().isMarvinEnabled()) {
        auto camera = Gothic::inst().camera();
        auto pl = Gothic::inst().player();
        if(camera!=nullptr && pl!=nullptr) {
          camera->setMarvinMode(Camera::M_Normal);
          camera->reset(pl);
          }
        }
      break;
    case Event::K_F5: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
#ifdef NDEBUG
      const bool debug = false;
#else
      const bool debug = true;
#endif
      if(!debug && Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Freeze);
          }
        }
      else if(Gothic::inst().isInGameAndAlive() && !Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickSave();
        }
      break;
      }

    case Event::K_F6:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        auto camera = Gothic::inst().camera();
        auto pl     = Gothic::inst().player();
        auto inter  = pl!=nullptr ? pl->interactive() : nullptr;
        if(camera!=nullptr && inter==nullptr)
          camera->setMarvinMode(Camera::M_Free);
        }
      break;
    case Event::K_F7:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Pinned);
          }
        }
      break;
    case Event::K_F8:
      //player.marvinF8();
      break;

    case Event::K_F9: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
      if(Gothic::inst().isMarvinEnabled()) {
        if(runtimeMode==R_Normal)
          runtimeMode = R_Suspended; else
          runtimeMode = R_Normal;
        }
      else if(!Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickLoad();
        }
      break;
      }
    case Event::K_F10:
      if(runtimeMode==R_Suspended)
        runtimeMode = R_Step;
      break;
    case Event::K_P:
      if(Gothic::inst().isMarvinEnabled()) {
        if(auto p = Gothic::inst().player()) {
          auto pos = p->position();
          string_frm buf("Position: ", pos.x,'/',pos.y,'/',pos.z);
          Gothic::inst().onPrint(buf);
          }
        }
      break;
    }
  }

uint64_t MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  // NOTE: limit to ~200 FPS in game logic to avoid math issues
  if(dt<5)
    return 0;
  lastTick  = time;
  GameMusic::inst().tick(); // Diathic: the menu track loops
  if(arpgWorldMap.isOpen()) {
    // Diathic: a talk, a window or the end of the hero closes the big map
    auto pl = Gothic::inst().player();
    if(pl==nullptr || pl->isDead() || dialogs.isActive() || inventory.isActive() || document.isActive() ||
       arpgMenu.isOpen() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
      arpgWorldMap.setOpen(false);
    }
  if(Camera::isArpgEnabled()) {
    // Diathic: what the hero sees becomes explored (minimap, big map; saved with the game)
    auto wx = Gothic::inst().world();
    auto pl = Gothic::inst().player();
    auto cm = Gothic::inst().camera();
    if(wx!=nullptr && pl!=nullptr && cm!=nullptr) {
      const float fow = cm->arpgView().fowRadius;
      arpgWorldMap.tick(*wx, *pl, std::min(fow>0.f ? fow : 1500.f, 2000.f));
      }
    }

  auto st = Gothic::inst().checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    Gothic::inst().finishLoading();
    if(st==Gothic::LoadState::FailedLoad)
      rootMenu.setMainMenu();
    if(st==Gothic::LoadState::FailedSave)
      Gothic::inst().onPrint("unable to write savegame file");
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      rootMenu.processMusicTheme();
    return 0;
    }

  video.tick();
  if(video.isActive())
    return 0;

  if(Gothic::inst().isPause()) {
    return 0;
    }

  if(dt>50)
    dt=50;

  if(runtimeMode==R_Step) {
    runtimeMode = R_Suspended;
    dt = 1000/60; //60 fps
    }
  else if(runtimeMode==R_Suspended) {
    auto camera = Gothic::inst().camera();
    if(camera!=nullptr && camera->isFree()) {
      tickMouse(dt);
      }
    update();
    return 0;
    }

  dialogs.tick(dt);
  inventory.tick(dt);
  Gothic::inst().tick(dt);
  player.tickFocus();

  if(Camera::isArpgEnabled()) {
    // gothic-diablo: a level up gives a skill point (ArpgProgress counts them from the level)
    auto pl = Gothic::inst().player();
    if(pl!=arpgLevelHero) {
      arpgLevelHero = pl;
      arpgLevelSeen = pl!=nullptr ? pl->level() : 0;
      }
    else if(pl!=nullptr && pl->level()>arpgLevelSeen) {
      arpgLevelSeen = pl->level();
      Gothic::inst().onPrint(ArpgMenu::gtxt("Nowy punkt umiejętności - drzewka pod "+ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Skills))));
      }
    // the first time with Biegłość (a new game, a save from before it): the weapon percents learned so far become ranks
    if(auto gs = Gothic::inst().gameSession(); gs!=nullptr && pl!=nullptr && !gs->arpgProgress().weaponsReady()) {
      ArpgProgress::Hero h;
      h.level = pl->level();
      for(int t=1; t<=4; ++t)
        h.pct[t] = pl->hitChance(Talent(t));
      gs->arpgProgress().initWeapons(h);
      }
  }

  if(dialogs.isActive())
    ;//clearInput();
  if(document.isActive())
    clearInput();
  {
    // gothic-diablo: a menu takes the keyboard (loot, inventory, dialog): key-ups may never reach the player
    const bool ui = dialogs.isActive() || inventory.isActive() || document.isActive();
    if(ui!=uiWasActive)
      player.clearInput();
    uiWasActive = ui;
  }
  tickMouse(dt);
  updateArpgAim();
  player.tickMove(dt);
  update();
  return dt;
  }

void MainWindow::updateAnimation(uint64_t dt) {
  Gothic::inst().updateAnimation(dt);
  }

void MainWindow::tickCamera(uint64_t dt) {
  auto pcamera = Gothic::inst().camera();
  auto pl      = Gothic::inst().player();
  if(pcamera==nullptr)
    return;

  // gothic-diablo: the ARPG window closes for dialogs, Gothic's menus, documents, death...;
  // while it is open the camera stands in front of the hero
  if(inventory.isArpgBook()) {
    // trade, a chest, looting: shown by the ARPG book while Gothic's menu keeps the state
    if(arpgMenu.current()!=ArpgMenu::P_Trade)
      arpgMenu.open(ArpgMenu::P_Trade);
    }
  else if(arpgMenu.current()==ArpgMenu::P_Trade) {
    arpgMenu.close();
    }
  if(arpgMenu.isOpen() && arpgMenu.current()!=ArpgMenu::P_Trade &&
     (pl==nullptr || pl->isDead() || dialogs.isActive() || inventory.isActive() ||
      document.isActive() || rootMenu.isActive() || video.isActive() || !pcamera->isArpgActive()))
    arpgMenu.close();
  // in front of the hero for his own windows
  pcamera->setArpgPortrait(arpgMenu.isOpen() && arpgMenu.current()!=ArpgMenu::P_Trade && !dialogs.isActive());
  {
    // trade, chest, looting: the trader (the chest, the body) in the middle between the pages
    Camera::ArpgBookShot bk;
    if(arpgMenu.current()==ArpgMenu::P_Trade && pl!=nullptr) {
      if(auto ch = inventory.arpgChest()) {
        bk.on       = true;
        bk.standing = false;
        bk.subject  = ch->position()+Vec3(0,45.f,0);
        }
      else if(auto o = inventory.arpgOther(); o!=nullptr && o!=pl) {
        bk.on       = true;
        bk.standing = !o->isDown();
        if(bk.standing)
          bk.subject = o->position()+Vec3(0,100.f,0); else
          bk.subject = (o->position()+o->mapHeadBone())*0.5f+Vec3(0,20.f,0); // the body on the ground
        }
      }
    pcamera->setArpgBook(bk);
  }

  auto&      camera       = *pcamera;
  const auto ws           = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos          = pl!=nullptr ? pl->cameraBone(camera.isFirstPerson()) : Vec3();

  if(!camera.isCutscene() && !camera.isFree()) {
    const bool fs = SystemApi::isFullscreen(hwnd());
    if(dialogs.isArpgCinema()) {
      // gothic-diablo: cinematic dialog: the director in the camera films the two
      camera.setArpgDialog(dialogs.arpgDialogInput());
      camera.setTarget(pos);
      }
    else if(!fs && mouseP[Event::ButtonLeft]) {
      camera.setSpin(camera.spin());
      camera.setTarget(pos);
      }
    else if(dialogs.isActive() && !dialogs.isMobsiDialog()) {
      dialogs.dialogCamera(camera);
      }
    else if(inventory.isActive()) {
      camera.setTarget(pos);
      }
    else if(player.focus().npc!=nullptr && meleeFocus && pl!=nullptr) {
      auto spin = camera.spin();
      spin.y = pl->rotation();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    else if(pl!=nullptr && !camera.isFree()) {
      auto spin = camera.spin();
      if(pl->interactive()==nullptr && !pl->isDown())
        spin.y = pl->rotation();
      if(pl->isDive() && !camera.isMarvin())
        spin.x = -pl->rotationY();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    }

  if(dt==0)
    return;
  if(camera.isToggleEnabled() && !camera.isCutscene())
    camera.setMode(solveCameraMode());
  camera.tick(dt);
  updateArpgDialogLight(camera);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  const auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isFree())
    return Camera::Normal;

  if(dialogs.isActive())
    return Camera::Dialog;

  if(camera!=nullptr && camera->isFirstPerson())
    return Camera::FirstPerson;

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(auto pl = Gothic::inst().player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    if(pl->isFallingDeep())
      return Camera::Fall;
    bool g2 = Gothic::inst().version().game==2;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return g2 ? Camera::Ranged : Camera::Normal;
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(std::string_view slot) {
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(slot));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::loadGame(std::string_view slot) {
  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
    setGameImpl(nullptr);
    }

  Gothic::inst().setBenchmarkMode(Benchmark::None);
  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(slot);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(s));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::saveGame(std::string_view slot, std::string_view name) {
  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;

  auto tex  = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
  auto lres = Attachment();

  static int32_t kThumbW = 800;
  const  int32_t kThumbH = tex.w()>0 ? int32_t((tex.h() * kThumbW) / tex.w()) : 0;
  if(kThumbW>0 && kThumbH>0 && kThumbW<tex.w() && kThumbH<tex.h()) {
    lres = device.attachment(Tempest::TextureFormat::RGBA8, uint32_t(kThumbW), uint32_t(kThumbH));
    }

  if(!lres.isEmpty()) {
    // reduce size of the save entry preview screenshot for faster save & load
    CommandBuffer cmd;
    {
    auto enc = cmd.startEncoding(device);
    enc.setDebugMarker("Downscale screenhoot");
    enc.setFramebuffer({{lres, Vec4(), Tempest::Preserve}});
    enc.setPushData(IVec2(lres.w(), lres.h()));
    enc.setBinding(0, tex, Sampler::nearest());
    enc.setPipeline(Shaders::inst().downscale);
    enc.draw(nullptr, 0, 3);
    }
    auto sync = device.submit(cmd);
    sync.wait();
    }

  auto& thumb = lres.isEmpty() ? tex : lres;
  auto  pm    = device.readPixels(textureCast<const Texture2d&>(thumb));

  Gothic::inst().startSave(std::move(textureCast<Texture2d&>(tex)),[slot=std::string(slot),name=std::string(name),pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    {
    Tempest::WFile  f(slot);
    Serialize      s(f);
    game->save(s,name,pm);
    }

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

void MainWindow::onVideo(std::string_view fname) {
  if(Gothic::inst().isBenchmarkMode())
    return;
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
  dropArpgDialogLight();
  arpgRemarks.clear();
  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
  dMouse = Point();

  if(Gothic::inst().isBenchmarkMode()) {
    if(auto world = Gothic::inst().world()) {
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->execTriggerEvent(evt);
      }
    benchmark.clear();
    }

  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  device.waitIdle();
  for(auto& c:commands)
    c = device.commandBuffer();

  if(auto c = Gothic::inst().camera())
    c->setViewport(uint32_t(w()),uint32_t(h()));

  renderer.onWorldChanged();

  if(auto pl = Gothic::inst().player())
    rootMenu.setPlayer(*pl);

  if(auto pl = Gothic::inst().player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
  }

void MainWindow::onSessionExit() {
  rootMenu.setMainMenu();
  }

void MainWindow::onBenchmarkFinished() {
  if(benchmark.numFrames==0)
    return;

  double fps  = benchmark.fpsSum/double(benchmark.numFrames);
  double low1 = 0;
  size_t num1 = 0;
  for(size_t i=0; i<benchmark.low1procent.size(); ++i) {
    auto v = benchmark.low1procent[i];
    if(v<=0)
      continue;
    low1 += 1000.0/double(v);
    num1 += 1;
    }
  low1 = num1>0 ? low1/double(num1) : 0.0;
  benchmark.clear();

  string_frm str("Benchmark: low 1% = ", low1, " fps = ", fps);
  Log::i(str.c_str());
  console.printLine(str);

  if(Gothic::inst().isBenchmarkModeCi()) {
    Log::i("Exiting benchmark");
    Tempest::SystemApi::exit();
    return;
    }

  console.setFocus(true);
  console.exec();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  // clear pointers to outdated game-state
  dropArpgDialogLight();
  arpgRemarks.clear();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::dropArpgDialogLight() {
  // the world is still there: the light leaves it properly
  if(arpgDlgLightOn && Gothic::inst().world()==arpgDlgLightWorld)
    arpgDlgLight = LightGroup::Light(); else
    arpgDlgLight.release();
  arpgDlgLightOn    = false;
  arpgDlgLightWorld = nullptr;
  }

void MainWindow::updateArpgDialogLight(const Camera& camera) {
  // gothic-diablo: a soft light from the camera on the faces in dialog shots (night, interiors)
  auto  world = Gothic::inst().world();
  Vec3  pos;
  float range = 0;
  const bool on = world!=nullptr && camera.arpgDialogLight(pos, range);
  if(arpgDlgLightOn && arpgDlgLightWorld!=world) {
    arpgDlgLight.release(); // another world: the old one took its lights with it
    arpgDlgLightOn = false;
    }
  if(!on) {
    if(arpgDlgLightOn)
      dropArpgDialogLight();
    return;
    }
  if(!arpgDlgLightOn) {
    zenkit::VLight v;
    v.preset    = "ARPG_DIALOG_FILL";
    v.range     = range;
    v.color     = zenkit::Color{150, 138, 120, 255};
    v.is_static = false;
    v.on        = true;
    v.position  = {pos.x, pos.y, pos.z};
    arpgDlgLight      = world->addLight(v);
    arpgDlgLightOn    = true;
    arpgDlgLightWorld = world;
    }
  arpgDlgLight.setPosition(pos);
  arpgDlgLight.setRange(range);
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  setCursorShape((fs && !Camera::isArpgEnabled()) ? CursorShape::Hidden : CursorShape::Arrow);
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

    static bool once=true;
    if(once) {
      // GAMESTART.WAV = the menu music of Noc Kruka (a sound, not a music theme); Diathic has its own track
      if(DiathicAsset::get("menu.ogg").empty())
        Gothic::inst().emitGlobalSoundWav("GAMESTART.WAV");
      once=false;
      }

    if(T_UNLIKELY(Gothic::inst().isBenchmarkModeCi())) {
      const auto st = Gothic::inst().checkLoading();
      if(st==Gothic::LoadState::Loading) {
        // skip loading frames in benchmark-ci mode, for sake of easier tooling
        return;
        }
      }

    /*
      Note: game update goes first
      once player position is updated, animation bones(cameraBone in particular) can be updated
      lastly - camera position
      */
    const uint64_t dt = tick();
    updateAnimation(dt);
    tickCamera(dt);

    auto& sync = fence[cmdId];
    if(!sync.wait(0)) {
      // GPU rendering is not done, pass to next frame
      std::this_thread::yield();
      return;
      }
    Resources::resetRecycled(cmdId);

    if(video.isActive()) {
      video.paint(device,cmdId);
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      if(!inventory.isActive() || inventory.isArpgBook()) {
        Painter pn(p);
        if(arpgMenu.isOpen()) {
          if(auto pl = Gothic::inst().player())
            arpgMenu.paintOverlay(pn, Gothic::interfaceScale(this), *pl);
          } else {
          paintArpgBarOverlay(pn);
          }
        }
      }
    uiMesh [cmdId].update(device,uiLayer);
    numMesh[cmdId].update(device,numOverlay);

    CommandBuffer& cmd = commands[cmdId];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(swapchain[swapchain.currentImage()],enc,cmdId,uiMesh[cmdId],numMesh[cmdId],inventory,video,&arpgBarItems,&arpgCursorItem);
    }
    sync = device.submit(cmd);
    device.present(swapchain);
    cmdId = (cmdId+1u)%Resources::MaxFramesInFlight;

    auto t = Application::tickCount();
    auto frameTime = maxFpsInv;
    if(!Gothic::inst().isInGame() && !video.isActive())
      frameTime = std::max<uint64_t>(frameTime,16);
    if(t-time<frameTime) {
      uint32_t delay = uint32_t(frameTime-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    fps.push(t-time);
    if(Gothic::inst().isBenchmarkMode() && Gothic::inst().world()!=nullptr && Gothic::inst().world()->currentCs()!=nullptr)
      benchmark.push(t-time);
    time = t;
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    Log::e("swapchain is outdated - reset renderer");
    device.waitIdle();
    swapchain.reset();
    }
  }

double MainWindow::Fps::get() const {
  uint64_t sum=0,num=0;
  for(auto& i:dt)
    if(i>0) {
      sum+=i;
      num++;
      }
  if(num==0 || sum==0)
    return 60;
  uint64_t fps = (1000*100*num)/sum;
  return double(fps)/100.0;
  }

void MainWindow::Fps::push(uint64_t t) {
  for(size_t i=9;i>0;--i)
    dt[i]=dt[i-1];
  dt[0]=t;
  }

void MainWindow::BenchmarkData::push(uint64_t t) {
  fpsSum += t>0 ? (1000.0/double((t))) : 60.0;
  numFrames++;
  auto at = std::lower_bound(low1procent.begin(), low1procent.end(), t, std::greater<uint64_t>());
  low1procent.insert(at, t);
  low1procent.resize(std::min(low1procent.size(), (numFrames+99)/100));
  }

void MainWindow::BenchmarkData::clear() {
  low1procent.reserve(128);
  low1procent.clear();
  numFrames = 0;
  fpsSum = 0;
  }

void MainWindow::paintArpgMinimap(Painter& p) {
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  auto cam   = Gothic::inst().camera();
  arpgMapRect = Rect();
  if(!arpgMinimap || world==nullptr || pl==nullptr || cam==nullptr || dialogs.isActive() || inventory.isActive())
    return;

  // the world map of the current level (from the map items of the scripts)
  if(arpgMapWorld!=world->name()) {
    arpgMapWorld = std::string(world->name());
    arpgMapValid = false;
    for(auto& m:world->script().arpgWorldMaps()) {
      if(m.level!=arpgMapWorld)
        continue;
      arpgMapTex    = m.texture;
      arpgMapBounds = Rect(m.left, m.top, m.right-m.left, m.bottom-m.top);
      arpgMapValid  = (m.right!=m.left && m.bottom!=m.top);
      }
    }
  const Texture2d* tex = arpgMapValid ? arpgWorldMap.wholeMap(*world, *pl) : nullptr;

  const Vec3  hero = pl->position();
  const auto& view = cam->arpgView();

  // explored cells (Diathic: ArpgMapData of the session, updated in tick, saved with the game)
  constexpr float cell = ArpgMapData::Cell;
  auto gs = Gothic::inst().gameSession();
  const std::unordered_set<int64_t>* explored = gs!=nullptr ? gs->arpgMap().explored(arpgMapWorld) : nullptr;
  auto  key      = [](int i, int j) { return ArpgMapData::key(i,j); };

  const float scale = Gothic::interfaceScale(this);
  const int   S     = int(220.f*scale);
  const int   x0    = w() - S - int(12.f*scale);
  const int   y0    = int(48.f*scale);
  const float cx    = float(x0) + float(S)*0.5f;
  const float cy    = float(y0) + float(S)*0.5f;
  const float half  = arpgMapHalf;            // world cm from the center to the edge (wheel over the map)
  arpgMapRect = Rect(x0, y0, S, S);
  const float k     = float(S)/(2.f*half);    // pixels per cm

  // screen up = where the camera looks, screen right = camera right (same as W/D)
  const float yaw = cam->arpgYaw()*float(M_PI)/180.f;
  const Vec2  f   = {std::cos(yaw),  std::sin(yaw)};
  const Vec2  r   = {std::sin(yaw), -std::cos(yaw)};
  auto toScreen = [&](float wx, float wz) {
    const float dx = wx-hero.x, dz = wz-hero.z;
    return Vec2(cx + k*(dx*r.x + dz*r.y), cy - k*(dx*f.x + dz*f.y));
    };
  auto toWorld = [&](float a, float b) { // a: along screen right, b: along screen up (cm)
    return Vec2(hero.x + a*r.x + b*f.x, hero.z + a*r.y + b*f.y);
    };

  p.pushState();
  p.setScissor(x0, y0, S, S);

  p.setBrush(Brush(Color(0.06f,0.05f,0.04f,0.85f), Painter::Alpha));
  p.drawRect(x0, y0, S, S);

  if(tex!=nullptr) {
    const auto& wb  = arpgMapBounds;
    const float tw  = float(tex->w()), th = float(tex->h());
    auto uv = [&](const Vec2& wp) {
      return Vec2((wp.x-float(wb.x))/float(wb.w)*tw, (wp.y-float(wb.y))/float(wb.h)*th);
      };
    const Vec2 tl = uv(toWorld(-half,  half)), tr = uv(toWorld( half,  half));
    const Vec2 br = uv(toWorld( half, -half)), bl = uv(toWorld(-half, -half));
    const float X0 = float(x0), Y0 = float(y0), X1 = float(x0+S), Y1 = float(y0+S);
    p.setBrush(Brush(*tex, Color(1.f,1.f,1.f,0.9f), Painter::Alpha, ClampMode::ClampToBorder));
    p.drawTriangle(X0,Y0, tl.x,tl.y,  X1,Y0, tr.x,tr.y,  X1,Y1, br.x,br.y);
    p.drawTriangle(X0,Y0, tl.x,tl.y,  X1,Y1, br.x,br.y,  X0,Y1, bl.x,bl.y);
    }

  // not explored yet: dark
  {
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.8f), Painter::Alpha));
    const float reach = half*1.42f;
    const int   i0 = int(std::floor((hero.x-reach)/cell)), i1 = int(std::floor((hero.x+reach)/cell));
    const int   j0 = int(std::floor((hero.z-reach)/cell)), j1 = int(std::floor((hero.z+reach)/cell));
    for(int j=j0; j<=j1; ++j)
      for(int i=i0; i<=i1; ++i) {
        if(explored!=nullptr && explored->find(key(i,j))!=explored->end())
          continue;
        const Vec2 a = toScreen(float(i)*cell,   float(j)*cell);
        const Vec2 b = toScreen(float(i+1)*cell, float(j)*cell);
        const Vec2 c = toScreen(float(i+1)*cell, float(j+1)*cell);
        const Vec2 d = toScreen(float(i)*cell,   float(j+1)*cell);
        p.drawTriangle(a.x,a.y,0,0, b.x,b.y,0,0, c.x,c.y,0,0);
        p.drawTriangle(a.x,a.y,0,0, c.x,c.y,0,0, d.x,d.y,0,0);
        }
  }

  // creatures in sight: enemies red, others light
  {
    const float sight = view.fowRadius>0.f ? view.fowRadius : 1500.f;
    const int   dot   = std::max(3, int(4.f*scale));
    for(uint32_t i=0; i<world->npcCount(); ++i) {
      auto npc = world->npcById(i);
      if(npc==nullptr || npc==pl || npc->isDead())
        continue;
      const auto  np = npc->position();
      const float dx = np.x-hero.x, dz = np.z-hero.z;
      if(dx*dx+dz*dz > sight*sight)
        continue;
      const Vec2 sp = toScreen(np.x, np.z);
      if(player.arpgIsEnemy(*npc))
        p.setBrush(Brush(Color(0.9f,0.15f,0.1f,1.f), Painter::Alpha)); else
        p.setBrush(Brush(Color(0.85f,0.85f,0.7f,1.f), Painter::Alpha));
      p.drawRect(int(sp.x)-dot/2, int(sp.y)-dot/2, dot, dot);
      }
  }

  // the hero: arrow in the direction he faces
  {
    const float rot = pl->rotation()*float(M_PI)/180.f;
    Vec2 hd = {std::cos(rot)*r.x + std::sin(rot)*r.y, -(std::cos(rot)*f.x + std::sin(rot)*f.y)};
    const float l = std::sqrt(hd.x*hd.x+hd.y*hd.y);
    if(l>0.f)
      hd = hd/l;
    const Vec2  pr  = {-hd.y, hd.x};
    const float a   = 8.f*scale, b = 5.f*scale;
    const Vec2  tip = Vec2(cx,cy) + hd*a;
    const Vec2  b0  = Vec2(cx,cy) - hd*b + pr*b;
    const Vec2  b1  = Vec2(cx,cy) - hd*b - pr*b;
    p.setBrush(Brush(Color(1.f,0.8f,0.25f,1.f), Painter::Alpha));
    p.drawTriangle(tip.x,tip.y,0,0, b0.x,b0.y,0,0, b1.x,b1.y,0,0);
  }

  // north
  {
    Vec2 nd = {r.y, -f.y};
    const float l = std::sqrt(nd.x*nd.x+nd.y*nd.y);
    if(l>0.f)
      nd = nd/l;
    auto&      fnt = Resources::font(scale);
    const auto ts  = fnt.textSize("N");
    const Vec2 np  = Vec2(cx,cy) + nd*(float(S)*0.5f - float(ts.h));
    p.setBrush(Color(1.0));
    fnt.drawText(p, int(np.x)-ts.w/2, int(np.y)+ts.h/2, "N");
  }

  // frame
  {
    const int bw = std::max(1, int(2.f*scale));
    p.setBrush(Brush(Color(0.55f,0.45f,0.25f,0.9f), Painter::Alpha));
    p.drawRect(x0,        y0,        S,  bw);
    p.drawRect(x0,        y0+S-bw,   S,  bw);
    p.drawRect(x0,        y0,        bw, S);
    p.drawRect(x0+S-bw,   y0,        bw, S);
  }

  p.popState();
  }

void MainWindow::paintArpgCombat(Painter& p, const Matrix4x4& vp) {
  // gothic-diablo: life bars above enemies in a fight, floating damage numbers, special attack cooldown
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  auto cam   = Gothic::inst().camera();
  if(world==nullptr || pl==nullptr || cam==nullptr || dialogs.isActive() || inventory.isActive())
    return;
  const float scale = Gothic::interfaceScale(this);
  auto toScreen = [&](Vec3 pos, Point& out) {
    float pw = 1.f;
    vp.project(pos.x,pos.y,pos.z,pw);
    if(pw<=0.f)
      return false;
    pos /= pw;
    if(pos.x<-1.2f || pos.x>1.2f || pos.y<-1.2f || pos.y>1.2f)
      return false;
    out = Point(int((0.5f*pos.x+0.5f)*float(w())), int((0.5f*pos.y+0.5f)*float(h())));
    return true;
    };
  // a quad of two triangles; the UI pipeline culls one winding (Tempest: CullMode::Back), so both are given
  auto quad = [&](const Point& p00, const Point& p01, const Point& p10, const Point& p11) {
    auto tri = [&](const Point& a, const Point& b, const Point& c) {
      p.drawTriangle(float(a.x),float(a.y),0,0, float(b.x),float(b.y),0,0, float(c.x),float(c.y),0,0);
      p.drawTriangle(float(a.x),float(a.y),0,0, float(c.x),float(c.y),0,0, float(b.x),float(b.y),0,0);
      };
    tri(p00, p01, p11);
    tri(p00, p11, p10);
    };

  // life bars
  {
    const auto& view  = cam->arpgView();
    const float sight = view.fowRadius>0.f ? view.fowRadius : 1500.f;
    const int   bw    = int(46.f*scale);
    const int   bh    = std::max(3, int(5.f*scale));
    const Vec3  hero  = pl->position();
    world->detectNpc(hero, sight, [&](Npc& n) {
      if(&n==pl || n.isDead() || !player.arpgIsEnemy(n))
        return;
      const int32_t hp  = n.attribute(ATR_HITPOINTS);
      const int32_t hpM = std::max(1, n.attribute(ATR_HITPOINTSMAX));
      if(hp>=hpM && n.target()!=pl)
        return; // not in a fight yet
      Point sp;
      if(!toScreen(n.position()+Vec3(0,n.isDown() ? 60.f : 210.f,0), sp))
        return;
      const float v = std::clamp(float(hp)/float(hpM), 0.f, 1.f);
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.7f), Painter::Alpha));
      p.drawRect(sp.x-bw/2-1, sp.y-1, bw+2, bh+2);
      p.setBrush(Brush(Color(0.75f,0.08f,0.05f,0.95f), Painter::Alpha));
      p.drawRect(sp.x-bw/2, sp.y, int(float(bw)*v), bh);
      });
  }

  // floating numbers: rise and fade in about a second
  {
    auto&          list = world->arpgPopups();
    const uint64_t now  = world->tickCount();
    for(size_t i=0; i<list.size();) {
      if(now>list[i].time+1100)
        list.erase(list.begin()+long(i)); else
        ++i;
      }
    for(auto& e:list) {
      if(now<e.time)
        continue; // not there yet (a missed arrow on its way)
      const float t = float(now-e.time)/1000.f;
      Point sp;
      if(!toScreen(e.pos+Vec3(0,70.f*t,0), sp))
        continue;
      const float a = std::clamp(1.f-(t-0.6f)/0.5f, 0.f, 1.f);
      Resources::FontType type = Resources::FontType::Normal;
      char txt[32] = {};
      if(e.kind==2) {
        type = Resources::FontType::Disabled;
        std::snprintf(txt, sizeof(txt), "Blok");
        }
      else if(e.kind==3) {
        type = Resources::FontType::Disabled;
        std::snprintf(txt, sizeof(txt), "Chybiono");
        }
      else if(e.kind==4) {
        type = Resources::FontType::Yellow;
        std::snprintf(txt, sizeof(txt), "Unik");
        }
      else if(e.kind==5) {
        // a skill of the trees went off
        type = Resources::FontType::Yellow;
        const char* nm = (e.value==1) ? "Wir!" : (e.value==2) ? "Fala uderzeniowa!" : (e.value==3) ? "Grad strzał!" : "";
        std::snprintf(txt, sizeof(txt), "%s", ArpgMenu::gtxt(nm).c_str());
        } else {
        type = (e.kind==1) ? Resources::FontType::Red : Resources::FontType::Normal;
        std::snprintf(txt, sizeof(txt), "%d", int(e.value));
        }
      auto&      fnt = Resources::font(type, scale*(e.kind==1 ? 1.f : 1.15f));
      const auto ts  = fnt.textSize(txt);
      p.setBrush(Color(1.f,1.f,1.f,a));
      fnt.drawText(p, sp.x-ts.w/2, sp.y, txt);
      }
  }

  // charging the special attack: bar above the head, gold = release now, red = held too long
  {
    const float t = player.arpgChargeTime();
    Point sp;
    if(t>=0.f && toScreen(pl->position()+Vec3(0,230.f,0), sp)) {
      const int   bw   = int(90.f*scale), bh = std::max(4, int(8.f*scale));
      const float full = player.arpgChargeTimeFull(), win = player.arpgChargeTimeWindow();
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.7f), Painter::Alpha));
      p.drawRect(sp.x-bw/2-2, sp.y-2, bw+4, bh+4);
      if(t<full) {
        p.setBrush(Brush(Color(0.85f,0.85f,0.85f,0.85f), Painter::Alpha));
        p.drawRect(sp.x-bw/2, sp.y, int(float(bw)*t/full), bh);
        }
      else if(t<=full+win) {
        p.setBrush(Brush(Color(1.f,0.8f,0.2f,1.f), Painter::Alpha));
        p.drawRect(sp.x-bw/2-2, sp.y-2, bw+4, bh+4);
        }
      else {
        p.setBrush(Brush(Color(0.85f,0.15f,0.1f,0.9f), Painter::Alpha));
        p.drawRect(sp.x-bw/2, sp.y, bw, bh);
        }
      }
  }

  // slash of the charged attack: a bright 180 degree crescent sweeping around the hero
  {
    const auto&    fx  = player.arpgSlash();
    const uint64_t now = world->tickCount();
    if(fx.kind==1 && fx.time!=0 && now>=fx.time && now<fx.time+450) {
      // charged arrow: a bright streak along the shot
      const float t    = float(now-fx.time)/1000.f;
      const float fade = 1.f - std::clamp(t/0.45f, 0.f, 1.f);
      const float rad  = fx.ang*float(M_PI)/180.f;
      const Vec3  fw   = {std::cos(rad), 0.f, std::sin(rad)};
      const Vec3  sd   = {-fw.z, 0.f, fw.x};
      const Vec3  base = fx.pos + Vec3(0,140.f,0);
      const int   N    = 12;
      const float len  = std::min(2500.f, 600.f + 9000.f*t);
      for(int i=0; i<N; ++i) {
        const float k0 = float(i)/float(N), k1 = float(i+1)/float(N);
        const float a  = fade*(1.f-k0)*0.9f;
        const float w0 = 18.f*(1.f-k0)+4.f, w1 = 18.f*(1.f-k1)+4.f;
        Point p00, p01, p10, p11;
        if(!toScreen(base+fw*(len*k0)-sd*w0, p00) || !toScreen(base+fw*(len*k0)+sd*w0, p01) ||
           !toScreen(base+fw*(len*k1)-sd*w1, p10) || !toScreen(base+fw*(len*k1)+sd*w1, p11))
          continue;
        p.setBrush(Brush(Color(1.f*a, 0.85f*a, 0.45f*a, 1.f), Painter::Add));
        quad(p00, p01, p10, p11);
        }
      }
    else if(fx.kind==2 && fx.time!=0 && now>=fx.time && now<fx.time+400) {
      // bow push: a wave rolling out 120 degrees in front of the hero
      const float t    = float(now-fx.time)/1000.f;
      const float k    = std::clamp(t/0.4f, 0.f, 1.f);
      const float fade = 1.f-k;
      const float r1   = 80.f + 280.f*std::sqrt(k);
      const float r0   = std::max(40.f, r1-70.f);
      const int   N    = 16;
      for(int i=0; i<N; ++i) {
        const float a0 = fx.ang - 60.f + 120.f*float(i)/float(N);
        const float a1 = fx.ang - 60.f + 120.f*float(i+1)/float(N);
        auto pt = [&](float a, float r, Point& out) {
          const float rad = a*float(M_PI)/180.f;
          return toScreen(fx.pos + Vec3(std::cos(rad)*r, 90.f, std::sin(rad)*r), out);
          };
        Point p00, p01, p10, p11;
        if(!pt(a0,r0,p00) || !pt(a0,r1,p01) || !pt(a1,r0,p10) || !pt(a1,r1,p11))
          continue;
        const float edge = 1.f - std::abs(float(i)+0.5f-float(N)/2.f)/(float(N)/2.f); // brighter in the middle
        const float a    = fade*(0.35f+0.65f*edge)*0.8f;
        p.setBrush(Brush(Color(0.75f*a, 0.85f*a, 1.f*a, 1.f), Painter::Add));
        quad(p00, p01, p10, p11);
        }
      }
    else if(fx.kind==0 && fx.time!=0 && now>=fx.time && now<fx.time+600) {
      const float t     = float(now-fx.time)/1000.f;
      const float sweep = std::clamp((t-0.06f)/0.18f, 0.f, 1.f);   // follows the swing
      const float fade  = 1.f - std::clamp((t-0.25f)/0.35f, 0.f, 1.f);
      if(sweep>0.f && fade>0.f) {
        const int   N  = 24;
        const float r0 = 90.f, r1 = 260.f, y = 110.f;
        auto pt = [&](float a, float r, Point& out) {
          const float rad = a*float(M_PI)/180.f;
          return toScreen(fx.pos + Vec3(std::cos(rad)*r, y, std::sin(rad)*r), out);
          };
        p.setBrush(Brush(Color(1.f,0.85f,0.45f,1.f), Painter::Add));
        for(int i=0; i<N; ++i) {
          const float k0 = float(i)/float(N), k1 = float(i+1)/float(N);
          if(k1>sweep)
            break;
          // the newest part of the sweep is brightest
          const float a  = fade*(0.25f + 0.75f*std::clamp(1.f-(sweep-k1)*2.f, 0.f, 1.f));
          const float a0 = fx.ang + fx.dir*(90.f - 180.f*k0);
          const float a1 = fx.ang + fx.dir*(90.f - 180.f*k1);
          Point p00, p01, p10, p11;
          if(!pt(a0,r0,p00) || !pt(a0,r1,p01) || !pt(a1,r0,p10) || !pt(a1,r1,p11))
            continue;
          p.setBrush(Brush(Color(1.f*a, 0.82f*a, 0.4f*a, 1.f), Painter::Add));
          quad(p00, p01, p10, p11);
          }
        }
      }
  }

  // the skill trees: Wir (a ring all around the hero), Fala uderzeniowa (a wave running ahead of the charged swing)
  {
    const auto&    fx  = pl->arpgFx();
    const uint64_t now = world->tickCount();
    const uint64_t len = (fx.kind==3) ? 450 : 700;
    if((fx.kind==3 || fx.kind==4) && fx.time!=0 && now>=fx.time && now<fx.time+len) {
      const float k    = float(now-fx.time)/float(len);
      const float fade = 1.f-k*k;
      // the ring of Wir ends where its hit ends (fx.reach); the wave runs out to its reach
      const float r1   = (fx.kind==3) ? 60.f+(fx.reach-60.f)*std::sqrt(k) : 80.f+fx.reach*std::sqrt(k);
      const float r0   = std::max(30.f, r1-(fx.kind==3 ? 70.f : 160.f));
      const int   N    = (fx.kind==3) ? 32 : 16;
      for(int i=0; i<N; ++i) {
        const float a0 = fx.ang - fx.arc*0.5f + fx.arc*float(i)/float(N);
        const float a1 = fx.ang - fx.arc*0.5f + fx.arc*float(i+1)/float(N);
        auto pt = [&](float ang, float r, Point& out) {
          const float rad = ang*float(M_PI)/180.f;
          return toScreen(fx.pos + Vec3(std::cos(rad)*r, 80.f, std::sin(rad)*r), out);
          };
        Point p00, p01, p10, p11;
        if(!pt(a0,r0,p00) || !pt(a0,r1,p01) || !pt(a1,r0,p10) || !pt(a1,r1,p11))
          continue;
        // brighter in the middle of the wave
        const float edge = (fx.kind==3) ? 1.f : 1.f-std::abs(float(i)+0.5f-float(N)/2.f)/(float(N)/2.f);
        const float a    = fade*(fx.kind==3 ? 0.8f : (0.45f+0.55f*edge));
        if(fx.kind==3)
          p.setBrush(Brush(Color(1.f*a, 0.82f*a, 0.4f*a, 1.f), Painter::Add)); else
          p.setBrush(Brush(Color(0.6f*a, 0.8f*a, 1.f*a, 1.f), Painter::Add));
        quad(p00, p01, p10, p11);
        if(fx.kind==4) {
          // twice: the wave shines over the ground
          quad(p00, p01, p10, p11);
          }
        }
      }
  }
  }

// gothic-diablo: skill bar at the bottom: mouse buttons, potions 1-2, spells 3-0
static constexpr int ArpgBarSlots = 13; // LPM PPM SPM | 1 2 | 3 4 5 6 7 8 9 0

bool MainWindow::arpgBarVisible() const {
  if(!isArpgMouse() || rootMenu.isActive() || chapter.isActive() || video.isActive())
    return false;
  auto pl = Gothic::inst().player();
  return pl!=nullptr && !pl->isDead() && Gothic::inst().checkLoading()==Gothic::LoadState::Idle;
  }

Tempest::Rect MainWindow::arpgBarSlot(int i) const {
  const float scale = Gothic::interfaceScale(this);
  const int   s     = int(46.f*scale);
  const int   gap   = std::max(2, int(4.f*scale));
  const int   group = int(16.f*scale);
  const int   wAll  = ArpgBarSlots*s + (ArpgBarSlots-1)*gap + 2*group;
  const int   x0    = (w()-wAll)/2;
  const int   y     = h() - s - int(12.f*scale);
  const int   grp   = (i<3) ? 0 : (i<5 ? 1 : 2);
  return Rect(x0 + i*(s+gap) + grp*group, y, s, s);
  }

// Diathic: Gothic's bars, nicer: life left of the skill bar, mana right of it, as tall as half a slot, with the
// numbers, a pale trail of what was just lost (shrinks after 0.5 s), life under 25% pulses red;
// a thin experience bar over the slots (level to level)
void MainWindow::paintArpgVitals(Painter& p) {
  auto pl = Gothic::inst().player();
  if(pl==nullptr || barBack==nullptr)
    return;
  const float    scale = Gothic::interfaceScale(this);
  const Rect     s0    = arpgBarSlot(0), sN = arpgBarSlot(ArpgBarSlots-1);
  const int      gap   = int(14.f*scale);
  const int      H     = int(float(s0.h)*0.62f);
  const int      W     = std::min(int(300.f*scale), s0.x-gap-int(12.f*scale));
  const int      y     = s0.y + (s0.h-H)/2;
  const uint64_t now   = Application::tickCount();
  const float    dt    = arpgVitalsAt==0 ? 0.f : float(std::min<uint64_t>(now-arpgVitalsAt, 200))/1000.f;
  arpgVitalsAt = now;

  GthFont fnt = Resources::font(scale*0.85f);
  fnt.setColor(Color(1.f, 0.96f, 0.88f, 1.f));

  auto vital = [&](const Texture2d* fill, int x, int32_t cur, int32_t mx, float& ghost, uint64_t& hitAt, bool life) {
    if(fill==nullptr || W<40)
      return;
    const float v = mx>0 ? std::clamp(float(cur)/float(mx), 0.f, 1.f) : 0.f;
    if(ghost<0.f || v>=ghost) {
      ghost = v;
      hitAt = 0;
      } else {
      if(hitAt==0)
        hitAt = now;
      if(now>hitAt+500)
        ghost = std::max(v, ghost - 0.6f*dt);
      }
    // frame and the inside like Gothic's drawBar (9 / 4 texels of the frame texture)
    const float kx = float(W)/float(std::max(1,barBack->w()));
    const int   ix = int(9.f*kx), iy = H/8;
    if(life && v<0.25f && cur>0) {
      const float a = 0.35f + 0.25f*std::sin(float(now%100000)*0.008f);
      p.setBrush(Brush(Color(0.9f, 0.05f, 0.02f, a), Painter::Alpha));
      const int g = std::max(2, int(4.f*scale));
      p.drawRect(x-g, y-g, W+2*g, H+2*g);
      }
    p.setBrush(*barBack);
    p.drawRect(x, y, W, H, 0, 0, barBack->w(), barBack->h());
    const int iw = W-2*ix, ih = H-2*iy;
    if(ghost>v) {
      p.setBrush(Brush(Color(1.f, 0.93f, 0.75f, 0.55f), Painter::Alpha));
      p.drawRect(x+ix+int(float(iw)*v), y+iy, std::max(1, int(float(iw)*(ghost-v))), ih);
      }
    p.setBrush(*fill);
    p.drawRect(x+ix, y+iy, int(float(iw)*v), ih, 0, 0, fill->w(), fill->h());
    char txt[48] = {};
    std::snprintf(txt, sizeof(txt), "%d / %d", int(cur), int(mx));
    const auto ts = fnt.textSize(txt);
    fnt.drawText(p, x+(W-ts.w)/2, y+H/2+ts.h/3, txt);
    };
  vital(barHp,   s0.x-gap-W,        pl->attribute(ATR_HITPOINTS), pl->attribute(ATR_HITPOINTSMAX), arpgHpGhost, arpgHpHit, true);
  vital(barMana, sN.x+sN.w+gap,     pl->attribute(ATR_MANA),      pl->attribute(ATR_MANAMAX),      arpgMpGhost, arpgMpHit, false);

  // experience: from this level's threshold to the next one (Gothic: next = previous + (level+1)*500)
  {
    const int32_t lvl  = pl->level();
    const int32_t next = pl->experienceNext();
    const int32_t prev = std::max(0, next - (lvl+1)*500);
    const float   v    = next>prev ? std::clamp(float(pl->experience()-prev)/float(next-prev), 0.f, 1.f) : 0.f;
    const int     xh   = std::max(3, int(5.f*scale));
    const int     x0   = s0.x, x1 = sN.x+sN.w;
    const int     xy   = s0.y - xh - int(4.f*scale);
    p.setBrush(Brush(Color(0.f, 0.f, 0.f, 0.65f), Painter::Alpha));
    p.drawRect(x0-1, xy-1, x1-x0+2, xh+2);
    if(barMisc!=nullptr) {
      p.setBrush(*barMisc);
      p.drawRect(x0, xy, int(float(x1-x0)*v), xh, 0, 0, barMisc->w(), barMisc->h());
      }
    if(isArpgMouse() && arpgCursor.y>=xy-int(6.f*scale) && arpgCursor.y<=xy+xh+int(6.f*scale) &&
       arpgCursor.x>=x0 && arpgCursor.x<=x1) {
      char txt[96] = {};
      std::snprintf(txt, sizeof(txt), "Poziom %d   doświadczenie %d / %d", int(lvl), int(pl->experience()), int(next));
      const std::string t  = ArpgMenu::gtxt(txt);
      const auto        ts = fnt.textSize(t);
      fnt.drawText(p, (x0+x1-ts.w)/2, xy-int(6.f*scale), t);
      }
  }
  }

void MainWindow::paintArpgBar(Painter& p) {
  if(!arpgBarVisible())
    return;
  auto pl = Gothic::inst().player();
  const auto bar   = player.arpgBar();
  const auto ws    = pl->weaponState();
  auto  slotTex    = Resources::loadTexture("INV_SLOT.TGA");
  auto  hiTex      = Resources::loadTexture("INV_SLOT_HIGHLIGHTED.TGA");

  auto tri = [&](float x0, float y0, float x1, float y1, float x2, float y2) {
    p.drawTriangle(x0,y0,0,0, x1,y1,0,0, x2,y2,0,0);
    };
  auto poly = [&](float cx, float cy, const float* pts, int n, float sc) {
    // convex polygon around (cx,cy): pts = x,y pairs in units of sc
    for(int i=0; i<n; ++i) {
      const int j = (i+1)%n;
      tri(cx, cy, cx+pts[2*i]*sc, cy+pts[2*i+1]*sc, cx+pts[2*j]*sc, cy+pts[2*j+1]*sc);
      }
    };
  auto drawSlot = [&](const Rect& r, bool hi) {
    auto t = (hi && hiTex!=nullptr) ? hiTex : slotTex;
    if(t!=nullptr) {
      p.setBrush(*t);
      p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
      } else {
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
      p.drawRect(r.x, r.y, r.w, r.h);
      }
    };
  auto drawFist = [&](const Rect& r) {
    const float cx = float(r.x)+float(r.w)*0.5f, cy = float(r.y)+float(r.h)*0.55f, u = float(r.w)*0.13f;
    p.setBrush(Brush(Color(0.78f,0.62f,0.48f,1.f), Painter::Alpha));
    p.drawRect(int(cx-2.f*u), int(cy-0.6f*u), int(4.f*u), int(2.2f*u));      // back of the hand
    for(int f=0; f<4; ++f)
      p.drawRect(int(cx-2.f*u+float(f)*u+1.f), int(cy-1.7f*u), int(u-2.f), int(1.3f*u)); // fingers
    p.drawRect(int(cx-2.7f*u), int(cy-0.2f*u), int(0.9f*u), int(1.3f*u));    // thumb
    };
  auto drawShield = [&](const Rect& r) {
    // block: the shield with Khorinis' coat of arms (wall decoration in the city), cut out along its edge
    auto t = Resources::loadTexture("OBJ_CITY_DECO_SHIELD_01.TGA");
    if(t==nullptr) {
      const float cx = float(r.x)+float(r.w)*0.5f, cy = float(r.y)+float(r.h)*0.46f, u = float(r.w)*0.30f;
      static const float shape[] = {-1.f,-1.f, 1.f,-1.f, 1.f,0.15f, 0.55f,0.8f, 0.f,1.15f, -0.55f,0.8f, -1.f,0.15f};
      p.setBrush(Brush(Color(0.36f,0.38f,0.42f,1.f), Painter::Alpha));
      poly(cx, cy, shape, 7, u);
      return;
      }
    // outline in texture space (256x256 texture: x 44..205, y 6..250)
    static const float uv[][2] = {{44,6},{205,6},{205,150},{190,190},{125,250},{60,190},{44,150}};
    const float u0 = 40.f, v0 = 4.f, uw = 169.f, vh = 248.f;          // the part that is drawn
    const float hh = float(r.h)*0.86f, ww = hh*uw/vh;
    const float x0 = float(r.x)+(float(r.w)-ww)*0.5f, y0 = float(r.y)+(float(r.h)-hh)*0.5f;
    const float tw = float(t->w())/256.f, th = float(t->h())/256.f;
    auto sx = [&](int i) { return x0 + (uv[i][0]-u0)/uw*ww; };
    auto sy = [&](int i) { return y0 + (uv[i][1]-v0)/vh*hh; };
    const float cu = 125.f, cv = 128.f;                                  // fan center
    const float cx = x0 + (cu-u0)/uw*ww, cy = y0 + (cv-v0)/vh*hh;
    p.setBrush(Brush(*t, Painter::Alpha, ClampMode::ClampToEdge));
    for(int i=0; i<7; ++i) {
      const int j = (i+1)%7;
      p.drawTriangle(cx, cy, cu*tw, cv*th,
                     sx(i), sy(i), uv[i][0]*tw, uv[i][1]*th,
                     sx(j), sy(j), uv[j][0]*tw, uv[j][1]*th);
      }
    };
  auto drawPush = [&](const Rect& r) {
    // push: the Wind Fist rune's symbol (its face in ITARRUNE1, cell 6 of the first row)
    auto t = Resources::loadTexture("ITARRUNE1.TGA");
    if(t==nullptr)
      return;
    const int pad = r.w/12;
    p.setBrush(Brush(*t, Painter::Alpha, ClampMode::ClampToEdge));
    p.drawRect(r.x+pad, r.y+pad, r.w-2*pad, r.h-2*pad,
               float(t->w())*0.625f, 0.f, float(t->w())*0.75f, float(t->h())*0.125f);
    };
  auto drawGlow = [&](const Rect& r) {
    // "skill": red glow inside the slot, model drawn over it
    for(int k=0; k<6; ++k) {
      const int   in = int(float(r.w)*0.06f*float(k));
      const float a  = 0.09f;
      p.setBrush(Brush(Color(0.9f*a,0.12f*a,0.08f*a,1.f), Painter::Add));
      p.drawRect(r.x+in, r.y+in, r.w-2*in, r.h-2*in);
      }
    p.setBrush(Brush(Color(0.85f,0.12f,0.08f,0.9f), Painter::Alpha));
    const int b = std::max(1, r.w/24);
    p.drawRect(r.x, r.y, r.w, b);
    p.drawRect(r.x, r.y+r.h-b, r.w, b);
    p.drawRect(r.x, r.y, b, r.h);
    p.drawRect(r.x+r.w-b, r.y, b, r.h);
    };
  auto model = [&](const Rect& r, const Item* itm) {
    if(itm==nullptr)
      return;
    const int pad = r.w/10;
    arpgBarItems.drawItem(r.x+pad, r.y+pad, r.w-2*pad, r.h-2*pad, *itm);
    };

  // LPM: the weapon in use, PPM: the weapon with a red glow (charged attack / shot = special skill), SPM: block or push
  {
    const Rect r = arpgBarSlot(0);
    drawSlot(r, arpgLeftDown);
    if(bar.weapon!=nullptr) model(r, bar.weapon); else drawFist(r);
  }
  {
    const Rect r = arpgBarSlot(1);
    drawSlot(r, bar.charging);
    drawGlow(r); // special skill
    if(bar.weapon!=nullptr) model(r, bar.weapon); else drawFist(r);
  }
  {
    const Rect r = arpgBarSlot(2);
    drawSlot(r, arpgMidBlock || arpgMidPush);
    if(bar.ranged) drawPush(r); else drawShield(r);
  }
  // 1-2: health / mana potion that the key drinks now
  for(int k=0; k<2; ++k) {
    const Rect r = arpgBarSlot(3+k);
    drawSlot(r, false);
    model(r, bar.potion[k]);
  }
  // 3-0: Gothic's rune / scroll slots
  for(uint8_t k=0; k<8; ++k) {
    const Rect r   = arpgBarSlot(5+k);
    auto       itm = pl->inventory().currentSpell(k);
    drawSlot(r, ws==WeaponState::Mage && itm!=nullptr && pl->inventory().currentSpellSlot()==k+3);
    model(r, itm);
  }
  }

void MainWindow::paintArpgBarOverlay(Painter& p) {
  // over the item models: cooldowns, key names, potion counts
  if(!arpgBarVisible())
    return;
  const float scale = Gothic::interfaceScale(this);
  const auto  bar   = player.arpgBar();
  auto&       fnt   = Resources::font(Resources::FontType::Normal,   scale*0.7f);
  auto&       fntD  = Resources::font(Resources::FontType::Disabled, scale*0.7f);

  auto cooldown = [&](const Rect& r, float left, float total) {
    if(left<=0.f)
      return;
    const int hh = int(float(r.h)*left);
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.65f), Painter::Alpha));
    p.drawRect(r.x, r.y, r.w, hh);
    char txt[16] = {};
    std::snprintf(txt, sizeof(txt), "%d", int(std::ceil(left*total)));
    auto& f  = Resources::font(Resources::FontType::Normal, scale*1.1f);
    auto  ts = f.textSize(txt);
    p.setBrush(Color(1.f));
    f.drawText(p, r.x+(r.w-ts.w)/2, r.y+(r.h+ts.h)/2-ts.h/4, txt);
    };
  auto label = [&](const Rect& r, std::string_view txt, bool dim) {
    p.setBrush(Color(1.f));
    auto& f = dim ? fntD : fnt;
    f.drawText(p, r.x+std::max(2,r.w/16), r.y+f.textSize(txt).h, txt);
    };

  const Rect rL = arpgBarSlot(0), rR = arpgBarSlot(1), rM = arpgBarSlot(2);
  cooldown(rR, player.arpgSpecialLeft(), player.arpgSpecialCooldown());
  if(bar.ranged)
    cooldown(rM, player.arpgPushLeft(), PlayerControl::PushCooldown);
  label(rL, "LPM", false);
  label(rR, "PPM", false);
  label(rM, "SPM", false);

  for(int k=0; k<2; ++k) {
    const Rect r = arpgBarSlot(3+k);
    // Diathic: the key of the potion from ArpgKeys (cut to fit the slot)
    std::string key = ArpgKeys::keyName(ArpgKeys::key(k==0 ? ArpgKeys::PotionHealth : ArpgKeys::PotionMana));
    if(key.size()>3)
      key = key.substr(0,3);
    label(r, ArpgMenu::gtxt(key), bar.potions[k]==0);
    char cnt[16] = {};
    std::snprintf(cnt, sizeof(cnt), "%d", int(bar.potions[k]));
    auto& f  = bar.potions[k]==0 ? fntD : fnt;
    auto  ts = f.textSize(cnt);
    p.setBrush(Color(1.f));
    f.drawText(p, r.x+r.w-ts.w-std::max(2,r.w/16), r.y+r.h-std::max(2,r.h/16), cnt);
    }

  auto pl    = Gothic::inst().player();
  auto world = Gothic::inst().world();
  for(uint8_t k=0; k<8; ++k) {
    const Rect  r   = arpgBarSlot(5+k);
    const Item* itm = (pl!=nullptr) ? pl->inventory().currentSpell(k) : nullptr;
    if(itm!=nullptr && world!=nullptr) {
      const int32_t cost = world->script().arpgSpellCost(*itm);
      const bool    poor = (cost>0 && pl->attribute(ATR_MANA)<cost);
      if(poor) {
        p.setBrush(Brush(Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
        p.drawRect(r.x, r.y, r.w, r.h);
        }
      if(cost>0) {
        char txt[16] = {};
        std::snprintf(txt, sizeof(txt), "%d", int(cost));
        auto& f  = Resources::font(poor ? Resources::FontType::Red : Resources::FontType::Normal, scale*0.7f);
        auto  ts = f.textSize(txt);
        p.setBrush(Color(1.f));
        f.drawText(p, r.x+r.w-ts.w-std::max(2,r.w/16), r.y+r.h-std::max(2,r.h/16), txt);
        }
      }
    char key[4] = {};
    std::snprintf(key, sizeof(key), "%d", (k+3)%10);
    label(r, key, itm==nullptr);
    }

  // skill points to spend: a gold line over the bar
  if(auto gs = Gothic::inst().gameSession(); gs!=nullptr && pl!=nullptr) {
    const int32_t pts = gs->arpgProgress().freePoints(pl->level());
    if(pts>0) {
      char txt[96] = {};
      std::snprintf(txt, sizeof(txt), "Punkty umiejętności do wydania: %d  (%s)", int(pts),
                    ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Skills)).c_str());
      GthFont f = Resources::font(Resources::FontType::Hi, scale*0.8f);
      f.setColor(Color(1.f, 0.82f, 0.42f, 1.f)); // gold
      f.drawText(p, rL.x, rL.y-int(16.f*scale), ArpgMenu::gtxt(txt)); // over the experience bar
      }
  }
  }

void MainWindow::onArpgSpeech(Npc& npc, std::string_view text, uint64_t ms, bool scripted) {
  // gothic-diablo: a third one in a cinematic talk goes to its subtitles, anything else near the hero over the head
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr || text.empty())
    return;
  if(scripted && dialogs.arpgThirdLine(npc, text, ms))
    return; // remarks (SVM) of people around do not cut into the film
  if(npc.qDistTo(*pl)>2500.f*2500.f)
    return;
  const uint64_t now = world->tickCount();
  for(auto& r:arpgRemarks)
    if(r.npc==&npc) {
      r.text  = std::string(text);
      r.start = now;
      r.until = now+ms+600;
      return;
      }
  if(arpgRemarks.size()>=4)
    arpgRemarks.erase(arpgRemarks.begin());
  ArpgRemark r;
  r.npc   = &npc;
  r.text  = std::string(text);
  r.start = now;
  r.until = now+ms+600;
  arpgRemarks.push_back(std::move(r));
  }

void MainWindow::paintArpgRemarks(Painter& p, const Matrix4x4& vp) {
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr) {
    arpgRemarks.clear();
    return;
    }
  if(arpgRemarks.empty())
    return;
  const uint64_t now = world->tickCount();

  // only creatures that are still here (around the hero)
  std::vector<const Npc*> alive;
  world->detectNpc(pl->position(), 3000.f, [&](Npc& n) { alive.push_back(&n); });
  for(size_t i=0; i<arpgRemarks.size();) {
    auto& r = arpgRemarks[i];
    const bool here = std::find(alive.begin(), alive.end(), r.npc)!=alive.end();
    if(now>r.until || !here || r.npc->isDead())
      arpgRemarks.erase(arpgRemarks.begin()+int(i)); else
      ++i;
    }

  const float k   = std::max(0.5f, float(h())/1080.f);
  auto&       fnt = Resources::font("font_old_10_white.tga", Resources::FontType::Hi, 1.15f*k);
  const int   tw  = int(380*k);
  const int   pad = int(8*k);
  for(auto& r:arpgRemarks) {
    Vec3  pos = r.npc->mapHeadBone()+Vec3(0, 45.f, 0);
    float pw  = 1.f;
    vp.project(pos.x, pos.y, pos.z, pw);
    if(pw<=0.f)
      continue;
    pos /= pw;
    if(pos.x<-1.1f || pos.x>1.1f || pos.y<-1.1f || pos.y>1.1f)
      continue;
    const int cx = int((0.5f*pos.x+0.5f)*float(w()));
    const int cy = int((0.5f*pos.y+0.5f)*float(h()));

    // in 0.15 s, out in the last 0.3 s
    float a = 1.f;
    if(now-r.start<150)
      a = float(now-r.start)/150.f;
    if(r.until-now<300)
      a = std::min(a, float(r.until-now)/300.f);

    const auto  sz = fnt.textSize(tw, r.text);
    const int   bw = std::min(tw, sz.w)+2*pad;
    const int   bh = sz.h+2*pad;
    const Rect  box(cx-bw/2, cy-bh, bw, bh);
    p.setBrush(Brush(Color(0.f, 0.f, 0.f, 0.55f*a), Painter::Alpha));
    p.drawRect(box.x, box.y, box.w, box.h);
    GthFont f = fnt;
    f.setColor(Color(1.f, 1.f, 1.f, a));
    f.drawText(p, box.x+pad, box.y+pad+f.pixelSize(), bw-2*pad, 0, r.text, AlignHCenter);
    }
  }
