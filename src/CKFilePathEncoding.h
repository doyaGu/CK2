#ifndef CKFILEPATHENCODING_H
#define CKFILEPATHENCODING_H

#include <ctype.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static inline const char *CKNextFilePathCharacter(const char *path) {
    if (!*path) return path;
#ifdef _WIN32
    if (IsDBCSLeadByteEx(GetACP(), (BYTE)*path) && path[1]) {
        wchar_t character;
        if (MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, path, 2, &character, 1) == 1)
            return path + 2;
    }
#endif
    return path + 1;
}

static inline int CKFilePathICompare(const char *left, const char *right) {
    while (*left && *right) {
        const char *nextLeft = CKNextFilePathCharacter(left), *nextRight = CKNextFilePathCharacter(right);
        int length = (int)(nextLeft - left);
        if (length != nextRight - right) return length - (int)(nextRight - right);
        int difference = length > 1 ? memcmp(left, right, length)
            : tolower((unsigned char)*left) - tolower((unsigned char)*right);
        if (difference) return difference;
        left = nextLeft; right = nextRight;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

#endif // CKFILEPATHENCODING_H
