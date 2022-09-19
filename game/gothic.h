#pragma once

#include <string>
#include <memory>
#include <thread>

#include <Tempest/Signal>
#include <Tempest/Dir>

#include <daedalus/DaedalusVM.h>

#include "game/gamesession.h"
#include "world/world.h"
#include "ui/documentmenu.h"
#include "ui/chapterscreen.h"
#include "utils/versioninfo.h"
#include "gamemusic.h"

class VersionInfo;
class CameraDefinitions;
class SoundDefinitions;
class VisualFxDefinitions;
class ParticlesDefinitions;
class MusicDefinitions;
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

    auto         version() const -> const VersionInfo&;

    bool         isInGame() const;

    std::string_view defaultWorld()  const;
    std::string_view defaultPlayer() const;
    std::string_view defaultSave()   const;

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

    bool         doFrate() const { return !noFrate; }
    void         setFRate(bool f) { noFrate = !f; }

    bool         doRayQuery() const;
    bool         doMeshShading() const;

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

    auto         updateDialog(const GameScript::DlgChoise& dlg, Npc& player, Npc& npc) -> std::vector<GameScript::DlgChoise>;
    void         dialogExec  (const GameScript::DlgChoise& dlg, Npc& player, Npc& npc);

    void         openDialogPipe (Npc& player, Npc& npc, AiOuputPipe*& pipe);
    bool         aiIsDlgFinished();

    Tempest::Signal<void(std::string_view)>                             onStartGame;
    Tempest::Signal<void(std::string_view)>                             onLoadGame;
    Tempest::Signal<void(std::string_view,std::string_view)>            onSaveGame;

    Tempest::Signal<void(Npc&,Npc&,AiOuputPipe*&)>                      onDialogPipe;
    Tempest::Signal<void(bool&)>                                        isDialogClose;

    Tempest::Signal<void(std::string_view,int,int,int,const GthFont&)>  onPrintScreen;
    Tempest::Signal<void(std::string_view)>                             onPrint;
    Tempest::Signal<void(const Daedalus::ZString&)>                     onVideo;

    Tempest::Signal<void(const ChapterScreen::Show&)>                   onIntroChapter;
    Tempest::Signal<void(const DocumentMenu::Show&)>                    onShowDocument;
    Tempest::Signal<void()>                                             onWorldLoaded;
    Tempest::Signal<void()>                                             onStartLoading;
    Tempest::Signal<void()>                                             onSessionExit;
    Tempest::Signal<void()>                                             onSettingsChanged;

    const Daedalus::ZString&              messageFromSvm(const Daedalus::ZString& id, int voice) const;
    const Daedalus::ZString&              messageByName (const Daedalus::ZString& id) const;
    uint32_t                              messageTime   (const Daedalus::ZString& id) const;

    std::u16string                        nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(std::string_view datFile);
    std::vector<uint8_t>                  loadScriptCode(std::string_view datFile);
    void                                  setupVmCommonApi(Daedalus::DaedalusVM &vm);

    static const FightAi&                 fai();
    static const SoundDefinitions&        sfx();
    static const MusicDefinitions&        musicDef();
    static const CameraDefinitions&       cameraDef();

    static int                            settingsGetI(std::string_view sec, std::string_view name);
    static void                           settingsSetI(std::string_view sec, std::string_view name, int val);
    static std::string_view               settingsGetS(std::string_view sec, std::string_view name);
    static void                           settingsSetS(std::string_view sec, std::string_view name, std::string_view val);
    static float                          settingsGetF(std::string_view sec, std::string_view name);
    static void                           settingsSetF(std::string_view sec, std::string_view name, float val);
    static void                           flushSettings();

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    VersionInfo                             vinfo;
    std::mt19937                            randGen;
    uint16_t                                pauseSum=0;
    bool                                    isMarvin = false;
    bool                                    noFrate  = false;
    bool                                    isMeshSh = false;
    std::string                             wrldDef, plDef;

    std::unique_ptr<IniFile>                defaults;
    std::unique_ptr<IniFile>                baseIniFile;
    std::unique_ptr<IniFile>                iniFile;
    std::unique_ptr<IniFile>                modFile;

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

    static void                             notImplementedRoutine(Daedalus::DaedalusVM &vm);

    static void                             concatstrings     (Daedalus::DaedalusVM& vm);
    static void                             inttostring       (Daedalus::DaedalusVM& vm);
    static void                             floattostring     (Daedalus::DaedalusVM& vm);
    static void                             floattoint        (Daedalus::DaedalusVM& vm);
    static void                             inttofloat        (Daedalus::DaedalusVM& vm);

    static void                             hlp_strcmp        (Daedalus::DaedalusVM& vm);
    void                                    hlp_random        (Daedalus::DaedalusVM& vm);

    void                                    introducechapter  (Daedalus::DaedalusVM &vm);
    void                                    playvideo         (Daedalus::DaedalusVM &vm);
    void                                    playvideoex       (Daedalus::DaedalusVM &vm);
    void                                    printscreen       (Daedalus::DaedalusVM &vm);
    void                                    printdialog       (Daedalus::DaedalusVM &vm);
    void                                    print             (Daedalus::DaedalusVM &vm);

    void                                    doc_create        (Daedalus::DaedalusVM &vm);
    void                                    doc_createmap     (Daedalus::DaedalusVM &vm);
    void                                    doc_setpage       (Daedalus::DaedalusVM &vm);
    void                                    doc_setpages      (Daedalus::DaedalusVM &vm);
    void                                    doc_printline     (Daedalus::DaedalusVM &vm);
    void                                    doc_printlines    (Daedalus::DaedalusVM &vm);
    void                                    doc_setmargins    (Daedalus::DaedalusVM &vm);
    void                                    doc_setfont       (Daedalus::DaedalusVM &vm);
    void                                    doc_setlevel      (Daedalus::DaedalusVM &vm);
    void                                    doc_setlevelcoords(Daedalus::DaedalusVM &vm);
    void                                    doc_show          (Daedalus::DaedalusVM &vm);

    void                                    exitgame          (Daedalus::DaedalusVM &vm);

    void                                    printdebug        (Daedalus::DaedalusVM &vm);
    void                                    printdebugch      (Daedalus::DaedalusVM &vm);
    void                                    printdebuginst    (Daedalus::DaedalusVM &vm);
    void                                    printdebuginstch  (Daedalus::DaedalusVM &vm);
  };
