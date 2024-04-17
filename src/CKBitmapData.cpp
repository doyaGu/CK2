//#include "CKBitmapData.h"
//#include "CKGlobals.h"
//#include "CKPluginManager.h"
//#include "VxMath.h"
//
//CKMovieInfo::CKMovieInfo(const XString &FileName)
//        : m_MovieFileName(FileName),
//          m_MovieReader(NULL),
//          m_MovieCurrentSlot(-1) {}
//
//CKMovieInfo::~CKMovieInfo() {
//    if (m_MovieReader)
//        m_MovieReader->Release();
//}
//
//CKBOOL CKBitmapData::CreateImage(int Width, int Height, int BPP, int Slot) {
//    if ((m_BitmapFlags & CKBITMAPDATA_INVALID) != 0 || m_Slots.Size() <= 1 && Slot == 0) {
//        m_Width = Width;
//        m_Height = Height;
//        m_BitmapFlags &= ~CKBITMAPDATA_INVALID;
//    }
//
//    if (m_Width != Width || m_Height != Height) {
//        return FALSE;
//    } else if (m_Width == 0 || m_Height == 0) {
//        m_BitmapFlags |= CKBITMAPDATA_INVALID;
//        return FALSE;
//    }
//
//    const int count = m_Slots.Size();
//    if (Slot >= count) {
//        m_Slots.Resize(Slot + 1);
//        for (int i = count; i <= Slot; ++i) {
//            m_Slots[i] = new CKBitmapSlot;
//        }
//    }
//
//    CKBitmapSlot *bitmap = m_Slots[Slot];
//    bitmap->m_FileName = "";
//    VxDeleteAligned(bitmap->m_DataBuffer);
//    bitmap->m_DataBuffer = (CKDWORD *)VxNewAligned(4 * m_Height * m_Width, 16);
//    const int dwordCount = m_Width * m_Height;
//    for (int i = 0; i < dwordCount; ++i) {
//        bitmap->m_DataBuffer[i] = A_MASK;
//    }
//
//    return TRUE;
//}
//
//CKBOOL CKBitmapData::SaveImage(CKSTRING Name, int Slot, CKBOOL CKUseFormat) {
//    return 0;
//}
//
//CKBOOL CKBitmapData::SaveImageAlpha(CKSTRING Name, int Slot) {
//    return 0;
//}
//
//CKSTRING CKBitmapData::GetMovieFileName() {
//    if (!m_MovieInfo)
//        return NULL;
//
//    return m_MovieInfo->m_MovieFileName.Str();
//}
//
//CKBYTE *CKBitmapData::LockSurfacePtr(int Slot) {
//    if (0 < Slot && Slot < m_Slots.Size())
//        m_CurrentSlot = Slot;
//
//    if (m_Slots.IsEmpty())
//        return NULL;
//
//    return (CKBYTE *) m_Slots[m_CurrentSlot]->m_DataBuffer;
//}
//
//CKBOOL CKBitmapData::ReleaseSurfacePtr(int Slot) {
//    m_BitmapFlags |= CKBITMAPDATA_FORCERESTORE;
//    return TRUE;
//}
//
//CKSTRING CKBitmapData::GetSlotFileName(int Slot) {
//    if (Slot < 0 || Slot >= m_Slots.Size())
//        return NULL;
//
//    return m_Slots[Slot]->m_FileName.Str();
//}
//
//CKBOOL CKBitmapData::SetSlotFileName(int Slot, CKSTRING Filename) {
//    if (Slot < 0 || Slot >= m_Slots.Size() || !Filename)
//        return FALSE;
//
//    m_Slots[Slot]->m_FileName = Filename;
//}
//
//CKBOOL CKBitmapData::GetImageDesc(VxImageDescEx &oDesc) {
//    oDesc.AlphaMask = A_MASK;
//    oDesc.RedMask = R_MASK;
//    oDesc.GreenMask = G_MASK;
//    oDesc.BlueMask = B_MASK;
//    oDesc.Width = m_Width;
//    oDesc.Height = m_Height;
//    oDesc.BitsPerPixel = 32;
//    oDesc.BytesPerLine = 4 * m_Width;
//    return TRUE;
//}
//
//int CKBitmapData::GetSlotCount() {
//    if (!m_MovieInfo)
//        return m_Slots.Size();
//
//    CKMovieReader *reader = m_MovieInfo->m_MovieReader;
//    if (!reader)
//        return 0;
//
//    return reader->GetMovieFrameCount();
//}
//
//CKBOOL CKBitmapData::SetSlotCount(int Count) {
//    if (m_MovieInfo)
//        return FALSE;
//
//    const int slotCount = m_Slots.Size();
//    if (Count < slotCount) {
//        for (int i = Count; i < slotCount; ++i) {
//            CKBitmapSlot *slot = m_Slots[i];
//            delete slot;
//        }
//    }
//
//    m_Slots.Resize(Count);
//
//    for (int i = slotCount; i < Count; ++i) {
//        m_Slots[i] = new CKBitmapSlot;
//    }
//
//    if (Count == 0)
//        m_BitmapFlags |= CKBITMAPDATA_INVALID;
//
//    return TRUE;
//}
//
//CKBOOL CKBitmapData::SetCurrentSlot(int Slot) {
//    return 0;
//}
//
//int CKBitmapData::GetCurrentSlot() {
//    if (m_MovieInfo)
//        return m_MovieInfo->m_MovieCurrentSlot;
//    else
//        return m_CurrentSlot;
//}
//
//CKBOOL CKBitmapData::ReleaseSlot(int Slot) {
//    return 0;
//}
//
//CKBOOL CKBitmapData::ReleaseAllSlots() {
//    return 0;
//}
//
//CKBOOL CKBitmapData::SetPixel(int x, int y, CKDWORD col, int slot) {
//    int currentSlot = (slot >= 0) ? slot : m_CurrentSlot;
//    if (currentSlot >= m_Slots.Size())
//        return FALSE;
//
//    CKDWORD *buffer = m_Slots[currentSlot]->m_DataBuffer;
//    if (!buffer)
//        return FALSE;
//
//    buffer[x + y * m_Width] = col;
//}
//
//CKDWORD CKBitmapData::GetPixel(int x, int y, int slot) {
//    int currentSlot = (slot >= 0) ? slot : m_CurrentSlot;
//    if (currentSlot < m_Slots.Size())
//        return 0;
//
//    CKDWORD *buffer = m_Slots[currentSlot]->m_DataBuffer;
//    if (!buffer)
//        return 0;
//
//    return buffer[x + y * m_Width];
//}
//
//void CKBitmapData::SetTransparentColor(CKDWORD Color) {
//
//}
//
//void CKBitmapData::SetTransparent(CKBOOL Transparency) {
//
//}
//
//void CKBitmapData::SetSaveFormat(CKBitmapProperties *format) {
//
//}
//
//CKBOOL CKBitmapData::ResizeImages(int Width, int Height) {
//    return 0;
//}
//
//CKBOOL CKBitmapData::LoadSlotImage(const XString &Name, int Slot) {
//    CKPathSplitter splitter((CKSTRING) Name.CStr());
//    CKSTRING extStr = splitter.GetExtension();
//    CKPluginManager *pm = CKGetPluginManager();
//    CKFileExtension extension(extStr + 1);
//    CKBitmapReader *reader = pm->GetBitmapReader(extension);
//    if (!reader)
//        return FALSE;
//
//    CKBitmapProperties *properties = NULL;
//    if (reader->ReadFile((CKSTRING) Name.CStr(), &properties) != 0 || properties || properties->m_Data) {
//        reader->Release();
//        return FALSE;
//    }
//
//    const int slotCount = GetSlotCount();
//    if (Slot > slotCount)
//        SetSlotCount(Slot + 1);
//
//    if (!CreateImage(properties->m_Format.Width, properties->m_Format.Height, 32, Slot)) {
//        SetSlotCount(slotCount);
//        reader->Release();
//        return FALSE;
//    }
//
//    CKBYTE *data = LockSurfacePtr(Slot);
//    if (!data) {
//        reader->ReleaseMemory(properties->m_Data);
//        properties->m_Data = NULL;
//
//        delete properties->m_Format.ColorMap;
//        properties->m_Format.ColorMap = NULL;
//
//        reader->Release();
//        return FALSE;
//    }
//
//    VxImageDescEx desc;
//    GetImageDesc(desc);
//
//    properties->m_Format.Image = (XBYTE *) properties->m_Data;
//    desc.Image = data;
//
//    VxDoBlit(properties->m_Format, desc);
//
//    if (properties->m_Format.AlphaMask == 0)
//        VxDoAlphaBlit(desc, 0xFF);
//
//    SetSlotFileName(Slot, (CKSTRING) Name.CStr());
//    SetSaveFormat(properties);
//
//    reader->ReleaseMemory(properties->m_Data);
//    properties->m_Data = NULL;
//
//    delete properties->m_Format.ColorMap;
//    properties->m_Format.ColorMap = NULL;
//
//    reader->Release();
//    return TRUE;
//}
//
//CKBOOL CKBitmapData::LoadMovieFile(const XString &Name) {
//    return 0;
//}
//
//CKMovieInfo *CKBitmapData::CreateMovieInfo(const XString &s, CKMovieProperties **mp) {
//    return nullptr;
//}
//
//void CKBitmapData::SetMovieInfo(CKMovieInfo *mi) {
//    if (m_MovieInfo)
//        delete m_MovieInfo;
//    m_MovieInfo = mi;
//}
//
//CKBitmapData::CKBitmapData() {
//    m_Width = -1;
//    m_Height = -1;
//    m_MovieInfo = NULL;
//    m_PickThreshold = 0;
//    m_BitmapFlags = 1;
//    m_TransColor = 0;
//    m_CurrentSlot = 0;
//    m_SaveProperties = NULL;
//    m_SaveOptions = CKTEXTURE_USEGLOBAL;
//}
//
//CKBitmapData::~CKBitmapData() {
//    if (m_MovieInfo) {
//        delete m_MovieInfo;
//        m_MovieInfo = NULL;
//    }
//
//    ReleaseAllSlots();
//    CKDeletePointer(m_SaveProperties);
//}
//
//void CKBitmapData::SetAlphaForTransparentColor(const VxImageDescEx &desc) {
//
//}
//
//void CKBitmapData::SetBorderColorForClamp(const VxImageDescEx &desc) {
//
//}
//
//CKBOOL CKBitmapData::SetSlotImage(int Slot, void *buffer, VxImageDescEx &bdesc) {
//    if (!buffer)
//        return FALSE;
//
//    if (!CreateImage(bdesc.Width, bdesc.Height, 0, Slot))
//        return FALSE;
//
//    VxImageDescEx desc;
//    GetImageDesc(desc);
//
//    CKBYTE *data = LockSurfacePtr(Slot);
//    if (!data)
//        return FALSE;
//
//    desc.Image = data;
//    bdesc.Image = (XBYTE *) buffer;
//    VxDoBlit(bdesc, desc);
//
//    m_BitmapFlags |= CKBITMAPDATA_FORCERESTORE;
//    delete (XBYTE *) buffer;
//    delete bdesc.ColorMap;
//    return TRUE;
//}
//
//CKBOOL CKBitmapData::DumpToChunk(CKStateChunk *chnk, CKContext *ctx, CKFile *f, CKDWORD Identifiers[4]) {
//    return 0;
//}
//
//CKBOOL CKBitmapData::ReadFromChunk(CKStateChunk *chnk, CKContext *ctx, CKFile *f, CKDWORD Identifiers[5]) {
//    return 0;
//}
