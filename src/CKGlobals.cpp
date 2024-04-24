#include "CKGlobals.h"

#include <time.h>

#include "VxMath.h"
#include "CKContext.h"
#include "CKPluginManager.h"
#include "CKObject.h"
#include "CKParameterIn.h"
#include "CKParameter.h"
#include "CKParameterOut.h"
#include "CKParameterLocal.h"
#include "CKParameterOperation.h"
#include "CKBehaviorLink.h"
#include "CKSceneObject.h"
#include "CKSynchroObject.h"
#include "CKInterfaceObjectManager.h"
#include "CKBeObject.h"
#include "CKBehavior.h"
#include "CKScene.h"
#include "CKLevel.h"
#include "CKGroup.h"
#include "CKSound.h"
#include "CKDataArray.h"
#include "CKWaveSound.h"
#include "CKMidiSound.h"

extern INSTANCE_HANDLE g_CKModule;

XString g_PluginPath;
XString g_StartPath;
XArray<CKContext*> g_Contextes;
XClassInfoArray g_CKClassInfo;
CKStats g_MainStats;
int g_MaxClassID = 55;
ProcessorsType g_TheProcessor;
CKPluginManager g_ThePluginManager;
VxImageDescEx Default32Desc;

typedef XHashTable<CKObjectDeclaration *, CKGUID> XObjDeclHashTable;
XObjDeclHashTable g_PrototypeDeclarationList;

void CKInitStartPath() {
    char buffer[260];
    if (VxGetModuleFileName(g_CKModule, buffer, 260)) {
        CKPathSplitter ps(buffer);
        g_StartPath = ps.GetDrive();
        g_StartPath << ps.GetDir();
        VxAddLibrarySearchPath(g_StartPath.Str());
        VxMakePath(buffer, g_StartPath.Str(), "Plugins");
        g_StartPath = buffer;
    }
}

CKERROR CKStartUp() {
    CKInitStartPath();

    srand(time(NULL));

    Default32Desc.BitsPerPixel = 32;
    Default32Desc.RedMask = R_MASK;
    Default32Desc.GreenMask = G_MASK;
    Default32Desc.BlueMask = B_MASK;
    Default32Desc.AlphaMask = A_MASK;

    g_CKClassInfo.Reserve(56);

    g_TheProcessor = GetProcessorType();

    g_ThePluginManager.AddCategory("Bitmap Readers");
    g_ThePluginManager.AddCategory("Sound Readers");
    g_ThePluginManager.AddCategory("Model Readers");
    g_ThePluginManager.AddCategory("Managers");
    g_ThePluginManager.AddCategory("BuildingBlocks");
    g_ThePluginManager.AddCategory("Render Engines");
    g_ThePluginManager.AddCategory("Movie Readers");
    g_ThePluginManager.AddCategory("Extensions");

    CKCLASSREGISTER(CKObject, CKObject);
    CKCLASSREGISTER(CKParameterIn, CKObject);
    CKCLASSREGISTER(CKParameter, CKObject);
    CKCLASSREGISTER(CKParameterOut, CKParameter);
    CKCLASSREGISTER(CKParameterLocal, CKParameter);
    CKCLASSREGISTER(CKParameterOperation, CKObject);
    CKCLASSREGISTER(CKBehaviorLink, CKObject);
    CKCLASSREGISTER(CKSceneObject, CKObject);
    CKCLASSREGISTER(CKSynchroObject, CKObject);
    CKCLASSREGISTER(CKStateObject, CKObject);
    CKCLASSREGISTER(CKCriticalSectionObject, CKObject);
    //CKCLASSREGISTER(CKInterfaceObjectManager, CKObject);
    CKCLASSREGISTER(CKBeObject, CKSceneObject);
    CKCLASSREGISTER(CKBehavior, CKSceneObject);
    CKCLASSREGISTER(CKScene, CKBeObject);
    CKCLASSREGISTER(CKLevel, CKBeObject);
    CKCLASSREGISTER(CKGroup, CKBeObject);
    CKCLASSREGISTER(CKSound, CKBeObject);
    CKCLASSREGISTER(CKDataArray, CKBeObject);
    CKCLASSREGISTER(CKWaveSound, CKSound);
    //CKCLASSREGISTER(CKMidiSound, CKSound);
    CKBuildClassHierarchyTable();

    return 0;
}

CKERROR CKShutdown() {
    return 0;
}

CKContext *GetCKContext(int pos) {
    return nullptr;
}

CKObject *CKGetObject(CKContext *iCtx, CK_ID iID) {
    return nullptr;
}

