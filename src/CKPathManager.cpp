#include "CKPathManager.h"

#include <stdio.h>

#include "CKContext.h"

CKPathManager::CKPathManager(CKContext *Context) : CKBaseManager(Context, PATH_MANAGER_GUID, "Path Manager") {

}

CKPathManager::~CKPathManager() {

}

int CKPathManager::AddCategory(XString &cat) {
    return 0;
}

CKERROR CKPathManager::RemoveCategory(int catIdx) {
    return 0;
}

int CKPathManager::GetCategoryCount() {
    return 0;
}

CKERROR CKPathManager::GetCategoryName(int catIdx, XString &catName) {
    return 0;
}

int CKPathManager::GetCategoryIndex(XString &cat) {
    return 0;
}

CKERROR CKPathManager::RenameCategory(int catIdx, XString &newName) {
    return 0;
}

int CKPathManager::AddPath(int catIdx, XString &path) {
    return 0;
}

CKERROR CKPathManager::RemovePath(int catIdx, int pathIdx) {
    return 0;
}

CKERROR CKPathManager::SwapPaths(int catIdx, int pathIdx1, int pathIdx2) {
    return 0;
}

int CKPathManager::GetPathCount(int catIdx) {
    return 0;
}

CKERROR CKPathManager::GetPathName(int catIdx, int pathIdx, XString &path) {
    return 0;
}

int CKPathManager::GetPathIndex(int catIdx, XString &path) {
    return 0;
}

CKERROR CKPathManager::RenamePath(int catIdx, int pathIdx, XString &path) {
    return 0;
}

const char* g_StartPath = "";
CKERROR CKPathManager::ResolveFileName(XString &file, int catIdx, int startIdx) {
    XString v4;
    if (file == "") return CKERR_INVALIDFILE;
    if (startIdx == -1) {
        if (PathIsAbsolute(file)) {
            FILE *fp = fopen(file.CStr(), "rb");
            if (fp) {
                fclose(fp);
                return CK_OK;
            }
        }

        if (PathIsURL(file)) {
            AddEscapedSpace(file);
            return CK_OK;
        }

        if (PathIsFile(file) || PathIsUNC(file))
            return CK_OK;

        {
            XString filepath = g_StartPath;
            filepath << file;
            FILE *fp = fopen(filepath.CStr(), "rb");
            if (fp) {
                fclose(fp);
                file = filepath;
                return CK_OK;
            }
        }

        CKSTRING lastCmoLoaded = m_Context->GetLastCmoLoaded();
        CKPathSplitter splitter(lastCmoLoaded);
    }

    return CK_OK;
}

CKBOOL CKPathManager::PathIsAbsolute(XString &file) {
    return 0;
}

CKBOOL CKPathManager::PathIsUNC(XString &file) {
    return 0;
}

CKBOOL CKPathManager::PathIsURL(XString &file) {
    return 0;
}

CKBOOL CKPathManager::PathIsFile(XString &file) {
    return 0;
}

void CKPathManager::RemoveEscapedSpace(char *str) {

}

void CKPathManager::AddEscapedSpace(XString &str) {

}

XString CKPathManager::GetVirtoolsTemporaryFolder() {
    return XString();
}

void CKPathManager::Clean() {

}

void CKPathManager::RemoveSpace(char *str) {

}

CKBOOL CKPathManager::TryOpenAbsolutePath(XString &file) {
    return 0;
}

CKBOOL CKPathManager::TryOpenFilePath(XString &file) {
    return 0;
}

CKBOOL CKPathManager::TryOpenURLPath(XString &file) {
    return 0;
}
