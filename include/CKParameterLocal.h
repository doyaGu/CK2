#ifndef CKPARAMETERLOCAL_H
#define CKPARAMETERLOCAL_H

#include "CKParameter.h"

/*************************************************************************
Name: CKParameterLocal

Summary: Local parameter providing a value

Remarks:
{Image:ParameterLocal}

+ A CKParameterLocal derives directly from CKParameterOut and differs only by its class ID, it provides no additional methods.

+ A CKParameterLocal is created with CKBehavior::CreateLocalParameter or CKContext::CreateParameterLocal.

+ The class id of CKParameterLocal is CKCID_PARAMETERLOCAL.

See also: CKParameterOut
*****************************************************************************/
class CKParameterLocal : public CKParameter
{
public:
    //--------------------------------------------
    // Special Parameter :: This (MySelf)

    DLL_EXPORT void SetAsMyselfParameter(CKBOOL act);

    CKBOOL IsMyselfParameter() { return (m_ObjectFlags & CK_PARAMETERIN_THIS); }

    virtual void SetOwner(CKObject *o);
    virtual CKERROR SetValue(const void *buf, int size = 0);
    virtual CKERROR CopyValue(CKParameter *param, CKBOOL UpdateParam = TRUE);

    virtual void *GetWriteDataPtr();
    virtual CKERROR SetStringValue(CKSTRING Value);

    //-------------------------------------------------------------------

    //---------------------------------------
    // Virtual functions
    CKParameterLocal(CKContext *Context, CKSTRING name = NULL, int type = -1);
    virtual ~CKParameterLocal();
    virtual CK_CLASSID GetClassID();

    virtual void PreDelete();

    virtual CKStateChunk *Save(CKFile *file, CKDWORD flags);
    virtual CKERROR Load(CKStateChunk *chunk, CKFile *file);

    virtual int GetMemoryOccupation();

    //--------------------------------------------
    // Dependencies Functions
    virtual CKERROR PrepareDependencies(CKDependenciesContext &context);
    virtual CKERROR RemapDependencies(CKDependenciesContext &context);
    virtual CKERROR Copy(CKObject &o, CKDependenciesContext &context);

    //--------------------------------------------
    // Class Registering
    static CKSTRING GetClassName();
    static int GetDependenciesCount(int mode);
    static CKSTRING GetDependencies(int i, int mode);
    static void Register();
    static CKParameterLocal *CreateInstance(CKContext *Context);
    static CK_CLASSID m_ClassID;

    // Dynamic Cast method (returns NULL if the object can't be cast)
    static CKParameterLocal *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_PARAMETERLOCAL) ? (CKParameterLocal *)iO : NULL;
    }
};

#endif // CKPARAMETERLOCAL_H
