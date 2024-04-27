#include "CKPluginManager.h"

#include "VxMath.h"
#include "CKGlobals.h"
#include "CKDirectoryParser.h"
#include "CKObjectDeclaration.h"
#include "CKParameterManager.h"

extern XArray<CKContext *> g_Contextes;

CKPluginEntry *g_TheCurrentPluginEntry;

CKPluginEntry &CKPluginEntry::operator=(const CKPluginEntry &ent) {
    if (this == &ent) {
        return *this;
    }

    if (ent.m_ReadersInfo) {
        if (!m_ReadersInfo) {
            m_ReadersInfo = new CKPluginEntryReadersData;
        }
        memcpy(m_ReadersInfo, ent.m_ReadersInfo, sizeof(CKPluginEntryReadersData));
    } else {
        delete m_ReadersInfo;
        m_ReadersInfo = nullptr;
    }

    if (ent.m_BehaviorsInfo) {
        if (!m_BehaviorsInfo) {
            m_BehaviorsInfo = new CKPluginEntryBehaviorsData;
        }
        m_BehaviorsInfo->m_BehaviorsGUID = ent.m_BehaviorsInfo->m_BehaviorsGUID;
    } else {
        delete m_BehaviorsInfo;
        m_BehaviorsInfo = nullptr;
    }

    m_PluginDllIndex = ent.m_PluginDllIndex;
    m_PositionInDll = ent.m_PositionInDll;
    m_PluginInfo = ent.m_PluginInfo;
    m_Active = ent.m_Active;
    m_NeededByFile = ent.m_NeededByFile;
    m_IndexInCategory = ent.m_IndexInCategory;
    m_PluginDllIndex = ent.m_PluginDllIndex;

    return *this;
}

CKPluginEntry::CKPluginEntry()
        : m_PluginInfo(),
          m_PluginDllIndex(0),
          m_PositionInDll(0),
          m_ReadersInfo(nullptr),
          m_BehaviorsInfo(nullptr),
          m_Active(FALSE),
          m_IndexInCategory(0),
          m_NeededByFile(FALSE) {}

CKPluginEntry::CKPluginEntry(const CKPluginEntry &ent) : m_PluginInfo() {
    m_PluginDllIndex = ent.m_PluginDllIndex;
    m_PositionInDll = ent.m_PositionInDll;
    m_PluginInfo = ent.m_PluginInfo;
    m_Active = ent.m_Active;
    m_NeededByFile = ent.m_NeededByFile;
    m_IndexInCategory = ent.m_IndexInCategory;
    if (ent.m_ReadersInfo) {
        m_ReadersInfo = new CKPluginEntryReadersData;
        memcpy(m_ReadersInfo, ent.m_ReadersInfo, sizeof(CKPluginEntryReadersData));
    } else {
        m_ReadersInfo = nullptr;
    }
    if (ent.m_BehaviorsInfo) {
        m_BehaviorsInfo = new CKPluginEntryBehaviorsData;
        m_BehaviorsInfo->m_BehaviorsGUID = ent.m_BehaviorsInfo->m_BehaviorsGUID;
    } else {
        m_BehaviorsInfo = nullptr;
    }
}

CKPluginEntry::~CKPluginEntry() {
    delete m_ReadersInfo;
    delete m_BehaviorsInfo;
}

CKPluginManager::CKPluginManager() {}

CKPluginManager::~CKPluginManager() {
    Clean();
    m_RunTimeDlls.Clear();
    m_PluginDlls.Clear();
    m_PluginCategories.Clear();
}

int CKPluginManager::ParsePlugins(CKSTRING Directory) {
    CKDirectoryParser parser(Directory, (CKSTRING) "*.dll", TRUE);
    VxAddLibrarySearchPath(Directory);
    int count = 0;
    for (char *plugin = parser.GetNextFile(); plugin != nullptr; plugin = parser.GetNextFile()) {
        if (RegisterPlugin(plugin) == CK_OK)
            ++count;
    }
    return count;
}

