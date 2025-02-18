#ifndef CKCALLBACKFUNCTIONS_H
#define CKCALLBACKFUNCTIONS_H

#include "CKDefines.h"

void CKInitializeParameterTypes(CKParameterManager *man);

CKERROR CKStructCreator(CKParameter *param);

void CKStructDestructor(CKParameter *param);

void CKStructSaver(CKParameter *param, CKStateChunk **chunk, CKBOOL load);

void CKStructCopier(CKParameter *dest, CKParameter *src);

int CKStructStringFunc(CKParameter *param, char *valueStr, CKBOOL readFromStr);

void CK_ParameterCopier_Memcpy(CKParameter *dest, CKParameter *src);

void CK_ParameterCopier_SaveLoad(CKParameter *dest, CKParameter *src);

void CK_ParameterCopier_SetValue(CKParameter *dest, CKParameter *src);

void CK_ParameterCopier_Dword(CKParameter *dest, CKParameter *src);

int CKIntStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString);

int CKEnumStringFunc(CKParameter *param, char *ValueString, CKBOOL ReadFromString);

int CKFlagsStringFunc(CKParameter *param, CKSTRING ValueString, CKBOOL ReadFromString);

CK_PARAMETERUICREATORFUNCTION GetUICreatorFunction(CKContext *context, CKParameterTypeDesc *desc);

#endif // CKCALLBACKFUNCTIONS_H