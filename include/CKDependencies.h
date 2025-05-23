#ifndef CKDEPENDENCIES_H
#define CKDEPENDENCIES_H

#include "XSArray.h"
#include "XNHashTable.h"
#include "XObjectArray.h"
#include "CKDependenciesConstants.h"

typedef XNHashTable<CK_ID, CK_ID> XHashID;
typedef XHashID::Iterator XHashItID;
typedef XHashID::ConstIterator XHashItCID;
typedef XHashID::Pair XHashPairID;

/*************************************
Summary: Dependencies options

See Also:CKDependencies
****************************************/
typedef enum CK_DEPENDENCIES_FLAGS
{
    CK_DEPENDENCIES_CUSTOM = 0x00000000, // Dependencies will depend on what options where modified in CKDependencies
    CK_DEPENDENCIES_NONE   = 0x00000001, // No dependencies will be taken into account
    CK_DEPENDENCIES_FULL   = 0x00000002  // Every dependency will be taken
} CK_DEPENDENCIES_FLAGS;

/*************************************************
Summary: Storage of dependencies options.

Remarks:
+ See Object Dependencies Options chapter for more details on this class.

+ This class derived from an XSArray. It contains
the dependencies options used for copying, destroying,
etc. objects. Each index of the array contains the
specific options for the class ID equals to the index.

+ These options are visible in the file CKDependenciesConstants.h

+ The m_Flags members is used to set a mode of
dependencies (either none, full or custom). in Full or None mode the

+ You can obtain a CKDependencies filled with
default dependencies for each class with the global
function CKGetDefaultClassDependencies(), or copy them
to a new CKDependencies for modifying them afterward with
CKCopyDefaultClassDependencies().

See also: CKGetDefaultClassDependencies,CKCopyDefaultClassDependencies,CKDependenciesContext,Object Dependencies Options
*************************************************/
class CKDependencies : public XSArray<CKDWORD>
{
public:
    CKDependencies() : XSArray<CKDWORD>(), m_Flags(CK_DEPENDENCIES_NONE) {}

    /*************************************************
    Summary: Modify the dependencies options for a
    specific class id.
    Remarks:
        For example we do not want that when copying objects, meshes
    of a CK3dEntity be copied. For this we modify the option for the CK3dEntity class
    Example:
            ModifyOptions(CKCID_3DENTITY,0,CK_DEPENDENCIES_COPY_3DENTITY_MESH);
    See Also:Object Dependencies Options
    *************************************************/
    void ModifyOptions(CK_CLASSID cid, CKDWORD add, CKDWORD rem)
    {
        (*this)[cid] |= add;
        (*this)[cid] &= ~rem;
    }

    /*************************************************
    Summary: Flags defining the mode of dependencies.
    See also: CK_DEPENDENCIES_FLAGS
    *************************************************/
    CK_DEPENDENCIES_FLAGS m_Flags;
};

#define StackDependencies() CKDependenciesContext::DynamicSentinel ds = context.StackPrepareDependencies(this, iCaller)

/*************************************************
Summary: Storage of dependencies, when performing objects
operations like copy, delete, etc.

Remarks:
See also: CKDependencies,CKContext
*************************************************/
class CKDependenciesContext
{
    friend class CKObject;
    friend class CKStateChunk;
    friend class CKContext;
    friend class CKUIManager;

public:
    DLL_EXPORT CKDependenciesContext(CKContext *context)
        : m_CKContext(context),
          m_Dependencies(NULL),
          m_Mode(CK_DEPENDENCIES_BUILD),
          m_CreationMode(CK_OBJECTCREATION_NONAMECHECK) {}

    // Objects Access
    DLL_EXPORT void AddObjects(CK_ID *ids, int count);
    DLL_EXPORT int GetObjectsCount();
    DLL_EXPORT CKObject *GetObjects(int i);

    /*************************************************
    Summary: Remaps a CK_ID.

    Arguments:
        id : the CK_ID to remap. If the object was in
    the dependencies context, it is changed. Otherwise, it remains unchanged.

    Return Value:
        Returns the remapped value of the CK_ID given, or the
    old value if it wasn't in the dependencies context.

    See also: Remap
    *************************************************/
    DLL_EXPORT CK_ID RemapID(CK_ID &id);

