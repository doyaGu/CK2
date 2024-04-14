#include "CK2dCurvePoint.h"
#include "CK2dCurve.h"
#include "CKStateChunk.h"

void CK2dCurvePoint::NotifyUpdate() {
    m_Curve->Update();
}

void CK2dCurvePoint::Read(CKStateChunk *chunk) {
    if (!chunk) return;

    if (chunk->SeekIdentifier(CK_STATESAVE_2DCURVEPOINTDEFAULTDATA)) {
        chunk->ReadObjectID();
        if (chunk->ReadInt() != 0)
            m_Flags |= CK2DCURVEPOINT_USETANGENTS;
        else
            m_Flags &= ~CK2DCURVEPOINT_USETANGENTS;
        if (chunk->ReadInt() != 0)
            m_Flags |= CK2DCURVEPOINT_LINEAR;
        else
            m_Flags &= ~CK2DCURVEPOINT_LINEAR;
        NotifyUpdate();
        chunk->ReadAndFillBuffer_LEndian(8, &m_Position);
    }

    if (chunk->SeekIdentifier(CK_STATESAVE_2DCURVEPOINTTCB)) {
        m_Tension = chunk->ReadFloat();
        m_Continuity = chunk->ReadFloat();
        m_Bias = chunk->ReadFloat();
    }

    if (chunk->SeekIdentifier(CK_STATESAVE_2DCURVEPOINTTANGENTS)) {
        chunk->ReadAndFillBuffer_LEndian(8, &m_InTang);
        chunk->ReadAndFillBuffer_LEndian(8, &m_OutTang);
    }
}