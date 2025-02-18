#include "CKCallbackFunctions.h"

#include "CKStateChunk.h"
#include "CKParameterManager.h"
#include "CKInterfaceManager.h"

void CKInitializeParameterTypes(CKParameterManager *man) {
    // TODO: Register all built-in parameter types
}

CKERROR CKStructCreator(CKParameter *param) {
    if (!param)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = param->m_Context;
    CKParameterManager *pm = context->GetParameterManager();

    CKParameterType type = param->GetType();
    CKStructStruct *desc = pm->GetStructDescByType(type);
    if (!desc)
        return CKERR_INVALIDPARAMETER;

    int memberCount = desc->NbData;
    CK_ID *memberIDs = new CK_ID[memberCount];
    memset(memberIDs, 0, sizeof(CK_ID) * memberCount);

    for (int i = 0; i < memberCount; ++i) {
        CKParameterOut *member = context->CreateCKParameterOut(desc->Desc[i], desc->Guids[i], param->IsDynamic());

        if (member) {
            memberIDs[i] = member->GetID();
        }
    }

    param->SetValue(memberIDs, sizeof(CK_ID) * memberCount);

    delete[] memberIDs;
    return CK_OK;
}

void CKStructDestructor(CKParameter *param) {
    if (!param)
        return;

    CKContext *context = param->m_Context;
    if (context->IsInClearAll())
        return;

    CKParameterManager *pm = context->GetParameterManager();
    CKStructStruct *desc = pm->GetStructDescByType(param->GetType());

    if (desc) {
        CK_ID *memberIDs = (CK_ID *)param->GetReadDataPtr();

        for (int i = 0; i < desc->NbData; ++i) {
            if (memberIDs[i] != 0) {
                context->DestroyObject(memberIDs[i], CK_DESTROY_NONOTIFY);
                memberIDs[i] = 0;
            }
        }
    }
}

void CKStructSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
    if (!param || !chunk)
        return;

    CKContext *context = param->m_Context;
    CKParameterManager *pm = context->GetParameterManager();
    CKParameterType type = param->GetType();
    CKStructStruct *desc = pm->GetStructDescByType(type);

    if (!desc)
        return;

    CK_ID *memberIDs = (CK_ID *)param->GetReadDataPtr();
    if (!memberIDs)
        return;

    if (load) {
        // Loading from chunk
        CKStateChunk *loadChunk = *chunk;
        if (!loadChunk)
            return;

        if (loadChunk->SeekIdentifier(0x12348765)) {
            int count = loadChunk->ReadInt();
            if (count != desc->NbData) {
                context->OutputToConsole("Structure member count mismatch!");
                return;
            }

            for (int i = 0; i < desc->NbData; ++i) {
                CKStateChunk *subChunk = loadChunk->ReadSubChunk();
                if (subChunk) {
                    CKObject *member = context->GetObject(memberIDs[i]);
                    if (member) {
                        member->Load(subChunk, NULL);
                    }
                    delete subChunk;
                }
            }
        }
    } else {
        // Saving to chunk
        CKStateChunk *saveChunk = new CKStateChunk(NULL);
        saveChunk->StartWrite();

        // Write structure header
        saveChunk->WriteIdentifier(0x12348765);
        saveChunk->WriteInt(desc->NbData);

        // Save each member
        for (int i = 0; i < desc->NbData; ++i) {
            CKObject *member = context->GetObject(memberIDs[i]);
            if (member) {
                CKStateChunk *memberChunk = member->Save(NULL, 0xFFFFFFFF);
                if (memberChunk) {
                    saveChunk->WriteSubChunk(memberChunk);
                    delete memberChunk;
                }
            }
        }

        saveChunk->CloseChunk();
        *chunk = saveChunk;
    }
}

void CKStructCopier(CKParameter *dest, CKParameter *src) {
    if (!dest || !src)
        return;

    CKContext *context = dest->m_Context;
    CKParameterManager *pm = context->GetParameterManager();

    CKParameterType type = dest->GetType();
    CKStructStruct *desc = pm->GetStructDescByType(type);
    if (!desc)
        return;

    CK_ID *srcMembers = (CK_ID *)src->GetReadDataPtr();
    CK_ID *destMembers = (CK_ID *)dest->GetWriteDataPtr();
    if (!srcMembers || !destMembers)
        return;

    for (int i = 0; i < desc->NbData; ++i) {
        CKParameterOut *srcParam = (CKParameterOut *)context->GetObject(srcMembers[i]);
        CKParameterOut *destParam = (CKParameterOut *)context->GetObject(destMembers[i]);
        if (srcParam && destParam) {
            destParam->CopyValue(srcParam, FALSE);
        }
    }
}

