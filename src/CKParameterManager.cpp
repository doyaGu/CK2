#include "CKParameterManager.h"

#include "CKPluginManager.h"
#include "CKStateChunk.h"

extern CKPluginManager g_ThePluginManager;
extern CKPluginEntry *g_TheCurrentPluginEntry;
extern XClassInfoArray g_CKClassInfo;

void CKInitializeParameterTypes(CKParameterManager *pm) {
    // TODO: Register all built-in parameter types
}

void CK_ParameterCopier_SaveLoad(CKParameter *dest, CKParameter *src) {
    CKStateChunk *stateChunk = nullptr;

    // First, load state from the source parameter.
    // The SaveLoadFunction is expected to be a function pointer
    // taking a CKParameter*, a pointer to a CKStateChunk*, and a flag.
    src->m_ParamType->SaveLoadFunction(src, &stateChunk, 0);

    // Then, save state into the destination parameter.
    dest->m_ParamType->SaveLoadFunction(dest, &stateChunk, 1);

    // If a state chunk was allocated, clean it up.
    if (stateChunk) {
        delete stateChunk;
    }
}

void CK_ParameterCopier_SetValue(CKParameter *dest, CKParameter *src) {
    dest->SetValue(src->m_Buffer, src->m_Size);
}

CKERROR CKParameterManager::RegisterParameterType(CKParameterTypeDesc *paramType) {
    // Check if GUID already exists
    if (ParameterGuidToType(paramType->Guid) >= 0)
        return CKERR_INVALIDPARAMETERTYPE;

    // Mark derivation tables as needing update
    m_DerivationMasksUpToDate = FALSE;

    // Find available slot in registered types array
    int freeSlot = -1;
    XArray<CKParameterTypeDesc *> &regTypes = m_RegisteredTypes;
    const int typeCount = regTypes.Size();

    // Search for first empty slot
    for (int i = 0; i < typeCount; ++i) {
        if (regTypes[i] == NULL) {
            freeSlot = i;
            break;
        }
    }

    // If no empty slot found, expand array
    if (freeSlot == -1) {
        freeSlot = regTypes.Size();
        regTypes.Reserve(regTypes.Allocated() * 2); // Common growth strategy
        regTypes.Resize(freeSlot + 1);
    }

    // Create and initialize new parameter type descriptor
    CKParameterTypeDesc *newDesc = new CKParameterTypeDesc();
    if (!newDesc) return CKERR_OUTOFMEMORY;

    // Copy descriptor properties
    *newDesc = *paramType;
    newDesc->Index = freeSlot;
    newDesc->Valid = TRUE;

    // Store in registered types array
    regTypes[freeSlot] = newDesc;

    // Set creator information
    CKPluginEntry *creator = g_TheCurrentPluginEntry;
    if (!creator) {
        CKBaseManager *currentMgr = m_Context->m_CurrentManager;
        if (currentMgr && currentMgr != this) {
            creator = g_ThePluginManager.FindComponent(
                currentMgr->m_ManagerGuid,
                CKPLUGIN_MANAGER_DLL
            );
        }
    }
    newDesc->CreatorDll = creator;

    // Set default copy function if needed
    if (!newDesc->CopyFunction) {
        newDesc->CopyFunction = newDesc->SaveLoadFunction
                                    ? CK_ParameterCopier_SaveLoad
                                    : CK_ParameterCopier_SetValue;
    }

    // Update GUID lookup table
    m_ParamGuids.Insert(paramType->Guid, freeSlot);

    // Mark type enumeration as outdated
    m_ParameterTypeEnumUpToDate = FALSE;

    return CK_OK;
}