    /*************************************************
    Summary: Remaps a CKObject.
    Arguments:
        o : the CKObject to remap.
    Return Value:
        Returns the remapped object, or the old object if it wasn't in the dependencies context.
    See also: RemapID
    *************************************************/
    DLL_EXPORT CKObject *Remap(const CKObject *o);

    /*************************************************
    Summary: Tests if a CK_ID is in the dependencies context.

    Arguments:
        id : CK_ID to test.

    Return Value:
        TRUE if the object is here, FALSE otherwise.

    See also: RemapID
    *************************************************/
    CKBOOL IsDependenciesHere(CK_ID id) { return m_MapID.IsHere(id); }

    /// Dependencies access
    DLL_EXPORT XObjectArray FillDependencies();
    DLL_EXPORT XObjectArray FillRemappedDependencies();

    /*************************************************
    Summary: Sets dependencies options for the subsequent operations
    to come.

    Arguments:
        d : a CKDependencies.

    Remarks:
        You'll need to call this function before calling Copy()
    for example if you want full or custom dependencies to be
    taken into account.

    See also: CKDependencies,StopDependencies
    *************************************************/
    void StartDependencies(CKDependencies *d)
    {
        m_DependenciesStack.PushBack(d);
        m_Dependencies = d;
    }

    /*************************************************
    Summary: Stops using the actual dependencies options.

    Remarks:
        You'll need to call this function before calling Copy()
    for example if you want full or custom dependencies to be
    taken into account.

    See also: CKDependencies, StartDependencies
    *************************************************/
    void StopDependencies()
    {
        m_DependenciesStack.PopBack();
        m_Dependencies = ((m_DependenciesStack.Size()) ? m_DependenciesStack.Back() : NULL);
    }

    DLL_EXPORT CKDWORD GetClassDependencies(int c);

    DLL_EXPORT void Copy(CKSTRING appendstring = NULL);

    /*************************************************
    Summary: Sets the operation mode for this dependencies context.

    Arguments:
        m : the operation mode desired.

    See also: CK_DEPENDENCIES_OPMODE, IsInMode
    *************************************************/
    void SetOperationMode(CK_DEPENDENCIES_OPMODE m) { m_Mode = m; }

    /*************************************************
    Summary: Gets the operation mode for this dependencies context.

    Return Value:
        The operation mode desired.

    See also: CK_DEPENDENCIES_OPMODE, SetOperationMode
    *************************************************/
    CKBOOL IsInMode(CK_DEPENDENCIES_OPMODE m) { return ((CK_DEPENDENCIES_OPMODE)m_Mode == m); }

    /*************************************************
    Summary: Sets the creation mode for subsequent object creation.

    Arguments:
        m : the creation mode desired.

    See also: CK_DEPENDENCIES_OPMODE, IsInMode
    *************************************************/
    void SetCreationMode(CK_OBJECTCREATION_OPTIONS m) { m_CreationMode = m; }

    DLL_EXPORT CKBOOL ContainClassID(CK_CLASSID Cid);

    //-------------------------------------------------------------------------
    // Internal functions

    XObjectPointerArray m_DynamicObjects;

    DLL_EXPORT CKERROR FinishPrepareDependencies(CKObject *iMySelf, CK_CLASSID Cid);

    // the context
    CKContext *m_CKContext;

    const XHashID &GetDependenciesMap() const
    {
        return m_MapID;
    }

protected:
    // Add Object
    DLL_EXPORT CKBOOL AddDependencies(CK_ID id);

    DLL_EXPORT void Clear();

    CKDependencies *m_Dependencies;
    // Remapping Hashtable
    XHashID m_MapID;
    // Objects to process
    XObjectArray m_Objects;
    // Objects to process
    XArray<CKDependencies *> m_DependenciesStack;
    // In there are scripts in the objects being processed they will also be stored here
    XObjectArray m_Scripts;

    CKDWORD m_Mode;
    CKDWORD m_CreationMode;

    XString m_CopyAppendString;
    XBitArray m_ObjectsClassMask;
};

#endif // CKDEPENDENCIES_H