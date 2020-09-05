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
    Gothic(int argc,const char** argv);
    ~Gothic();

    enum class LoadState:int {
      Idle       = 0,
      Loading    = 1,
      Saving     = 2,
      Finalize   = 3,
      FailedLoad = 4,
      FailedSave = 5
      };

    enum GraphicBackend : uint8_t {
      Vulkan,
      DirectX12
      };

    auto         version() const -> const VersionInfo&;
    auto         graphicsApi() const -> GraphicBackend;

    bool         isInGame() const;
    bool         doStartMenu() const { return !noMenu; }
    bool         doFrate() const { return !noFrate; }

    void         setGame(std::unique_ptr<GameSession> &&w);
    auto         clearGame() -> std::unique_ptr<GameSession>;
    const World* world() const;
    World*       world();
    WorldView*   worldView() const;
    Npc*         player();
    Camera*      gameCamera();
    auto         questLog() const -> const QuestLog*;

    auto      loadingBanner() const -> const Tempest::Texture2d*;
    int       loadingProgress() const;
    void      setLoadingProgress(int v);

    SoundFx*  loadSoundFx   (const char* name);
    SoundFx*  loadSoundWavFx(const char *name);

    auto      loadParticleFx(const char* name) -> const ParticleFx*;
    auto      loadVisualFx  (const char* name) -> const VisualFx*;

    void      emitGlobalSound(const char*        sfx);
    void      emitGlobalSound(const std::string& sfx);
    void      emitGlobalSound(const SoundFx*     sfx);
    void      emitGlobalSound(const Tempest::Sound& sfx);

    void      emitGlobalSoundWav(const std::string& wav);

    void      pushPause();
    void      popPause();
    bool      isPause() const;

    bool      isDebugMode() const;
    bool      isRamboMode() const;
    bool      isWindowMode() const { return isWindow; }

    LoadState checkLoading() const;
    bool      finishLoading();
    void      startLoad(const char *banner, const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);
    void      startSave(Tempest::Texture2d&& tex, const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);
    void      cancelLoading();

    void      tick(uint64_t dt);

    void      updateAnimation();
    void      quickSave();
    void      quickLoad();
    void      save(const std::string& slot);
    void      load(const std::string& slot);

    auto      updateDialog(const GameScript::DlgChoise& dlg, Npc& player, Npc& npc) -> std::vector<GameScript::DlgChoise>;
    void      dialogExec  (const GameScript::DlgChoise& dlg, Npc& player, Npc& npc);

    void      openDialogPipe (Npc& player, Npc& npc, AiOuputPipe*& pipe);
    bool      aiIsDlgFinished();

    auto      getFightAi(size_t i) const -> const FightAi::FA&;
    auto      getSoundScheme(const char* name) -> const Daedalus::GEngineClasses::C_SFX&;
    auto      getCameraDef() const -> const CameraDefinitions&;
    auto      getMusicDef(const char *clsTheme) const -> const Daedalus::GEngineClasses::C_MusicTheme*;

    Tempest::Signal<void(const std::string&)>              onStartGame;
    Tempest::Signal<void(const std::string&)>              onLoadGame;
    Tempest::Signal<void(const std::string&)>              onSaveGame;

    Tempest::Signal<void(Npc&,Npc&,AiOuputPipe*&)>         onDialogPipe;
    Tempest::Signal<void(bool&)>                           isDialogClose;

    Tempest::Signal<void(const char*,int,int,int,const GthFont&)>       onPrintScreen;
    Tempest::Signal<void(const char*)>                                  onPrint;
    Tempest::Signal<void(const Daedalus::ZString&)>                     onVideo;

    Tempest::Signal<void(const ChapterScreen::Show&)>                   onIntroChapter;
    Tempest::Signal<void(const DocumentMenu::Show&)>                    onShowDocument;
    Tempest::Signal<void()>                                             onWorldLoaded;
    Tempest::Signal<void()>                                             onStartLoading;
    Tempest::Signal<void()>                                             onSessionExit;
    Tempest::Signal<void()>                                             onSettingsChanged;

    const Daedalus::ZString&              messageFromSvm(const Daedalus::ZString& id, int voice) const;
    const Daedalus::ZString&              messageByName(const Daedalus::ZString &id) const;
    uint32_t                              messageTime(const Daedalus::ZString& id) const;

    std::u16string                        nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;
    const std::string&                    defaultWorld() const;
    const std::string&                    defaultSave() const;
    std::unique_ptr<Daedalus::DaedalusVM> createVm(const char16_t *datFile);
    void                                  setupVmCommonApi(Daedalus::DaedalusVM &vm);

    int                                   settingsGetI(const char* sec, const char* name) const;
    void                                  settingsSetI(const char* sec, const char* name, int val);
    const std::string&                    settingsGetS(const char* sec, const char* name) const;
    void                                  settingsSetS(const char* sec, const char* name, const char* val) const;
    float                                 settingsGetF(const char* sec, const char* name) const;
    void                                  settingsSetF(const char* sec, const char* name, float val);
    void                                  flushSettings() const;

    static void debug(const ZenLoad::zCMesh &mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedMesh& mesh, std::ostream& out);
    static void debug(const ZenLoad::PackedSkeletalMesh& mesh, std::ostream& out);

  private:
    std::u16string                          gpath, gscript;
    std::string                             wdef;
    std::string                             saveDef;
    bool                                    noMenu=false;
    bool                                    noFrate=false;
    bool                                    isWindow=false;
    GraphicBackend                          graphics = GraphicBackend::Vulkan;
    uint16_t                                pauseSum=0;
    bool                                    isDebug=false;
    bool                                    isRambo=false;
    VersionInfo                             vinfo;
    std::mt19937                            randGen;

    std::unique_ptr<IniFile>                baseIniFile;
    std::unique_ptr<IniFile>                iniFile;

    const Tempest::Texture2d*               loadTex=nullptr;
    Tempest::Texture2d                      saveTex;
    std::atomic_int                         loadProgress{0};
    std::thread                             loaderTh;
    std::atomic<LoadState>                  loadingFlag{LoadState::Idle};

    std::unique_ptr<GameSession>            game, pendingGame;
    std::unique_ptr<FightAi>                fight;
    std::unique_ptr<CameraDefinitions>      camera;
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

    void                                    implStartLoadSave(const char *banner,
                                                              bool load,
                                                              const std::function<std::unique_ptr<GameSession>(std::unique_ptr<GameSession>&&)> f);

    bool                                    validateGothicPath() const;
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
    void                                    ai_printscreen    (Daedalus::DaedalusVM &vm);
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