CKERROR CKParameterManager::UnRegisterParameterType(CKGUID guid) {
    // Invalidate derivation cache
    m_DerivationMasksUpToDate = FALSE;
    CKParameterTypeDesc *targetDesc = NULL;

    // Find and remove from registered types
    for (XArray<CKParameterTypeDesc *>::Iterator it = m_RegisteredTypes.Begin();
         it != m_RegisteredTypes.End(); ++it) {
        CKParameterTypeDesc *desc = *it;
        if (desc && desc->Guid == guid) {
            // Cleanup descriptor
            delete desc;
            *it = NULL;
            targetDesc = desc;
            break;
        }
    }

    m_ParamGuids.Remove(guid);

    if (!targetDesc)
        return CKERR_INVALIDPARAMETER;

    // Cleanup parameter references in different object types
    bool referencesCleared = false;

    // Process CKParameterIn objects
    int paramInCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETERIN);
    CK_ID *paramInList = m_Context->GetObjectsListByClassID(CKCID_PARAMETERIN);
    for (int i = 0; i < paramInCount; ++i) {
        CKParameterIn *param = (CKParameterIn *)m_Context->GetObject(paramInList[i]);
        if (param->m_ParamType == targetDesc) {
            param->m_ParamType = nullptr;
            referencesCleared = true;
        }
    }

    // Process CKParameter objects
    int paramCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETER);
    CK_ID *paramList = m_Context->GetObjectsListByClassID(CKCID_PARAMETER);
    for (int i = 0; i < paramCount; ++i) {
        CKParameter *io = (CKParameter *)m_Context->GetObject(paramList[i]);
        if (io->m_ParamType == targetDesc) {
            io->m_ParamType = nullptr;
            referencesCleared = true;
        }
    }

    // Process CKParameterOut objects
    int paramOutCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETEROUT);
    CK_ID *paramOutList = m_Context->GetObjectsListByClassID(CKCID_PARAMETEROUT);
    for (int i = 0; i < paramOutCount; ++i) {
        CKParameterOut *param = (CKParameterOut *)m_Context->GetObject(paramOutList[i]);
        if (param->m_ParamType == targetDesc) {
            param->m_ParamType = nullptr;
            referencesCleared = true;
        }
    }

    // Process CKParameterLocal objects
    int paramLocalCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETERLOCAL);
    CK_ID *paramLocalList = m_Context->GetObjectsListByClassID(CKCID_PARAMETERLOCAL);
    for (int i = 0; i < paramLocalCount; ++i) {
        CKParameterLocal *param = (CKParameterLocal *)m_Context->GetObject(paramLocalList[i]);
        if (param->m_ParamType == targetDesc) {
            param->m_ParamType = nullptr;
            referencesCleared = true;
        }
    }

    if (referencesCleared) {
        m_Context->OutputToConsole("Warning: Removed parameter type still in use");
    }

    // Remove associated operations
    for (int opIndex = 0; opIndex < m_NbOperations; ++opIndex) {
        OperationCell &operation = m_OperationTree[opIndex];
        //
        // // Remove parameter type from operation's parameter lists
        // for (int paramIndex = 0; paramIndex < operation.ParamCount1; ++paramIndex) {
        //     RemoveOperationsUsingParameter(operation.paramList + paramIndex, guid);
        // }
    }

    // Recursively unregister derived types
    for (XArray<CKParameterTypeDesc *>::Iterator it = m_RegisteredTypes.Begin();
         it != m_RegisteredTypes.End(); ++it) {
        CKParameterTypeDesc *desc = *it;
        if (desc && desc->DerivedFrom == guid) {
            UnRegisterParameterType(desc->Guid);
        }
    }

    // Invalidate type enumeration cache
    m_ParameterTypeEnumUpToDate = FALSE;

    return CK_OK;
}

CKParameterTypeDesc *CKParameterManager::GetParameterTypeDescription(int type) {
    if (type < 0 || type > m_RegisteredTypes.Size())
        return nullptr;
    return m_RegisteredTypes[type];
}

CKParameterTypeDesc *CKParameterManager::GetParameterTypeDescription(CKGUID guid) {
    auto type = ParameterGuidToType(guid);
    if (type == -1)
        return nullptr;
    return m_RegisteredTypes[type];
}

int CKParameterManager::GetParameterSize(CKParameterType type) {
    if (CheckParamTypeValidity(type))
        return m_RegisteredTypes[type]->DefaultSize;
    return 0;
}

int CKParameterManager::GetParameterTypesCount() {
    return m_RegisteredTypes.Size();
}

CKParameterType CKParameterManager::ParameterGuidToType(CKGUID guid) {
    auto *pType = m_ParamGuids.FindPtr(guid);
    if (pType)
        return *pType;
    return -1;
}

CKSTRING CKParameterManager::ParameterGuidToName(CKGUID guid) {
    CKParameterType type = ParameterGuidToType(guid);
    return ParameterTypeToName(type);
}

CKGUID CKParameterManager::ParameterTypeToGuid(CKParameterType type) {
    if (CheckParamTypeValidity(type)) {
        return m_RegisteredTypes[type]->Guid;
    }
    return CKGUID();
}

CKSTRING CKParameterManager::ParameterTypeToName(CKParameterType type) {
    if (CheckParamTypeValidity(type)) {
        return m_RegisteredTypes[type]->TypeName.Str();
    }
    return nullptr;
}

CKGUID CKParameterManager::ParameterNameToGuid(CKSTRING name) {
    CKGUID result;

    if (!name || name[0] == '\0') {
        return result;
    }

    const int typeCount = m_RegisteredTypes.Size();

    for (int i = 0; i < typeCount; ++i) {
        CKParameterTypeDesc *typeDesc = m_RegisteredTypes[i];
        if (!typeDesc) continue;

        // Get type name with null safety
        const char *registeredName = typeDesc->TypeName.CStr();
        if (!registeredName) {
            registeredName = "";
        }

        if (strcmp(registeredName, name) == 0) {
            result = typeDesc->Guid;
            break;
        }
    }

    return result;
}

