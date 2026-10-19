#ifndef AVI_READER_MIXED_INDEX_DATA_H
#define AVI_READER_MIXED_INDEX_DATA_H
#include "AVIReaderContinuationData.h"
struct AviMixedIndexData : AviReaderTestData {
    unsigned Payloads[80]={}, HeaderIndex=0, Classic=0, Fake=0, Count=0;
    unsigned Read(unsigned at) const { return unsigned(Data[at])|(unsigned(Data[at+1])<<8)|(unsigned(Data[at+2])<<16)|(unsigned(Data[at+3])<<24); }
    void Header(unsigned entries, bool shortHeader=false) {
        const unsigned insertion=Movi,extra=shortHeader?12:32+entries*8;
        std::memmove(Data+insertion+extra,Data+insertion,Size-insertion); std::memset(Data+insertion,0,extra);
        Size+=extra; Movi+=extra; Word(16,Read(16)+extra); Word(92,Read(92)+extra);
        HeaderIndex=insertion; std::memcpy(Data+insertion,"indx",4); Word(insertion+4,extra-8);
        if(!shortHeader) { Word(insertion+8,2,2); Word(insertion+11,1,1); Word(insertion+12,entries); std::memcpy(Data+insertion+16,"00db",4); }
    }
    void Emit(unsigned value, const char *id="00db") {
        Payloads[Count++]=Size+8; unsigned at=Begin(id); uint8_t pixels[16]={};
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)for(int c=0;c<3;++c)pixels[y*8+x*3+c]=uint8_t(value);
        Bytes(pixels,sizeof(pixels)); End(at); ++Frames;
    }
    void Standard(unsigned slot, unsigned frame, unsigned size=16) { Word(HeaderIndex+32+slot*8,Payloads[frame]); Word(HeaderIndex+36+slot*8,size); }
    void Make(unsigned mode=0) {
        const unsigned n=mode==8||mode==9?70:3; Start(24,0); Count=0;
        if(mode>=20) Header(mode==24?2:mode==29?4:3,mode==25);
        for(unsigned i=0;i<n;++i) Emit((i%7+1)*32);
        unsigned junk=Begin("JUNK"); Fake=Size; Bytes("00db",4); unsigned at=Size; Size+=4; Word(at,16); uint8_t bytes[16]; std::memset(bytes,238,16); Bytes(bytes,16); End(junk); End(Movi);
        if(mode>=20&&mode!=25) {
            for(unsigned i=0;i<(mode==24?2:3);++i) Standard(i,i);
            if(mode==21) Word(HeaderIndex+40,0xfffffff0u);
            if(mode==22) Word(HeaderIndex+40,Fake+8);
            if(mode==23) Word(HeaderIndex+44,8);
            if(mode==26) Word(HeaderIndex+11,0x80,1);
            if(mode==27) Word(HeaderIndex+12,999);
            if(mode==28) { Standard(0,2); Standard(2,0); }
            if(mode==29) { Standard(0,0); Word(HeaderIndex+40,0); Word(HeaderIndex+44,0); Standard(2,1); Standard(3,2); }
        }
        const bool must=mode==10||mode==11||mode==12||mode==13||mode==14||mode==28;
        if(mode<20&&mode!=14) {
            Classic=Begin("idx1"); unsigned entries=mode==6||mode==13?2:n;
            for(unsigned i=0;i<entries;++i) {
                unsigned frame=mode==10?2-i:mode==12?(i==1?0:2):mode==13?(i?0:2):i;
                Bytes("00db",4); at=Size; Size+=12; Word(at,0x10);
                Word(at+4,Payloads[frame]-8-Movi-8); Word(at+8,16);
                if(i==1) { if(mode==1||mode==11)Word(at+4,0xfffffff0u); if(mode==3)Word(at+8,8); if(mode==4)Word(at+8,32); if(mode==5)Word(at+4,Payloads[0]-8-Movi-8); }
                if(i==n-1&&mode==2)Word(at+4,0xfffffff0u);
                if(i==65&&mode==8)Word(at+4,Fake-Movi-8);
                if(i>=64&&mode==9)Word(at+4,Payloads[frame]-8);
            }
            if(mode==7) { Data[Size++]=0x7f; } End(Classic);
        }
        if(mode>=20&&mode<28&&mode!=25) {
            Classic=Begin("idx1"); for(unsigned i=0;i<n;++i) { Bytes("00db",4); at=Size; Size+=12; Word(at,0x10); Word(at+4,Payloads[i]-8-Movi-8); Word(at+8,16); } End(Classic);
        }
        End(0); Word(FrameCountAt,n); Word(StreamCountAt,mode==13?2:mode==29?4:n); if(must)Word(44,0x30);
    }
    void TwoStreams(bool badFirst=false, bool wrongOwner=false, bool audio=false) {
        Start(24,0); Count=0; Header(3); const unsigned clone=Movi-88, insertion=Movi;
        std::memmove(Data+insertion+clone,Data+insertion,Size-insertion); std::memcpy(Data+insertion,Data+88,clone);
        std::memcpy(Data+insertion+HeaderIndex-88,"JUNK",4); Size+=clone; Movi+=clone; Word(16,Read(16)+clone); Word(56,2);
        if(audio) {
            std::memcpy(Data+insertion+20,"auds",4); const unsigned wave=insertion+84; std::memset(Data+wave,0,40);
            Word(wave,1,2); Word(wave+2,1,2); Word(wave+4,8000); Word(wave+8,16000); Word(wave+12,2,2); Word(wave+14,16,2);
        }
        for(unsigned i=0;i<3;++i) { Emit((i+1)*32); Emit((i+1)*32+7,audio?"01wb":"01db"); }
        for(unsigned i=0;i<3;++i)Standard(i,i*2+(wrongOwner?1:0));
        if(badFirst)Word(HeaderIndex+40,0xfffffff0u); if(wrongOwner)std::memcpy(Data+HeaderIndex+16,"01db",4);
        End(Movi); Classic=Begin("idx1");
        for(unsigned i=0;i<3;++i) { Bytes(audio?"01wb":"01db",4); unsigned at=Size; Size+=12; Word(at,0x10); Word(at+4,Payloads[i*2+1]-8-Movi-8); Word(at+8,16); }
        End(Classic); End(0); Word(FrameCountAt,3); Word(StreamCountAt,3); Word(insertion+52,audio?24:3);
    }
};
#endif
