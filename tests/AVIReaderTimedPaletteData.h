#ifndef AVI_READER_TIMED_PALETTE_DATA_H
#define AVI_READER_TIMED_PALETTE_DATA_H
#include "AVIReaderOpenDmlPaletteData.h"

struct AviTimedPaletteData : AviOpenDmlPaletteData {
    unsigned Timed[2]={}, Segments=0;
    void Convert(unsigned at,const unsigned *durations) {
        unsigned refs[6];
        for(unsigned i=0;i<3;++i){refs[i*2]=Read(at+32+i*8);refs[i*2+1]=Read(at+36+i*8);}
        Word(at+8,3,2);Word(at+11,2,1);
        for(unsigned i=0;i<3;++i){Word(at+32+i*12,refs[i*2]);Word(at+36+i*12,refs[i*2+1]);Word(at+40+i*12,durations[i]);}
    }
    unsigned Block(const unsigned *entries,const unsigned *durations,unsigned count,unsigned base) {
        unsigned at=Begin("ix00");Size+=24;Word(at+8,3,2);Word(at+11,2,1);
        Word(at+12,count);BytesAt(at+16,"00pc");Word(at+20,base);
        for(unsigned i=0;i<count;++i){unsigned p=Size;Size+=12;Reference(p,entries[i],base);Word(p+8,durations[i]);}
        End(at);return at;
    }
    void BytesAt(unsigned at,const char *id){std::memcpy(Data+at,id,4);}
    void Make(unsigned options,const unsigned *durations,unsigned codec=0,unsigned bpp=8) {
        AviOpenDmlPaletteData::Make(options,codec,bpp);Segments=(options&DmlSplit)?2:1;
        if(options&DmlPaletteSuper){
            unsigned base=(options&DmlBase)?Index[Changes[0]].Offset:0;
            unsigned first=Segments==2?1:3;Timed[0]=Block(Changes,durations,first,base);
            SuperReference(PaletteIndex+32,Timed[0],0);Word(PaletteIndex+12,Segments);
            if(Segments==2){Timed[1]=Block(Changes+1,durations+1,2,base);SuperReference(PaletteIndex+48,Timed[1],0);}
            End(Root);
        }else{Timed[0]=PaletteIndex;Convert(PaletteIndex,durations);}
    }
    unsigned Row(unsigned i) const {return Segments==2&&i?Timed[1]+32+(i-1)*12:Timed[0]+32+i*12;}
    void Select(unsigned row,unsigned change){unsigned block=Segments==2&&row?Timed[1]:Timed[0];Reference(Row(row),Changes[change],Read(block+20));}
    void TwoStreams(const unsigned *first,const unsigned *second) {
        AviOpenDmlPaletteData::TwoStreams();Segments=1;Timed[0]=PaletteIndex;
        Convert(PaletteIndex,first);Convert(PaletteIndex+(Movi-88)/2,second);
    }
    void Recovery(unsigned bpp,unsigned codec) {
        const unsigned d[]={1,1,1};Make(0,d,codec,bpp);
        uint8_t colors[768]={};colors[0]=colors[1]=colors[3]=colors[6]=colors[7]=255;
        unsigned full=Entries;Palette(0,1u<<bpp,colors);unsigned image=Entries;
        const uint8_t raw8[]={1,2,0,0,2,1,0,0},raw4[]={0x12,0,0,0,0x21,0,0,0},raw1[]={0x80,0,0,0,0x40,0,0,0},eob[]={0,1};
        Frame(codec?eob:bpp==1?raw1:bpp==4?raw4:raw8,codec?2:8);End(Movi);End(0);
        Word(VideoIndex+12,4);Reference(VideoIndex+56,image);Word(PaletteIndex+12,4);Reference(PaletteIndex+68,full);Word(PaletteIndex+76,1);
        Word(Row(1),0xfffffff0u);LogicalCount=4;Word(FrameCountAt,4);Word(StreamCountAt,4);
    }
};
#endif
