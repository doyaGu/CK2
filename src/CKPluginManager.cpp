#include "CKPluginManager.h"

#include "CKDirectoryParser.h"
#include "VxMath.h"

CKPluginEntry &CKPluginEntry::operator=(const CKPluginEntry &ent) {
    return *this;
}

CKPluginEntry::CKPluginEntry() {

}

CKPluginEntry::CKPluginEntry(const CKPluginEntry &ent) {

}

CKPluginEntry::~CKPluginEntry() {

}

CKPluginManager::CKPluginManager() {

}

CKPluginManager::~CKPluginManager() {

}

int CKPluginManager::ParsePlugins(CKSTRING Directory) {
    CKDirectoryParser parser(Directory, "*.dll", TRUE);
    VxAddLibrarySearchPath(Directory);
    char* plugin = nullptr;
    int count = 0;
    while (plugin = parser.GetNextFile()) {
        if (plugin == nullptr) break;
        if (RegisterPlugin(plugin) == CK_OK)
            ++count;
    }
    return count;
}

typedef int (*pCountFunc) (void);
typedef CKPluginInfo* (*pPluginInfoFunc) (int);
CKERROR CKPluginManager::RegisterPlugin(CKSTRING path
) {
    if (!path)
        return CKERR_INVALIDPARAMETER;

    int idx = -1;
    CKPluginDll* plugin = GetPluginDllInfo(path, &idx);
    if (plugin) {
        if (!plugin->m_DllInstance)
            return ReLoadPluginDll(idx);
        return CKERR_ALREADYPRESENT;
    }

    VxSharedLibrary vxLibrary;
    auto* handle = vxLibrary.Load(path);
    if (handle)
        return CKERR_INVALIDPLUGIN;
    pCountFunc pFuncCKGetPluginInfoCount = (pCountFunc)vxLibrary.GetFunctionPtr("CKGetPluginInfoCount");
    pPluginInfoFunc pFuncCKGetPluginInfo = (pPluginInfoFunc)vxLibrary.GetFunctionPtr("CKGetPluginInfo");

    if (!pFuncCKGetPluginInfo) {
        vxLibrary.ReleaseLibrary();
        return CKERR_INVALIDPLUGIN;
    }

    CKPluginDll pluginDll;
    pluginDll.m_DllFileName = path;
    pluginDll.m_PluginInfoCount =
        pFuncCKGetPluginInfoCount ? pFuncCKGetPluginInfoCount() : 1;
    pluginDll.m_DllInstance = handle;

    for (int i = 0; i < pluginDll.m_PluginInfoCount; ++i) {
        CKPluginInfo* info = pFuncCKGetPluginInfo(i);
        if (!info)
            continue;

        CKPluginEntry entry;
        entry.m_PluginDllIndex = m_PluginDlls.Size();
        entry.m_PositionInDll = i;
        entry.m_PluginInfo = *info;

        CK_PLUGIN_TYPE type = info->m_Type;
        switch (type) {
        case CKPLUGIN_BITMAP_READER: {
            entry.m_ReadersInfo = new CKPluginEntryReadersData;
            VxSharedLibrary library;
            library.Attach(pluginDll.m_DllInstance);
            entry.m_ReadersInfo->m_GetReaderFct = (CKReaderGetReaderFunction)library.GetFunctionPtr("CKGetReader");
            break;
        }
        case CKPLUGIN_MODEL_READER:
        case CKPLUGIN_BEHAVIOR_DLL:
        case CKPLUGIN_MOVIE_READER:
        default:
            break;
        }
    }

    return CK_OK;
}

CKPluginEntry *CKPluginManager::FindComponent(CKGUID Component, int catIdx) {
    return nullptr;
}

int CKPluginManager::AddCategory(CKSTRING cat) {
    return 0;
}

CKERROR CKPluginManager::RemoveCategory(int catIdx) {
    return 0;
}

int CKPluginManager::GetCategoryCount() {
    return 0;
}

