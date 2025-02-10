#ifndef CKBEHAVIORPROTOTYPE_H
#define CKBEHAVIORPROTOTYPE_H

#include "CKObject.h"
#include "CKObjectDeclaration.h"

#ifdef STRINGDUPLICATION
#define MAKESTRING(a) CKStrdup(a)
#define DELETESTRING(a) CKDeletePointer(a)
#else
#define MAKESTRING(a) a
#define DELETESTRING(a)
#endif

struct CKPARAMETER_DESC
{
    CKSTRING Name; // Parameter Name
    CKGUID Guid;   // Guid identifying the type
    int Type;      // In, Out or local
    CKSTRING DefaultValueString;
    CKBYTE *DefaultValue;
    int DefaultValueSize;
    int Owner;

public:
    CKPARAMETER_DESC()
    {
        Type = 0;
        Name = NULL;
        DefaultValueString = NULL;
        DefaultValue = NULL;
        DefaultValueSize = 0;
        Guid = CKGUID();
        Owner = -1;
    }
    ~CKPARAMETER_DESC()
    {
        // CKDeletePointer(Name);
        // CKDeletePointer(DefaultValueString);
        delete[] DefaultValue;
    }
    CKPARAMETER_DESC &operator=(const CKPARAMETER_DESC &d)
    {
        Type = d.Type;
        Name = d.Name;
        DefaultValueString = d.DefaultValueString;
        DefaultValue = NULL;
        DefaultValueSize = 0;
        Guid = d.Guid;
        Owner = d.Owner;
        if (d.DefaultValue && d.DefaultValueSize)
        {
            DefaultValue = new CKBYTE[d.DefaultValueSize];
            DefaultValueSize = d.DefaultValueSize;
            memcpy(DefaultValue, d.DefaultValue, DefaultValueSize);
        }

        return *this;
    }
};

struct CKBEHAVIORIO_DESC
{
    CKSTRING Name; // Name of this IO
    CKDWORD Flags; // Flags	In/Out/active etc

public:
    // Ctor
    CKBEHAVIORIO_DESC() : Name(0)
    {
    }
    // Dtor
    ~CKBEHAVIORIO_DESC()
    {
        // CKDeletePointer(Name);
    }
};

/*************************************************
Summary: Class describing the prototype of a behavior

Remarks:
    + A behavior prototype is a kind of model for behaviors. Behavior prototypes describe the behavior's
    internal structure and relationships with other objects.
    Prototypes are usually created in external dlls, but may be created in memory.
    For more information on creation of behavior dlls take a look to the behavior samples joined
    in this SDK.

    + Prototypes are usually defined in a Dll. This prototype is created the first time it is needed,
    that is either when a behavior in Virtools is drag-n-dropped onto an object, or when a behavior is loaded
    that is derived from the prototype. In this case, the main dll entry point CKDLL_CREATEPROTOFUNCTION
    will create the behavior prototype, that will be used to create the behavior using the CKBehavior::InitFromPrototype method.
    A prototype may use other prototypes, as sub-prototypes, thus generating sub-behaviors.

    + Each prototype that should be standalone, either in a dll, or in a standalone file, should have a GUID.
    A guid can be generated by using the CKGetSecureGuid function. If the prototype is created in a dll, the GUID is provided
    using the object declaration. If the prototype is created in memory, the GUID can be set using the CKBehaviorPrototype::SetGuid method.

    + A name can be provided for various parts of the behavior. This name is not used for indexing
    and does not have to be unique. It is provided as a convenience for display and debugging purposes.

    + A CKBehaviorPrototype is created with CreateCKBehaviorPrototype.


See also: CreateCKBehaviorPrototype,CKBehavior
*************************************************/
class DLL_EXPORT CKBehaviorPrototype
{
    friend class CKBehavior;
    friend class CKBehaviorPrototypeTemporary;

public:
    //---------------------------------------------------------------------
    // Inputs, outputs, out parameters, in parameters, local parameters and settings (special local parameters)
    virtual int DeclareInput(CKSTRING name);
    virtual int DeclareOutput(CKSTRING name);