CKParameterType CKParameterManager::ParameterNameToType(CKSTRING name) {
    if (!name) return -1;

    const int typeCount = m_RegisteredTypes.Size();
    for (int i = 0; i < typeCount; ++i) {
        CKParameterTypeDesc *desc = m_RegisteredTypes[i];
        if (!desc) continue;

        const char *registeredName = desc->TypeName.CStr();
        if (!registeredName) registeredName = "";

        if (strcmp(registeredName, name) == 0) {
            return i;
        }
    }
    return -1;
}

CKBOOL CKParameterManager::IsDerivedFrom(CKGUID guid1, CKGUID parent) {
    CKParameterType type1 = ParameterGuidToType(guid1);
    CKParameterType type2 = ParameterGuidToType(parent);
    return IsDerivedFrom(type1, type2);
}

CKBOOL CKParameterManager::IsDerivedFrom(CKParameterType child, CKParameterType parent) {
    if (child == parent)
        return true;

    if (!m_DerivationMasksUpToDate) {
        UpdateDerivationTables();
    }

    CKParameterTypeDesc *childDesc = GetParameterTypeDescription(child);
    if (!childDesc || parent >= childDesc->DerivationMask.Size()) {
        return false;
    }

    return childDesc->DerivationMask.IsSet(parent) != 0;
}

CKBOOL CKParameterManager::IsTypeCompatible(CKGUID guid1, CKGUID guid2) {
    CKParameterType type1 = ParameterGuidToType(guid1);
    CKParameterType type2 = ParameterGuidToType(guid2);
    return IsTypeCompatible(type1, type2);
}

CKBOOL CKParameterManager::IsTypeCompatible(CKParameterType type1, CKParameterType type2) {
    if (type1 == type2)
        return true;

    if (!m_DerivationMasksUpToDate) {
        UpdateDerivationTables();
    }

    CKParameterTypeDesc *desc1 = GetParameterTypeDescription(type1);
    CKParameterTypeDesc *desc2 = GetParameterTypeDescription(type2);
    if (!desc1 || !desc2)
        return false;

    return IsDerivedFrom(type1, type2) || IsDerivedFrom(type2, type1);
}

CK_CLASSID CKParameterManager::TypeToClassID(CKParameterType type) {
    if (CheckParamTypeValidity(type))
        return m_RegisteredTypes[type]->Cid;
    return 0;
}

CK_CLASSID CKParameterManager::GuidToClassID(CKGUID guid) {
    CKParameterType type = ParameterGuidToType(guid);
    if (type >= 0)
        return m_RegisteredTypes[type]->Cid;
    return 0;
}

CKParameterType CKParameterManager::ClassIDToType(CK_CLASSID cid) {
    CKGUID guid = ClassIDToGuid(cid);
    return ParameterGuidToType(guid);
}

CKGUID CKParameterManager::ClassIDToGuid(CK_CLASSID cid) {
    if (cid < 0 || cid >= g_CKClassInfo.Size())
        return CKGUID();
    return g_CKClassInfo[cid].Parameter;
}

CKERROR CKParameterManager::RegisterNewFlags(CKGUID FlagsGuid, CKSTRING FlagsName, CKSTRING FlagsData) {
    // Validate input parameters
    if (!FlagsData || !FlagsName) return CKERR_INVALIDPARAMETER;

    // Check if GUID already registered
    if (ParameterGuidToType(FlagsGuid) >= 0) {
        return CKERR_INVALIDGUID;
    }

    // Create parameter type descriptor for flags
    CKParameterTypeDesc flagDesc;
    flagDesc.Guid = FlagsGuid;
    flagDesc.TypeName = FlagsName;
    flagDesc.DerivedFrom = CKPGUID_FLAGS; // Predefined flags base type
    flagDesc.DefaultSize = sizeof(CKDWORD);
    // flagDesc.CopyFunction = CK_ParameterCopier_Dword;
    // flagDesc.StringFunction = CKFlagsStringFunc;
    // flagDesc.UICreatorFunction = GetUICreatorFunction(m_Context, &flagDesc);
    flagDesc.dwParam = m_NbFlagsDefined;
    flagDesc.dwFlags = CKPARAMETERTYPE_FLAGS;

    // Register the base flags type
    CKERROR error = RegisterParameterType(&flagDesc);
    if (error != CK_OK) return error;

    // Parse flag definitions
    XArray<CKFlagDefinition> flagEntries;
    char *context = NULL;
    char *token = strtok(FlagsData, ",");

    while (token) {
        CKFlagDefinition def;
        char *valuePos = strchr(token, '=');

        // Extract flag name and value
        if (valuePos) {
            *valuePos = '\0';
            def.Name = XString(token).Trim();
            def.Value = atoi(valuePos + 1);
        } else {
            def.Name = XString(token).Trim();
            def.Value = flagEntries.Size(); // Auto-increment if no value specified
        }

        flagEntries.PushBack(def);
        token = strtok(NULL, ",");
    }

    // Reallocate flags array with new entry
    CKFlagsStruct *newFlags = new CKFlagsStruct[m_NbFlagsDefined + 1];
    if (m_Flags) {
        memcpy(newFlags, m_Flags, sizeof(CKFlagsStruct) * m_NbFlagsDefined);
        delete[] m_Flags;
    }
    m_Flags = newFlags;

    // Store parsed flag definitions
    CKFlagsStruct &newFlag = m_Flags[m_NbFlagsDefined];
    newFlag.NbData = flagEntries.Size();
    newFlag.Vals = new int[newFlag.NbData];
    newFlag.Desc = new CKSTRING[newFlag.NbData];

    for (int i = 0; i < flagEntries.Size(); ++i) {
        newFlag.Desc[i] = new char[flagEntries[i].Name.Length() + 1];
        strcpy(newFlag.Desc[i], flagEntries[i].Name.CStr());
        newFlag.Vals[i] = flagEntries[i].Value;
    }

    m_NbFlagsDefined++;
    return CK_OK;
}

