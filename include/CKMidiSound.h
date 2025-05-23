#ifndef CKMIDISOUND_H
#define CKMIDISOUND_H

#include "CKSound.h"

class CKMidiManager;

/**************************************************************************
Name: CKMidiSound
Summary: Midi sound.

Remarks:
+ CKMidiSound provides the basic methods for playing a midi sound file.
+ Its class id is CKCID_MIDISOUND
See also: CKWaveSound,CKSoundManager
******************************************************************************/
class CKMidiSound : public CKSound
{
public:
    //---------------------------------------------------
    // Sound File
    DLL_EXPORT CKERROR SetSoundFileName(CKSTRING filename);
    DLL_EXPORT CKSTRING GetSoundFileName();

    //----------------------------------------------------
    // Current Position In MilliSeconds
    DLL_EXPORT CKDWORD GetCurrentPos();

    //-----------------------------------------------------
    // Sound File
    DLL_EXPORT CKERROR Play();
    DLL_EXPORT CKERROR Stop();
    DLL_EXPORT CKERROR Pause(CKBOOL pause = TRUE);
    DLL_EXPORT CKBOOL IsPlaying();
    DLL_EXPORT CKBOOL IsPaused();

    //-------------------------------------------------------------------

    CKMidiSound(CKContext *Context, CKSTRING name = NULL);
    virtual ~CKMidiSound();
    virtual CK_CLASSID GetClassID();

    virtual CKStateChunk *Save(CKFile *file, CKDWORD flags);
    virtual CKERROR Load(CKStateChunk *chunk, CKFile *file);

    virtual int GetMemoryOccupation();

    //--------------------------------------------
    // Class Registering
    static CKSTRING GetClassName();
    static int GetDependenciesCount(int mode);
    static CKSTRING GetDependencies(int i, int mode);
    static void Register();
    static CKMidiSound *CreateInstance(CKContext *Context);
    static CK_CLASSID m_ClassID;

    // Dynamic Cast method (returns NULL if the object can't be casted)
    static CKMidiSound *Cast(CKObject *iO)
    {
        return CKIsChildClassOf(iO, CKCID_MIDISOUND) ? (CKMidiSound *)iO : NULL;
    }

protected:
    void *m_Source;
    CKMidiManager *m_MidiManager;
};

#endif // CKMIDISOUND_H