int CKStructStringFunc(CKParameter *param, char *valueStr, CKBOOL readFromStr) {
    const char DELIMITER = ';';
    const int MAX_MEMBER_LENGTH = 256;
    const int MAX_STRUCT_LENGTH = 2048;

    if (!param)
        return 0;

    CKContext *context = param->m_Context;
    CKParameterManager *pm = context->GetParameterManager();
    CKParameterType type = param->GetType();

    CKStructStruct *desc = pm->GetStructDescByType(type);
    if (!desc)
        return -1;

    CK_ID *memberIDs = (CK_ID *)param->GetReadDataPtr();
    if (!memberIDs)
        return -1;

    if (readFromStr) {
        if (!valueStr)
            return 0;

        const char *current = valueStr;
        for (int i = 0; i < desc->NbData; ++i) {
            CKParameterOut *member = (CKParameterOut *)context->GetObject(memberIDs[i]);
            if (!member)
                continue;

            // Find next delimiter
            size_t len = strcspn(current, &DELIMITER);
            if (len > 0) {
                char buffer[MAX_MEMBER_LENGTH] = {0};
                strncpy(buffer, current, XMin(len, sizeof(buffer) - 1));
                member->SetStringValue(buffer);
            }

            // Move to next member data
            current += len;
            if (*current == DELIMITER) current++;
        }
        return 0;
    } else {
        // Convert structure members to string
        char output[MAX_STRUCT_LENGTH] = {0};
        int totalLength = 0;

        for (int i = 0; i < desc->NbData; ++i) {
            CKParameterOut *member = (CKParameterOut *)context->GetObject(memberIDs[i]);

            char memberStr[MAX_MEMBER_LENGTH] = {0};
            if (member && member->GetStringValue(memberStr, sizeof(memberStr))) {
                // Check buffer space: existing length + member + delimiter + null
                if (totalLength + strlen(memberStr) + 1 >= sizeof(output))
                    break;

                strcat(output, memberStr);
                totalLength += strlen(memberStr);
            } else {
                strcat(output, "NULL");
                totalLength += 4;
            }

            // Add delimiter (will remove last one later)
            strcat(output, &DELIMITER);
            totalLength++;
        }

        // Remove trailing delimiter if any data was added
        if (totalLength > 0) {
            output[totalLength - 1] = '\0';
            totalLength--;
        }

        // Copy to output buffer if provided
        if (valueStr) {
            strncpy(valueStr, output, totalLength + 1);
            valueStr[totalLength] = '\0';
        }

        return totalLength + 1;
    }
}

void CK_ParameterCopier_Memcpy(CKParameter *dest, CKParameter *src) {
    memcpy(dest->m_Buffer, src->m_Buffer, dest->m_Size);
}

void CK_ParameterCopier_SaveLoad(CKParameter *dest, CKParameter *src) {
    CKStateChunk *stateChunk = nullptr;
    src->m_ParamType->SaveLoadFunction(src, &stateChunk, FALSE);
    dest->m_ParamType->SaveLoadFunction(dest, &stateChunk, TRUE);
    if (stateChunk) {
        delete stateChunk;
    }
}

void CK_ParameterCopier_SetValue(CKParameter *dest, CKParameter *src) {
    dest->SetValue(src->m_Buffer, src->m_Size);
}

void CK_ParameterCopier_Dword(CKParameter *dest, CKParameter *src) {
    CKDWORD *srcValue = (CKDWORD *)src->m_Buffer;
    CKDWORD *destValue = (CKDWORD *)dest->m_Buffer;
    *destValue = *srcValue;
}

int CKIntStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    if (!param)
        return 0;

    if (ReadFromString) {
        if (!ValueString)
            return 0;

        int intValue = 0;
        if (sscanf(ValueString, "%d", &intValue) == 1) {
            param->SetValue(&intValue, sizeof(intValue));
        }
        return 0;
    } else {
        int paramValue = 0;
        param->GetValue(&paramValue, sizeof(paramValue));

        char buffer[64];
        int written = sprintf(buffer, "%d", paramValue);

        if (ValueString) {
            strcpy(ValueString, buffer);
        }

        return written + 1;
    }
}

int CKEnumStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    if (!param)
        return 0;

    CKContext *context = param->m_Context;
    CKParameterManager *paramManager = context->GetParameterManager();

    CKParameterType paramType = param->GetType();
    CKEnumStruct *enumDesc = paramManager->GetEnumDescByType(paramType);

    // Fallback to integer conversion if not an enum type
    if (!enumDesc) {
        return CKIntStringFunc(param, ValueString, ReadFromString);
    }

    if (ReadFromString) {
        // Parse string to enum value
        if (!ValueString)
            return 0;

        int foundValue = -1;
        for (int i = 0; i < enumDesc->NbData; ++i) {
            if (enumDesc->Desc[i] && strcmp(ValueString, enumDesc->Desc[i]) == 0) {
                foundValue = enumDesc->Vals[i];
                break;
            }
        }

        if (foundValue == -1) {
            // Fallback to integer parsing if no enum match
            return CKIntStringFunc(param, ValueString, ReadFromString);
        }

        param->SetValue(&foundValue, sizeof(foundValue));
        return 1;
    } else {
        // Convert enum value to string
        int currentValue;
        param->GetValue(&currentValue, sizeof(currentValue));

        char buffer[256];
        strcpy(buffer, "Invalid Enum Value");

        // Search for matching value
        for (int i = 0; i < enumDesc->NbData; ++i) {
            if (enumDesc->Vals[i] == currentValue && enumDesc->Desc[i]) {
                strncpy(buffer, enumDesc->Desc[i], sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination
                break;
            }
        }

        if (ValueString) {
            strcpy(ValueString, buffer);
        }
        return strlen(buffer) + 1; // Include null terminator in size
    }
}

int CKFlagsStringFunc(CKParameter *param, CKSTRING ValueString, CKBOOL ReadFromString) {
    if (!param)
        return 0;

    CKContext *context = param->m_Context;
    CKParameterType paramType = param->GetType();
    CKParameterManager *paramManager = context->GetParameterManager();
    CKFlagsStruct *flagsDesc = paramManager->GetFlagsDescByType(paramType);

    // Fallback to integer conversion if not a flags type
    if (!flagsDesc) {
        return CKIntStringFunc(param, ValueString, ReadFromString);
    }

    if (ReadFromString) {
        CKDWORD flagValue = 0;
        const char *current = ValueString;

        while (current && *current) {
            // Find next comma or end of string
            const char *commaPos = strchr(current, ',');
            size_t tokenLen = commaPos ? (commaPos - current) : strlen(current);

            // Extract current token
            char tokenBuffer[256];
            strncpy(tokenBuffer, current, tokenLen);
            tokenBuffer[tokenLen] = '\0';

            // Search for matching flag name
            for (int i = 0; i < flagsDesc->NbData; ++i) {
                if (flagsDesc->Desc[i] &&
                    _stricmp(tokenBuffer, flagsDesc->Desc[i]) == 0) {
                    flagValue |= flagsDesc->Vals[i];
                    break;
                }
            }

            // Move to next token
            current = commaPos ? commaPos + 1 : NULL;
        }

        // Update parameter value
        param->SetValue(&flagValue, sizeof(flagValue));
        return 1;
    } else {
        CKDWORD currentFlags;
        param->GetValue(&currentFlags, sizeof(currentFlags));

        char outputBuffer[1024];
        outputBuffer[0] = '\0';
        int requiredSize = 1; // Start with null terminator

        // Build comma-separated list of flag names
        for (int i = 0; i < flagsDesc->NbData; ++i) {
            if ((currentFlags & flagsDesc->Vals[i]) && flagsDesc->Desc[i]) {
                strcat(outputBuffer, flagsDesc->Desc[i]);
                strcat(outputBuffer, ",");
                requiredSize += strlen(flagsDesc->Desc[i]) + 1;
            }
        }

        // Remove trailing comma if any
        size_t len = strlen(outputBuffer);
        if (len > 0) {
            outputBuffer[len - 1] = '\0';
            requiredSize--;
        }

        // Copy to output if buffer provided
        if (ValueString) {
            strcpy(ValueString, outputBuffer);
        }

        return requiredSize;
    }
}

CK_PARAMETERUICREATORFUNCTION GetUICreatorFunction(CKContext *context, CKParameterTypeDesc *desc) {
    CKInterfaceManager *interfaceManager = (CKInterfaceManager *)context->GetManagerByGuid(INTERFACE_MANAGER_GUID);
    if (interfaceManager) {
        return interfaceManager->GetEditorFunctionForParameterType(desc);
    }
    return NULL;
}

