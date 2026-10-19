#ifndef AVI_READER_VPRP_DATA_H
#define AVI_READER_VPRP_DATA_H
#include "AVIReaderInterlacedData.h"
struct AviVprpData : AviReaderTestData {
    unsigned Vprp=0;
    unsigned Read(unsigned p) const { return unsigned(Data[p])|(unsigned(Data[p+1])<<8)|(unsigned(Data[p+2])<<16)|(unsigned(Data[p+3])<<24); }
    void Properties(unsigned fields, unsigned encodedW, unsigned encodedH, unsigned validW, unsigned validH,
                    unsigned x, unsigned y, bool topFirst=true, bool early=false) {
        unsigned extra=44+fields*32, insertion=early?164:Movi;
        std::memmove(Data+insertion+extra,Data+insertion,Size-insertion); std::memset(Data+insertion,0,extra);
        Size+=extra; Movi+=extra; Word(16,Read(16)+extra); Word(92,Read(92)+extra);
        Vprp=insertion; std::memcpy(Data+Vprp,"vprp",4); Word(Vprp+4,extra-8);
        Word(Vprp+8+12,encodedW); Word(Vprp+8+16,encodedH*fields); Word(Vprp+8+20,0x00040003u);
        Word(Vprp+8+24,validW); Word(Vprp+8+28,validH*fields); Word(Vprp+8+32,fields);
        for(unsigned i=0;i<fields;++i) {
            unsigned p=Vprp+44+i*32; Word(p,encodedH); Word(p+4,encodedW); Word(p+8,validH); Word(p+12,validW);
            Word(p+16,x); Word(p+20,y); Word(p+24,122); Word(p+28,fields==1?0:(topFirst?i:1-i));
        }
    }
    void Raw(bool topDown=false, bool early=false, unsigned fields=1, bool topFirst=true) {
        Start(24,0,8,topDown?-8:8); Properties(fields,8,8/fields,4,4/fields,2,1,topFirst,early);
        uint8_t pixels[192];
        for(int stored=0;stored<8;++stored) for(int x=0;x<8;++x) {
            int topY=topDown?stored:7-stored, v=topY*16+x;
            for(int c=0;c<3;++c) pixels[(stored*8+x)*3+c]=uint8_t(v);
        }
        Frame(pixels,sizeof(pixels)); Frame(pixels,sizeof(pixels)); Finish();
    }
    void Jpeg(bool fields=true, bool topFirst=true, bool reverse=false, bool omit=false) {
        Start(24,0x47504a4du,8,fields?32:16); Properties(fields?2:1,8,16,4,8,2,4,topFirst);
        uint8_t bytes[2048]; unsigned n;
        if(reverse&&fields) { n=AviFieldBytes(bytes,AviFieldB,sizeof(AviFieldB),2,omit); n+=AviFieldBytes(bytes+n,AviFieldA,sizeof(AviFieldA),1,omit); }
        else { n=AviFieldBytes(bytes,AviFieldA,sizeof(AviFieldA),fields?1:0,omit); if(fields)n+=AviFieldBytes(bytes+n,AviFieldB,sizeof(AviFieldB),2,omit); }
        Frame(bytes,n); Frame(bytes,n); Finish();
    }
};
#endif
