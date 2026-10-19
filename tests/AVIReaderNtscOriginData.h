#ifndef AVI_READER_NTSC_ORIGIN_DATA_H
#define AVI_READER_NTSC_ORIGIN_DATA_H
#include "AVIReaderBroadcastData.h"
struct AviNtscOriginData : AviBroadcastData {
    void FullRaw(unsigned first, unsigned second, unsigned canvas=480, bool topDown=false, bool odd=false) {
        const int height=odd?486:480;
        Start(24,0,8,topDown?-height:height); Properties(2,8,height/2,8,height/2,0,0);
        if(odd) Word(Vprp+76+8,242);
        Timing(525,first,second,canvas);
        uint8_t pixels[8*486*3];
        for(int stored=0;stored<height;++stored) for(int x=0;x<8;++x) {
            const int topY=topDown?stored:height-1-stored;
            for(int c=0;c<3;++c) pixels[(stored*8+x)*3+c]=uint8_t((topY*7+x)%251+1);
        }
        Frame(pixels,8*height*3); Frame(pixels,8*height*3); Finish();
    }
};
#endif
