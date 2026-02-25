#include "CKPathManager.h"

#include "VxWindowFunctions.h"
#include "CKGlobals.h"
#include "CKContext.h"

#include <ctype.h>
#ifndef _MSC_VER
#include <strings.h>
#endif

static const int g_FileSchemeLength = (int)(sizeof("file:") - 1);
static const int g_LocalHostLength = (int)(sizeof("localhost") - 1);

static bool HasDrivePrefix(const char *path) {
    return path && isalpha((unsigned char)path[0]) && path[1] == ':';
}

static bool IsLocalHostPrefix(const char *path) {
    if (!path) {
        return false;
    }

#ifdef _MSC_VER
    if (_strnicmp(path, "localhost", g_LocalHostLength) != 0) {
#else
    if (strncasecmp(path, "localhost", g_LocalHostLength) != 0) {
#endif
        return false;
    }

    return path[g_LocalHostLength] == '\0' || path[g_LocalHostLength] == '/' || path[g_LocalHostLength] == '\\';
}

static bool TryDecodeFileUri(const XString &uri, XString &decodedPath) {
    if (strncmp(uri.CStr(), "file:", g_FileSchemeLength) != 0) {
        return false;
    }

    const char *cursor = uri.CStr() + g_FileSchemeLength;
    if (!cursor || cursor[0] == '\0') {
        return false;
    }

    XString result;

    if (cursor[0] == '/' && cursor[1] == '/') {
        cursor += 2;

        if (IsLocalHostPrefix(cursor)) {
            cursor += 9;
            while (*cursor == '/' || *cursor == '\\') {
                ++cursor;
            }
        }

        if (cursor[0] == '/' && HasDrivePrefix(cursor + 1)) {
            result = &cursor[1];
        } else if (HasDrivePrefix(cursor)) {
            result = cursor;
        } else {
            result = "\\\\";
            result << cursor;
        }
    } else if (cursor[0] == '/' && HasDrivePrefix(cursor + 1)) {
        result = &cursor[1];
    } else {
        result = cursor;
    }

    if (result.Length() <= 0) {
        return false;
    }

    for (int i = 0; i < result.Length(); ++i) {
        if (result[i] == '/') {
            result[i] = '\\';
        }
    }

    decodedPath = result;
    return true;
}

XString CKGetTempPath() {
    char buf[_MAX_PATH];
    char dir[64];
    snprintf(dir, sizeof(dir), "VTmp%d", rand());

    XString path = VxGetTempPath();
    VxMakePath(buf, path.Str(), dir);
    return XString(buf);
}

CKPathManager::CKPathManager(CKContext *Context) : CKBaseManager(Context, PATH_MANAGER_GUID, "Path Manager") {
    XString bitmap = "Bitmap Paths";
    AddCategory(bitmap);
    XString data = "Data Paths";
    AddCategory(data);
    XString sound = "Sound Paths";
    AddCategory(sound);

    m_TemporaryFolderExist = FALSE;
    m_TemporaryFolder = CKGetTempPath();

    m_Context->RegisterNewManager(this);
}

CKPathManager::~CKPathManager() {
    if (m_TemporaryFolderExist) {
        VxDeleteDirectory(m_TemporaryFolder.Str());
    }

    Clean();
}

int CKPathManager::AddCategory(XString &cat) {
    if (GetCategoryIndex(cat) != -1) {
        return -1;
    }

    CKPATHCATEGORY category;
    category.m_Name = cat;
    category.m_Entries.Clear();
    m_Categories.PushBack(category);
    return m_Categories.Size() - 1;
}

CKERROR CKPathManager::RemoveCategory(int catIdx) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    m_Categories.RemoveAt(catIdx);
    return CK_OK;
}

int CKPathManager::GetCategoryCount() {
    return m_Categories.Size();
}

CKERROR CKPathManager::GetCategoryName(int catIdx, XString &catName) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    catName = m_Categories[catIdx].m_Name;
    return CK_OK;
}

int CKPathManager::GetCategoryIndex(XString &cat) {
    if (m_Categories.Size() <= 0) {
        return -1;
    }

    for (int i = 0; i < m_Categories.Size(); i++) {
        if (m_Categories[i].m_Name == cat) {
            return i;
        }
    }
    return -1;
}

CKERROR CKPathManager::RenameCategory(int catIdx, XString &newName) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    const int duplicateIdx = GetCategoryIndex(newName);
    if (duplicateIdx != -1 && duplicateIdx != catIdx) {
        return CKERR_ALREADYPRESENT;
    }

    m_Categories[catIdx].m_Name = newName;
    return CK_OK;
}