CKERROR CKParameterManager::RegisterNewEnum(CKGUID EnumGuid, CKSTRING EnumName, CKSTRING EnumData) {
    // Validate input parameters
    if (!EnumData || !EnumName) return CKERR_INVALIDPARAMETER;

    // Check if GUID already registered
    if (ParameterGuidToType(EnumGuid) >= 0) {
        return CKERR_INVALIDGUID;
    }

    // Create parameter type descriptor for enum
    CKParameterTypeDesc enumDesc;
    enumDesc.Guid = EnumGuid;
    enumDesc.TypeName = EnumName;
    enumDesc.DerivedFrom = CKPGUID_INT;
    enumDesc.DefaultSize = sizeof(CKDWORD);
    // enumDesc.CopyFunction = CK_ParameterCopier_Dword;
    // enumDesc.StringFunction = CKEnumStringFunc;
    // enumDesc.UICreatorFunction = GetUICreatorFunction(m_Context, &enumDesc);
    enumDesc.dwFlags = CKPARAMETERTYPE_ENUMS;

    // Register the enum type
    CKERROR error = RegisterParameterType(&enumDesc);
    if (error != CK_OK) return error;

    // Parse enum definitions
    XArray<CKEnumEntry> enumEntries;
    char *context = NULL;
    char *token = strtok(EnumData, ",");
    int currentValue = 0;

    while (token) {
        CKEnumEntry entry;
        char *valuePos = strchr(token, '=');

        // Extract enum name and value
        if (valuePos) {
            *valuePos = '\0';
            entry.Name = XString(token).Trim();
            entry.Value = atoi(valuePos + 1);
            currentValue = entry.Value; // Update current value
        } else {
            entry.Name = XString(token).Trim();
            entry.Value = currentValue;
        }

        enumEntries.PushBack(entry);
        currentValue++; // Auto-increment for next entry
        token = strtok(NULL, ",");
    }

    // Reallocate enums array with new entry
    CKEnumStruct *newEnums = new CKEnumStruct[m_NbEnumsDefined + 1];
    if (m_Enums) {
        memcpy(newEnums, m_Enums, sizeof(CKEnumStruct) * m_NbEnumsDefined);
        delete[] m_Enums;
    }
    m_Enums = newEnums;

    // Store parsed enum entries
    CKEnumStruct &newEnum = m_Enums[m_NbEnumsDefined];
    newEnum.NbData = enumEntries.Size();
    newEnum.Vals = new int[newEnum.NbData];
    newEnum.Desc = new CKSTRING[newEnum.NbData];

    for (int i = 0; i < enumEntries.Size(); ++i) {
        newEnum.Desc[i] = new char[enumEntries[i].Name.Length() + 1];
        strcpy(newEnum.Desc[i], enumEntries[i].Name.CStr());
        newEnum.Vals[i] = enumEntries[i].Value;
    }

    m_NbEnumsDefined++;
    return CK_OK;
}

