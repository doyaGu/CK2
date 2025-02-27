#ifndef CKPARAMETERMANAGER_H
#define CKPARAMETERMANAGER_H

#include "CKBaseManager.h"
#include "CKContext.h"
#include "CKParameterIn.h"
#include "CKParameterOut.h"
#include "CKParameterLocal.h"

/********************************************************
 Kept for compatibility issues : On macintosh the
 const CKGUID& must be used to conform to Codewarrior, On PC
 we do not need to this (and  must not to keep CK2 compatible
 with previously created DLLs) {secret}
*********************************************************/
#ifdef macintosh
#define CKGUIDCONSTREF const CKGUID &
#define CKGUIDREF const CKGUID &
#else
#define CKGUIDCONSTREF CKGUID
#define CKGUIDREF CKGUID &
#endif

typedef XNHashTable<CKParameterType, CKGUID> XHashGuidToType;

/***********************************************************
Summary: Helper class to access a parameter of type structure.
Remarks:


+ New parameter types can be created like structures with the
CKParameterManager::RegisterNewStructure method, this class provides
method to easily access a structure members.

+ A structure defined in C like this:

          typedef struct MyStructure
          {
              float		  Priority;
              VxVector	  Position;
              CK_ID		  ObstacleId;
          } MyStructure;

is equivalent to declare

        #define  CKPGUID_MYSTRUCT CKGUID(0x4a893652,0x76e72d5c)
        ParameterManager->RegisterNewStructure(CKPGUID_MYSTRUCT,"MyStructure","Priority,Position,Obstacle",CKPGUID_FLOAT,CKPGUID_VECTOR,CKPGUID_3DENTITY);

then the CKStructHelper can help to have a description of this structure later :

        CKStructHelper	StructDesc(Context,CKPGUID_MYSTRUCT);
        int Count = StructDesc.GetMemberCount();
        for (int i=0;i<Count;++i) {
            StructDesc.GetMemberName(i);  // Priority,Position,Obstacle
            StructDesc.GetMemberGUID(i);  // CKPGUID_FLOAT,CKPGUID_VECTOR,CKPGUID_3DENTITY
        }

or it can be use to access the sub-members of a parameter of the type CKPGUID_MYSTRUCT for example:

        CKStructParameter MyStruct(param);

        //--- Access sub-members
        float		  Priority;
        VxVector	  Position;
        CK_ID		  ObstacleId;

        MyStruct[0]->GetValue(&Priority);
        MyStruct[1]->GetValue(&Position);
        MyStruct[2]->GetValue(&ObstacleId);

See Also: RegisterNewStructure,
************************************************************/
class DLL_EXPORT CKStructHelper
{
public:
    CKStructHelper(CKParameter *Param);
    CKStructHelper(CKContext *ctx, CKGUID PGuid, CK_ID *Data = NULL);
    CKStructHelper(CKContext *ctx, CKParameterType PType, CK_ID *Data = NULL);

    //------- Members description (These methods do not require
    // Data pointer to be valid )
    char *GetMemberName(int Pos);
    CKGUID GetMemberGUID(int Pos);
    int GetMemberCount();

    /************************************************************
    Summary: Returns a member of this structure as a parameter
    Arguments:
        i: Index of the member to return.
    Return Value:
        A pointer to a CKParameter that holds the ith member.
    Remarks:
    This method does not perform any check concerning the validity of the given index, it is
    the user responsibility to ensure it is below GetMemberCount
    See also:GetMemberGUID,GetMemberName,GetMemberCount
    *****************************************************************************/
    CKParameter *operator[](int i) { return (CKParameter *)m_Context->GetObject(m_SubIDS[i]); }

protected:
    CKContext *m_Context;
    CKStructStruct *m_StructDescription;
    CK_ID *m_SubIDS;
};

/*************************************************
Summary: Description of an available parametric operation.

Remarks:
    +  The CKParameterManager::GetAvailableOperationsDesc fills an array
    of this structure according to search criteria. The 4 CKGUID defines the
    parameter operation and the Fct member is the function the engine will
    call to process the operation.
See also: CKParameterManager::GetAvailableOperationsDesc
*************************************************/
typedef struct CKOperationDesc
{
    CKGUID OpGuid;             // Operation GUID
    CKGUID P1Guid;             // Input Parameter 1 GUID
    CKGUID P2Guid;             // Input Parameter 2 GUID
    CKGUID ResGuid;            // Output Parameter GUID
    CK_PARAMETEROPERATION Fct; // Function to call to process the operation.
} CKOperationDesc;

struct TreeCell {
    CKGUID Guid;
    int ChildCount;
    union {
        TreeCell *Children;
        CK_PARAMETEROPERATION Operation;
    };