int CKPathManager::AddPath(int catIdx, XString &path) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return -1;
    }

    if (GetPathIndex(catIdx, path) != -1) {
        return -1;
    }

    m_Categories[catIdx].m_Entries.PushBack(path);
    return m_Categories[catIdx].m_Entries.Size() - 1;
}

CKERROR CKPathManager::RemovePath(int catIdx, int pathIdx) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    if (pathIdx < 0 || pathIdx >= m_Categories[catIdx].m_Entries.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    m_Categories[catIdx].m_Entries.RemoveAt(pathIdx);
    return CK_OK;
}

CKERROR CKPathManager::SwapPaths(int catIdx, int pathIdx1, int pathIdx2) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    if (pathIdx1 < 0 || pathIdx1 >= m_Categories[catIdx].m_Entries.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    if (pathIdx2 < 0 || pathIdx2 >= m_Categories[catIdx].m_Entries.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    XString tmp = m_Categories[catIdx].m_Entries[pathIdx1];
    m_Categories[catIdx].m_Entries[pathIdx1] = m_Categories[catIdx].m_Entries[pathIdx2];
    m_Categories[catIdx].m_Entries[pathIdx2] = tmp;
    return CK_OK;
}

int CKPathManager::GetPathCount(int catIdx) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return 0;
    }
    return m_Categories[catIdx].m_Entries.Size();
}

CKERROR CKPathManager::GetPathName(int catIdx, int pathIdx, XString &path) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    if (pathIdx < 0 || pathIdx >= m_Categories[catIdx].m_Entries.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    path = m_Categories[catIdx].m_Entries[pathIdx];
    return CK_OK;
}

int CKPathManager::GetPathIndex(int catIdx, XString &path) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return -1;
    }
    for (int i = 0; i < m_Categories[catIdx].m_Entries.Size(); i++) {
        if (m_Categories[catIdx].m_Entries[i] == path) {
            return i;
        }
    }
    return -1;
}

CKERROR CKPathManager::RenamePath(int catIdx, int pathIdx, XString &path) {
    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    if (pathIdx < 0 || pathIdx >= m_Categories[catIdx].m_Entries.Size()) {
        return CKERR_INVALIDPARAMETER;
    }
    m_Categories[catIdx].m_Entries[pathIdx] = path;
    return CK_OK;
}

CKERROR CKPathManager::ResolveFileName(XString &file, int catIdx, int startIdx) {
    if (file.Length() <= 0) {
        return CKERR_INVALIDFILE;
    }
    if (startIdx < -1) {
        return CKERR_INVALIDPARAMETER;
    }

    // If starting index is unspecified, check special locations first
    if (startIdx == -1) {
        // Check absolute paths
        if (PathIsAbsolute(file)) {
            FILE* fp = fopen(file.CStr(), "rb");
            if (fp) {
                fclose(fp);
                return CK_OK;
            }
        }

        // Handle URL paths
        if (PathIsURL(file)) {
            AddEscapedSpace(file);
            return CK_OK;
        }

        // Check existing file URLs
        if (PathIsFile(file)) {
            XString localFilePath;
            if (TryDecodeFileUri(file, localFilePath) && TryOpenAbsolutePath(localFilePath)) {
                file = localFilePath;
                return CK_OK;
            }
        }

        // Check UNC paths
        if (PathIsUNC(file)) {
            return CK_OK;
        }

        // Check application start path
        XString startPath = XString(CKGetStartPath()) + file;
        if (TryOpenAbsolutePath(startPath)) {
            file = startPath;
            return CK_OK;
        }

        // Check directory of last loaded CMO file
        CKPathSplitter cmoSplitter(m_Context->GetLastCmoLoaded());
        CKPathMaker cmoMaker(cmoSplitter.GetDrive(), cmoSplitter.GetDir(), file.Str(), nullptr);
        XString cmoPath = cmoMaker.GetFileName();
        if (TryOpenAbsolutePath(cmoPath)) {
            file = cmoPath;
            return CK_OK;
        }

        // Check current working directory
        char curDir[4096];
        VxGetCurrentDirectory(curDir);
        CKPathMaker curDirMaker(nullptr, curDir, file.Str(), nullptr);
        XString curPath = curDirMaker.GetFileName();
        if (TryOpenAbsolutePath(curPath)) {
            file = curPath;
            return CK_OK;
        }

        // Check Virtools temporary folder
        XString tempFolder = GetVirtoolsTemporaryFolder();
        CKPathMaker tempMaker(nullptr, tempFolder.Str(), file.Str(), nullptr);
        XString tempPath = tempMaker.GetFileName();
        if (TryOpenAbsolutePath(tempPath)) {
            file = tempPath;
            return CK_OK;
        }

        // Fall back to category path search
        startIdx = 0;
    }

    // Split filename into components
    CKPathSplitter fileSplitter(file.Str());
    XString baseName = fileSplitter.GetName();
    baseName += fileSplitter.GetExtension();

    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    // Get category information
    CKPATHCATEGORY &category = m_Categories[catIdx];
    const int pathCount = category.m_Entries.Size();

    // Search through category paths
    for (int i = startIdx; i < pathCount; ++i) {
        XString &pathEntry = category.m_Entries[i];
        char path[4096];
        if (!VxMakePath(path, pathEntry.Str(), baseName.Str()))
            continue;

        // Handle different path types
        if (PathIsAbsolute(pathEntry) || PathIsUNC(pathEntry)) {
            RemoveEscapedSpace(path);
            XString fullPath = path;
            if (TryOpenAbsolutePath(fullPath)) {
                file = fullPath;
                return CK_OK;
            }
        } else if (PathIsFile(pathEntry)) {
            RemoveEscapedSpace(path);
            XString fullPath = path;
            if (TryOpenFilePath(fullPath)) {
                XString localFilePath;
                if (TryDecodeFileUri(fullPath, localFilePath)) {
                    file = localFilePath;
                } else {
                    file = fullPath;
                }
                return CK_OK;
            }
        } else if (PathIsURL(pathEntry)) {
            XString fullPath = path;
            AddEscapedSpace(fullPath);
            if (TryOpenURLPath(fullPath)) {
                file = fullPath;
                return CK_OK;
            }
        }
    }

    return CKERR_NOTFOUND;
}