CKERROR CKParameterManager::ChangeEnumDeclaration(CKGUID EnumGuid, CKSTRING EnumData) {
    // Validate inputs
    if (!m_Enums || !EnumData) return CKERR_INVALIDPARAMETER;

    // Verify enum type exists
    CKParameterType enumType = ParameterGuidToType(EnumGuid);
    if (enumType <= 0)
        return CKERR_INVALIDGUID;

    // Get enum descriptor
    CKParameterTypeDesc *typeDesc = GetParameterTypeDescription(enumType);
    if (!typeDesc)
        return CKERR_INVALIDGUID;

    // Get enum index
    const int enumIndex = typeDesc->dwParam;
    if (enumIndex >= m_NbEnumsDefined) return CKERR_INVALIDPARAMETER;

    // Parse new enum entries
    XArray<CKEnumEntry> newEntries;
    char *token = strtok(EnumData, ",");
    int currentValue = 0;

    while (token) {
        CKEnumEntry entry;
        char *valuePos = strchr(token, '=');

        // Extract name and value
        if (valuePos) {
            *valuePos = '\0';
            entry.Name = XString(token).Trim();
            entry.Value = atoi(valuePos + 1);
            currentValue = entry.Value; // Update current value
        } else {
            entry.Name = XString(token).Trim();
            entry.Value = currentValue;
        }

        newEntries.PushBack(entry);
        currentValue++; // Auto-increment for next entry
        token = strtok(NULL, ",");
    }

    // Clean up old enum data
    CKEnumStruct &oldEnum = m_Enums[enumIndex];

    // Delete existing values
    delete[] oldEnum.Vals;
    oldEnum.Vals = NULL;

    // Delete existing descriptions
    for (int i = 0; i < oldEnum.NbData; ++i) {
        delete[] oldEnum.Desc[i];
    }
    delete[] oldEnum.Desc;
    oldEnum.Desc = NULL;

    // Allocate new memory
    try {
        oldEnum.NbData = newEntries.Size();
        oldEnum.Vals = new int[oldEnum.NbData];
        oldEnum.Desc = new CKSTRING[oldEnum.NbData];

        // Copy new values
        for (int i = 0; i < oldEnum.NbData; ++i) {
            oldEnum.Desc[i] = new char[newEntries[i].Name.Length() + 1];
            strcpy(oldEnum.Desc[i], newEntries[i].Name.CStr());
            oldEnum.Vals[i] = newEntries[i].Value;
        }
    } catch (...) {
        // Cleanup if allocation fails
        delete[] oldEnum.Vals;
        if (oldEnum.Desc) {
            for (int i = 0; i < oldEnum.NbData; ++i) {
                delete[] oldEnum.Desc[i];
            }
        }
        delete[] oldEnum.Desc;
        oldEnum.NbData = 0;
        return CKERR_OUTOFMEMORY;
    }

    return CK_OK;
}

CKERROR CKParameterManager::ChangeFlagsDeclaration(CKGUID FlagsGuid, CKSTRING FlagsData) {
    // Validate inputs
    if (!m_Flags || !FlagsData) return CKERR_INVALIDPARAMETER;

    // Verify flags type exists
    CKParameterType flagsType = ParameterGuidToType(FlagsGuid);
    if (flagsType <= 0)
        return CKERR_INVALIDGUID;

    // Get flags descriptor
    CKParameterTypeDesc *typeDesc = GetParameterTypeDescription(flagsType);
    if (!typeDesc) return CKERR_INVALIDGUID;

    // Get flags index
    const int flagsIndex = typeDesc->dwParam;
    if (flagsIndex >= m_NbFlagsDefined) return CKERR_INVALIDPARAMETER;

    // Parse new flag entries
    XArray<CKFlagEntry> newEntries;
    // char* context = NULL;
    // char* token = strtok_r(FlagsData, ",", &context);
    // int currentValue = 0;
    //
    // while (token) {
    //     CKFlagEntry entry;
    //     char* valuePos = strchr(token, '=');
    //
    //     // Extract name and value
    //     if (valuePos) {
    //         *valuePos = '\0';
    //         entry.Name = XString(token).Trim();
    //         entry.Value = atoi(valuePos + 1);
    //         currentValue = entry.Value; // Update current value
    //     } else {
    //         entry.Name = XString(token).Trim();
    //         entry.Value = 1 << currentValue; // Auto-assign bit flag
    //         currentValue++;
    //     }
    //
    //     newEntries.PushBack(entry);
    //     token = strtok_r(NULL, ",", &context);
    // }

    // Clean up old flag data
    CKFlagsStruct &oldFlags = m_Flags[flagsIndex];

    // Delete existing values
    delete[] oldFlags.Vals;
    oldFlags.Vals = NULL;

    // Delete existing descriptions
    for (int i = 0; i < oldFlags.NbData; ++i) {
        delete[] oldFlags.Desc[i];
    }
    delete[] oldFlags.Desc;
    oldFlags.Desc = NULL;

    // Allocate new memory
    try {
        oldFlags.NbData = newEntries.Size();
        oldFlags.Vals = new int[oldFlags.NbData];
        oldFlags.Desc = new CKSTRING[oldFlags.NbData];

        // Copy new values
        for (int i = 0; i < oldFlags.NbData; ++i) {
            oldFlags.Desc[i] = new char[newEntries[i].Name.Length() + 1];
            strcpy(oldFlags.Desc[i], newEntries[i].Name.CStr());
            oldFlags.Vals[i] = newEntries[i].Value;
        }
    } catch (...) {
        // Cleanup if allocation fails
        delete[] oldFlags.Vals;
        if (oldFlags.Desc) {
            for (int i = 0; i < oldFlags.NbData; ++i) {
                delete[] oldFlags.Desc[i];
            }
        }
        delete[] oldFlags.Desc;
        oldFlags.NbData = 0;
        return CKERR_OUTOFMEMORY;
    }

    return CK_OK;
}

