#ifndef AVI_READER_BROADCAST_DATA_H
#define AVI_READER_BROADCAST_DATA_H
#include "AVIReaderVprpData.h"
struct AviBroadcastData : AviVprpData {
    void Timing(unsigned total, unsigned first, unsigned second, unsigned height) {
        Word(Vprp+12,total==625?1:2); Word(Vprp+16,total==625?50:60);
        Word(Vprp+24,total); Word(Vprp+36,height);
        Word(Vprp+72,first); Word(Vprp+104,second);
    }
    void Make(unsigned total=525, unsigned first=20, unsigned second=283, unsigned height=12,
              bool jpeg=false, bool topDown=false, bool reverse=false) {
        if(jpeg) Jpeg(true,true,reverse,true); else Raw(topDown,false,2);
        Timing(total,first,second,height);
    }
};
#endif