    TreeCell() : ChildCount(0), Children(NULL) {}
};

struct OperationCell {
    char Name[30];
    CKGUID OperationGuid;
    int CellCount;
    TreeCell *Tree;
    CKBOOL IsActive;
};

/************************************************************************
Name: CKParameterManager

Summary: Parameter and operation types management


Remarks:

+ There is only one instance of the CKParameterManager per context, which can
be accessed using the CKContext::GetParameterManager global function. It manages the list of parameter types,
and the list of operation types. It gives access to the creation and registration of new operations, and
overwriting of existing operations.

+ Operations are subdivided into families, where members of a family of operations all
perform the same kind of operations on different types of parameters. For example, you can
add floats and also imagine adding 3D entities.
In order to define a new operation type, its family should first be registered, then its function should
be declared. When a family is unregistered, all the operations of that family are unregistered too. In all
cases the name argument is provided as a convenience for debugging and display purposes.

See Also: CKParameterOperation, CKParameter,CKParameterIn, CKParameterOut,ParameterOperation Types,Pre-Registered Parameter Types
********************************************/
class DLL_EXPORT CKParameterManager : public CKBaseManager
{
    friend class CKParameter;
    friend class CKContext;

public:
    //-----------------------------------------------------------------------
    // Parameter Types registration
    CKERROR RegisterParameterType(CKParameterTypeDesc *paramType);
    CKERROR UnRegisterParameterType(CKGUIDCONSTREF guid);
    CKParameterTypeDesc *GetParameterTypeDescription(int type);
    CKParameterTypeDesc *GetParameterTypeDescription(CKGUIDCONSTREF guid);
    int GetParameterSize(CKParameterType type);
    int GetParameterTypesCount();

    //-----------------------------------------------------------------------
    // Parameter Types <=> Parameter Type Name <=> GUID conversion functions
    CKParameterType ParameterGuidToType(CKGUIDCONSTREF guid);
    CKSTRING ParameterGuidToName(CKGUIDCONSTREF guid);
    CKGUID ParameterTypeToGuid(CKParameterType type);
    CKSTRING ParameterTypeToName(CKParameterType type);
    CKGUID ParameterNameToGuid(CKSTRING name);
    CKParameterType ParameterNameToType(CKSTRING name);

    ///----------------------------------------------------------------------
    // Derivated types functions
    CKBOOL IsDerivedFrom(CKGUIDCONSTREF guid1, CKGUIDCONSTREF parent);
    CKBOOL IsDerivedFrom(CKParameterType child, CKParameterType parent);
    CKBOOL IsTypeCompatible(CKGUIDCONSTREF guid1, CKGUIDCONSTREF guid2);
    CKBOOL IsTypeCompatible(CKParameterType type1, CKParameterType type2);

    //-----------------------------------------------------------------------
    // Parameter Type to Class ID conversions functions
    CK_CLASSID TypeToClassID(CKParameterType type);
    CK_CLASSID GuidToClassID(CKGUIDCONSTREF guid);

    CKParameterType ClassIDToType(CK_CLASSID cid);
    CKGUID ClassIDToGuid(CK_CLASSID cid);

    //-----------------------------------------------------------------------
    // Special Types : Flags,enums and Structures
    CKERROR RegisterNewFlags(CKGUIDCONSTREF FlagsGuid, CKSTRING FlagsName, CKSTRING FlagsData);
    CKERROR RegisterNewEnum(CKGUIDCONSTREF EnumGuid, CKSTRING EnumName, CKSTRING EnumData);
    CKERROR ChangeEnumDeclaration(CKGUIDCONSTREF EnumGuid, CKSTRING EnumData);
    CKERROR ChangeFlagsDeclaration(CKGUIDCONSTREF FlagsGuid, CKSTRING FlagsData);
    CKERROR RegisterNewStructure(CKGUIDCONSTREF StructGuid, CKSTRING StructName, CKSTRING StructData, ...);
    CKERROR RegisterNewStructure(CKGUIDCONSTREF StructGuid, CKSTRING StructName, CKSTRING StructData, XArray<CKGUID> &ListGuid);

    int GetNbFlagDefined();
    int GetNbEnumDefined();
    int GetNbStructDefined();
    CKFlagsStruct *GetFlagsDescByType(CKParameterType pType);
    CKEnumStruct *GetEnumDescByType(CKParameterType pType);
    CKStructStruct *GetStructDescByType(CKParameterType pType);

    //------------ Parameter Operations -------------------------------------//

