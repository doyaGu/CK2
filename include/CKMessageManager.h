#ifndef CKMESSAGEMANAGER_H
#define CKMESSAGEMANAGER_H

#include "CKDefines.h"
#include "CKTypes.h"
#include "CKMessage.h"
#include "CKBaseManager.h"
#include "XClassArray.h"
#include "XObjectArray.h"

struct CKMessageWaitingList;

struct CKMessageDesc;

struct CKWaitingObject {
    CKBeObject *m_BeObject;
    CKBehavior *m_Behavior;
    CKBehaviorIO *m_Output;
};

typedef XArray<CKWaitingObject> CKWaitingObjectArray;

typedef XClassArray<XString> CKStringArray;

/****************************************************************************
Name: CKMessageManager

Summary: Messages management

Remarks:

+ There is only one instance of the message manager per application. This instance receives
and dispatches the messages to the objects or the behaviors that have requested it. It also
manages a list of message types.

+ Behaviors that wish to wait for a message should register themselves using
the RegisterWait method specifying the input to activate when the message will be received.
They will then receive the messages sent with the SendMessage method. After the first message
received, they will be automatically unregistered. They can also unregister themselves
at any time using the unregisterWait methods.

+ Another way to proceed is to specify that an object accept all messages using CKBeObject::SetAsWaitingForMessages.
This way all messages sent to an object will be stored for one frame. Behaviors may then
ask the object about how many and which messages did he received last frame.

+ Messages have a type. The list of accepted types is maintained by the message manager. If an object
or a behavior registers itself as waiting for a new type of message, this type is automatically added
to the list of types. One can also add a new type manually using the AddMessageType method. The list of types
can be accessed with the GetMessageTypeCount and GetMessageTypeName methods. In the registration methods,
the type can be referred to by its name, or for optimization reasons, by its number.

+ When the SendMessage method is called, the given message is stacked by the message manager. When the Process method
of the message manager is called, it unstacks the messages and dispatches them to the behaviors or objects, and then
unregisters them. The messages to behaviors activate the provided input or output. The messages to the object
are stacked in the object's "last frame messages" list.

+ The unique instance of CKMessageManager is accessed through the CKContext::GetMessageManager function.


See also: CKContext::GetMessageManager,CKMessage
****************************************************************************/
class CKMessageManager : public CKBaseManager
{
    friend class CKMessage;

public:
    //---------------------------------------------
    // Message Types
    DLL_EXPORT CKMessageType AddMessageType(CKSTRING MsgName);
    DLL_EXPORT CKSTRING GetMessageTypeName(CKMessageType MsgType);
    DLL_EXPORT int GetMessageTypeCount();
    DLL_EXPORT void RenameMessageType(CKMessageType MsgType, CKSTRING NewName);
    DLL_EXPORT void RenameMessageType(CKSTRING OldName, CKSTRING NewName);

    //---------------------------------------------
    // register incoming message
    DLL_EXPORT CKERROR SendMessage(CKMessage *msg);
    DLL_EXPORT CKMessage *SendMessageSingle(CKMessageType MsgType, CKBeObject *dest, CKBeObject *sender = NULL);
    DLL_EXPORT CKMessage *SendMessageGroup(CKMessageType MsgType, CKGroup *group, CKBeObject *sender = NULL);
    DLL_EXPORT CKMessage *SendMessageBroadcast(CKMessageType MsgType, CK_CLASSID cid = CKCID_BEOBJECT, CKBeObject *sender = NULL);

    //----------------------------------------------
    // Waiting behaviors
    DLL_EXPORT CKERROR RegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate, CKBeObject *obj);
    DLL_EXPORT CKERROR RegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate, CKBeObject *obj);
    DLL_EXPORT CKERROR UnRegisterWait(CKMessageType MsgType, CKBehavior *behav, int OutputToActivate);
    DLL_EXPORT CKERROR UnRegisterWait(CKSTRING MsgName, CKBehavior *behav, int OutputToActivate);
    DLL_EXPORT CKERROR RegisterDefaultMessages();

    //-------------------------------------------------------------------
    //-------------------------------------------------------------------
    // Internal functions
    //-------------------------------------------------------------------
    //-------------------------------------------------------------------

    virtual CKStateChunk *SaveData(CKFile *SavedFile);
    virtual CKERROR LoadData(CKStateChunk *chunk, CKFile *LoadedFile);
    virtual CKERROR PreClearAll();
    virtual CKERROR PostProcess();
    virtual CKERROR OnCKReset();
    virtual CKERROR SequenceToBeDeleted(CK_ID *objids, int count);

    virtual CKDWORD GetValidFunctionsMask() { return CKMANAGER_FUNC_PreClearAll |
                                                     CKMANAGER_FUNC_PostProcess |
                                                     CKMANAGER_FUNC_OnCKReset |
                                                     CKMANAGER_FUNC_OnSequenceToBeDeleted; }

    virtual ~CKMessageManager();

    CKMessageManager(CKContext *Context);

protected:
    void AddMessageToObject(CKObject *obj, CKMessage *msg, CKScene *currentscene, CKBOOL recurse);

    CKMessage *CreateMessageSingle(CKMessageType type, CKBeObject *dest, CKBeObject *sender);
    CKMessage *CreateMessageGroup(CKMessageType type, CKGroup *group, CKBeObject *sender);
    CKMessage *CreateMessageBroadcast(CKMessageType type, CK_CLASSID objType, CKBeObject *sender);

    CKStringArray m_RegisteredMessageTypes;
    CKWaitingObjectArray **m_MsgWaitingList;
    XArray<CKMessage *> m_ReceivedMsgThisFrame;
    XObjectPointerArray m_LastFrameObjects;
};

#endif // CKMESSAGEMANAGER_H