CKERROR CKCreateContext(CKContext **iContext, WIN_HANDLE iWin, int iRenderEngine, CKDWORD Flags) {
    if (iRenderEngine < 0 || iRenderEngine >= g_ThePluginManager.GetPluginCount(CKPLUGIN_RENDERENGINE_DLL))
        iRenderEngine = 0;

    auto* ctx = new CKContext(iWin, iRenderEngine, Flags);
    g_Contextes.PushBack(ctx);
    g_ThePluginManager.InitializePlugins(ctx);
    memset(&g_MainStats, 0, sizeof(g_MainStats));
    ctx->m_InitManagerOnRegister = TRUE;
    ctx->ExecuteManagersOnCKInit();
    *iContext = ctx;
    return CK_OK;
}

CKERROR CKCloseContext(CKContext *) {
    return 0;
}

CKSTRING CKGetStartPath() {
    return nullptr;
}

CKSTRING CKGetPluginsPath() {
    return nullptr;
}

void CKDestroyObject(CKObject *o, CKDWORD Flags, CKDependencies *dep) {

}

CKDWORD CKGetVersion() {
    return 0;
}

void CKBuildClassHierarchyTable() {

}

CKPluginManager *CKGetPluginManager() {
    return &g_ThePluginManager;
}

int CKGetPrototypeDeclarationCount() {
    return g_PrototypeDeclarationList.Size();
}

CKObjectDeclaration *CKGetPrototypeDeclaration(int n) {
    return nullptr;
}

XObjDeclHashTableIt CKGetPrototypeDeclarationStartIterator() {
    return XObjDeclHashTableIt();
}

XObjDeclHashTableIt CKGetPrototypeDeclarationEndIterator() {
    return XObjDeclHashTableIt();
}

CKObjectDeclaration *CKGetObjectDeclarationFromGuid(CKGUID guid) {
    return nullptr;
}

CKBehaviorPrototype *CKGetPrototypeFromGuid(CKGUID guid) {
    return nullptr;
}

CKERROR CKRemovePrototypeDeclaration(CKObjectDeclaration *objdecl) {
    return 0;
}

CKObjectDeclaration *CreateCKObjectDeclaration(CKSTRING Name) {
    return nullptr;
}

CKBehaviorPrototype *CreateCKBehaviorPrototype(CKSTRING Name) {
    return nullptr;
}

CKBehaviorPrototype *CreateCKBehaviorPrototypeRunTime(CKSTRING Name) {
    return nullptr;
}

int CKGetClassCount() {
    return 0;
}

CKClassDesc *CKGetClassDesc(CK_CLASSID cid) {
    return nullptr;
}

CKSTRING CKClassIDToString(CK_CLASSID cid) {
    return nullptr;
}

CK_CLASSID CKStringToClassID(CKSTRING classname) {
    return 0;
}

CKBOOL CKIsChildClassOf(CK_CLASSID child, CK_CLASSID parent) {
    return 0;
}

CKBOOL CKIsChildClassOf(CKObject *obj, CK_CLASSID parent) {
    return 0;
}

CK_CLASSID CKGetParentClassID(CK_CLASSID child) {
    return 0;
}

CK_CLASSID CKGetParentClassID(CKObject *obj) {
    return 0;
}

CK_CLASSID CKGetCommonParent(CK_CLASSID cid1, CK_CLASSID cid2) {
    return 0;
}

CKObjectArray *CreateCKObjectArray() {
    return nullptr;
}

void DeleteCKObjectArray(CKObjectArray *obj) {

}

CKStateChunk *CKSaveObjectState(CKObject *obj, CKDWORD Flags) {
    return nullptr;
}

CKERROR CKReadObjectState(CKObject *obj, CKStateChunk *chunk) {
    return 0;
}

BITMAP_HANDLE CKLoadBitmap(CKSTRING filename) {
    return nullptr;
}

CKBOOL CKSaveBitmap(CKSTRING filename, BITMAP_HANDLE bm) {
    return 0;
}

CKBOOL CKSaveBitmap(CKSTRING filename, VxImageDescEx &desc) {
    return 0;
}

void CKConvertEndianArray32(void *buf, int DwordCount) {

}

void CKConvertEndianArray16(void *buf, int DwordCount) {

}

CKDWORD CKConvertEndian32(CKDWORD dw) {
    return dw;
}

CKWORD CKConvertEndian16(CKWORD w) {
    return w;
}

CKDWORD CKComputeDataCRC(char *data, int size, CKDWORD PreviousCRC) {
    return 0;
}

char *CKPackData(char *Data, int size, int &NewSize, int compressionlevel) {
    return nullptr;
}

char *CKUnPackData(int DestSize, char *SrcBuffer, int SrcSize) {
    return nullptr;
}

CKSTRING CKStrdup(CKSTRING string) {
    if (!string)
        return NULL;

    size_t len = strlen(string);
    CKSTRING str = new char[len + 1];
    strcpy(str, string);
    return str;
}