    //-----------------------------------------------------------------------
    // Operation Family Registration
    CKOperationType RegisterOperationType(CKGUIDCONSTREF OpCode, CKSTRING name);
    CKERROR UnRegisterOperationType(CKGUIDCONSTREF opguid);
    CKERROR UnRegisterOperationType(CKOperationType opcode);

    //-----------------------------------------------------------------------
    // Operation function access
    CKERROR RegisterOperationFunction(CKGUIDREF operation, CKGUIDREF type_paramres, CKGUIDREF type_param1, CKGUIDREF type_param2, CK_PARAMETEROPERATION op);

    CK_PARAMETEROPERATION GetOperationFunction(CKGUIDREF operation, CKGUIDREF type_paramres, CKGUIDREF type_param1, CKGUIDREF type_param2);
    CKERROR UnRegisterOperationFunction(CKGUIDREF operation, CKGUIDREF type_paramres, CKGUIDREF type_param1, CKGUIDREF type_param2);

    //-----------------------------------------------------------------------
    // operation type conversion functions : Name <-> GUID <-> internal code for operation
    CKGUID OperationCodeToGuid(CKOperationType type);
    CKSTRING OperationCodeToName(CKOperationType type);
    CKOperationType OperationGuidToCode(CKGUIDCONSTREF guid);
    CKSTRING OperationGuidToName(CKGUIDCONSTREF guid);
    CKGUID OperationNameToGuid(CKSTRING name);
    CKOperationType OperationNameToCode(CKSTRING name);

    int GetAvailableOperationsDesc(const CKGUID &opGuid,
                                   CKParameterOut *res,
                                   CKParameterIn *p1,
                                   CKParameterIn *p2,
                                   CKOperationDesc *list);

    int GetParameterOperationCount();

    void BuildParameterHierarchy(CKParameterType type, CKGUID *parents, int &count);
    void ProcessParameterCombinations(
        OperationCell &opCell,
        CKGUID *p1Hierarchy, int p1Count,
        CKGUID *p2Hierarchy, int p2Count,
        CKGUID *resHierarchy, int resCount,
        CKOperationDesc *list,
        int &opCount);
    void ProcessSecondParameterLevel(
        TreeCell &p1Cell,
        CKGUID *p2Hierarchy, int p2Count,
        CKGUID *resHierarchy, int resCount,
        CKOperationDesc *list,
        int &opCount,
        const CKGUID &opGuid,
        const CKGUID &p1Guid);
    void ProcessResultTypeLevel(
        TreeCell &p2Cell,
        CKGUID *resHierarchy, int resCount,
        CKOperationDesc *list,
        int &opCount,
        const CKGUID &opGuid,
        const CKGUID &p1Guid,
        const CKGUID &p2Guid);
    void AddCompatibleDerivedOperations(
        TreeCell &resCell,
        CKOperationDesc *list,
        int &opCount,
        const CKGUID &opGuid,
        const CKGUID &p1Guid,
        const CKGUID &p2Guid);

    TreeCell *FindOrCreateGuidCell(TreeCell *&cells, int &cellCount, CKGUID &guid);

    CKBOOL IsParameterTypeToBeShown(CKParameterType type);
    CKBOOL IsParameterTypeToBeShown(CKGUIDCONSTREF guid);

    void UpdateParameterEnum();

    CKParameterManager(CKContext *Context);

    ~CKParameterManager();

    CKBOOL m_ParameterTypeEnumUpToDate;

protected:
    XArray<CKParameterTypeDesc *> m_RegisteredTypes;

    int m_NbOperations, m_NbAllocatedOperations;
    OperationCell *m_OperationTree;

    XHashGuidToType m_ParamGuids;
    XHashGuidToType m_OpGuids;

    CKBOOL m_DerivationMasksUpToDate;

    int m_NbFlagsDefined;
    CKFlagsStruct *m_Flags;
    int m_NbStructDefined;
    CKStructStruct *m_Structs;
    int m_NbEnumsDefined = 0;
    CKEnumStruct *m_Enums = nullptr;

    CKBOOL CheckParamTypeValidity(CKParameterType type);
    CKBOOL CheckOpCodeValidity(CKOperationType type);

private:
    CKBOOL GetParameterGuidParentGuid(CKGUID child, CKGUID &parent);
    void UpdateDerivationTables();
    void RecurseDeleteParam(TreeCell *cell, CKGUID param);
    int DichotomicSearch(int start, int end, TreeCell *tab, CKGUID key);
    void RecurseDelete(TreeCell *cell);

    CKBOOL IsDerivedFromIntern(int child, int parent);
    CKBOOL RemoveAllParameterTypes();
    CKBOOL RemoveAllOperations();

    int BuildGuidHierarchy(CKGUID guid, CKGUID* buffer, int max);
};

#endif // CKPARAMETERMANAGER_H