CKERROR CKPluginManager::RegisterPlugin(CKSTRING path) {
    if (!path)
        return CKERR_INVALIDPARAMETER;

    int idx = -1;
    CKPluginDll *plugin = GetPluginDllInfo(path, &idx);
    if (plugin) {
        if (!plugin->m_DllInstance)
            return ReLoadPluginDll(idx);
        return CKERR_ALREADYPRESENT;
    }

    VxSharedLibrary shl;
    auto *handle = shl.Load(path);

    CKPathSplitter ps(path);

    if (!handle)
        return CKERR_INVALIDPLUGIN;

    auto getPluginInfoCountFunc = (CKPluginGetInfoCountFunction) shl.GetFunctionPtr("CKGetPluginInfoCount");
    auto getPluginInfoFunc = (CKPluginGetInfoFunction) shl.GetFunctionPtr("CKGetPluginInfo");
    if (!getPluginInfoFunc) {
        shl.ReleaseLibrary();
        return CKERR_INVALIDPLUGIN;
    }

    XClassArray<CKPluginEntry> pluginEntries;

    CKPluginDll pluginDll;
    pluginDll.m_DllFileName = path;
    pluginDll.m_PluginInfoCount = getPluginInfoCountFunc ? getPluginInfoCountFunc() : 1;
    pluginDll.m_DllInstance = handle;

    CKERROR err = CKERR_INVALIDPLUGIN;
    for (int i = 0; i < pluginDll.m_PluginInfoCount; ++i) {
        CKPluginInfo *info = getPluginInfoFunc(i);
        if (!info)
            continue;

        CKPluginEntry *pEntry = new CKPluginEntry;
        CKPluginEntry &entry = *pEntry;
        entry.m_PluginDllIndex = m_PluginDlls.Size();
        entry.m_PositionInDll = i;
        entry.m_PluginInfo = *info;

        CK_PLUGIN_TYPE type = info->m_Type;
        if (type > m_PluginCategories.Size() - 1)
            m_PluginCategories.Resize(type + 1);
        entry.m_IndexInCategory = m_PluginCategories[type].m_Entries.Size();
        switch (type) {
            case CKPLUGIN_BITMAP_READER:
            case CKPLUGIN_SOUND_READER:
            case CKPLUGIN_MODEL_READER:
            case CKPLUGIN_MOVIE_READER: {
                entry.m_ReadersInfo = new CKPluginEntryReadersData;
                entry.m_ReadersInfo->m_GetReaderFct = (CKReaderGetReaderFunction) shl.GetFunctionPtr("CKGetReader");
                break;
            }
            case CKPLUGIN_BEHAVIOR_DLL: {
                entry.m_BehaviorsInfo = new CKPluginEntryBehaviorsData;
                auto registerBehaviorDeclarationsFunc = (CKDLL_OBJECTDECLARATIONFUNCTION) shl.GetFunctionPtr("RegisterBehaviorDeclarations");
                if (!registerBehaviorDeclarationsFunc)
                    registerBehaviorDeclarationsFunc = (CKDLL_OBJECTDECLARATIONFUNCTION) shl.GetFunctionPtr("RegisterNEMOExtensions");
                InitializeBehaviors(registerBehaviorDeclarationsFunc, entry);
                break;
            }
            default:
                break;
        }

        m_PluginCategories[type].m_Entries.PushBack(pEntry);
        if (!g_Contextes.IsEmpty()) {
            pluginEntries.PushBack(entry);
        }

        err = CK_OK;
    }

    if (err != CK_OK) {
        shl.ReleaseLibrary();
        return err;
    }

    m_PluginDlls.PushBack(pluginDll);
    if (!g_Contextes.IsEmpty()) {
        for (XArray<CKContext *>::Iterator cit = g_Contextes.Begin(); cit != g_Contextes.End(); ++cit) {
            for (XClassArray<CKPluginEntry>::Iterator eit = pluginEntries.Begin(); eit != pluginEntries.End(); ++eit) {
                InitInstancePluginEntry(&*eit, *cit);
            }
        }
    }

    return err;
}