CKERROR CKParameterManager::RegisterNewStructure(CKGUID StructGuid, CKSTRING StructName, CKSTRING StructData, ...) {
    if (!StructName || !StructData)
        return CKERR_INVALIDPARAMETER;

    if (ParameterGuidToType(StructGuid) >= 0)
        return CKERR_INVALIDGUID;

    // Create parameter type descriptor for the structure
    CKParameterTypeDesc structDesc;
    structDesc.Guid = StructGuid;
    structDesc.TypeName = StructName;
    structDesc.Valid = TRUE;
    // structDesc.StringFunction = CKStructStringFunc;
    // structDesc.CreateDefaultFunction = CKStructCreator;
    // structDesc.DeleteFunction = CKStructDestructor;
    // structDesc.SaveLoadFunction = CKStructSaver;
    // structDesc.CopyFunction = CKStructCopier;
    // structDesc.UICreatorFunction = GetUICreatorFunction(m_Context, &structDesc);
    structDesc.dwFlags = CKPARAMETERTYPE_STRUCT;

    // Parse member names
    XArray<XString> memberNames;
    // char* context = NULL;
    // char* token = strtok_r(const_cast<char*>(StructData), ",", &context);
    // while (token) {
    //     memberNames.PushBack(XString(token).Trim());
    //     token = strtok_r(NULL, ",", &context);
    // }

    // Get member GUIDs from variadic arguments
    XArray<CKGUID> memberGuids;
    va_list args;
    va_start(args, StructData);
    for (size_t i = 0; i < memberNames.Size(); ++i) {
        memberGuids.PushBack(va_arg(args, CKGUID));
    }
    va_end(args);

    // Verify member count matches GUID count
    if (memberNames.Size() != memberGuids.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    // Register the structure type
    structDesc.DefaultSize = sizeof(CKGUID) * memberNames.Size();
    CKERROR err = RegisterParameterType(&structDesc);
    if (err != CK_OK) return err;

    // Allocate memory for structure description
    CKStructStruct *newStructs = new CKStructStruct[m_NbStructDefined + 1];
    if (m_Structs) {
        memcpy(newStructs, m_Structs, sizeof(CKStructStruct) * m_NbStructDefined);
        delete[] m_Structs;
    }
    m_Structs = newStructs;

    // Populate structure members
    CKStructStruct &structDef = m_Structs[m_NbStructDefined];
    structDef.NbData = memberNames.Size();

    try {
        structDef.Desc = new CKSTRING[structDef.NbData];
        structDef.Guids = new CKGUID[structDef.NbData];

        // Copy member names and GUIDs
        for (int i = 0; i < structDef.NbData; ++i) {
            structDef.Desc[i] = new char[memberNames[i].Length() + 1];
            strcpy(structDef.Desc[i], memberNames[i].CStr());
            structDef.Guids[i] = memberGuids[i];
        }
    } catch (...) {
        // Cleanup on allocation failure
        delete[] structDef.Desc;
        delete[] structDef.Guids;
        return CKERR_OUTOFMEMORY;
    }

    m_NbStructDefined++;
    return CK_OK;
}

CKERROR CKParameterManager::RegisterNewStructure(CKGUID StructGuid, CKSTRING StructName, CKSTRING StructData,
                                                 XArray<CKGUID> &ListGuid) {
    // Validate input parameters
    if (!StructData || !StructName[0])
        return CKERR_INVALIDPARAMETER;

    // Check for existing GUID registration
    if (ParameterGuidToType(StructGuid) >= 0)
        return CKERR_INVALIDGUID;

    // Create parameter type descriptor
    CKParameterTypeDesc structDesc;
    structDesc.Guid = StructGuid;
    structDesc.TypeName = StructName;
    structDesc.Valid = TRUE;
    // structDesc.StringFunction = CKStructStringFunc;
    // structDesc.CreateDefaultFunction = CKStructCreator;
    // structDesc.DeleteFunction = CKStructDestructor;
    // structDesc.SaveLoadFunction = CKStructSaver;
    // structDesc.CopyFunction = CKStructCopier;
    // structDesc.UICreatorFunction = GetUICreatorFunction(m_Context, &structDesc);
    structDesc.dwFlags = CKPARAMETERTYPE_STRUCT;

    // Parse member names
    XArray<XString> members;
    char *copy = strdup(StructData);
    char *token = strtok(copy, ",");

    while (token) {
        members.PushBack(XString(token).Trim());
        token = strtok(NULL, ",");
    }
    free(copy);

    // Validate member count matches GUID count
    if (members.Size() != ListGuid.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    // Register the base structure type
    structDesc.DefaultSize = sizeof(CKGUID) * members.Size();
    CKERROR err = RegisterParameterType(&structDesc);
    if (err != CK_OK) return err;

    // Reallocate structures array
    CKStructStruct *newStructs = new CKStructStruct[m_NbStructDefined + 1];
    if (m_Structs) {
        memcpy(newStructs, m_Structs, sizeof(CKStructStruct) * m_NbStructDefined);
        delete[] m_Structs;
    }
    m_Structs = newStructs;

    // Initialize new structure entry
    CKStructStruct &newStruct = m_Structs[m_NbStructDefined];
    newStruct.NbData = members.Size();

    try {
        // Allocate member arrays
        newStruct.Desc = new CKSTRING[newStruct.NbData];
        newStruct.Guids = new CKGUID[newStruct.NbData];

        // Copy member data
        for (int i = 0; i < newStruct.NbData; ++i) {
            // Allocate and copy name
            newStruct.Desc[i] = new char[members[i].Length() + 1];
            strcpy(newStruct.Desc[i], members[i].CStr());

            // Copy GUID
            newStruct.Guids[i] = ListGuid[i];
        }
    } catch (...) {
        // Cleanup on allocation failure
        if (newStruct.Desc) {
            for (int i = 0; i < newStruct.NbData; ++i) {
                delete[] newStruct.Desc[i];
            }
            delete[] newStruct.Desc;
        }
        delete[] newStruct.Guids;
        return CKERR_OUTOFMEMORY;
    }

    m_NbStructDefined++;
    return CK_OK;
}

int CKParameterManager::GetNbFlagDefined() {
    return m_NbFlagsDefined;
}

int CKParameterManager::GetNbEnumDefined() {
    return m_NbEnumsDefined;
}

int CKParameterManager::GetNbStructDefined() {
    return m_NbStructDefined;
}

CKFlagsStruct *CKParameterManager::GetFlagsDescByType(CKParameterType pType) {
    return nullptr;
}

CKEnumStruct *CKParameterManager::GetEnumDescByType(CKParameterType pType) {
    return nullptr;
}

CKStructStruct *CKParameterManager::GetStructDescByType(CKParameterType pType) {
    return nullptr;
}

CKOperationType CKParameterManager::RegisterOperationType(CKGUID OpCode, CKSTRING name) {
    return 0;
}

CKERROR CKParameterManager::UnRegisterOperationType(CKGUID opguid) {
    return 0;
}

CKERROR CKParameterManager::UnRegisterOperationType(CKOperationType opcode) {
    return 0;
}

CKERROR CKParameterManager::RegisterOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                                      CKGUID &type_param2, CK_PARAMETEROPERATION op) {
    return 0;
}

CK_PARAMETEROPERATION
CKParameterManager::GetOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                         CKGUID &type_param2) {
    return nullptr;
}

