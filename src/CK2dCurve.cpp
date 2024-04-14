#include "CK2dCurve.h"
#include "CKStateChunk.h"

CKERROR CK2dCurve::GetPos(float step, Vx2DVector *pos) {
    return 0;
}

float CK2dCurve::GetY(float X) {
    return 0;
}

void CK2dCurve::AddControlPoint(const Vx2DVector &pos) {

}

void CK2dCurve::Update() {

}

CK2dCurve::CK2dCurve() {
    m_Length = 0.0f;
    FittingCoef = 0.0f;
    AddControlPoint(Vx2DVector(0.0f, 0.0f));
    AddControlPoint(Vx2DVector(1.0f, 1.0f));
}

CKStateChunk *CK2dCurve::Dump() {
    CKStateChunk *chunk = new CKStateChunk(CKCHNK_2DCURVE, NULL);
    chunk->StartWrite();
    chunk->SetDataVersion(CHUNK_DEV_2_1);
    chunk->WriteIdentifier(0x100);

    XSObjectArray objects;
    objects.Resize(m_ControlPoints.Size() / sizeof(CK2dCurvePoint));
    objects.Fill(NULL);
    objects.Save(chunk, NULL);

    chunk->WriteFloat(FittingCoef);
    for (XArray<CK2dCurvePoint>::Iterator it = m_ControlPoints.Begin(); it != m_ControlPoints.End(); it++)
    {
        chunk->WriteInt(it->m_Flags | 0x10000000);
        chunk->WriteFloat(it->m_Position.x);
        chunk->WriteFloat(it->m_Position.y);
        chunk->WriteFloat(it->m_Tension);
        chunk->WriteFloat(it->m_Continuity);
        chunk->WriteFloat(it->m_Bias);
        chunk->WriteFloat(0.0f);
        chunk->WriteFloat(0.0f);
        chunk->WriteFloat(it->m_InTang.x);
        chunk->WriteFloat(it->m_InTang.y);
        chunk->WriteFloat(it->m_OutTang.x);
        chunk->WriteFloat(it->m_OutTang.y);
    }

    chunk->CloseChunk();
    return chunk;
}

CKERROR CK2dCurve::Read(CKStateChunk *chunk) {
    return 0;
}

void CK2dCurve::UpdatePointsAndTangents() {

}