CKPluginEntry *CKPluginManager::FindComponent(CKGUID Component, int catIdx) {
    if (catIdx < 0) {
        for (XClassArray<CKPluginCategory>::Iterator cit = m_PluginCategories.Begin(); cit != m_PluginCategories.End(); ++cit) {
            for (XArray<CKPluginEntry *>::Iterator eit = cit->m_Entries.Begin(); eit != cit->m_Entries.End(); ++eit) {
                if ((*eit)->m_PluginInfo.m_GUID == Component) {
                    return *eit;
                }
            }
        }

        return nullptr;
    }

    CKPluginCategory &cat = m_PluginCategories[catIdx];
    for (XArray<CKPluginEntry *>::Iterator eit = cat.m_Entries.Begin(); eit != cat.m_Entries.End(); ++eit) {
        if ((*eit)->m_PluginInfo.m_GUID == Component) {
            return *eit;
        }
    }

    if (catIdx != CKPLUGIN_BEHAVIOR_DLL)
        return nullptr;

    CKObjectDeclaration *decl = CKGetObjectDeclarationFromGuid(Component);
    if (!decl)
        return nullptr;

    return GetPluginInfo(CKPLUGIN_BEHAVIOR_DLL, decl->m_PluginIndex);
}

int CKPluginManager::AddCategory(CKSTRING cat) {
    if (!cat || strlen(cat) == 0)
        return -1;

    int idx = GetCategoryIndex(cat);
    if (idx == -1) {
        CKPluginCategory category;
        category.m_Name = cat;
        m_PluginCategories.PushBack(category);
    }

    return idx;
}

CKERROR CKPluginManager::RemoveCategory(int catIdx) {
    if (catIdx < 0 || catIdx >= m_PluginCategories.Size())
        return CKERR_INVALIDPARAMETER;

    CKPluginCategory &cat = m_PluginCategories[catIdx];
    if (!cat.m_Entries.IsEmpty()) {
        for (XArray<CKPluginEntry *>::Iterator eit = cat.m_Entries.Begin(); eit != cat.m_Entries.End(); ++eit) {
            if (*eit) {
                delete (*eit);
            }
        }
    }

    m_PluginCategories.RemoveAt(catIdx);
    return CK_OK;
}

int CKPluginManager::GetCategoryCount() {
    return m_PluginCategories.Size();
}

CKSTRING CKPluginManager::GetCategoryName(int catIdx) {
    return m_PluginCategories[catIdx].m_Name.Str();
}

int CKPluginManager::GetCategoryIndex(CKSTRING cat) {
    if (m_PluginCategories.Size() == 0)
        return -1;

    for (int i = 0; i < m_PluginCategories.Size(); ++i) {
        if (m_PluginCategories[i].m_Name.Compare(cat) == 0)
            return i;
    }

    return -1;
}

CKERROR CKPluginManager::RenameCategory(int catIdx, CKSTRING newName) {
    if (catIdx < 0 || catIdx >= m_PluginCategories.Size())
        return CKERR_INVALIDPARAMETER;

    m_PluginCategories[catIdx].m_Name = newName;
    return CK_OK;
}

int CKPluginManager::GetPluginDllCount() {
    return m_PluginDlls.Size();
}

CKPluginDll *CKPluginManager::GetPluginDllInfo(int PluginDllIdx) {
    return &m_PluginDlls[PluginDllIdx];
}