CKBOOL CKPathManager::PathIsAbsolute(XString &file) {
    if (file.Length() < 3)
        return FALSE;
    if (file[1] == ':' && file[2] == '\\')
        return TRUE;
    if (file[1] == ':' && file[2] == '/')
        return TRUE;
    return FALSE;
}

CKBOOL CKPathManager::PathIsUNC(XString &file) {
    if (file.Length() < 2)
        return FALSE;
    if (file[0] == '\\' && file[1] == '\\')
        return TRUE;
    if (file[0] == '/' && file[1] == '/')
        return TRUE;
    return FALSE;
}

CKBOOL CKPathManager::PathIsURL(XString &file) {
    if (file.Length() <= 0)
        return FALSE;
    if (strncmp(file.Str(), "http:", 5) == 0)
        return TRUE;
    if (strncmp(file.Str(), "ftp:", 4) == 0)
        return TRUE;
    if (strncmp(file.Str(), "javascript:", 11) == 0)
        return TRUE;
    return FALSE;
}

CKBOOL CKPathManager::PathIsFile(XString &file) {
    if (file.Length() <= 0)
        return FALSE;
    if (strncmp(file.Str(), "file:", 5) == 0)
        return TRUE;
    return FALSE;
}

void CKPathManager::RemoveEscapedSpace(char *str) {
    XString tmp = str;
    VxUnEscapeUrl(tmp);
    strcpy(str, tmp.Str());
}

void CKPathManager::AddEscapedSpace(XString &str) {
    XString tmp = str;
    VxEscapeURL(tmp.Str(), str);
}

XString CKPathManager::GetVirtoolsTemporaryFolder() {
    if (!m_TemporaryFolderExist) {
        VxMakeDirectory(m_TemporaryFolder.Str());
        m_TemporaryFolderExist = TRUE;
    }
    return m_TemporaryFolder;
}

void CKPathManager::Clean() {
    m_Categories.Clear();
}

void CKPathManager::RemoveSpace(char *str) {
    RemoveEscapedSpace(str);
}

CKBOOL CKPathManager::TryOpenAbsolutePath(XString &file) {
    FILE *fp = fopen(file.CStr(), "rb");
    if (fp) {
        fclose(fp);
        return TRUE;
    }
    return FALSE;
}

CKBOOL CKPathManager::TryOpenFilePath(XString &file) {
    XString path;
    if (!TryDecodeFileUri(file, path)) {
        return FALSE;
    }
    return TryOpenAbsolutePath(path);
}

CKBOOL CKPathManager::TryOpenURLPath(XString &file) {
    return TRUE;
}