CKERROR CKParameterManager::UnRegisterOperationFunction(CKGUID &operation, CKGUID &type_paramres, CKGUID &type_param1,
                                                        CKGUID &type_param2) {
    return 0;
}

CKGUID CKParameterManager::OperationCodeToGuid(CKOperationType type) {
    return CKGUID();
}

CKSTRING CKParameterManager::OperationCodeToName(CKOperationType type) {
    return nullptr;
}

CKOperationType CKParameterManager::OperationGuidToCode(CKGUID guid) {
    return 0;
}

CKSTRING CKParameterManager::OperationGuidToName(CKGUID guid) {
    return nullptr;
}

CKGUID CKParameterManager::OperationNameToGuid(CKSTRING name) {
    return CKGUID();
}

CKOperationType CKParameterManager::OperationNameToCode(CKSTRING name) {
    return 0;
}

int CKParameterManager::GetAvailableOperationsDesc(const CKGUID &opGuid, CKParameterOut *res, CKParameterIn *p1,
                                                   CKParameterIn *p2, CKOperationDesc *list) {
    if (!m_DerivationMasksUpToDate) {
        UpdateDerivationTables();
    }

    // Initialize GUID buffers for parameter hierarchies
    CKGUID p1Guids[128], p2Guids[128], resGuids[128];
    int numP1Guids = 0, numP2Guids = 0, numResGuids = 0;

    // Process p1 parameter GUID hierarchy
    if (p1) {
        CKParameterTypeDesc *p1Type = p1->m_ParamType;
        CKGUID p1Guid = ParameterTypeToGuid(p1Type ? p1Type->Index : -1);
        numP1Guids = BuildGuidHierarchy(p1Guid, p1Guids, 128);
    }

    // Process p2 parameter GUID hierarchy
    if (p2) {
        CKParameterTypeDesc *p2Type = p2->m_ParamType;
        CKGUID p2Guid = ParameterTypeToGuid(p2Type ? p2Type->Index : -1);
        numP2Guids = BuildGuidHierarchy(p2Guid, p2Guids, 128);
    }

    // Process result parameter GUID hierarchy
    if (res) {
        CKParameterType resType = res->GetType();
        CKGUID resGuid = ParameterTypeToGuid(resType);
        numResGuids = BuildGuidHierarchy(resGuid, resGuids, 128);
    }

    // Convert operation GUID to operation code
    CKOperationType opCode = OperationGuidToCode(opGuid);
    int operationCount = 0;

    // // Iterate through relevant operations
    // for (CKOperationType currentOp = opCode; currentOp < m_NbOperations; ++currentOp) {
    //     OperationCell* operation = &m_OperationTree[currentOp];
    //
    //     // Check input parameter compatibility
    //     bool p1Compatible = CheckParameterCompatibility(operation->param1, p1Guids, numP1Guids);
    //     bool p2Compatible = CheckParameterCompatibility(operation->param2, p2Guids, numP2Guids);
    //     bool resCompatible = CheckParameterCompatibility(operation->result, resGuids, numResGuids);
    //
    //     if (p1Compatible && p2Compatible && resCompatible) {
    //         if (list) {
    //             FillOperationDesc(list[operationCount], operation, currentOp);
    //         }
    //         operationCount++;
    //
    //         // Handle derived types when parameters are null
    //         if (!res) AddDerivedOperations(operation, list, operationCount, currentOp);
    //         if (!p1) AddDerivedOperations(operation, list, operationCount, currentOp);
    //         if (!p2) AddDerivedOperations(operation, list, operationCount, currentOp);
    //     }
    // }

    return operationCount;
}

