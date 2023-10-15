#pragma once

#include <string>
#include <memory>
#include <thread>

#include <Tempest/Signal>
#include <Tempest/Dir>

#include <phoenix/vm.hh>

#include "game/gamesession.h"
#include "world/world.h"
#include "ui/documentmenu.h"
#include "ui/chapterscreen.h"
#include "utils/versioninfo.h"

class VersionInfo;
class CameraDefinitions;
class SoundDefinitions;
class VisualFxDefinitions;
class ParticlesDefinitions;
class MusicDefinitions;
class FightAi;
class IniFile;

class Gothic final {
  public:
    Gothic();
    ~Gothic();

    static Gothic& inst();

    enum class LoadState:int {
      Idle       = 0,
      Loading    = 1,
      Saving     = 2,
      Finalize   = 3,
      FailedLoad = 4,
      FailedSave = 5
      };

    struct Options {
      bool doRayQuery    = false;
      bool doRtGi        = false;
      bool doMeshShading = false;
      };

    auto         version() const -> const VersionInfo&;

    bool         isInGame() const;
    bool         isInGameAndAlive() const;

    std::string_view defaultWorld()       const;
    std::string_view defaultPlayer()      const;
    std::string_view defaultSave()        const;
    std::string_view defaultGameDatFile() const;
    std::string_view defaultOutputUnits() const;

    void         setGame(std::unique_ptr<GameSession> &&w);
    auto         clearGame() -> std::unique_ptr<GameSession>;

    const World* world() const;
    World*       world();
    WorldView*   worldView() const;
    Npc*         player();
    Camera*      camera();
    auto         questLog() const -> const QuestLog*;

    void         setupGlobalScripts();

    auto         loadingBanner() const -> const Tempest::Texture2d*;
    int          loadingProgress() const;
    void         setLoadingProgress(int v);

    SoundFx*     loadSoundFx   (std::string_view name);
    SoundFx*     loadSoundWavFx(std::string_view name);

    auto         loadParticleFx(std::string_view name, bool relaxed=false) -> const ParticleFx*;
    auto         loadParticleFx(const ParticleFx* base, const VisualFx::Key* key) -> const ParticleFx*;
    auto         loadVisualFx  (std::string_view name) -> const VisualFx*;

    void         emitGlobalSound(std::string_view   sfx);
    void         emitGlobalSound(const SoundFx*     sfx);
    void         emitGlobalSound(const Tempest::Sound& sfx);

    void         emitGlobalSoundWav(std::string_view wav);

    static auto  invCatOrder()  -> const std::vector<ItmFlags>&;

    void         pushPause();
    void         popPause();
    bool         isPause() const;

    bool         isMarvinEnabled() const;
    void         setMarvinEnabled(bool m);

    static auto  options() -> const Options&;
    bool         doHideFocus () const { return hideFocus; }

    bool         isGodMode() const { return godMode; }
    void         setGodMode(bool g) { godMode = g; }

    void         toggleDesktop() { desktop = !desktop; }
    bool         isDesktop() { return desktop; }

    bool         doFrate() const { return showFpsCounter; }
    void         setFRate(bool f) { showFpsCounter = f; }

    bool         doClock() const { return showTime; }
    void         setClock(bool t) { showTime = t; }

    Tempest::Signal<void()> toggleGi;

    LoadState    checkLoading() const;
    bool         finishLoading();
    void         startLoad(std::string_view banner, const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);
    void         startSave(Tempest::Texture2d&& tex, const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);
    void         cancelLoading();

    void         tick(uint64_t dt);

    void         updateAnimation(uint64_t dt);
    void         quickSave();
    void         quickLoad();
    void         save(std::string_view slot, std::string_view usrName);
    void         load(std::string_view slot);

    auto         updateDialog(const GameScript::DlgChoice& dlg, Npc& player, Npc& npc) -> std::vector<GameScript::DlgChoice>;
    void         dialogExec  (const GameScript::DlgChoice& dlg, Npc& player, Npc& npc);

    void         openDialogPipe (Npc& player, Npc& npc, AiOuputPipe*& pipe);
    bool         aiIsDlgFinished();

    Tempest::Signal<void(std::string_view)>                             onStartGame;
    Tempest::Signal<void(std::string_view)>                             onLoadGame;
    Tempest::Signal<void(std::string_view,std::string_view)>            onSaveGame;

    Tempest::Signal<void(Npc&,Npc&,AiOuputPipe*&)>                      onDialogPipe;
    Tempest::Signal<void(bool&)>                                        isDialogClose;

    Tempest::Signal<void(std::string_view,int,int,int,const GthFont&)>  onPrintScreen;
    Tempest::Signal<void(std::string_view)>                             onPrint;
    Tempest::Signal<void(std::string_view)>                             onVideo;

    Tempest::Signal<void(const ChapterScreen::Show&)>                   onIntroChapter;
    Tempest::Signal<void(const DocumentMenu::Show&)>                    onShowDocument;
    Tempest::Signal<void()>                                             onWorldLoaded;
    Tempest::Signal<void()>                                             onStartLoading;
    Tempest::Signal<void()>                                             onSessionExit;
    Tempest::Signal<void()>                                             onSettingsChanged;

    std::string_view                      messageFromSvm(std::string_view id, int voice) const;
    std::string_view                      messageByName (std::string_view id) const;
    uint32_t                              messageTime   (std::string_view id) const;