CKPluginDll *CKPluginManager::GetPluginDllInfo(CKSTRING PluginName, int *idx) {
    XString path(PluginName);
    auto i = m_PluginDlls.Begin();
    for (; i != m_PluginDlls.End(); ++i) {
        if (path.Compare(i->m_DllFileName) == 0)
            break;
    }

    if (i == m_PluginDlls.End()) {
        return nullptr;
    }

    if (idx) {
        *idx = i - m_PluginDlls.Begin();
    }
    return i;
}

CKERROR CKPluginManager::UnLoadPluginDll(int PluginDllIdx) {
    CKPluginDll *pluginDll = GetPluginDllInfo(PluginDllIdx);
    if (!pluginDll || !pluginDll->m_DllInstance)
        return CKERR_INVALIDPARAMETER;

    for (XClassArray<CKPluginCategory>::Iterator cit = m_PluginCategories.Begin(); cit != m_PluginCategories.End(); ++cit) {
        int index = 0;
        for (XArray<CKPluginEntry *>::Iterator eit = cit->m_Entries.Begin(); eit != cit->m_Entries.End(); ++eit) {
            CKPluginEntry *entry = *eit;
            if (entry->m_PluginDllIndex == PluginDllIdx) {
                auto &pluginInfo = entry->m_PluginInfo;
                if (pluginInfo.m_Type == CKPLUGIN_BEHAVIOR_DLL) {
                    RemoveBehaviors(index);
                }

                for (XArray<CKContext *>::Iterator tit = g_Contextes.Begin(); tit != g_Contextes.End(); ++tit) {
                    ExitInstancePluginEntry(entry, *tit);
                }

                pluginInfo.m_InitInstanceFct = nullptr;
                pluginInfo.m_ExitInstanceFct = nullptr;
            }

            ++index;
        }
    }

    VxSharedLibrary shl;
    shl.Attach(pluginDll->m_DllInstance);
    shl.ReleaseLibrary();
    pluginDll->m_DllInstance = nullptr;

    return CK_OK;
}

CKERROR CKPluginManager::ReLoadPluginDll(int PluginDllIdx) {
    CKPluginDll *pluginDll = GetPluginDllInfo(PluginDllIdx);
    if (!pluginDll)
        return CKERR_INVALIDPARAMETER;

    VxSharedLibrary shl;
    if (!pluginDll->m_DllInstance) {
        pluginDll->m_DllInstance = shl.Load(pluginDll->m_DllFileName.Str());
        if (!pluginDll->m_DllInstance)
            return CKERR_INVALIDPARAMETER;
    }

    auto *getPluginInfoCountFunc = (CKPluginGetInfoCountFunction) shl.GetFunctionPtr("CKGetPluginInfoCount");
    auto *getPluginInfoFunc = (CKPluginGetInfoFunction) shl.GetFunctionPtr("CKGetPluginInfo");
    if (!getPluginInfoFunc)
        return CKERR_INVALIDPARAMETER;

    int pluginInfoCount = (getPluginInfoCountFunc) ? getPluginInfoCountFunc() : 1;

    for (XClassArray<CKPluginCategory>::Iterator cit = m_PluginCategories.Begin(); cit != m_PluginCategories.End(); ++cit) {
        for (XArray<CKPluginEntry *>::Iterator eit = cit->m_Entries.Begin(); eit != cit->m_Entries.End(); ++eit) {
            CKPluginEntry *entry = *eit;
            if (entry->m_PluginDllIndex != PluginDllIdx)
                continue;

            if (entry->m_PositionInDll >= pluginInfoCount)
                continue;

            CKPluginInfo *pluginInfo = getPluginInfoFunc(entry->m_PositionInDll);
            if (!pluginInfo)
                continue;

            entry->m_PluginInfo = *pluginInfo;

            switch (pluginInfo->m_Type) {
                case CKPLUGIN_BITMAP_READER:
                case CKPLUGIN_SOUND_READER:
                case CKPLUGIN_MODEL_READER:
                case CKPLUGIN_MOVIE_READER: {
                    entry->m_ReadersInfo->m_GetReaderFct = (CKReaderGetReaderFunction) shl.GetFunctionPtr("CKGetReader");
                    break;
                }
                case CKPLUGIN_BEHAVIOR_DLL: {
                    auto registerBehaviorDeclarationsFunc = (CKDLL_OBJECTDECLARATIONFUNCTION) shl.GetFunctionPtr("RegisterBehaviorDeclarations");
                    if (!registerBehaviorDeclarationsFunc)
                        registerBehaviorDeclarationsFunc = (CKDLL_OBJECTDECLARATIONFUNCTION) shl.GetFunctionPtr("RegisterNEMOExtensions");
                    InitializeBehaviors(registerBehaviorDeclarationsFunc, *entry);
                    break;
                }
                default:
                    break;
            }
        }
    }

    return CK_OK;
}

