#include "CKPathManager.h"

#include "VxWindowFunctions.h"
#include "CKGlobals.h"
#include "CKContext.h"

static void NormalizeNativePathSeparators(XString &path) {
    for (int i = 0; i < path.Length(); ++i) {
#ifdef _WIN32
        if (path[i] == '/')
            path[i] = '\\';
#else
        if (path[i] == '\\')
            path[i] = '/';
#endif
    }
}

#ifndef _WIN32
struct FindDirectoryEntryData {
    const char *Name;
    XBOOL WantDirectory;
    XBOOL Found;
    XString Match;
};

static XBOOL FindDirectoryEntryCallback(const VxDirectoryEntry *entry, void *userData) {
    FindDirectoryEntryData *data = (FindDirectoryEntryData *)userData;
    if (!entry || !data || !data->Name)
        return TRUE;
    if (entry->IsDirectory != data->WantDirectory)
        return TRUE;

    XString entryName = entry->Name;
    XString requestedName = data->Name;
    if (entryName.Compare(requestedName) == 0) {
        data->Match = entry->Name;
        data->Found = TRUE;
    } else if (!data->Found && entryName.ICompare(requestedName) == 0) {
        data->Match = entry->Name;
        data->Found = TRUE;
    }

    return TRUE;
}

static CKBOOL FindDirectoryEntry(const char *directory, const XString &name, XBOOL wantDirectory, XString &match) {
    FindDirectoryEntryData data;
    data.Name = name.CStr();
    data.WantDirectory = wantDirectory;
    data.Found = FALSE;

    VxListDirectory(directory, "*", TRUE, FindDirectoryEntryCallback, &data);
    if (!data.Found)
        return FALSE;

    match = data.Match;
    return TRUE;
}

static void RemoveLastNativePathComponent(XString &path) {
    if (path.Length() <= 1) {
        path = "/";
        return;
    }

    if (path.Back() == '/')
        path.PopBack();

    XWORD slash = path.RFind('/');
    if (slash == XString::NOTFOUND || slash == 0) {
        path = "/";
    } else {
        path.Crop(0, slash);
    }
}

static CKBOOL ResolveCaseInsensitiveFilePathFromDirectory(const char *directory, const char *file, XString &resolved) {
    if (!directory || !file || !VxDirectoryExists(directory))
        return FALSE;

    XString current = directory;
    NormalizeNativePathSeparators(current);

    XString relative = file;
    NormalizeNativePathSeparators(relative);

    const char *part = relative.CStr();
    while (*part) {
        while (*part == '/')
            ++part;
        if (!*part)
            break;

        const char *end = part;
        while (*end && *end != '/')
            ++end;

        XString name(part, (int)(end - part));
        part = end;
        if (name == ".")
            continue;
        if (name == "..") {
            RemoveLastNativePathComponent(current);
            continue;
        }

        const XBOOL wantDirectory = (*part != '\0') ? TRUE : FALSE;
        XString match;
        if (!FindDirectoryEntry(current.CStr(), name, wantDirectory, match))
            return FALSE;

        char next[_MAX_PATH];
        if (!VxMakePath(next, current.CStr(), match.CStr()))
            return FALSE;
        current = next;
    }

    if (!VxFileExists(current.CStr()))
        return FALSE;

    resolved = current;
    return TRUE;
}

static CKBOOL ResolveCaseInsensitiveFilePath(const char *path, XString &resolved) {
    if (!path || path[0] != '/')
        return FALSE;

    return ResolveCaseInsensitiveFilePathFromDirectory("/", path + 1, resolved);
}
#endif

static CKBOOL MakeNativePathCandidate(XString &candidate, const char *directory, const char *file) {
    char path[_MAX_PATH];
    if (!VxMakePath(path, directory ? directory : "", file ? file : ""))
        return FALSE;

    candidate = path;
    NormalizeNativePathSeparators(candidate);
    return TRUE;
}

static CKBOOL MakeLogicalPathCandidate(XString &candidate, const char *directory, const char *file) {
    char path[_MAX_PATH];
    if (!VxMakePath(path, directory ? directory : "", file ? file : ""))
        return FALSE;

    candidate = path;
    return TRUE;
}