    static std::u16string                 nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type);
    std::unique_ptr<phoenix::vm>          createPhoenixVm(std::string_view datFile, const ScriptLang lang = ScriptLang::NONE);
    phoenix::script                       loadScript(std::string_view datFile, const ScriptLang lang);
    void                                  setupVmCommonApi(phoenix::vm &vm);

    static const FightAi&                 fai();
    static const SoundDefinitions&        sfx();
    static const MusicDefinitions&        musicDef();
    static const CameraDefinitions&       cameraDef();

    static bool                           settingsHasSection(std::string_view sec);
    static int                            settingsGetI(std::string_view sec, std::string_view name);
    static void                           settingsSetI(std::string_view sec, std::string_view name, int val);
    static std::string_view               settingsGetS(std::string_view sec, std::string_view name);
    static void                           settingsSetS(std::string_view sec, std::string_view name, std::string_view val);
    static float                          settingsGetF(std::string_view sec, std::string_view name);
    static void                           settingsSetF(std::string_view sec, std::string_view name, float val);
    static void                           flushSettings();

  private:
    VersionInfo                             vinfo;
    Options                                 opts;
    std::mt19937                            randGen;
    uint16_t                                pauseSum=0;
    bool                                    isMarvin       = false;
    bool                                    godMode        = false;
    bool                                    desktop        = false;
    bool                                    showFpsCounter = false;
    bool                                    showTime       = false;
    bool                                    hideFocus      = false;

    std::string                             wrldDef, plDef, gameDatDef, ouDef;

    std::unique_ptr<IniFile>                defaults;
    std::unique_ptr<IniFile>                baseIniFile;
    std::unique_ptr<IniFile>                iniFile;
    std::unique_ptr<IniFile>                modFile;
    std::unique_ptr<IniFile>                systemPackIniFile;

    const Tempest::Texture2d*               loadTex=nullptr;
    Tempest::Texture2d                      saveTex;
    std::atomic_int                         loadProgress{0};
    std::thread                             loaderTh;
    std::atomic<LoadState>                  loadingFlag{LoadState::Idle};

    std::unique_ptr<GameSession>            game, pendingGame;
    std::unique_ptr<FightAi>                fight;
    std::unique_ptr<CameraDefinitions>      camDef;
    std::unique_ptr<SoundDefinitions>       soundDef;
    std::unique_ptr<VisualFxDefinitions>    vfxDef;
    std::unique_ptr<ParticlesDefinitions>   particleDef;
    std::unique_ptr<MusicDefinitions>       music;

    std::mutex                              syncSnd;
    Tempest::SoundDevice                    sndDev;
    std::unordered_map<std::string,SoundFx> sndFxCache;
    std::unordered_map<std::string,SoundFx> sndWavCache;
    std::vector<Tempest::SoundEffect>       sndStorage;

    std::vector<std::unique_ptr<DocumentMenu::Show>> documents;
    ChapterScreen::Show                     chapter;
    bool                                    pendingChapter=false;

    std::vector<ItmFlags>                   inventoryOrder;

    static Gothic*                          instance;

    void                                    implStartLoadSave(std::string_view banner,
                                                              bool load,
                                                              const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);

    void                                    detectGothicVersion();
    void                                    setupSettings();

    auto                                    getDocument(int id) -> std::unique_ptr<DocumentMenu::Show>&;

    static void                             notImplementedRoutine(std::string_view fn);

    static std::string                      concatstrings     (std::string_view a, std::string_view b);
    static std::string                      inttostring       (int i);
    static std::string                      floattostring     (float f);
    static int                              floattoint        (float f);
    static float                            inttofloat        (int i);

    static bool                             hlp_strcmp        (std::string_view a, std::string_view b);
    int                                     hlp_random        (int max);

    void                                    introducechapter  (std::string_view title, std::string_view subtitle, std::string_view img, std::string_view sound, int time);
    bool                                    playvideo         (std::string_view name);
    bool                                    playvideoex       (std::string_view name, bool screenBlend, bool exitSession);
    bool                                    printscreen       (std::string_view msg, int posx, int posy, std::string_view font, int timesec);
    bool                                    printdialog       (int dialognr, std::string_view msg, int posx, int posy, std::string_view font, int timesec);
    void                                    print             (std::string_view msg);

    int                                     doc_create        ();
    int                                     doc_createmap     ();
    void                                    doc_setpage       (int handle, int page, std::string_view img, int scale);
    void                                    doc_setpages      (int handle, int count);
    void                                    doc_printline     (int handle, int page, std::string_view text);
    void                                    doc_printlines    (int handle, int page, std::string_view text);
    void                                    doc_setmargins    (int handle, int page, int left, int top, int right, int bottom, int mul);
    void                                    doc_setfont       (int handle, int page, std::string_view font);
    void                                    doc_setlevel      (int handle, std::string_view level);
    void                                    doc_setlevelcoords(int handle, int left, int top, int right, int bottom);
    void                                    doc_show          (int handle);

    void                                    exitgame          ();

    void                                    printdebug        (std::string_view msg);
    void                                    printdebugch      (int ch, std::string_view msg);
    void                                    printdebuginst    (std::string_view msg);
    void                                    printdebuginstch  (int ch, std::string_view msg);
  };
