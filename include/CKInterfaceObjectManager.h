//#ifndef CKINTERFACE_H
//#define CKINTERFACE_H
//
//#include "CKObject.h"
//
//class CKInterfaceObjectManager : public CKObject
//{
//public:
//    void SetGuid(CKGUID guid);
//    CKGUID GetGuid();
//
//    ////////////////////////////////////////
//    // Datas
//    void AddStateChunk(CKStateChunk *chunk);
//    void RemoveStateChunk(CKStateChunk *chunk);
//    int GetChunkCount();
//    CKStateChunk *GetChunk(int pos);
//
//    //-------------------------------------------------------------------
//    //-------------------------------------------------------------------
//    // Internal functions
//    //-------------------------------------------------------------------
//    //-------------------------------------------------------------------
//
//    CKInterfaceObjectManager(CKContext *Context, CKSTRING name = NULL);
//    virtual ~CKInterfaceObjectManager();
//    virtual CK_CLASSID GetClassID();
//
//    virtual CKStateChunk *Save(CKFile *file, CKDWORD flags);
//    virtual CKERROR Load(CKStateChunk *chunk, CKFile *file);
//
//    //--------------------------------------------
//    // Class Registering
//    static CKSTRING GetClassName();
//    static int GetDependenciesCount(int mode);
//    static CKSTRING GetDependencies(int i, int mode);
//    static void Register();
//    static CKInterfaceObjectManager *CreateInstance(CKContext *Context);
//    static CK_ID m_ClassID;
//
//private:
//    int m_Count;
//    CKStateChunk **m_Chunks;
//    CKGUID m_Guid;
//};
//
//#endif // CKINTERFACE_H