static CKBOOL ResolveNativePathCandidate(XString &candidate, const char *directory, const char *file, XBOOL unescape) {
    XString base = directory ? directory : "";
    XString name = file ? file : "";
    if (unescape) {
        VxUnEscapeUrl(base);
        VxUnEscapeUrl(name);
    }

    if (!MakeNativePathCandidate(candidate, base.CStr(), name.CStr()))
        return FALSE;

    if (VxFileExists(candidate.CStr()))
        return TRUE;

#ifndef _WIN32
    XString resolved;
    if (ResolveCaseInsensitiveFilePathFromDirectory(base.CStr(), name.CStr(), resolved)) {
        candidate = resolved;
        return TRUE;
    }
#endif

    return FALSE;
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

    XString filesystemFile = file;
    NormalizeNativePathSeparators(filesystemFile);

    // If starting index is unspecified, check special locations first
    if (startIdx == -1) {
        // Check absolute paths
        if (PathIsAbsolute(filesystemFile)) {
            if (ResolveNativeFilePath(filesystemFile)) {
                file = filesystemFile;
                return CK_OK;
            }
        }

        // Handle URL paths
        if (PathIsURL(file)) {
            AddEscapedSpace(file);
            return CK_OK;
        }

        // Check existing files/UNC paths
        if (PathIsFile(file) || PathIsUNC(filesystemFile)) {
            if (!PathIsFile(file))
                file = filesystemFile;
            return CK_OK;
        }

        // Check application start path
        XString startPath;
        if (ResolveNativePathCandidate(startPath, CKGetStartPath(), filesystemFile.Str(), FALSE)) {
            file = startPath;
            return CK_OK;
        }

        // Check directory of last loaded CMO file
        CKPathSplitter cmoSplitter(m_Context->GetLastCmoLoaded());
        CKPathMaker cmoDir(cmoSplitter.GetDrive(), cmoSplitter.GetDir(), nullptr, nullptr);
        XString cmoPath;
        if (ResolveNativePathCandidate(cmoPath, cmoDir.GetFileName(), filesystemFile.Str(), FALSE)) {
            file = cmoPath;
            return CK_OK;
        }

        // Check current working directory
        char curDir[_MAX_PATH];
        if (VxGetCurrentDirectory(curDir)) {
            XString curPath;
            if (ResolveNativePathCandidate(curPath, curDir, filesystemFile.Str(), FALSE)) {
                file = curPath;
                return CK_OK;
            }
        }

        // Check Virtools temporary folder
        XString tempFolder = GetVirtoolsTemporaryFolder();
        XString tempPath;
        if (ResolveNativePathCandidate(tempPath, tempFolder.Str(), filesystemFile.Str(), FALSE)) {
            file = tempPath;
            return CK_OK;
        }

        // Fall back to category path search
        startIdx = 0;
    }

    // Split filename into components
    CKPathSplitter fileSplitter(filesystemFile.Str());
    XString baseName = fileSplitter.GetName();
    baseName += fileSplitter.GetExtension();
    XString searchName = filesystemFile;
    if (PathIsAbsolute(filesystemFile) || PathIsUNC(filesystemFile) || searchName.Length() == 0)
        searchName = baseName;

    if (catIdx < 0 || catIdx >= m_Categories.Size()) {
        return CKERR_INVALIDPARAMETER;
    }

    // Get category information
    CKPATHCATEGORY &category = m_Categories[catIdx];
    const int pathCount = category.m_Entries.Size();

    // Search through category paths
    for (int i = startIdx; i < pathCount; ++i) {
        XString &pathEntry = category.m_Entries[i];

        // Handle different path types
        if (PathIsAbsolute(pathEntry) || PathIsUNC(pathEntry)) {
            XString path;
            if (ResolveNativePathCandidate(path, pathEntry.Str(), searchName.Str(), TRUE)) {
                file = path;
                return CK_OK;
            }
        } else if (PathIsFile(pathEntry)) {
            XString path;
            if (!MakeLogicalPathCandidate(path, pathEntry.Str(), searchName.Str()))
                continue;
            RemoveEscapedSpace(path.Str());
            XString fullPath = path;
            if (TryOpenFilePath(fullPath)) {
                file = &fullPath[(int)sizeof("file://") - 1];
                return CK_OK;
            }
        } else if (PathIsURL(pathEntry)) {
            XString path;
            if (!MakeLogicalPathCandidate(path, pathEntry.Str(), searchName.Str()))
                continue;
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
    if (file.Length() <= 0)
        return FALSE;
#ifndef _WIN32
    if (file[0] == '/')
        return TRUE;
#endif
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

CKBOOL CKPathManager::ResolveNativeFilePath(XString &file) {
    NormalizeNativePathSeparators(file);
    if (VxFileExists(file.CStr()))
        return TRUE;
#ifndef _WIN32
    XString resolved;
    if (ResolveCaseInsensitiveFilePath(file.CStr(), resolved)) {
        file = resolved;
        return TRUE;
    }
#endif
    return FALSE;
}

CKBOOL CKPathManager::TryOpenFilePath(XString &file) {
    XString path = &file[(int)(sizeof("file://") - 1)];
    if (!ResolveNativeFilePath(path))
        return FALSE;

    file = "file://";
    file += path;
    return TRUE;
}

CKBOOL CKPathManager::TryOpenURLPath(XString &file) {
    return TRUE;
}
