#include "Player.h"

#include <cstdio>
#include <cstring>

#define ARRAY_NUM(Array) \
    (sizeof(Array) / sizeof(Array[0]))

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

constexpr const char *const DefaultPaths[] = {
    "Plugins" PATH_SEPARATOR,
    "RenderEngines" PATH_SEPARATOR,
    "Managers" PATH_SEPARATOR,
    "BuildingBlocks" PATH_SEPARATOR,
    "Sounds" PATH_SEPARATOR,
    "Textures" PATH_SEPARATOR,
    "3D Entities" PATH_SEPARATOR,
};

class Logger {
public:
    enum Level {
        LEVEL_OFF   = 0,
        LEVEL_ERROR = 1,
        LEVEL_WARN  = 2,
        LEVEL_INFO  = 3,
        LEVEL_DEBUG = 4,
    };

    static Logger &Get();

    Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    ~Logger();

    void Open(const char *filename, bool overwrite = true, int level = LEVEL_INFO);
    void Close();

    bool IsConsoleOpened() const { return m_ConsoleOpened; }
    void OpenConsole(bool opened);

    int GetLevel() const { return m_Level; }
    void SetLevel(int level) { m_Level = level; }

    void Debug(const char *fmt, ...);
    void Info(const char *fmt, ...);
    void Warn(const char *fmt, ...);
    void Error(const char *fmt, ...);

private:

    void Log(const char *level, const char *fmt, va_list args);

    int m_Level;
    bool m_ConsoleOpened;
    FILE *m_File;
};

Logger &Logger::Get() {
    static Logger logger;
    return logger;
}

Logger::Logger() : m_Level(LEVEL_OFF), m_ConsoleOpened(false), m_File(nullptr) {}

Logger::~Logger() {
    Close();
}

void Logger::Open(const char *filename, bool overwrite, int level) {
    m_Level = level;

    if (m_File)
        return;

    if (overwrite)
        m_File = fopen(filename, "w");
    else
        m_File = fopen(filename, "a");
}

void Logger::Close() {
    if (m_ConsoleOpened) {
        ::FreeConsole();
        m_ConsoleOpened = false;
    }

    if (m_File) {
        fclose(m_File);
        m_File = nullptr;
    }
}

void Logger::OpenConsole(bool opened) {
    if (opened) {
        ::AllocConsole();
        freopen("CONOUT$", "w", stdout);
        m_ConsoleOpened = true;
    } else {
        freopen("CON", "w", stdout);
        ::FreeConsole();
        m_ConsoleOpened = false;
    }
}

void Logger::Debug(const char *fmt, ...) {
    if (m_Level >= LEVEL_DEBUG) {
        va_list args;
        va_start(args, fmt);
        Log("DEBUG", fmt, args);
        va_end(args);
    }
}

void Logger::Info(const char *fmt, ...) {
    if (m_Level >= LEVEL_INFO) {
        va_list args;
        va_start(args, fmt);
        Log("INFO", fmt, args);
        va_end(args);
    }
}

void Logger::Warn(const char *fmt, ...) {
    if (m_Level >= LEVEL_WARN) {
        va_list args;
        va_start(args, fmt);
        Log("WARN", fmt, args);
        va_end(args);
    }
}

void Logger::Error(const char *fmt, ...) {
    if (m_Level >= LEVEL_ERROR) {
        va_list args;
        va_start(args, fmt);
        Log("ERROR", fmt, args);
        va_end(args);
    }
}

