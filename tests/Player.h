#ifndef PLAYER_GAMEPLAYER_H
#define PLAYER_GAMEPLAYER_H

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "CKAll.h"

#define PLAYER_DEFAULT_WIDTH 640
#define PLAYER_DEFAULT_HEIGHT 480
#define PLAYER_DEFAULT_BPP 32

enum PlayerPathCategory {
    ePluginPath,
    eRenderEnginePath,
    eManagerPath,
    eBuildingBlockPath,
    eSoundPath,
    eBitmapPath,
    eDataPath,
    ePathCategoryCount
};

class PlayerConfig {
public:
    // Graphics
    int driver;
    int screenMode;
    int bpp;
    int width;
    int height;
    bool fullscreen;

    // Window Settings
    bool childWindowRendering;
    bool borderless;
    bool alwaysHandleInput;

    PlayerConfig();
    PlayerConfig &operator=(const PlayerConfig &config);

    bool HasPath(PlayerPathCategory category) const;
    const char *GetPath(PlayerPathCategory category) const;
    void SetPath(PlayerPathCategory category, const char *path);
    bool ResetPath(PlayerPathCategory category = ePathCategoryCount);

private:
    std::string m_Paths[ePathCategoryCount];
};

class Player {
public:
    Player();

    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;

    ~Player();

    bool Init(HINSTANCE hInstance = nullptr);
    bool Load(const char *filename);

    void Run();
    bool Update();
    void Process();
    void Render();
    void Shutdown();

    void Play();
    void Pause();
    void Reset();

    CKContext *GetCKContext() {
        return m_CKContext;
    }

    CKRenderContext *GetRenderContext() {
        return m_RenderContext;
    }

    CKRenderManager *GetRenderManager() {
        return m_RenderManager;
    }

private:
    enum PlayerState {
        eInitial = 0,
        eReady,
        ePlaying,
        ePaused,
        eFocusLost,
    };

    PlayerConfig &GetConfig() {
        return m_Config;
    }

    bool InitWindow(HINSTANCE hInstance);
    void ShutdownWindow();

    bool InitEngine(HWND mainWindow);
    void ShutdownEngine();

    bool InitDriver();

    bool FinishLoad(const char *filename);
    void ReportMissingGuids(CKFile *file, const char *resolvedFile);

    bool InitPlugins(CKPluginManager *pluginManager);
    bool LoadRenderEngines(CKPluginManager *pluginManager);
    bool LoadManagers(CKPluginManager *pluginManager);
    bool LoadBuildingBlocks(CKPluginManager *pluginManager);
    bool LoadPlugins(CKPluginManager *pluginManager);
    bool UnloadPlugins(CKPluginManager *pluginManager, CK_PLUGIN_TYPE type, CKGUID guid);

    int FindRenderEngine(CKPluginManager *pluginManager);

    bool SetupManagers();
    bool SetupPaths();

    void ResizeWindow();

    int FindScreenMode(int width, int height, int bpp, int driver);
    bool GetDisplayMode(int &width, int &height, int &bpp, int driver, int screenMode);
    void SetDefaultValuesForDriver();

    bool IsRenderFullscreen() const;
    bool GoFullscreen();
    bool StopFullscreen();

    void OnDestroy();
    void OnMove();
    void OnSize();
    void OnPaint();
    void OnClose();
    void OnActivateApp(bool active);
    void OnSetCursor();
    void OnGetMinMaxInfo(LPMINMAXINFO lpmmi);
    int OnSysKeyDown(UINT uKey);
    void OnClick(bool dblClk = false);
    void OnExitToSystem();
    int OnChangeScreenMode(int driver, int screenMode);
    void OnGoFullscreen();
    void OnStopFullscreen();
    void OnSwitchFullscreen();

    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static bool RegisterMainWindowClass(HINSTANCE hInstance);
    static bool RegisterRenderWindowClass(HINSTANCE hInstance);
    static bool UnregisterMainWindowClass(HINSTANCE hInstance);
    static bool UnregisterRenderWindowClass(HINSTANCE hInstance);

    static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    int m_State;
    HINSTANCE m_hInstance;
    HWND m_MainWindow;
    HWND m_RenderWindow;

    CKContext *m_CKContext;
    CKRenderContext *m_RenderContext;
    CKRenderManager *m_RenderManager;
    CKMessageManager *m_MessageManager;
    CKTimeManager *m_TimeManager;
    CKAttributeManager *m_AttributeManager;
    CKInputManager *m_InputManager;

    CKMessageType m_MsgClick;
    CKMessageType m_MsgDoubleClick;

    PlayerConfig m_Config;
};

#endif /* PLAYER_GAMEPLAYER_H */
