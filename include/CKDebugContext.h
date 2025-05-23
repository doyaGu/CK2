#ifndef CKDEBUGCONTEXT_H
#define CKDEBUGCONTEXT_H

#include "CKObjectArray.h"

typedef enum CKDEBUG_STATE
{
    CKDEBUG_NOP               = 0x00000000,
    CKDEBUG_BEHEXECUTE        = 0x00000001,
    CKDEBUG_BEHEXECUTEDONE    = 0x00000002,
    CKDEBUG_SCRIPTEXECUTEDONE = 0x00000004,
} CKDEBUG_STATE;

class CKDebugContext
{
public:
    float delta;

    CKBeObject *CurrentObject;
    CKBehavior *CurrentScript;
    CKBehavior *CurrentBehavior;
    CKBehavior *SubBehavior;
    CKObjectArray ObjectsToExecute;
    CKObjectArray ScriptsToExecute;
    CKObjectArray BehaviorStack;
    CKContext *m_Context;

    // Behavior Part
    CKDEBUG_STATE CurrentBehaviorAction;
    CKBOOL InDebug;

    DLL_EXPORT void Init(XObjectPointerArray &array, float delta);
    DLL_EXPORT void StepInto(CKBehavior *beh);
    DLL_EXPORT void StepBehavior();
    DLL_EXPORT CKBOOL DebugStep();
    DLL_EXPORT void Clear();

    CKDebugContext(CKContext *context)
    {
        m_Context = context;
        delta = 0;
        CurrentObject = NULL;
        CurrentScript = NULL;
        CurrentBehavior = NULL;
        SubBehavior = NULL;

        InDebug = 0;
        CurrentBehaviorAction = CKDEBUG_NOP;
    }
};

#endif // CKDEBUGCONTEXT_H