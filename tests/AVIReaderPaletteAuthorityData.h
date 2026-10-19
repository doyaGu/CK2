#ifndef AVI_READER_PALETTE_AUTHORITY_DATA_H
#define AVI_READER_PALETTE_AUTHORITY_DATA_H
#include "AVIReaderOpenDmlPaletteData.h"

enum AviPaletteAuthorityOptions {
    AuthorityClassic=1, AuthorityRequired=2, AuthorityAlternate=4, AuthoritySecond=8,
    AuthoritySuper=16, AuthorityPaletteFirst=32, AuthorityBadClassic=64,
    AuthorityShortClassic=128, AuthorityDifferentClassic=256,
    AuthorityReverseDml=512, AuthorityNoClassicPalette=1024
};

// Three actual pictures, with palette events only after the initial picture.
// This matches the ordinary original/VFW lifecycle, unlike leading-PC probes.
struct AviPaletteAuthorityData : AviOpenDmlPaletteData {
    void Make(unsigned options) {
        Start(8,0,2,2);
        const unsigned insertion=Movi, extra=480;
        std::memmove(Data+insertion+extra,Data+insertion,Size-insertion);
        std::memset(Data+insertion,0,extra);Size+=extra;Movi+=extra;
        Word(16,Read(16)+extra);Word(92,Read(92)+extra);
        VideoIndex=insertion+((options&AuthorityPaletteFirst)?160:0);
        PaletteIndex=insertion+((options&AuthorityPaletteFirst)?0:160);
        unsigned second=insertion+320;
        IndexHeader(VideoIndex,"00db",(options&AuthoritySuper)!=0,0);
        IndexHeader(PaletteIndex,"00pc",false,2);
        if(options&AuthoritySecond)IndexHeader(second,"00pc",false,2);
        else {std::memcpy(Data+second,"JUNK",4);Word(second+4,152);}
        const uint8_t raw[3][8]={{1,2,0,0,2,1,0,0},{2,1,0,0,1,2,0,0},{1,1,0,0,2,2,0,0}};
        const uint8_t green[]={0,255,0},yellow[]={255,255,0};
        Pictures[0]=Entries;Frame(raw[0],8);
        Changes[0]=Entries;Palette(1,1,green);
        Pictures[1]=Entries;Frame(raw[1],8);
        Changes[1]=Entries;Palette(1,1,yellow);
        Pictures[2]=Entries;Frame(raw[2],8);End(Movi);
        unsigned pictures[]={Pictures[0],Pictures[1],Pictures[2]};
        if(options&AuthorityReverseDml){pictures[0]=Pictures[2];pictures[1]=Pictures[0];pictures[2]=Pictures[1];}
        if(options&AuthoritySuper) {
            unsigned child=Standard("00db",pictures,3,0);
            Word(VideoIndex+12,1);SuperReference(VideoIndex+32,child,3);
        } else {
            Word(VideoIndex+12,3);
            for(unsigned i=0;i<3;++i)Reference(VideoIndex+32+i*8,pictures[i]);
        }
        for(unsigned turn=0;turn<((options&AuthoritySecond)?2u:1u);++turn) {
            unsigned at=turn?second:PaletteIndex;
            Word(at+8,3,2);Word(at+11,2,1);
            for(unsigned i=0;i<2;++i) {
                unsigned which=(options&AuthorityAlternate)?1-i:i;
                if(turn)which=1-which;
                Reference(at+32+i*12,Changes[which]);Word(at+40+i*12,1);
            }
        }
        if(options&AuthorityClassic) {
            unsigned idx=Begin("idx1");
            unsigned sequence[]={Pictures[0],Changes[0],Pictures[1],Changes[1],Pictures[2]};
            if(options&AuthorityDifferentClassic)sequence[2]=Pictures[0];
            for(unsigned i=0;i<5;++i) {
                if((options&AuthorityShortClassic)&&i==4)continue;
                if((options&AuthorityNoClassicPalette)&&(i==1||i==3))continue;
                unsigned entry=sequence[i];Bytes(Index[entry].Id,4);unsigned at=Size;Size+=12;
                Word(at,Index[entry].Flags);Word(at+4,Index[entry].Offset-Movi-8);Word(at+8,Index[entry].Size);
                if((options&AuthorityBadClassic)&&i==4)Word(at+4,0xfffffff0u);
            }
            End(idx);
        }
        Word(FrameCountAt,3);Word(StreamCountAt,3);
        Word(44,((options&AuthorityClassic)?0x10:0)|((options&AuthorityRequired)?0x20:0));
        End(0);LogicalCount=3;
    }
};
#endif