CKSTRING CKPluginManager::GetCategoryName(int catIdx) {
    return nullptr;
}

int CKPluginManager::GetCategoryIndex(CKSTRING cat) {
    return 0;
}

CKERROR CKPluginManager::RenameCategory(int catIdx, CKSTRING newName) {
    return 0;
}

int CKPluginManager::GetPluginDllCount() {
    return 0;
}

CKPluginDll *CKPluginManager::GetPluginDllInfo(int PluginDllIdx) {
    return nullptr;
}

CKPluginDll *CKPluginManager::GetPluginDllInfo(CKSTRING PluginName, int *idx) {
    return nullptr;
}

CKERROR CKPluginManager::UnLoadPluginDll(int PluginDllIdx) {
    return 0;
}

CKERROR CKPluginManager::ReLoadPluginDll(int PluginDllIdx) {
    return 0;
}

int CKPluginManager::GetPluginCount(int catIdx) {
    return m_PluginCategories[catIdx].m_Entries.Size();
}

CKPluginEntry *CKPluginManager::GetPluginInfo(int catIdx, int PluginIdx) {
    return nullptr;
}

CKBOOL CKPluginManager::SetReaderOptionData(CKContext *context, void *memdata, CKParameterOut *Param, CKFileExtension ext, CKGUID *guid) {
    return 0;
}

CKParameterOut *CKPluginManager::GetReaderOptionData(CKContext *context, void *memdata, CKFileExtension ext, CKGUID *guid) {
    return nullptr;
}

CKBitmapReader *CKPluginManager::GetBitmapReader(CKFileExtension &ext, CKGUID *preferedGUID) {
    return nullptr;
}

CKSoundReader *CKPluginManager::GetSoundReader(CKFileExtension &ext, CKGUID *preferedGUID) {
    return nullptr;
}

CKModelReader *CKPluginManager::GetModelReader(CKFileExtension &ext, CKGUID *preferedGUID) {
    return nullptr;
}

CKMovieReader *CKPluginManager::GetMovieReader(CKFileExtension &ext, CKGUID *preferedGUID) {
    return nullptr;
}

CKERROR CKPluginManager::Load(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CK_LOAD_FLAGS LoadFlags, CKCharacter *carac, CKGUID *Readerguid) {
    return 0;
}

CKERROR CKPluginManager::Save(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD SaveFlags, CKGUID *Readerguid) {
    return 0;
}

void CKPluginManager::ReleaseAllPlugins() {

}

void CKPluginManager::InitializePlugins(CKContext *context) {

}

void CKPluginManager::ComputeDependenciesList(CKFile *file) {

}

void CKPluginManager::MarkComponentAsNeeded(CKGUID Component, int catIdx) {

}

void CKPluginManager::InitInstancePluginEntry(CKPluginEntry *entry, CKContext *context) {

}

void CKPluginManager::ExitInstancePluginEntry(CKPluginEntry *entry, CKContext *context) {

}

void CKPluginManager::InitializeBehaviors(VxSharedLibrary &lib, CKPluginEntry &entry) {

}

void CKPluginManager::InitializeBehaviors(CKDLL_OBJECTDECLARATIONFUNCTION Fct, CKPluginEntry &entry) {

}

void CKPluginManager::RemoveBehaviors(int PluginIndex) {

}

int CKPluginManager::AddPlugin(int catIdx, CKPluginEntry &Plugin) {
    return 0;
}

CKERROR CKPluginManager::RemovePlugin(int catIdx, int PluginIdx) {
    return 0;
}

CKPluginEntry *CKPluginManager::EXTFindEntry(CKFileExtension &ext, int Category) {
    return nullptr;
}

CKDataReader *CKPluginManager::EXTFindReader(CKFileExtension &ext, int Category) {
    return nullptr;
}

CKDataReader *CKPluginManager::GUIDFindReader(CKGUID &guid, int Category) {
    return nullptr;
}

void CKPluginManager::Clean() {

}

void CKPluginManager::Init() {

}