int CKPluginManager::GetPluginCount(int catIdx) {
    return m_PluginCategories[catIdx].m_Entries.Size();
}

CKPluginEntry *CKPluginManager::GetPluginInfo(int catIdx, int PluginIdx) {
    return m_PluginCategories[catIdx].m_Entries[PluginIdx];
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
    for (XClassArray<CKPluginDll>::Iterator it = m_PluginDlls.Begin(); it != m_PluginDlls.End(); ++it) {
        if (it->m_DllInstance) {
            VxSharedLibrary sl;
            sl.Attach(it->m_DllInstance);
            sl.ReleaseLibrary();
            it->m_DllInstance = nullptr;
        }
    }

    for (XClassArray<VxSharedLibrary *>::Iterator it = m_RunTimeDlls.Begin(); it != m_RunTimeDlls.End(); ++it) {
        (*it)->ReleaseLibrary();
        delete *it;
    }

    Clean();
}

void CKPluginManager::InitializePlugins(CKContext *context) {
    for (XClassArray<CKPluginCategory>::Iterator cit = m_PluginCategories.Begin(); cit != m_PluginCategories.End(); ++cit) {
        for (XArray<CKPluginEntry *>::Iterator eit = cit->m_Entries.Begin(); eit != cit->m_Entries.End(); ++eit) {
            InitInstancePluginEntry(*eit, context);
        }
    }
}

void CKPluginManager::ComputeDependenciesList(CKFile *file) {

}

void CKPluginManager::MarkComponentAsNeeded(CKGUID Component, int catIdx) {

}