CKSTRING CKStrupr(CKSTRING string) {
    if (!string)
        return NULL;

    for (char *pch = string; *pch; ++pch) {
        char ch = *pch;
        if (ch >= 'a' && ch <= 'z')
            *pch = ch - 0x20;
    }
    return string;
}

CKSTRING CKStrlwr(CKSTRING string) {
    if (!string)
        return NULL;

    for (char *pch = string; *pch; ++pch) {
        char ch = *pch;
        if (ch >= 'A' && ch <= 'Z')
            *pch = ch + 0x20;
    }
    return string;
}

CKBitmapProperties *CKCopyBitmapProperties(CKBitmapProperties *bp) {
    if (!bp)
        return NULL;
    if (bp->m_Size <= 0 || bp->m_Size >= 256)
        return NULL;
    CKBitmapProperties* nbp = nullptr;// (CKBitmapProperties*)VxNew(bp->m_Size);
    memcpy(nbp, bp, bp->m_Size);
    return nbp;
}

void CKCopyDefaultClassDependencies(CKDependencies &d, CK_DEPENDENCIES_OPMODE mode) {

}

CKDependencies *CKGetDefaultClassDependencies(CK_DEPENDENCIES_OPMODE mode) {
    return nullptr;
}

void CKDeletePointer(void *ptr) {
    //VxDelete(ptr);
}

CKERROR CKCopyAllAttributes(CKBeObject *Src, CKBeObject *Dest) {
    return 0;
}

CKERROR CKMoveAllScripts(CKBeObject *Src, CKBeObject *Dest) {
    return 0;
}

CKERROR CKMoveScript(CKBeObject *Src, CKBeObject *Dest, CKBehavior *Beh) {
    return 0;
}

void CKRemapObjectParameterValue(CKContext *ckContext, CK_ID oldID, CK_ID newID, CK_CLASSID cid, CKBOOL derived) {
    const XObjectPointerArray &params = ckContext->GetObjectListByType(CKCID_PARAMETEROUT, TRUE);
    for (XObjectPointerArray::Iterator it = params.Begin(); it != params.End(); ++it)
    {
        CKParameter *param = (CKParameter *)*it;
        CK_CLASSID pcid = param->GetParameterClassID();
        if (pcid == cid || (derived && CKIsChildClassOf(pcid, cid)))
        {
            if (*(CK_ID *)param->GetReadDataPtr(TRUE) == oldID)
                param->SetValue(&newID);
        }
    }
}

void CKStoreDeclaration(XObjectDeclarationArray *reg, CKObjectDeclaration *a) {
    reg->PushBack(a);
}

void CKClassNeedNotificationFrom(CK_CLASSID Cid1, CK_CLASSID Cid2) {
    g_CKClassInfo[Cid1].ToBeNotify.Or(g_CKClassInfo[Cid2].Children);
}

void CKClassRegisterAssociatedParameter(CK_CLASSID Cid, CKGUID pguid) {
    g_CKClassInfo[Cid].Parameter = pguid;
}

void CKClassRegisterDefaultDependencies(CK_CLASSID Cid, CKDWORD depend_Mask, int mode) {
    switch (mode) {
        case CK_DEPENDENCIES_COPY:
            g_CKClassInfo[Cid].DefaultCopyDependencies = depend_Mask;
            break;
        case CK_DEPENDENCIES_DELETE:
            g_CKClassInfo[Cid].DefaultDeleteDependencies = depend_Mask;
            break;
        case CK_DEPENDENCIES_REPLACE:
            g_CKClassInfo[Cid].DefaultReplaceDependencies = depend_Mask;
            break;
        case CK_DEPENDENCIES_SAVE:
            g_CKClassInfo[Cid].DefaultSaveDependencies = depend_Mask;
            break;
        default:
            break;
    }
}

void CKClassRegisterDefaultOptions(CK_CLASSID Cid, CKDWORD options_Mask) {
    g_CKClassInfo[Cid].DefaultOptions = options_Mask;
}

CK_CLASSID CKClassGetNewIdentifier() {
    CK_CLASSID cid = g_CKClassInfo.Size();
    if (cid < 55)
        cid = 55;
    if (cid >= g_MaxClassID)
        g_MaxClassID = cid + 1;
    return cid;
}

void CKClassRegister(CK_CLASSID Cid, CK_CLASSID Parent_Cid, CKCLASSREGISTERFCT registerfct, CKCLASSCREATIONFCT creafct,
                     CKCLASSNAMEFCT NameFct, CKCLASSDEPENDENCIESFCT DependsFct,
                     CKCLASSDEPENDENCIESCOUNTFCT DependsCountFct) {

}
