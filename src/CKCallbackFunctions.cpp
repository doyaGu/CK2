#include "CKCallbackFunctions.h"

#include "VxMath.h"
#include "CKStateChunk.h"
#include "CKParameterManager.h"
#include "CKInterfaceManager.h"

void CKInitializeParameterTypes(CKParameterManager *man) {
    CKParameterTypeDesc noneTypeDesc;
    noneTypeDesc.Guid = CKPGUID_NONE;
    noneTypeDesc.TypeName = "None";
    noneTypeDesc.Valid = TRUE;
    man->RegisterParameterType(&noneTypeDesc);

    CKParameterTypeDesc pointerTypeDesc;
    pointerTypeDesc.Guid = CKPGUID_POINTER;
    pointerTypeDesc.TypeName = "Pointer";
    pointerTypeDesc.Valid = TRUE;
    pointerTypeDesc.DefaultSize = sizeof(void *);
    pointerTypeDesc.CopyFunction = CK_ParameterCopier_Nope;
    pointerTypeDesc.SaveLoadFunction = CKPointerSaver;
    pointerTypeDesc.dwFlags = CKPARAMETERTYPE_HIDDEN;
    man->RegisterParameterType(&pointerTypeDesc);

    CKParameterTypeDesc bufferTypeDesc;
    bufferTypeDesc.Guid = CKPGUID_VOIDBUF;
    bufferTypeDesc.TypeName = "Buffer";
    bufferTypeDesc.Valid = TRUE;
    bufferTypeDesc.DefaultSize = 10;
    bufferTypeDesc.CopyFunction = CK_ParameterCopier_SetValue;
    bufferTypeDesc.dwFlags = CKPARAMETERTYPE_VARIABLESIZE | CKPARAMETERTYPE_HIDDEN | CKPARAMETERTYPE_NOENDIANCONV;
    man->RegisterParameterType(&bufferTypeDesc);

    CKParameterTypeDesc copyDependenciesTypeDesc;
    copyDependenciesTypeDesc.Guid = CKPGUID_COPYDEPENDENCIES;
    copyDependenciesTypeDesc.TypeName = "Copy Dependencies";
    copyDependenciesTypeDesc.Valid = TRUE;
    copyDependenciesTypeDesc.DefaultSize = sizeof(CKDWORD);
    copyDependenciesTypeDesc.CreateDefaultFunction = CKDependenciesCreator;
    copyDependenciesTypeDesc.DeleteFunction = CKDependenciesDestructor;
    copyDependenciesTypeDesc.SaveLoadFunction = CKDependenciesSaver;
    copyDependenciesTypeDesc.CopyFunction = CKDependenciesCopier;
    man->RegisterParameterType(&copyDependenciesTypeDesc);

    CKParameterTypeDesc deleteDependenciesTypeDesc = copyDependenciesTypeDesc;
    deleteDependenciesTypeDesc.Guid = CKPGUID_DELETEDEPENDENCIES;
    deleteDependenciesTypeDesc.TypeName = "Delete Dependencies";
    man->RegisterParameterType(&deleteDependenciesTypeDesc);

    CKParameterTypeDesc replaceDependenciesTypeDesc = copyDependenciesTypeDesc;
    replaceDependenciesTypeDesc.Guid = CKPGUID_REPLACEDEPENDENCIES;
    replaceDependenciesTypeDesc.TypeName = "Replace Dependencies";
    man->RegisterParameterType(&replaceDependenciesTypeDesc);

    CKParameterTypeDesc saveDependenciesTypeDesc = copyDependenciesTypeDesc;
    saveDependenciesTypeDesc.Guid = CKPGUID_SAVEDEPENDENCIES;
    saveDependenciesTypeDesc.TypeName = "Save Dependencies";
    man->RegisterParameterType(&saveDependenciesTypeDesc);

    CKParameterTypeDesc stateChunkTypeDesc;
    stateChunkTypeDesc.Guid = CKPGUID_STATECHUNK;
    stateChunkTypeDesc.TypeName = "State Chunk";
    stateChunkTypeDesc.Valid = TRUE;
    stateChunkTypeDesc.DefaultSize = sizeof(CKStateChunk *);
    stateChunkTypeDesc.CreateDefaultFunction = CKStateChunkCreator;
    stateChunkTypeDesc.DeleteFunction = CKStateChunkDestructor;
    stateChunkTypeDesc.SaveLoadFunction = CKStateChunkSaver;
    stateChunkTypeDesc.CopyFunction = CKStateChunkCopier;
    man->RegisterParameterType(&stateChunkTypeDesc);

    CKParameterTypeDesc floatTypeDesc;
    floatTypeDesc.Guid = CKPGUID_FLOAT;
    floatTypeDesc.TypeName = "Float";
    floatTypeDesc.Valid = TRUE;
    floatTypeDesc.DefaultSize = sizeof(float);
    floatTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    floatTypeDesc.StringFunction = CKFloatStringFunc;
    man->RegisterParameterType(&floatTypeDesc);

    CKParameterTypeDesc percentageTypeDesc;
    percentageTypeDesc.Guid = CKPGUID_PERCENTAGE;
    percentageTypeDesc.DerivedFrom = CKPGUID_FLOAT;
    percentageTypeDesc.TypeName = "Percentage";
    percentageTypeDesc.Valid = TRUE;
    percentageTypeDesc.DefaultSize = sizeof(float);
    percentageTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    percentageTypeDesc.StringFunction = CKPercentageStringFunc;
    man->RegisterParameterType(&percentageTypeDesc);

    CKParameterTypeDesc angleTypeDesc;
    angleTypeDesc.Guid = CKPGUID_ANGLE;
    angleTypeDesc.DerivedFrom = CKPGUID_FLOAT;
    angleTypeDesc.TypeName = "Angle";
    angleTypeDesc.Valid = TRUE;
    angleTypeDesc.DefaultSize = sizeof(float);
    angleTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    angleTypeDesc.StringFunction = CKAngleStringFunc;
    man->RegisterParameterType(&angleTypeDesc);

    CKParameterTypeDesc intTypeDesc;
    intTypeDesc.Guid = CKPGUID_INT;
    intTypeDesc.TypeName = "Integer";
    intTypeDesc.Valid = TRUE;
    intTypeDesc.DefaultSize = sizeof(int);
    intTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    intTypeDesc.StringFunction = CKIntStringFunc;
    man->RegisterParameterType(&intTypeDesc);

    CKParameterTypeDesc timeTypeDesc;
    timeTypeDesc.Guid = CKPGUID_TIME;
    timeTypeDesc.DerivedFrom = CKPGUID_FLOAT;
    timeTypeDesc.TypeName = "Time";
    timeTypeDesc.Valid = TRUE;
    timeTypeDesc.DefaultSize = sizeof(float);
    timeTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    timeTypeDesc.StringFunction = CKTimeStringFunc;
    man->RegisterParameterType(&timeTypeDesc);

    CKParameterTypeDesc boolTypeDesc;
    boolTypeDesc.Guid = CKPGUID_BOOL;
    boolTypeDesc.DerivedFrom = CKPGUID_INT;
    boolTypeDesc.TypeName = "Boolean";
    boolTypeDesc.Valid = TRUE;
    boolTypeDesc.DefaultSize = sizeof(int);
    boolTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    boolTypeDesc.StringFunction = CKBoolStringFunc;
    man->RegisterParameterType(&boolTypeDesc);

    CKParameterTypeDesc stringTypeDesc;
    stringTypeDesc.Guid = CKPGUID_STRING;
    stringTypeDesc.TypeName = "String";
    stringTypeDesc.Valid = TRUE;
    stringTypeDesc.DefaultSize = 64;
    stringTypeDesc.CopyFunction = CK_ParameterCopier_SetValue;
    stringTypeDesc.StringFunction = CKStringStringFunc;
    stringTypeDesc.dwFlags = CKPARAMETERTYPE_VARIABLESIZE | CKPARAMETERTYPE_NOENDIANCONV;
    man->RegisterParameterType(&stringTypeDesc);

    CKParameterTypeDesc vectorTypeDesc;
    vectorTypeDesc.Guid = CKPGUID_VECTOR;
    vectorTypeDesc.TypeName = "Vector";
    vectorTypeDesc.Valid = TRUE;
    vectorTypeDesc.DefaultSize = sizeof(VxVector);
    vectorTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    vectorTypeDesc.StringFunction = CKVectorStringFunc;
    man->RegisterParameterType(&vectorTypeDesc);

    CKParameterTypeDesc quaternionTypeDesc;
    quaternionTypeDesc.Guid = CKPGUID_QUATERNION;
    quaternionTypeDesc.TypeName = "Quaternion";
    quaternionTypeDesc.Valid = TRUE;
    quaternionTypeDesc.DefaultSize = sizeof(VxQuaternion);
    quaternionTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    quaternionTypeDesc.StringFunction = CKQuaternionStringFunc;
    man->RegisterParameterType(&quaternionTypeDesc);

    CKParameterTypeDesc eulerAnglesTypeDesc;
    eulerAnglesTypeDesc.Guid = CKPGUID_EULERANGLES;
    eulerAnglesTypeDesc.DerivedFrom = CKPGUID_VECTOR;
    eulerAnglesTypeDesc.TypeName = "Euler Angles";
    eulerAnglesTypeDesc.Valid = TRUE;
    eulerAnglesTypeDesc.DefaultSize = sizeof(VxVector);
    eulerAnglesTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    eulerAnglesTypeDesc.StringFunction = CKEulerStringFunc;
    man->RegisterParameterType(&eulerAnglesTypeDesc);

    CKParameterTypeDesc rectTypeDesc;
    rectTypeDesc.Guid = CKPGUID_RECT;
    rectTypeDesc.TypeName = "Rectangle";
    rectTypeDesc.Valid = TRUE;
    rectTypeDesc.DefaultSize = sizeof(VxRect);
    rectTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    rectTypeDesc.StringFunction = CKRectStringFunc;
    man->RegisterParameterType(&rectTypeDesc);

    CKParameterTypeDesc vector2DTypeDesc;
    vector2DTypeDesc.Guid = CKPGUID_2DVECTOR;
    vector2DTypeDesc.TypeName = "Vector 2D";
    vector2DTypeDesc.Valid = TRUE;
    vector2DTypeDesc.DefaultSize = sizeof(Vx2DVector);
    vector2DTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    vector2DTypeDesc.StringFunction = CK2DVectorStringFunc;
    man->RegisterParameterType(&vector2DTypeDesc);

    CKParameterTypeDesc matrixTypeDesc;
    matrixTypeDesc.Guid = CKPGUID_MATRIX;
    matrixTypeDesc.TypeName = "Matrix";
    matrixTypeDesc.Valid = TRUE;
    matrixTypeDesc.DefaultSize = sizeof(VxMatrix);
    matrixTypeDesc.CreateDefaultFunction = CKMatrixCreator;
    matrixTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    matrixTypeDesc.StringFunction = CKMatrixStringFunc;
    man->RegisterParameterType(&matrixTypeDesc);

    CKParameterTypeDesc colorTypeDesc;
    colorTypeDesc.Guid = CKPGUID_COLOR;
    colorTypeDesc.TypeName = "Color";
    colorTypeDesc.Valid = TRUE;
    colorTypeDesc.DefaultSize = sizeof(VxColor);
    colorTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    colorTypeDesc.StringFunction = CKColorStringFunc;
    man->RegisterParameterType(&colorTypeDesc);

    CKParameterTypeDesc boxTypeDesc;
    boxTypeDesc.Guid = CKPGUID_BOX;
    boxTypeDesc.TypeName = "Box";
    boxTypeDesc.Valid = TRUE;
    boxTypeDesc.DefaultSize = sizeof(VxBbox);
    boxTypeDesc.CopyFunction = CK_ParameterCopier_Memcpy;
    boxTypeDesc.StringFunction = CKBoxStringFunc;
    man->RegisterParameterType(&boxTypeDesc);

    CKParameterTypeDesc collectionTypeDesc;
    collectionTypeDesc.Guid = CKPGUID_OBJECTARRAY;
    collectionTypeDesc.TypeName = "Collection";
    collectionTypeDesc.Valid = TRUE;
    collectionTypeDesc.DefaultSize = sizeof(CKDWORD);
    collectionTypeDesc.CreateDefaultFunction = CKCollectionCreator;
    collectionTypeDesc.DeleteFunction = CKCollectionDestructor;
    collectionTypeDesc.SaveLoadFunction = CKCollectionSaver;
    collectionTypeDesc.CheckFunction = CKCollectionCheck;
    collectionTypeDesc.CopyFunction = CKCollectionCopy;
    collectionTypeDesc.StringFunction = CKCollectionStringFunc;
    man->RegisterParameterType(&collectionTypeDesc);

    CKParameterTypeDesc messageTypeDesc;
    messageTypeDesc.Guid = CKPGUID_MESSAGE;
    messageTypeDesc.TypeName = "Message";
    messageTypeDesc.Valid = TRUE;
    messageTypeDesc.DefaultSize = sizeof(CKDWORD);
    messageTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    messageTypeDesc.StringFunction = CKMessageStringFunc;
    messageTypeDesc.Saver_Manager = MESSAGE_MANAGER_GUID;
    man->RegisterParameterType(&messageTypeDesc);

    CKParameterTypeDesc attributeTypeDesc;
    attributeTypeDesc.Guid = CKPGUID_ATTRIBUTE;
    attributeTypeDesc.TypeName = "Attribute";
    attributeTypeDesc.Valid = TRUE;
    attributeTypeDesc.DefaultSize = sizeof(CKDWORD);
    attributeTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    attributeTypeDesc.StringFunction = CKAttributeStringFunc;
    attributeTypeDesc.Saver_Manager = ATTRIBUTE_MANAGER_GUID;
    man->RegisterParameterType(&attributeTypeDesc);

    CKParameterTypeDesc curve2DTypeDesc;
    curve2DTypeDesc.Guid = CKPGUID_2DCURVE;
    curve2DTypeDesc.TypeName = "2D Curve";
    curve2DTypeDesc.Valid = TRUE;
    curve2DTypeDesc.DefaultSize = sizeof(CKDWORD);
    curve2DTypeDesc.CreateDefaultFunction = CK2dCurveCreator;
    curve2DTypeDesc.DeleteFunction = CK2dCurveDestructor;
    curve2DTypeDesc.SaveLoadFunction = CK2dCurveSaver;
    curve2DTypeDesc.CopyFunction = CK2dCurveCopier;
    man->RegisterParameterType(&curve2DTypeDesc);

    CKParameterTypeDesc paramTypeDesc;
    paramTypeDesc.Valid = TRUE;
    paramTypeDesc.DefaultSize = sizeof(CKDWORD);
    paramTypeDesc.CheckFunction = CKObjectCheckFunc;
    paramTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    paramTypeDesc.StringFunction = CKObjectStringFunc;
    man->RegisterParameterType(&paramTypeDesc);

    CKParameterTypeDesc scriptTypeDesc;
    scriptTypeDesc.Guid = CKPGUID_SCRIPT;
    scriptTypeDesc.DerivedFrom = CKPGUID_OBJECT;
    scriptTypeDesc.TypeName = "Script";
    scriptTypeDesc.Valid = TRUE;
    scriptTypeDesc.CopyFunction = CK_ParameterCopier_Dword;
    scriptTypeDesc.Cid = CKCID_BEHAVIOR;
    man->RegisterParameterType(&scriptTypeDesc);

    man->RegisterNewEnum(CKPGUID_DIRECTION, "Direction", "X=1,-X=2,Y=3,-Y=4,Z=5,-Z=6");
    man->RegisterNewEnum(CKPGUID_BLENDMODE, "Texture Blend Mode", "Decal=1,Modulate=2,DecalAlpha=3,ModulateAlpha=4,DecalMask=5,ModulateMask=6,Copy=7,Add=8");
    man->RegisterNewEnum(CKPGUID_FILTERMODE, "Filter Mode", "Nearest=1,Linear=2,MipNearest=3,MipLinear=4,LinearMipNearest=5,LinearMipLinear=6,Anisotropic=7");
    man->RegisterNewEnum(CKPGUID_BLENDFACTOR, "Blend Factor", "Zero=1,One=2,Source Color=3,Inverse Source Color=4,Source Alpha=5,Inverse Source Alpha=6,Destination Alpha=7,Inverse Destination Alpha=8,Destination Color=9,Inverse Destination Color=10,Source Alpha Saturation=11,Both Source Alpha=12,Both Inverse Source Alpha=13");
    man->RegisterNewEnum(CKPGUID_FILLMODE, "Fill Mode", "Point=1,WireFrame=2,Solid=3,OnlyZ=4");
    man->RegisterNewEnum(CKPGUID_LITMODE, "Lit Mode", "Prelit=0,Lit=1");
    man->RegisterNewEnum(CKPGUID_SHADEMODE, "Shade Mode", "Flat=1,Gouraud=2");
    man->RegisterNewEnum(CKPGUID_GLOBALEXMODE, "Global Shade Mode", "Wireframe=0,Flat=1,Gouraud=2,Material Default=4");
    man->RegisterNewEnum(CKPGUID_ZFUNC, "Z Compare Function", "Never=1,Less=2,Equal=3,Less Equal=4,Greater=5,Not Equal=6,Greater Equal=7,Always=8");
    man->RegisterNewEnum(CKPGUID_ADDRESSMODE, "Address Mode", "Wrap=1,Mirror=2,Clamp=3,Border=4");
    man->RegisterNewEnum(CKPGUID_WRAPMODE, "Wrap Mode", "None=0,V Looping=1,U Looping=2,UV Looping=3");
    man->RegisterNewEnum(CKPGUID_3DSPRITEMODE, "3DSprite Mode", "Billboard=0,XRotate=1,YRotate=2,Orientable=3");
    man->RegisterNewEnum(CKPGUID_FOGMODE, "Fog Mode", "None=0,Exponential=1,Exponential Squared=2,Linear=3");
    man->RegisterNewEnum(CKPGUID_LIGHTTYPE, "Light Type", "Point=1,Spot=2,Directional=3");
    man->RegisterNewEnum(CKPGUID_COMPOPERATOR, "Comparison Operator", "Equal=1,Not Equal=2,Less than=3,Less or equal=4,Greater than=5,Greater or equal=6");
    man->RegisterNewEnum(CKPGUID_BINARYOPERATOR, "Binary Operator", "+=1,-=2,*=3,/=4");
    man->RegisterNewEnum(CKPGUID_SETOPERATOR, "Sets Operator", "Union=1,Intersection=2,Subtraction=3");
    man->RegisterNewEnum(CKPGUID_SPRITETEXTALIGNMENT, "Sprite Text Alignment", "Center=1,Left=2,Right=4,Top=8,Bottom=16,VCenter=32,HCenter=64");
    man->RegisterNewEnum(CKPGUID_ARRAYTYPE, "Array Column Type", "Integer=1,Float=2,String=3,Object=4,Parameter=5");
    man->RegisterNewFlags(CKPGUID_FILTER, "Filter", "1=1,2=2,3=4,4=8,5=16,6=32,7=64,8=128,9=256,10=512,11=1024,12=2048,13=4096,14=8192,15=16384,16=32768,17=65536,18=131072,19=262144,20=524288,21=1048576,22=2097152,23=4194304,24=8388608,25=16777216,26=33554432,27=67108864,28=134217728,29=268435456,30=536870912,31=1073741824,32=2147483648");
    man->RegisterNewEnum(CKPGUID_SCENEACTIVITYFLAGS, "Scene Activity Options", "Scene Defaults=0,Force activate=1,Force deactivate=2,Do nothing=3");
    man->RegisterNewEnum(CKPGUID_SCENERESETFLAGS, "Scene Reset Options", "Scene Defaults=0,Force Reset=1,Do nothing=2");
    man->RegisterNewFlags(CKPGUID_RENDEROPTIONS, "Render Options", "Background Sprites=1,Foreground Sprites=2,Use Camera Ratio=8,Clear ZBuffer=16,Clear Back Buffer=32,Clear Stencil Buffer=64,Buffer Swapping=128,Clear Only Viewport=256,Vertical Sync=512,Disable 3D=1024");


    XString enumData;
    const int classCount = CKGetClassCount();
    XClassArray<XString> classNames(classCount);

    for (CK_CLASSID classId = CKCID_OBJECT; classId < classCount; ++classId) {
        CKClassDesc* classDesc = CKGetClassDesc(classId);
        if (!classDesc || !classDesc->NameFct) continue;

        // Get class name
        XString className(classDesc->NameFct());

        if (classDesc->Parameter != CKGUID(0, 0)) {
            CKParameterTypeDesc paramDesc;
            paramDesc.Cid = classId;
            paramDesc.TypeName = className;
            paramDesc.Guid = classDesc->Parameter;
            paramDesc.DerivedFrom = CKGUID(0, 0);
            CKClassDesc *parentDesc = classDesc;
            while (parentDesc->Parent != 0) {
                parentDesc = CKGetClassDesc(parentDesc->Parent);
                if (parentDesc && (parentDesc->Parameter != CKGUID(0, 0))) {
                    paramDesc.DerivedFrom = parentDesc->Parameter;
                    break;
                }
            }

            man->RegisterParameterType(&paramDesc);
        }

        className << " (" << (unsigned int)classId << ")";
        classNames.PushBack(className);
    }

    classNames.Sort();

    for (int i = 0; i < classNames.Size(); ++i) {
        enumData << classNames[i];
        if (i < classNames.Size() - 1) {
            enumData << ",";
        }
    }

    if (enumData.Length() == 0) {
        enumData = "";
    }

    man->RegisterNewEnum(CKPGUID_CLASSID, "Registered Class", enumData.Str());
    man->RegisterNewEnum(CKPGUID_PARAMETERTYPE, "Registered Parameter", "NA=0");
    CKParameterTypeDesc *desc = man->GetParameterTypeDescription(CKPGUID_PARAMETERTYPE);
    if (desc) {
        desc->CreateDefaultFunction = CKParameterTypesEnumCreator;
    }
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

void CK_ParameterCopier_Memcpy(CKParameter *dest, CKParameter *src) {
    memcpy(dest->m_Buffer, src->m_Buffer, dest->m_Size);
}

void CK_ParameterCopier_BufferCompatibleWithDWORD(CKParameter *dest, CKParameter *src) {
    if (src->GetDataSize() == sizeof(CKDWORD)) {
        dest->SetValue(src->GetReadDataPtr(), sizeof(CKDWORD));
    } else {
        memcpy(dest->GetWriteDataPtr(), src->GetReadDataPtr(), src->GetDataSize());
    }
}

void CK_ParameterCopier_Nope(CKParameter *dest, CKParameter *src) {
    // Do nothing
}

CKERROR CKParameterTypesEnumCreator(CKParameter *param) {
    CKParameterManager* pm = param->m_Context->GetParameterManager();
    if (!pm->m_ParameterTypeEnumUpToDate) {
        pm->UpdateParameterEnum();
    }

    int valueToSet = 23;
    param->SetValue(&valueToSet);
    return CK_OK;
}

CKERROR CKDependenciesCreator(CKParameter *param) {
    return CK_OK;
}

void CKDependenciesDestructor(CKParameter *param) {
}

void CKDependenciesSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
}