    virtual int DeclareInParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval = NULL);
    virtual int DeclareInParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize);

    virtual int DeclareOutParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval = NULL);
    virtual int DeclareOutParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize);

    int DeclareLocalParameter(CKSTRING name, CKGUID guid_type, CKSTRING defaultval = NULL);
    int DeclareLocalParameter(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize);

    int DeclareSetting(CKSTRING name, CKGUID guid_type, CKSTRING defaultval = NULL);
    int DeclareSetting(CKSTRING name, CKGUID guid_type, void *defaultval, int valsize);

    //--------------------------------------------------------------------
    // Behavior Description functions
    void SetGuid(CKGUID guid);
    CKGUID GetGuid();

    void SetFlags(CK_BEHAVIORPROTOTYPE_FLAGS flags);
    CK_BEHAVIORPROTOTYPE_FLAGS GetFlags();

    void SetApplyToClassID(CK_CLASSID cid);
    CK_CLASSID GetApplyToClassID();

    void SetFunction(CKBEHAVIORFCT fct);
    CKBEHAVIORFCT GetFunction();

    void SetBehaviorCallbackFct(CKBEHAVIORCALLBACKFCT fct, CKDWORD CallbackMask = CKCB_BEHAVIORALL, void *param = NULL);
    CKBEHAVIORCALLBACKFCT GetBehaviorCallbackFct();

    void SetBehaviorFlags(CK_BEHAVIOR_FLAGS flags);
    CK_BEHAVIOR_FLAGS GetBehaviorFlags();

    /*******************************************
    Summary: Returns the name of this prototype
    Return Value:
        Prototype name.
    See Also: GetGuid
    ********************************************/
    CKSTRING GetName() { return m_Name.Str(); }

    //-------------------------------------------------------------------
    // Internal functions

    //---------------------------------------------------------------------
    // Info Functions {Secret}
    int GetInputCount() { return m_InIOCount; }
    int GetOutputCount() { return m_OutIOCount; }
    int GetInParameterCount() { return m_InParameterCount; }
    int GetOutParameterCount() { return m_OutParameterCount; }
    int GetLocalParameterCount() { return m_LocalParameterCount; }
    CKBEHAVIORIO_DESC **GetInIOList() { return m_InIOList; }
    CKBEHAVIORIO_DESC **GetOutIOList() { return m_OutIOList; }
    CKPARAMETER_DESC **GetInParameterList() { return m_InParameterList; }
    CKPARAMETER_DESC **GetOutParameterList() { return m_OutParameterList; }
    CKPARAMETER_DESC **GetLocalParameterList() { return m_LocalParameterList; }

    CKObjectDeclaration *GetSoureObjectDeclaration();

    int GetInIOIndex(CKSTRING name);
    int GetOutIOIndex(CKSTRING name);
    int GetInParamIndex(CKSTRING name);
    int GetOutParamIndex(CKSTRING name);
    int GetLocalParamIndex(CKSTRING name);

    CKSTRING GetInIOName(int);
    CKSTRING GetOutIOIndex(int);
    CKSTRING GetInParamIndex(int);
    CKSTRING GetOutParamIndex(int);
    CKSTRING GetLocalParamIndex(int);

    void SetSourceObjectDeclaration(CKObjectDeclaration *decl) { m_SourceObjectDeclaration = decl; }

    virtual ~CKBehaviorPrototype();
    CKBehaviorPrototype(CKSTRING Name);

    virtual CKBOOL IsRunTime() { return FALSE; }

private:
    CKGUID m_Guid;
    CK_BEHAVIORPROTOTYPE_FLAGS m_bFlags;
    CK_BEHAVIOR_FLAGS m_BehaviorFlags;
    CK_CLASSID m_ApplyTo;
    CKBEHAVIORFCT m_FctPtr;
    CKBEHAVIORCALLBACKFCT m_CallbackFctPtr;
    CKDWORD m_CallBackMask;

    int m_InIOCount;
    int m_OutIOCount;
    int m_InParameterCount;
    int m_LocalParameterCount;
    int m_OutParameterCount;
    CKBEHAVIORIO_DESC **m_InIOList;
    CKBEHAVIORIO_DESC **m_OutIOList;
    CKPARAMETER_DESC **m_InParameterList;
    CKPARAMETER_DESC **m_OutParameterList;
    CKPARAMETER_DESC **m_LocalParameterList;
    CK_BEHAVIOR_TYPE m_BehaviorType;
    void *m_CallBackParam;
    CKObjectDeclaration *m_SourceObjectDeclaration;

    XString m_Name;
};

#endif // CKBEHAVIORPROTOTYPE_H
