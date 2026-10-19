#ifndef AVI_READER_APERTURE_DATA_H
#define AVI_READER_APERTURE_DATA_H
#include "AVIReaderVprpData.h"
struct AviApertureData : AviVprpData {
    void Geometry(bool jpeg, unsigned mode) {
        const unsigned first=Vprp+44, second=first+32;
        Word(first+12,3); Word(first+16,1); Word(first+20,jpeg?4:1);
        Word(second+12,mode>=4&&mode<=5?3:2); Word(second+16,4); Word(second+20,jpeg?8:0);
        Word(first+24,mode==2?124:122); Word(second+24,mode==1||mode==6?124:mode==3?127:mode==4?123:122);
        if(mode==6) { Word(first+8,jpeg?4:1); Word(second+8,jpeg?10:3); Word(first+20,0); Word(second+20,0); }
    }
    void Make(bool jpeg=false, unsigned mode=1, bool topFirst=true, bool topDown=false, bool reverse=false) {
        if(jpeg) Jpeg(true,topFirst,reverse,true); else Raw(topDown,false,2,topFirst);
        Geometry(jpeg,mode);
    }
};
#endif