void CKDependenciesCopier(CKParameter *dest, CKParameter *src) {
}

CKERROR CKStateChunkCreator(CKParameter *param) {
    return CK_OK;
}

void CKStateChunkDestructor(CKParameter *param) {
}

void CKStateChunkSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
}

void CKStateChunkCopier(CKParameter *dest, CKParameter *src) {
}

CKERROR CKCollectionCreator(CKParameter *param) {
    return CK_OK;
}

void CKCollectionDestructor(CKParameter *param) {
}

void CKCollectionSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
}

void CKCollectionCheck(CKParameter *param) {
}

void CKCollectionCopy(CKParameter *dest, CKParameter *src) {
}

CKERROR CKMatrixCreator(CKParameter *param) {
    return CK_OK;
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

CKERROR CK2dCurveCreator(CKParameter *param) {
    return CK_OK;
}

void CK2dCurveDestructor(CKParameter *param) {
}

void CK2dCurveCopier(CKParameter *dest, CKParameter *src) {
}

void CK2dCurveSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
}

void CKPointerSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load) {
}

void CKObjectCheckFunc(CKParameter *param) {
}

int CKMatrixStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKQuaternionStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKObjectStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKStringStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKAttributeStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKMessageStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKCollectionStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
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

int CKAngleStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKEulerStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CK2DVectorStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKVectorStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKRectStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKBoxStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKColorStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKBoolStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKFloatStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKPercentageStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

int CKTimeStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString) {
    return 0;
}

CK_PARAMETERUICREATORFUNCTION GetUICreatorFunction(CKContext *context, CKParameterTypeDesc *desc) {
    CKInterfaceManager *interfaceManager = (CKInterfaceManager *)context->GetManagerByGuid(INTERFACE_MANAGER_GUID);
    if (interfaceManager) {
        return interfaceManager->GetEditorFunctionForParameterType(desc);
    }
    return NULL;
}