void Logger::Log(const char *level, const char *fmt, va_list args) {
    SYSTEMTIME sys;
    GetLocalTime(&sys);

    FILE *outs[] = {stdout, m_File};

    for (int i = 0; i < 2; ++i) {
        FILE *out = outs[i];
        if (!out)
            continue;

        fprintf(out, "[%02d/%02d/%d %02d:%02d:%02d.%03d] ",
                sys.wMonth, sys.wDay, sys.wYear,
                sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
        fprintf(out, "[%s]: ", level);
        vfprintf(out, fmt, args);
        fputc('\n', out);
        fflush(out);
    }
}

static bool FileOrDirectoryExists(const char *file) {
    if (!file || file[0] == '\0')
        return false;
    const DWORD attributes = ::GetFileAttributesA(file);
    return attributes != INVALID_FILE_ATTRIBUTES;
}

static bool DirectoryExists(const char *dir) {
    if (!dir || dir[0] == '\0')
        return false;
    const DWORD attributes = ::GetFileAttributesA(dir);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

PlayerConfig::PlayerConfig() {
    driver = 0;
    screenMode = -1;
    bpp = PLAYER_DEFAULT_BPP;
    width = PLAYER_DEFAULT_WIDTH;
    height = PLAYER_DEFAULT_HEIGHT;
    fullscreen = false;

    childWindowRendering = false;
    borderless = false;
    alwaysHandleInput = false;

    ResetPath();
}

PlayerConfig &PlayerConfig::operator=(const PlayerConfig &config) {
    if (this == &config)
        return *this;

    driver = config.driver;
    screenMode = config.screenMode;
    bpp = config.bpp;
    width = config.width;
    height = config.height;
    fullscreen = config.fullscreen;

    childWindowRendering = config.childWindowRendering;
    borderless = config.borderless;
    alwaysHandleInput = config.alwaysHandleInput;

    for (int i = 0; i < ePathCategoryCount; ++i)
        m_Paths[i] = config.m_Paths[i];

    return *this;
}

bool PlayerConfig::HasPath(PlayerPathCategory category) const {
    if (category < 0 || category >= ePathCategoryCount)
        return false;
    return !m_Paths[category].empty();
}

const char *PlayerConfig::GetPath(PlayerPathCategory category) const {
    if (category < 0 || category >= ePathCategoryCount)
        return nullptr;
    return m_Paths[category].c_str();
}

void PlayerConfig::SetPath(PlayerPathCategory category, const char *path) {
    if (category < 0 || category >= ePathCategoryCount || !path)
        return;
    m_Paths[category] = path;
}

bool PlayerConfig::ResetPath(PlayerPathCategory category) {
    if (category < 0 || category > ePathCategoryCount)
        return false;

    if (category == ePathCategoryCount) {
        // Reset all paths
        for (int i = 0; i < ePathCategoryCount; ++i) {
            SetPath(static_cast<PlayerPathCategory>(i), DefaultPaths[i]);
        }
    } else {
        // Reset specific path
        SetPath(category, DefaultPaths[category]);
    }

    return true;
}

Player::Player()
    : m_State(eInitial),
      m_hInstance(nullptr),
      m_MainWindow(nullptr),
      m_RenderWindow(nullptr),
      m_CKContext(nullptr),
      m_RenderContext(nullptr),
      m_RenderManager(nullptr),
      m_MessageManager(nullptr),
      m_TimeManager(nullptr),
      m_AttributeManager(nullptr),
      m_InputManager(nullptr),
      m_MsgClick(-1),
      m_MsgDoubleClick(-1) {}

Player::~Player() {
    Shutdown();
}

bool Player::Init(HINSTANCE hInstance) {
    if (m_State != eInitial)
        return true;

    if (!hInstance)
        m_hInstance = ::GetModuleHandle(nullptr);
    else
        m_hInstance = hInstance;

    if (!InitWindow(m_hInstance)) {
        Logger::Get().Error("Failed to initialize window!");
        ShutdownWindow();
        return false;
    }

    if (!InitEngine(m_MainWindow)) {
        Logger::Get().Error("Failed to initialize CK Engine!");
        return false;
    }

    if (!InitDriver()) {
        Logger::Get().Error("Failed to initialize Render Driver!");
        Shutdown();
        return false;
    }

    RECT rc;
    ::GetClientRect(m_MainWindow, &rc);
    if (rc.right - rc.left != m_Config.width || rc.bottom - rc.top != m_Config.height) {
        ResizeWindow();
    }

    HWND handle = !m_Config.childWindowRendering ? m_MainWindow : m_RenderWindow;
    CKRECT rect = {0, 0, m_Config.width, m_Config.height};
    m_RenderContext = m_RenderManager->CreateRenderContext(handle, m_Config.driver, &rect, FALSE, m_Config.bpp);
    if (!m_RenderContext) {
        Logger::Get().Error("Failed to create Render Context!");
        return false;
    }

    Logger::Get().Debug("Render Context created.");

    if (m_Config.fullscreen)
        OnGoFullscreen();

    ::ShowWindow(m_MainWindow, SW_SHOW);
    ::SetFocus(m_MainWindow);

    m_State = eReady;
    return true;
}

bool Player::Load(const char *filename) {
    if (!filename || filename[0] == '\0') {
        Logger::Get().Error("Invalid filename!");
        return false;
    }

    Logger::Get().Debug("Loading %s", filename);

    if (m_State == eInitial) {
        Logger::Get().Error("Player is not initialized!");
        return false;
    }

    if (!m_CKContext) {
        Logger::Get().Error("CKContext is not initialized!");
        return false;
    }

    CKPathManager *pm = m_CKContext->GetPathManager();
    if (!pm) {
        Logger::Get().Error("Failed to get CKPathManager!");
        return false;
    }

    XString resolvedFile = filename;
    CKERROR err = pm->ResolveFileName(resolvedFile, DATA_PATH_IDX);
    if (err != CK_OK) {
        Logger::Get().Error("Failed to resolve filename %s", filename);
        return false;
    }

    m_CKContext->Reset();
    m_CKContext->ClearAll();

    // Load the file and fills the array with loaded objects
    CKFile *f = m_CKContext->CreateCKFile();
    if (!f) {
        Logger::Get().Error("Failed to create CKFile!");
        return false;
    }

    CKERROR res = f->OpenFile(resolvedFile.Str(), (CK_LOAD_FLAGS) (CK_LOAD_DEFAULT | CK_LOAD_CHECKDEPENDENCIES));
    if (res != CK_OK) {
        // something failed
        if (res == CKERR_PLUGINSMISSING) {
            // log the missing guids
            ReportMissingGuids(f, resolvedFile.CStr());
        }
        m_CKContext->DeleteCKFile(f);

        Logger::Get().Error("Failed to open file: %s", resolvedFile.CStr());
        return false;
    }

    CKObjectArray *array = CreateCKObjectArray();
    if (!array) {
        Logger::Get().Error("Failed to create CKObjectArray!");
        m_CKContext->DeleteCKFile(f);
        return false;
    }

    res = f->LoadFileData(array);
    if (res != CK_OK) {
        Logger::Get().Error("Failed to load file: %s", resolvedFile.CStr());
        m_CKContext->DeleteCKFile(f);
        DeleteCKObjectArray(array);
        return false;
    }

    m_CKContext->DeleteCKFile(f);
    DeleteCKObjectArray(array);

    Logger::Get().Info("Loaded %s", resolvedFile.CStr());

    return FinishLoad(filename);
}

void Player::Run() {
    while (Update())
        continue;
}

bool Player::Update() {
    MSG msg;
    if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;

        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    } else {
        float beforeRender = 0.0f;
        float beforeProcess = 0.0f;
        m_TimeManager->GetTimeToWaitForLimits(beforeRender, beforeProcess);
        if (beforeProcess <= 0) {
            m_TimeManager->ResetChronos(FALSE, TRUE);
            Process();
        }
        if (beforeRender <= 0) {
            m_TimeManager->ResetChronos(TRUE, FALSE);
            Render();
        }
    }

    return true;
}

void Player::Process() {
    m_CKContext->Process();
}

void Player::Render() {
    m_RenderContext->Render();
}

void Player::Shutdown() {
    ShutdownEngine();
    ShutdownWindow();

    if (m_State != eInitial) {
        m_State = eInitial;
        Logger::Get().Debug("Player shut down.");
    }
}

void Player::Play() {
    m_State = ePlaying;
    m_CKContext->Play();
    Logger::Get().Debug("Game is playing.");
}

void Player::Pause() {
    m_State = ePaused;
    m_CKContext->Pause();
    Logger::Get().Debug("Game is paused.");
}

void Player::Reset() {
    m_State = ePlaying;
    m_CKContext->Reset();
    m_CKContext->Play();
    Logger::Get().Debug("Game is reset.");
}

static CKERROR LogRedirect(CKUICallbackStruct &cbStruct, void *) {
    if (cbStruct.Reason == CKUIM_OUTTOCONSOLE ||
        cbStruct.Reason == CKUIM_OUTTOINFOBAR ||
        cbStruct.Reason == CKUIM_DEBUGMESSAGESEND) {
        static XString text = "";
        if (text.Compare(cbStruct.ConsoleString)) {
            Logger::Get().Info(cbStruct.ConsoleString);
            text = cbStruct.ConsoleString;
        }
    }
    return CK_OK;
}

bool Player::InitWindow(HINSTANCE hInstance) {
    if (!hInstance)
        return false;

    if (!RegisterMainWindowClass(hInstance)) {
        Logger::Get().Error("Failed to register main window class!");
        return false;
    }

    Logger::Get().Debug("Main window class registered.");

    if (m_Config.childWindowRendering) {
        if (!RegisterRenderWindowClass(hInstance)) {
            Logger::Get().Error("Failed to register render window class!");
            m_Config.childWindowRendering = false;
        } else {
            Logger::Get().Debug("Render window class registered.");
        }
    }

    DWORD style = (m_Config.fullscreen || m_Config.borderless) ? WS_POPUP : WS_OVERLAPPEDWINDOW;

    RECT rect = {0, 0, m_Config.width, m_Config.height};
    ::AdjustWindowRect(&rect, style, FALSE);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    m_MainWindow = ::CreateWindowEx(WS_EX_LEFT, TEXT("Player"), TEXT("Player"), style,
                                    CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hInstance, this);
    if (!m_MainWindow) {
        Logger::Get().Error("Failed to create main window!");
        UnregisterMainWindowClass(hInstance);
        if (m_Config.childWindowRendering)
            UnregisterRenderWindowClass(hInstance);
        return false;
    }

    Logger::Get().Debug("Main window created.");

    if (m_Config.childWindowRendering) {
        m_RenderWindow = ::CreateWindowEx(WS_EX_TOPMOST, TEXT("Player Render"), TEXT("Player Render"),
                                          WS_CHILD | WS_VISIBLE, 0, 0, m_Config.width, m_Config.height, m_MainWindow,
                                          nullptr, hInstance, this);
        if (!m_RenderWindow) {
            Logger::Get().Error("Failed to create render window!");
            UnregisterRenderWindowClass(hInstance);
            m_Config.childWindowRendering = false;
        } else {
            Logger::Get().Debug("Render window created.");
        }
    }

    return true;
}

void Player::ShutdownWindow() {
    if (m_Config.childWindowRendering)
        ::DestroyWindow(m_RenderWindow);
    ::DestroyWindow(m_MainWindow);

    if (m_hInstance) {
        if (m_Config.childWindowRendering)
            UnregisterRenderWindowClass(m_hInstance);
        UnregisterMainWindowClass(m_hInstance);
    }
}

bool Player::InitEngine(HWND mainWindow) {
    if (CKStartUp() != CK_OK) {
        Logger::Get().Error("CK Engine can not start up!");
        return false;
    }

    Logger::Get().Debug("CK Engine starts up successfully.");

    CKPluginManager *pluginManager = CKGetPluginManager();
    if (!InitPlugins(pluginManager)) {
        Logger::Get().Error("Failed to initialize plugins.");
        CKShutdown();
        return false;
    }

    int renderEngine = FindRenderEngine(pluginManager);
    if (renderEngine == -1) {
        Logger::Get().Error("Failed to initialize render engine.");
        CKShutdown();
        return false;
    }

    Logger::Get().Debug("Render Engine initialized.");
#if CKVERSION == 0x13022002
    CKERROR res = CKCreateContext(&m_CKContext, mainWindow, renderEngine, 0);
#else
    CKERROR res = CKCreateContext(&m_CKContext, mainWindow, 0);
#endif
    if (res != CK_OK) {
        Logger::Get().Error("Failed to initialize CK Engine.");
        ShutdownEngine();
        return false;
    }

    Logger::Get().Debug("CK Engine initialized.");

    m_CKContext->SetVirtoolsVersion(CK_VIRTOOLS_DEV, 0x2000043);
    m_CKContext->SetInterfaceMode(FALSE, LogRedirect, nullptr);

    if (!SetupManagers()) {
        Logger::Get().Error("Failed to setup managers.");
        ShutdownEngine();
        return false;
    }

    if (!SetupPaths()) {
        Logger::Get().Error("Failed to setup paths.");
        ShutdownEngine();
        return false;
    }

    return true;
}

void Player::ShutdownEngine() {
    if (m_CKContext) {
        m_CKContext->Reset();
        m_CKContext->ClearAll();

        if (m_RenderManager && m_RenderContext) {
            m_RenderManager->DestroyRenderContext(m_RenderContext);
            m_RenderContext = nullptr;
        }

        CKCloseContext(m_CKContext);
        m_CKContext = nullptr;

        m_RenderManager = nullptr;
        m_MessageManager = nullptr;
        m_TimeManager = nullptr;
        m_AttributeManager = nullptr;
        m_InputManager = nullptr;

        CKShutdown();
    }
}

bool Player::InitDriver() {
    int driverCount = m_RenderManager->GetRenderDriverCount();
    if (driverCount == 0) {
        Logger::Get().Error("No render driver found.");
        return false;
    }

    if (driverCount == 1) {
        Logger::Get().Debug("Found a render driver");
    } else {
        Logger::Get().Debug("Found %d render drivers", driverCount);
    }

    VxDriverDesc *drDesc = m_RenderManager->GetRenderDriverDescription(m_Config.driver);
    if (!drDesc) {
        Logger::Get().Error("Unable to find driver %d", m_Config.driver);
        m_Config.driver = 0;
        Logger::Get().Error("Use default driver %d", m_Config.driver);
        SetDefaultValuesForDriver();
    }

    drDesc = m_RenderManager->GetRenderDriverDescription(m_Config.driver);
    if (!drDesc) {
        Logger::Get().Error("Unable to find driver %d", m_Config.driver);
        return false;
    }

    Logger::Get().Debug("Render Driver ID: %d", m_Config.driver);
    Logger::Get().Debug("Render Driver Name: %s", drDesc->DriverName);
    Logger::Get().Debug("Render Driver Desc: %s", drDesc->DriverDesc);

    m_Config.screenMode = FindScreenMode(m_Config.width, m_Config.height, m_Config.bpp, m_Config.driver);
    if (m_Config.screenMode == -1) {
        Logger::Get().Error("Unable to find screen mode: %d x %d x %d", m_Config.width, m_Config.height, m_Config.bpp);
        return false;
    }

    Logger::Get().Debug("Screen Mode: %d x %d x %d", m_Config.width, m_Config.height, m_Config.bpp);

    return true;
}

bool Player::FinishLoad(const char *filename) {
    if (!filename)
        return false;

    // Retrieve the level
    CKLevel *level = m_CKContext->GetCurrentLevel();
    if (!level) {
        Logger::Get().Error("Failed to retrieve the level!");
        return false;
    }

    // Add the render context to the level
    level->AddRenderContext(m_RenderContext, TRUE);

    // Take the first camera we found and attach the viewpoint to it.
    // (in case it is not set by the composition later)
    const XObjectPointerArray cams = m_CKContext->GetObjectListByType(CKCID_CAMERA, TRUE);
    if (cams.Size() != 0)
        m_RenderContext->AttachViewpointToCamera((CKCamera *) cams[0]);

    // Hide curves ?
    int curveCount = m_CKContext->GetObjectsCountByClassID(CKCID_CURVE);
    CK_ID *curve_ids = m_CKContext->GetObjectsListByClassID(CKCID_CURVE);
    for (int i = 0; i < curveCount; ++i) {
        CKMesh *mesh = ((CKCurve *) m_CKContext->GetObject(curve_ids[i]))->GetCurrentMesh();
        if (mesh)
            mesh->Show(CKHIDE);
    }

    // Launch the default scene
    level->LaunchScene(nullptr);

    // ReRegister OnClick Message in case it changed
    m_MsgClick = m_MessageManager->AddMessageType((CKSTRING) "OnClick");
    m_MsgDoubleClick = m_MessageManager->AddMessageType((CKSTRING) "OnDblClick");

    // Render the first frame
    m_RenderContext->Render();

    return true;
}

void Player::ReportMissingGuids(CKFile *file, const char *resolvedFile) {
    // Retrieve the list of missing plugins/guids
    const XClassArray<CKFilePluginDependencies> *p = file->GetMissingPlugins();
    for (CKFilePluginDependencies *it = p->Begin(); it != p->End(); it++) {
        const int count = it->m_Guids.Size();
        for (int i = 0; i < count; i++) {
            if (!it->ValidGuids[i]) {
                if (resolvedFile)
                    Logger::Get().Error("File Name : %s\nMissing GUIDS:\n", resolvedFile);
                Logger::Get().Error("%x,%x\n", it->m_Guids[i].d1, it->m_Guids[i].d2);
            }
        }
    }
}

bool Player::InitPlugins(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return false;

    if (!LoadRenderEngines(pluginManager)) {
        Logger::Get().Error("Failed to load render engine!");
        return false;
    }

    if (!LoadManagers(pluginManager)) {
        Logger::Get().Error("Failed to load managers!");
        return false;
    }

    if (!LoadBuildingBlocks(pluginManager)) {
        Logger::Get().Error("Failed to load building blocks!");
        return false;
    }

    if (!LoadPlugins(pluginManager)) {
        Logger::Get().Error("Failed to load plugins!");
        return false;
    }

    return true;
}

bool Player::LoadRenderEngines(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return false;

    const char *path = m_Config.GetPath(eRenderEnginePath);
    if (!DirectoryExists(path) || pluginManager->ParsePlugins((CKSTRING) (path)) == 0) {
        Logger::Get().Error("Render engine parse error.");
        return false;
    }

    Logger::Get().Debug("Render engine loaded.");

    return true;
}

bool Player::LoadManagers(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return false;

    const char *path = m_Config.GetPath(eManagerPath);
    if (!DirectoryExists(path)) {
        Logger::Get().Error("Managers directory does not exist!");
        return false;
    }

    Logger::Get().Debug("Loading managers from %s", path);

    if (pluginManager->ParsePlugins((CKSTRING) path) == 0) {
        Logger::Get().Error("Managers parse error.");
        return false;
    }

    Logger::Get().Debug("Managers loaded.");

    return true;
}

bool Player::LoadBuildingBlocks(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return false;

    const char *path = m_Config.GetPath(eBuildingBlockPath);
    if (!DirectoryExists(path)) {
        Logger::Get().Error("BuildingBlocks directory does not exist!");
        return false;
    }

    Logger::Get().Debug("Loading building blocks from %s", path);

    if (pluginManager->ParsePlugins((CKSTRING) path) == 0) {
        Logger::Get().Error("Behaviors parse error.");
        return false;
    }

    Logger::Get().Debug("Building blocks loaded.");

    return true;
}

bool Player::LoadPlugins(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return false;

    const char *path = m_Config.GetPath(ePluginPath);
    if (!DirectoryExists(path)) {
        Logger::Get().Error("Plugins directory does not exist!");
        return false;
    }

    Logger::Get().Debug("Loading plugins from %s", path);

    if (pluginManager->ParsePlugins((CKSTRING) path) == 0) {
        Logger::Get().Error("Plugins parse error.");
        return false;
    }

    Logger::Get().Debug("Plugins loaded.");

    return true;
}

bool Player::UnloadPlugins(CKPluginManager *pluginManager, CK_PLUGIN_TYPE type, CKGUID guid) {
    if (!pluginManager)
        return false;

    const int count = pluginManager->GetPluginCount(type);
    for (int i = 0; i < count; ++i) {
        CKPluginEntry *entry = pluginManager->GetPluginInfo(type, i);
        CKPluginInfo &info = entry->m_PluginInfo;
        if (info.m_GUID == guid) {
            if (pluginManager->UnLoadPluginDll(entry->m_PluginDllIndex) == CK_OK)
                return true;
            break;
        }
    }

    return false;
}

int Player::FindRenderEngine(CKPluginManager *pluginManager) {
    if (!pluginManager)
        return -1;

    int count = pluginManager->GetPluginCount(CKPLUGIN_RENDERENGINE_DLL);
    for (int i = 0; i < count; ++i) {
        CKPluginEntry *entry = pluginManager->GetPluginInfo(CKPLUGIN_RENDERENGINE_DLL, i);
        if (!entry)
            break;

        CKPluginDll *dll = pluginManager->GetPluginDllInfo(entry->m_PluginDllIndex);
        if (!dll)
            break;

        char filename[MAX_PATH];
        _splitpath(dll->m_DllFileName.Str(), nullptr, nullptr, filename, nullptr);
        if (!_strnicmp("CK2_3D", filename, strlen(filename)))
            return i;
    }

    return -1;
}

bool Player::SetupManagers() {
    m_RenderManager = m_CKContext->GetRenderManager();
    if (!m_RenderManager) {
        Logger::Get().Error("Unable to get Render Manager.");
        return false;
    }

    m_MessageManager = m_CKContext->GetMessageManager();
    if (!m_MessageManager) {
        Logger::Get().Error("Unable to get Message Manager.");
        return false;
    }

    m_TimeManager = m_CKContext->GetTimeManager();
    if (!m_TimeManager) {
        Logger::Get().Error("Unable to get Time Manager.");
        return false;
    }

    m_AttributeManager = m_CKContext->GetAttributeManager();
    if (!m_AttributeManager) {
        Logger::Get().Error("Unable to get Attribute Manager.");
        return false;
    }

    m_InputManager = (CKInputManager *) m_CKContext->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (!m_InputManager) {
        Logger::Get().Error("Unable to get Input Manager.");
        return false;
    }

    return true;
}

bool Player::SetupPaths() {
    CKPathManager *pm = m_CKContext->GetPathManager();
    if (!pm) {
        Logger::Get().Error("Unable to get Path Manager.");
        return false;
    }

    char path[MAX_PATH];
    char dir[MAX_PATH];
    ::GetCurrentDirectoryA(MAX_PATH, dir);

    if (!DirectoryExists(m_Config.GetPath(eDataPath))) {
        Logger::Get().Error("Data path is not found.");
        return false;
    }
    _snprintf(path, MAX_PATH, "%s\\%s", dir, m_Config.GetPath(eDataPath));
    XString dataPath = path;
    pm->AddPath(DATA_PATH_IDX, dataPath);
    Logger::Get().Debug("Data path: %s", dataPath.CStr());

    if (!DirectoryExists(m_Config.GetPath(eSoundPath))) {
        Logger::Get().Error("Sounds path is not found.");
        return false;
    }
    _snprintf(path, MAX_PATH, "%s\\%s", dir, m_Config.GetPath(eSoundPath));
    XString soundPath = path;
    pm->AddPath(SOUND_PATH_IDX, soundPath);
    Logger::Get().Debug("Sounds path: %s", soundPath.CStr());

    if (!DirectoryExists(m_Config.GetPath(eBitmapPath))) {
        Logger::Get().Error("Bitmap path is not found.");
        return false;
    }
    _snprintf(path, MAX_PATH, "%s\\%s", dir, m_Config.GetPath(eBitmapPath));
    XString bitmapPath = path;
    pm->AddPath(BITMAP_PATH_IDX, bitmapPath);
    Logger::Get().Debug("Bitmap path: %s", bitmapPath.CStr());

    return true;
}

void Player::ResizeWindow() {
    RECT rc = {0, 0, m_Config.width, m_Config.height};
    DWORD style = ::GetWindowLong(m_MainWindow, GWL_STYLE);
    ::AdjustWindowRect(&rc, style, FALSE);
    ::SetWindowPos(m_MainWindow, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    if (m_Config.childWindowRendering)
        ::SetWindowPos(m_RenderWindow, nullptr, 0, 0, m_Config.width, m_Config.height, SWP_NOMOVE | SWP_NOZORDER);
}

int Player::FindScreenMode(int width, int height, int bpp, int driver) {
    if (!m_RenderManager) {
        Logger::Get().Error("RenderManager is not initialized");
        return -1;
    }

    VxDriverDesc *drDesc = m_RenderManager->GetRenderDriverDescription(driver);
    if (!drDesc) {
        Logger::Get().Error("Unable to find render driver %d.", driver);
        return false;
    }

#if CKVERSION == 0x13022002
    VxDisplayMode *dm = drDesc->DisplayModes;
    const int dmCount = drDesc->DisplayModeCount;
#else
    XArray<VxDisplayMode> &dm = drDesc->DisplayModes;
    const int dmCount = dm.Size();
#endif

    int refreshRate = 0;
    for (int i = 0; i < dmCount; ++i) {
        if (dm[i].Width == width &&
            dm[i].Height == height &&
            dm[i].Bpp == bpp) {
            if (dm[i].RefreshRate > refreshRate)
                refreshRate = dm[i].RefreshRate;
        }
    }

    if (refreshRate == 0) {
        Logger::Get().Error("No matching refresh rate found for %d x %d x %d", width, height, bpp);
        return -1;
    }

    int screenMode = -1;
    for (int j = 0; j < dmCount; ++j) {
        if (dm[j].Width == width &&
            dm[j].Height == height &&
            dm[j].Bpp == bpp &&
            dm[j].RefreshRate == refreshRate) {
            screenMode = j;
            break;
        }
    }

    return screenMode;
}

bool Player::GetDisplayMode(int &width, int &height, int &bpp, int driver, int screenMode) {
    VxDriverDesc *drDesc = m_RenderManager->GetRenderDriverDescription(driver);
    if (!drDesc)
        return false;

#if CKVERSION == 0x13022002
    if (screenMode < 0 || screenMode >= drDesc->DisplayModeCount)
        return false;
#else
    if (screenMode < 0 || screenMode >= drDesc->DisplayModes.Size())
        return false;
#endif

    const VxDisplayMode &dm = drDesc->DisplayModes[screenMode];
    width = dm.Width;
    height = dm.Height;
    bpp = dm.Bpp;

    return true;
}

void Player::SetDefaultValuesForDriver() {
    m_Config.driver = 0;
    m_Config.width = PLAYER_DEFAULT_WIDTH;
    m_Config.height = PLAYER_DEFAULT_HEIGHT;
    m_Config.bpp = PLAYER_DEFAULT_BPP;
}

bool Player::IsRenderFullscreen() const {
    if (!m_RenderContext)
        return false;
    return m_RenderContext->IsFullScreen() == TRUE;
}

bool Player::GoFullscreen() {
    if (!m_RenderContext)
        return false;

    if (IsRenderFullscreen())
        return true;

    m_Config.fullscreen = true;
    if (m_RenderContext->GoFullScreen(m_Config.width, m_Config.height, m_Config.bpp, m_Config.driver) != CK_OK) {
        Logger::Get().Debug("GoFullScreen Failed");
        m_Config.fullscreen = false;
        return false;
    }

    return true;
}

bool Player::StopFullscreen() {
    if (!m_RenderContext || !IsRenderFullscreen())
        return false;

    if (m_RenderContext->StopFullScreen() != CK_OK) {
        Logger::Get().Debug("StopFullscreen Failed");
        return false;
    }

    m_Config.fullscreen = false;
    return true;
}

void Player::OnDestroy() {
    ::PostQuitMessage(0);
}

void Player::OnMove() {
}

void Player::OnSize() {
}

void Player::OnPaint() {
    if (m_RenderContext && !m_Config.fullscreen)
        Render();
}

void Player::OnClose() {
    ::DestroyWindow(m_MainWindow);
}

void Player::OnActivateApp(bool active) {
    static bool wasFullscreen = false;
    static bool firstDeActivate = true;

    if (m_State == eInitial)
        return;

    if (!active) {
        if (m_CKContext) {
            if (!m_Config.alwaysHandleInput)
                m_InputManager->Pause(TRUE);

            if (IsRenderFullscreen()) {
                if (firstDeActivate)
                    wasFullscreen = true;
                OnStopFullscreen();
            } else if (firstDeActivate) {
                wasFullscreen = false;
            }
        }
        firstDeActivate = false;
        m_State = eFocusLost;
    } else {
        if (wasFullscreen && !firstDeActivate)
            OnGoFullscreen();

        if (!m_Config.alwaysHandleInput)
            m_InputManager->Pause(FALSE);

        firstDeActivate = true;
        m_State = ePlaying;
    }
}

void Player::OnSetCursor() {
    if (m_State == ePaused)
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
}

void Player::OnGetMinMaxInfo(LPMINMAXINFO lpmmi) {
    if (lpmmi) {
        lpmmi->ptMinTrackSize.x = PLAYER_DEFAULT_WIDTH;
        lpmmi->ptMinTrackSize.y = PLAYER_DEFAULT_HEIGHT;
    }
}

int Player::OnSysKeyDown(UINT uKey) {
    // Manage system key (ALT + KEY)
    switch (uKey) {
    case VK_RETURN:
        // ALT + ENTER -> SwitchFullscreen
        OnSwitchFullscreen();
        break;

    case VK_F4:
        // ALT + F4 -> Quit the application
        OnExitToSystem();
        return 1;

    default:
        break;
    }
    return 0;
}

void Player::OnClick(bool dblClk) {
    if (!m_RenderManager)
        return;

    POINT pt;
    ::GetCursorPos(&pt);
    if (m_Config.childWindowRendering)
        ::ScreenToClient(m_RenderWindow, &pt);
    else
        ::ScreenToClient(m_MainWindow, &pt);

    CKMessageType msgType = (!dblClk) ? m_MsgClick : m_MsgDoubleClick;

#if CKVERSION == 0x13022002
    CKPOINT ckpt = {(int) pt.x, (int) pt.y};
    CKPICKRESULT res;
    CKObject *obj = m_RenderContext->Pick(ckpt, &res, FALSE);
    if (obj && CKIsChildClassOf(obj, CKCID_BEOBJECT))
        m_MessageManager->SendMessageSingle(msgType, (CKBeObject *) obj, nullptr);
    if (res.Sprite) {
        CKObject *sprite = m_CKContext->GetObject(res.Sprite);
        if (sprite)
            m_MessageManager->SendMessageSingle(msgType, (CKBeObject *) sprite, nullptr);
    }
#else
    VxIntersectionDesc desc;
    CKObject *obj = m_RenderContext->Pick(pt.x, pt.y, &desc);
    if (obj && CKIsChildClassOf(obj, CKCID_BEOBJECT))
        m_MessageManager->SendMessageSingle(msgType, (CKBeObject *)obj);
    if (desc.Sprite)
        m_MessageManager->SendMessageSingle(msgType, (CKBeObject *)desc.Sprite);
#endif
}

void Player::OnExitToSystem() {
    bool fullscreen = m_Config.fullscreen;
    OnStopFullscreen();
    m_Config.fullscreen = fullscreen;

    ::PostMessage(m_MainWindow, WM_CLOSE, 0, 0);
}

int Player::OnChangeScreenMode(int driver, int screenMode) {
    int width, height, bpp;
    if (!GetDisplayMode(width, height, bpp, driver, screenMode)) {
        Logger::Get().Error("Failed to change screen mode.");
        return 0;
    }

    bool fullscreen = m_Config.fullscreen;
    if (fullscreen)
        StopFullscreen();

    m_Config.driver = driver;
    m_Config.screenMode = screenMode;
    m_Config.width = width;
    m_Config.height = height;
    m_Config.bpp = bpp;

    ResizeWindow();
    m_RenderContext->Resize();

    if (fullscreen)
        GoFullscreen();

    return 1;
}

void Player::OnGoFullscreen() {
    Pause();

    if (GoFullscreen()) {
        ::SetWindowLong(m_MainWindow, GWL_STYLE, WS_POPUP);
        ::SetWindowPos(m_MainWindow, nullptr, 0, 0, m_Config.width, m_Config.height,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

        ::ShowWindow(m_MainWindow, SW_SHOW);
        ::SetFocus(m_MainWindow);

        if (m_Config.childWindowRendering) {
            ::ShowWindow(m_RenderWindow, SW_SHOW);
            ::SetFocus(m_RenderWindow);
        }

        ::UpdateWindow(m_MainWindow);
        if (m_Config.childWindowRendering)
            ::UpdateWindow(m_RenderWindow);
    }

    Play();
}

void Player::OnStopFullscreen() {
    Pause();

    if (StopFullscreen()) {
        LONG style = (m_Config.borderless) ? WS_POPUP : WS_POPUP | WS_CAPTION;
        RECT rc = {0, 0, m_Config.width, m_Config.height};
        ::AdjustWindowRect(&rc, style, FALSE);

        ::SetWindowLong(m_MainWindow, GWL_STYLE, style);
        ::SetWindowPos(m_MainWindow, HWND_NOTOPMOST, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                       rc.bottom - rc.top, SWP_FRAMECHANGED);

        ::ShowWindow(m_MainWindow, SW_SHOW);
        ::SetFocus(m_MainWindow);

        if (m_Config.childWindowRendering) {
            ::SetWindowPos(m_RenderWindow, nullptr, 0, 0, m_Config.width, m_Config.height, SWP_NOMOVE | SWP_NOZORDER);
            ::SetFocus(m_RenderWindow);
        }

        ::UpdateWindow(m_MainWindow);
        if (m_Config.childWindowRendering)
            ::UpdateWindow(m_RenderWindow);
    }

    Play();
}

void Player::OnSwitchFullscreen() {
    if (m_State == eInitial)
        return;

    if (!IsRenderFullscreen())
        OnGoFullscreen();
    else
        OnStopFullscreen();
}

LRESULT Player::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        OnDestroy();
        return 0;

    case WM_MOVE:
        OnMove();
        break;

    case WM_SIZE:
        OnSize();
        break;

    case WM_PAINT:
        OnPaint();
        break;

    case WM_CLOSE:
        OnClose();
        return 0;

    case WM_ACTIVATEAPP:
        // WM_ACTIVATEAPP: wParam is a BOOL (non-zero means activated)
        OnActivateApp(wParam != FALSE);
        break;

    case WM_SETCURSOR:
        OnSetCursor();
        break;

    case WM_GETMINMAXINFO:
        OnGetMinMaxInfo((LPMINMAXINFO) lParam);
        break;

    case WM_SYSKEYDOWN:
        return OnSysKeyDown(wParam);

    case WM_LBUTTONDOWN:
        OnClick();
        break;

    case WM_LBUTTONDBLCLK:
        OnClick(true);
        break;

    default:
        break;
    }

    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool Player::RegisterMainWindowClass(HINSTANCE hInstance) {
    WNDCLASSEX mainWndClass;
    memset(&mainWndClass, 0, sizeof(WNDCLASSEX));

    mainWndClass.lpfnWndProc = Player::MainWndProc;
    mainWndClass.cbSize = sizeof(WNDCLASSEX);
    mainWndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    mainWndClass.cbClsExtra = 0;
    mainWndClass.cbWndExtra = 0;
    mainWndClass.hInstance = hInstance;
    // IDI_APPLICATION is already a resource identifier; don't wrap with MAKEINTRESOURCE again.
    mainWndClass.hIcon = ::LoadIcon(nullptr, IDI_APPLICATION);
    mainWndClass.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    mainWndClass.hbrBackground = (HBRUSH) ::GetStockObject(BLACK_BRUSH);
    mainWndClass.lpszMenuName = nullptr;
    mainWndClass.lpszClassName = TEXT("Player");
    mainWndClass.hIconSm = ::LoadIcon(nullptr, IDI_APPLICATION);

    return ::RegisterClassEx(&mainWndClass) != 0;
}

bool Player::RegisterRenderWindowClass(HINSTANCE hInstance) {
    WNDCLASS renderWndClass;
    memset(&renderWndClass, 0, sizeof(WNDCLASS));

    renderWndClass.lpfnWndProc = Player::MainWndProc;
    renderWndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    renderWndClass.hInstance = hInstance;
    renderWndClass.lpszClassName = TEXT("Player Render");

    return ::RegisterClass(&renderWndClass) != 0;
}

bool Player::UnregisterMainWindowClass(HINSTANCE hInstance) {
    if (!hInstance)
        return false;
    ::UnregisterClass(TEXT("Player"), hInstance);
    return true;
}

bool Player::UnregisterRenderWindowClass(HINSTANCE hInstance) {
    if (!hInstance)
        return false;
    ::UnregisterClass(TEXT("Player Render"), hInstance);
    return true;
}

LRESULT Player::MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Player *player = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
        player = static_cast<Player *>(pCreate->lpCreateParams);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(player));
    } else {
        player = reinterpret_cast<Player *>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (player) {
        return player->HandleMessage(hWnd, uMsg, wParam, lParam);
    }

    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#ifdef PLAYER_STANDALONE
#include <tchar.h>
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Player player;

    Logger::Get().Open("Player.log");
    Logger::Get().SetLevel(Logger::LEVEL_DEBUG);

    if (!player.Init(hInstance)) {
        Logger::Get().Error("Failed to initialize player!");
        ::MessageBox(nullptr, TEXT("Failed to initialize player!"), TEXT("Error"), MB_OK);
        return -1;
    }

    if (!player.Load("base.cmo")) {
        Logger::Get().Error("Failed to load game composition!");
        ::MessageBox(nullptr, TEXT("Failed to load game composition!"), TEXT("Error"), MB_OK);
        return -1;
    }

    player.Play();
    player.Run();
    player.Shutdown();

    return 0;
}
#endif // PLAYER_STANDALONE