void CKPluginManager::InitInstancePluginEntry(CKPluginEntry *entry, CKContext *context) {
    auto &pluginInfo = entry->m_PluginInfo;

    if ((context->m_StartOptions & CK_CONFIG_DISABLEDINPUT) != 0 && pluginInfo.m_GUID == INPUT_MANAGER_GUID)
        return;

    g_TheCurrentPluginEntry = entry;

    if (pluginInfo.m_InitInstanceFct &&
        (pluginInfo.m_Type != CKPLUGIN_RENDERENGINE_DLL ||
         context->m_SelectedRenderEngine == entry->m_IndexInCategory)) {
        pluginInfo.m_InitInstanceFct(context);
    }

    auto pluginType = pluginInfo.m_Type;
    if (pluginType == CKPLUGIN_MANAGER_DLL) {
        entry->m_Active = context->m_ManagerTable.IsHere(pluginInfo.m_GUID);
    } else if (pluginType == CKPLUGIN_RENDERENGINE_DLL) {
        entry->m_Active = entry->m_IndexInCategory == context->m_SelectedRenderEngine;
    } else {
        entry->m_Active = TRUE;
    }

    auto &readerInfo = entry->m_ReadersInfo;
    if (!readerInfo) {
        g_TheCurrentPluginEntry = nullptr;
        return;
    }

    auto *readerFct = readerInfo->m_GetReaderFct;
    if (!readerFct) {
        g_TheCurrentPluginEntry = nullptr;
        return;
    }

    auto *reader = readerFct(entry->m_PositionInDll);
    if (!reader) {
        g_TheCurrentPluginEntry = nullptr;
        return;
    }

    int optionCount = reader->GetOptionsCount();
    readerInfo->m_OptionCount = optionCount;
    readerInfo->m_ReaderFlags = reader->GetFlags();

    if (optionCount == 0) {
        g_TheCurrentPluginEntry = nullptr;
        reader->Release();
        return;
    }

    CKParameterManager *pm = context->m_ParameterManager;

    XArray<CKGUID> optionGuids(optionCount);

    size_t optionNamesLength = 0;
    auto *optionNames = new CKSTRING[optionCount];
    memset(optionNames, 0, sizeof(CKSTRING) * optionCount);

    for (int i = 0; i < optionCount; ++i) {
        CKSTRING desc = CKStrdup(reader->GetOptionDescription(i));
        char *name = strchr(desc, ':');
        if (!name)
            continue;

        *name = '\0';
        ++name;

        char *data = strchr(name, ':');
        if (data) {
            *data = '\0';
            ++data;
        }

        CKGUID guid;

        if (stricmp(desc, "Flags") == 0) {
            guid = context->GetSecureGuid();
            pm->RegisterNewFlags(guid, name, data);
        } else if (stricmp(desc, "Enum") == 0) {
            guid = context->GetSecureGuid();
            pm->RegisterNewEnum(guid, name, data);
            CKParameterTypeDesc *paramTypeDesc = pm->GetParameterTypeDescription(guid);
            if (paramTypeDesc)
                paramTypeDesc->dwFlags |= CKPARAMETERTYPE_HIDDEN;
        } else {
            guid = pm->ParameterNameToGuid(desc);
        }

        optionGuids.PushBack(guid);

        optionNames[i] = CKStrdup(name);
        optionNamesLength += strlen(name) + 2;

        CKDeletePointer(desc);
    }

    auto *data = new char[optionNamesLength];
    strcpy(data, "");
    for (int i = 0; i < optionCount; ++i) {
        strcat(data, optionNames[i]);
        strcat(data, ",");
    }
    data[strlen(data) - 1] = '\0';

    XString name = pluginInfo.m_Description;
    name << " Options Parameter";
    readerInfo->m_SettingsParameterGuid = context->GetSecureGuid();

    pm->RegisterNewStructure(readerInfo->m_SettingsParameterGuid, name.Str(), data, optionGuids);
    CKParameterTypeDesc *paramTypeDesc = pm->GetParameterTypeDescription(readerInfo->m_SettingsParameterGuid);
    if (paramTypeDesc)
        paramTypeDesc->dwFlags |= CKPARAMETERTYPE_HIDDEN;

    for (int i = 0; i < optionCount; ++i) {
        CKDeletePointer(optionNames[i]);
    }
    memset(optionNames, 0, sizeof(CKSTRING) * optionCount);
    delete[] optionNames;
    delete[] data;

    g_TheCurrentPluginEntry = nullptr;
    reader->Release();
}

void CKPluginManager::ExitInstancePluginEntry(CKPluginEntry *entry, CKContext *context) {
}

void CKPluginManager::InitializeBehaviors(CKDLL_OBJECTDECLARATIONFUNCTION Fct, CKPluginEntry &entry) {

}

void CKPluginManager::RemoveBehaviors(int PluginIndex) {

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
    for (XClassArray<CKPluginCategory>::Iterator cit = m_PluginCategories.Begin(); cit != m_PluginCategories.End(); ++cit) {
        for (XArray<CKPluginEntry *>::Iterator eit = cit->m_Entries.Begin(); eit != cit->m_Entries.End(); ++eit) {
            if (*eit) {
                delete (*eit);
            }
        }
    }

    m_PluginCategories.Clear();
    m_PluginDlls.Clear();
    m_RunTimeDlls.Clear();
}