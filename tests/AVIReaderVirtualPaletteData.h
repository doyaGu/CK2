#ifndef AVI_READER_VIRTUAL_PALETTE_DATA_H
#define AVI_READER_VIRTUAL_PALETTE_DATA_H
#include "AVIReaderTestData.h"
#include <initializer_list>
struct AviVirtualPaletteData : AviReaderTestData {
    unsigned HeaderIndex=0, LogicalCount=0, Logical[32]={}, LogicalSize[32]={}, Classic=0;
    void Slot(int entry) { Logical[LogicalCount]=entry<0?0:Index[entry].Offset+8; LogicalSize[LogicalCount++]=entry<0?0:Index[entry].Size; }
    unsigned Read(unsigned at) const { return unsigned(Data[at])|(unsigned(Data[at+1])<<8)|(unsigned(Data[at+2])<<16)|(unsigned(Data[at+3])<<24); }
    void Header() {
        unsigned insertion=Movi,extra=32+16*8; std::memmove(Data+insertion+extra,Data+insertion,Size-insertion); std::memset(Data+insertion,0,extra);
        Size+=extra; Movi+=extra; Word(16,Read(16)+extra); Word(92,Read(92)+extra); HeaderIndex=insertion;
        std::memcpy(Data+insertion,"indx",4); Word(insertion+4,extra-8); Word(insertion+8,2,2); Word(insertion+11,1,1); std::memcpy(Data+insertion+16,Codec?"00dc":"00db",4);
    }
    void Make(unsigned mode=0,unsigned codec=0,unsigned bpp=8,bool records=false) {
        Start(bpp,codec,2,2); LogicalCount=0; if(mode<10)Header(); if(records)Record=List("rec ");
        const uint8_t green[]={0,255,0},yellow[]={255,255,0},red[]={255,0,0},gray[]={128,128,128,128,128,128};
        unsigned prefix=Entries;
        if(mode==21) { uint8_t colors[768]; for(unsigned i=0;i<256;++i){colors[i*3]=17;colors[i*3+1]=33;colors[i*3+2]=65;} Palette(0,256,colors); }
        else if(mode==7)Palette(1,2,gray); else Palette(1,1,green);
        const uint8_t raw8[]={1,2,0,0,2,1,0,0},raw4[]={0x12,0,0,0,0x21,0,0,0},raw1[]={0x80,0,0,0,0x40,0,0,0};
        const uint8_t run[]={2,1,0,0,2,2,0,1},eob[]={0,1};
        unsigned first=Entries,second=0,physical=0,change=0,tail=0;
        if(mode!=5&&mode!=18)Frame(codec?run:bpp==1?raw1:bpp==4?raw4:raw8,8);
        if(mode==6) { if(Record){End(Record);Record=0;} End(Movi); End(0); unsigned root=Begin("RIFF"); Bytes("AVIX",4); Movi=List("movi"); }
        change=Entries; if(mode==7)Palette(1,1,red); else Palette(bpp==1?0:2,1,yellow);
        if(mode==3||mode==8||mode==16){physical=Entries;Frame(nullptr,0);}
        second=Entries; if(mode!=5&&mode!=18)Frame(codec?eob:bpp==1?raw1:bpp==4?raw4:raw8,codec?2:8);
        if(mode==13||mode==14||mode==15||mode==16||mode==20){tail=Entries;Palette(1,1,red);}
        if(Record){End(Record);Record=0;}End(Movi);
        if(mode<10) {
            if(mode==1||mode==9){Slot(-1);if(mode==9)Slot(-1);} if(mode!=5)Slot(mode==4?second:first);
            if(mode==8)Slot(physical);else {Slot(-1);if(mode==2||mode==9||mode==5)Slot(-1);}
            if(mode==3) { LogicalCount=1;Slot(physical);Slot(-1); }
            if(mode!=5)Slot(mode==4?first:second); if(mode==2||mode==4||mode==6)Slot(-1);
            Word(HeaderIndex+12,LogicalCount);for(unsigned i=0;i<LogicalCount;++i){Word(HeaderIndex+32+i*8,Logical[i]);Word(HeaderIndex+36+i*8,LogicalSize[i]);}
            if(mode==6){unsigned root=Read(4)+8;Word(root+4,Size-root-8);}else End(0);
        } else {
            int sequence[32]={};unsigned count=0;
            if(mode==13){const int q[]={int(change),int(prefix),int(first),int(tail),-1,int(second)};count=6;std::memcpy(sequence,q,sizeof(q));}
            else if(mode==15){const int q[]={int(tail),int(change),int(first),int(prefix),-1,int(second)};count=6;std::memcpy(sequence,q,sizeof(q));}
            else if(mode==20){const int q[]={int(change),int(second),int(tail),-1,int(first),int(prefix),-1,int(first)};count=8;std::memcpy(sequence,q,sizeof(q));}
            else if(mode==17){sequence[count++]=first;sequence[count++]=-1;sequence[count++]=second;}
            else if(mode==18){sequence[count++]=prefix;sequence[count++]=-1;sequence[count++]=-1;}
            else {sequence[count++]=prefix;if(mode==23)sequence[count++]=prefix;if(mode==11)sequence[count++]=-1;sequence[count++]=first;sequence[count++]=change;if(mode==16)sequence[count++]=physical;
                sequence[count++]=-1;if(mode==12)sequence[count++]=-1;if(mode==14||mode==16){sequence[count++]=tail;sequence[count++]=-1;}sequence[count++]=second;}
            Classic=Begin("idx1");for(unsigned i=0;i<count;++i){int e=sequence[i];if(e<0){Bytes(codec?"00dc":"00db",4);unsigned at=Size;Size+=12;Word(at,0);Word(at+4,0);Word(at+8,0);Slot(-1);}
                else {Bytes(Index[e].Id,4);unsigned at=Size;Size+=12;Word(at,Index[e].Flags);Word(at+4,Index[e].Offset-Movi-8);Word(at+8,Index[e].Size);if(std::memcmp(Index[e].Id,"00pc",4))Slot(e);}}
            End(Classic);End(0);if(mode==19){Word(Classic+8+8,0xfffffff0u);Word(44,0x30);}
        }
        if(mode==22)Data[Index[prefix].Offset+9]=255;
        if(mode==4||mode==13||mode==14||mode==15||mode==16||mode==20)Word(44,0x30);
        Word(FrameCountAt,LogicalCount);Word(StreamCountAt,LogicalCount);
    }
    void TwoStreams() {
        Start(8,0); const unsigned insertion=Movi,clone=Movi-88;
        std::memmove(Data+insertion+clone,Data+insertion,Size-insertion); std::memcpy(Data+insertion,Data+88,clone);
        Size+=clone;Movi+=clone;Word(16,Read(16)+clone);Word(56,2);
        const uint8_t green[]={0,255,0},yellow[]={255,255,0},red[]={255,0,0},raw[]={1,2,0,0,2,1,0,0};
        Palette(1,1,green);Palette(1,1,yellow,"01pc");Frame(raw,8);Add("01db",raw,8,0x10);
        Palette(2,1,yellow);Palette(1,1,red,"01pc");Frame(raw,8);Add("01db",raw,8,0x10);End(Movi);
        const int sequence[]={0,1,2,3,4,5,-1,-2,6,7};Classic=Begin("idx1");
        for(int e:sequence){if(e<0){Bytes(e==-1?"00db":"01db",4);Size+=12;}else{Bytes(Index[e].Id,4);unsigned at=Size;Size+=12;Word(at,Index[e].Flags);Word(at+4,Index[e].Offset-Movi-8);Word(at+8,Index[e].Size);}}
        End(Classic);End(0);Word(FrameCountAt,3);Word(StreamCountAt,3);Word(insertion+52,3);Word(44,0x30);
    }
    void Recovery(unsigned codec=0,unsigned bpp=8) {
        Make(10,codec,bpp);uint8_t table[80];std::memcpy(table,Data+Classic+8,80);Size=Classic;
        uint8_t colors[768]={};colors[3]=255;colors[4]=255;colors[6]=255;
        unsigned full=Entries;Palette(0,1u<<bpp,colors);unsigned frame=Entries;
        const uint8_t raw[]={1,2,0,0,2,1,0,0},raw4[]={0x12,0,0,0,0x21,0,0,0},raw1[]={0x80,0,0,0,0x40,0,0,0},eob[]={0,1};Frame(codec?eob:bpp==4?raw4:bpp==1?raw1:raw,codec?2:8);End(Movi);
        Classic=Begin("idx1");Bytes(table,80);for(unsigned e:{full,frame}){Bytes(Index[e].Id,4);unsigned at=Size;Size+=12;Word(at,Index[e].Flags);Word(at+4,Index[e].Offset-Movi-8);Word(at+8,Index[e].Size);}
        End(Classic);End(0);Word(Classic+48,0xfffffff0u);Word(FrameCountAt,4);Word(StreamCountAt,4);
    }
};
#endif
