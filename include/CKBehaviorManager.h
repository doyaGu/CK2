#ifndef CKBEHAVIORMANAGER_H
#define CKBEHAVIORMANAGER_H

#include "CKDefines.h"
#include "CKBaseManager.h"
#include "CKBeObject.h"
#include "CKContext.h"
#include "XSHashTable.h"

typedef XSHashTable<int, CK_ID> BeObjectTable;
typedef BeObjectTable::Iterator BeObjectTableIt;

/************************************************************************
Name: CKBehaviorManager

Summary: Behavior execution management.

Remarks:

+ There is only one instance of the CKBehaviorManager class in a Context.
+ This instance may be obtained with the CKContext::GetBehaviorManager() method.
+ The behavior manager manages the list of object which behaviors need to be executed at each process loop, and manages their execution.
+ The behavior manager is automatically called by the process loop and by the scenes, but direct access is
provided for special behavior processing and direct access to the execution of behaviors.
See Also: CKBehavior, CKBeObject, CKScene
*************************************************************************/
class DLL_EXPORT CKBehaviorManager : public CKBaseManager
{
    friend class CKBehavior;

public:
    //----------------------------------------------
    // Main Process
    CKERROR Execute(float delta);
    CKERROR ExecuteDebugStart(float delta);
    void ManageObjectsActivity();

    //----------------------------------------------
    // Object Management
    int GetObjectsCount();
    CKBeObject *GetObject(int pos);
    CKERROR AddObject(CKBeObject *beo);
    void SortObjects();
    void RemoveAllObjects();

    //-----------------------------------------------
    // Setup
    int GetBehaviorMaxIteration();
    void SetBehaviorMaxIteration(int n);

    int AddObjectNextFrame(CKBeObject *beo);
    int RemoveObjectNextFrame(CKBeObject *beo);

    //-------------------------------------------------------------------------
    // Internal functions
    CKBehaviorManager(CKContext *Context);
    virtual ~CKBehaviorManager();

    virtual CKStateChunk *SaveData(CKFile *SavedFile);
    virtual CKERROR LoadData(CKStateChunk *chunk, CKFile *LoadedFile);
    virtual CKERROR OnCKPlay();
    virtual CKERROR OnCKPause();
    virtual CKERROR PreProcess();
    virtual CKERROR PreClearAll();
    virtual CKERROR SequenceDeleted(CK_ID *objids, int count);
    virtual CKERROR SequenceToBeDeleted(CK_ID *objids, int count);
    virtual CKDWORD GetValidFunctionsMask()
    {
        return CKMANAGER_FUNC_PreClearAll |
               CKMANAGER_FUNC_OnCKPause |
               CKMANAGER_FUNC_PreProcess |
               CKMANAGER_FUNC_OnSequenceDeleted |
               CKMANAGER_FUNC_OnSequenceToBeDeleted |
               CKMANAGER_FUNC_OnCKPlay;
    }

    XObjectPointerArray m_Behaviors;
    XObjectPointerArray m_BeObjects;
    BeObjectTable m_BeObjectNextFrame;
    CKBehavior *m_CurrentBehavior;
    int m_BehaviorMaxIteration;
};

#endif // CKBEHAVIORMANAGER_H