int CKParameterManager::GetParameterOperationCount() {
    return m_NbOperations;
}

CKBOOL CKParameterManager::IsParameterTypeToBeShown(CKParameterType type) {
    return 0;
}

CKBOOL CKParameterManager::IsParameterTypeToBeShown(CKGUID guid) {
    return 0;
}

CKParameterManager::CKParameterManager(CKContext *Context) : CKBaseManager(
    Context, PARAMETER_MANAGER_GUID, "Parameter Manager") {
    m_NbOperations = 0;
    m_NbAllocatedOperations = 0;
    m_OperationTree = nullptr;
    m_DerivationMasksUpToDate = 0;
    m_NbFlagsDefined = 0;
    m_Flags = nullptr;
    m_NbStructDefined = 0;
    m_Structs = nullptr;
    m_NbEnumsDefined = 0;
    m_Enums = nullptr;
    m_Context->RegisterNewManager(this);
    CKInitializeParameterTypes(this);
}

CKParameterManager::~CKParameterManager() {
    RemoveAllOperations();
    RemoveAllParameterTypes();
}

CKBOOL CKParameterManager::CheckParamTypeValidity(CKParameterType type) {
    return type < m_RegisteredTypes.Size() && m_RegisteredTypes[type] != nullptr;
}

CKBOOL CKParameterManager::CheckOpCodeValidity(CKOperationType type) {
    return type < m_NbOperations && m_OperationTree[type].IsActive;
}

CKBOOL CKParameterManager::GetParameterGuidParentGuid(CKGUID child, CKGUID &parent) {
    CKParameterType type = ParameterGuidToType(child);
    if (CheckParamTypeValidity(type)) {
        parent = m_RegisteredTypes[type]->DerivedFrom;
        if (parent != CKGUID())
            return TRUE;
    }
    return FALSE;
}

void CKParameterManager::UpdateDerivationTables() {
}

void CKParameterManager::RecurseDeleteParam(TreeCell *cell, CKGUID param) {
}

int CKParameterManager::DichotomicSearch(int start, int end, TreeCell *tab, CKGUID key) {
    return 0;
}

void CKParameterManager::RecurseDelete(TreeCell *cell) {
}

CKBOOL CKParameterManager::IsDerivedFromIntern(int child, int parent) {
    return 0;
}

CKBOOL CKParameterManager::RemoveAllParameterTypes() {
    return 0;
}

CKBOOL CKParameterManager::RemoveAllOperations() {
    return 0;
}

int CKParameterManager::BuildGuidHierarchy(CKGUID guid, CKGUID *buffer, int max) {
    int count = 0;
    buffer[count++] = guid;
    CKGUID parent;

    while (count < max && GetParameterGuidParentGuid(guid, parent)) {
        buffer[count++] = parent;
        guid = parent;
    }
    return count;
}

CKStructHelper::CKStructHelper(CKParameter *Param) {
}

CKStructHelper::CKStructHelper(CKContext *ctx, CKGUID PGuid, CK_ID *Data) {
}

CKStructHelper::CKStructHelper(CKContext *ctx, CKParameterType PType, CK_ID *Data) {
}

char *CKStructHelper::GetMemberName(int Pos) {
    return nullptr;
}

CKGUID CKStructHelper::GetMemberGUID(int Pos) {
    return CKGUID();
}

int CKStructHelper::GetMemberCount() {
    return 0;
}
