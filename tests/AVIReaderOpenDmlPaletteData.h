#ifndef AVI_READER_OPEN_DML_PALETTE_DATA_H
#define AVI_READER_OPEN_DML_PALETTE_DATA_H
#include "AVIReaderTestData.h"

enum AviDmlPaletteOptions {
    DmlPaletteFirst=1, DmlVideoSuper=2, DmlPaletteSuper=4, DmlAvix=8,
    DmlReverse=16, DmlVirtual=32, DmlNoPalette=64, DmlBadReference=128,
    DmlBadChild=256, DmlRepeated=512, DmlBadVideo=1024, DmlBadOwner=2048,
    DmlSplit=4096, DmlBase=8192, DmlFieldPalette=16384, DmlEmptyPalette=32768,
    DmlWrongType=65536, DmlTruncated=131072, DmlNoVideo=262144,
    DmlClassic=524288
};

// Independent standard/super indexes; neither production parsing nor its writer is used.
struct AviOpenDmlPaletteData : AviReaderTestData {
    unsigned VideoIndex=0, PaletteIndex=0, Root=0, VideoChild=0, PaletteChild=0;
    unsigned Pictures[3]={}, Changes[3]={}, LogicalCount=0, Options=0;
    unsigned Read(unsigned at) const {
        return unsigned(Data[at])|(unsigned(Data[at+1])<<8)|
            (unsigned(Data[at+2])<<16)|(unsigned(Data[at+3])<<24);
    }
    void IndexHeader(unsigned at,const char *id,bool super,unsigned entries,unsigned base=0) {
        std::memcpy(Data+at,"indx",4);Word(at+4,152);Word(at+8,super?4:2,2);
        Word(at+11,super?0:1,1);Word(at+12,entries);std::memcpy(Data+at+16,id,4);Word(at+20,base);
    }
    void Reference(unsigned at,unsigned entry,unsigned base=0) {
        Word(at,Index[entry].Offset+8-base);Word(at+4,Index[entry].Size);
    }
    unsigned Standard(const char *id,const unsigned *entries,unsigned count,unsigned base) {
        unsigned at=Begin("ix00");Size+=24;Word(at+8,2,2);Word(at+11,1,1);
        Word(at+12,count);std::memcpy(Data+at+16,id,4);Word(at+20,base);
        for(unsigned i=0;i<count;++i){unsigned p=Size;Size+=8;Reference(p,entries[i],base);}
        End(at);return at;
    }
    void SuperReference(unsigned at,unsigned child,unsigned duration) {
        Word(at,child);Word(at+8,Read(child+4)+8);Word(at+12,duration);
    }
    void Make(unsigned options=0,unsigned codec=0,unsigned bpp=8) {
        Options=options;Start(bpp,codec,2,2);Root=0;
        const unsigned insertion=Movi,extra=320;
        std::memmove(Data+insertion+extra,Data+insertion,Size-insertion);
        std::memset(Data+insertion,0,extra);Size+=extra;Movi+=extra;
        Word(16,Read(16)+extra);Word(92,Read(92)+extra);
        PaletteIndex=insertion+((options&DmlPaletteFirst)?0:160);
        VideoIndex=insertion+((options&DmlPaletteFirst)?160:0);
        const char *videoId=codec?"00dc":"00db";
        IndexHeader(VideoIndex,videoId,(options&DmlVideoSuper)!=0,0);
        IndexHeader(PaletteIndex,"00pc",(options&DmlPaletteSuper)!=0,0);
        const uint8_t green[]={0,255,0},yellow[]={255,255,0},red[]={255,0,0};
        const uint8_t raw8[]={1,2,0,0,2,1,0,0},raw4[]={0x12,0,0,0,0x21,0,0,0},raw1[]={0x80,0,0,0,0x40,0,0,0};
        const uint8_t run8[]={2,1,0,0,2,2,0,1},run4[]={2,0x11,0,0,2,0x22,0,1},eob[]={0,1};
        const uint8_t *raw=bpp==1?raw1:bpp==4?raw4:raw8;
        Changes[0]=Entries;Palette(1,1,green);Pictures[0]=Entries;
        Frame(codec?(codec==2?run4:run8):raw,8);
        if(options&DmlAvix){End(Movi);End(0);Root=Begin("RIFF");Bytes("AVIX",4);Movi=List("movi");}
        Changes[1]=Entries;Palette(bpp==1?0:2,1,yellow);Pictures[1]=Entries;Frame(codec?eob:raw,codec?2:8);
        Changes[2]=Entries;Palette(1,1,red);Pictures[2]=Entries;Frame(codec?eob:raw,codec?2:8);End(Movi);
        const unsigned base=(options&DmlBase)?Index[Changes[0]].Offset:0;
        unsigned sequence[4]={Pictures[0],Pictures[1],Pictures[2],0};LogicalCount=3;
        if(options&DmlReverse){sequence[0]=Pictures[2];sequence[1]=Pictures[0];sequence[2]=Pictures[1];}
        if(options&DmlVirtual){sequence[3]=sequence[2];sequence[2]=sequence[1];sequence[1]=unsigned(-1);LogicalCount=4;}
        if(options&DmlVideoSuper){
            // Construct the frame list explicitly so virtual entries remain zero/zero.
            VideoChild=Begin("ix00");Size+=24;Word(VideoChild+8,2,2);Word(VideoChild+11,1,1);
            Word(VideoChild+12,LogicalCount);std::memcpy(Data+VideoChild+16,videoId,4);Word(VideoChild+20,base);
            for(unsigned i=0;i<LogicalCount;++i){unsigned at=Size;Size+=8;if(sequence[i]!=unsigned(-1))Reference(at,sequence[i],base);}
            End(VideoChild);Word(VideoIndex+12,1);SuperReference(VideoIndex+32,VideoChild,LogicalCount);
        }else{
            Word(VideoIndex+12,LogicalCount);Word(VideoIndex+20,base);
            for(unsigned i=0;i<LogicalCount;++i)if(sequence[i]!=unsigned(-1))Reference(VideoIndex+32+i*8,sequence[i],base);
        }
        if(options&DmlPaletteSuper){
            unsigned count=(options&DmlSplit)?1:3;
            PaletteChild=Standard("00pc",Changes,count,base);Word(PaletteIndex+12,(options&DmlSplit)?2:1);
            SuperReference(PaletteIndex+32,PaletteChild,0);
            if(options&DmlSplit){unsigned second=Standard("00pc",Changes+1,2,base);SuperReference(PaletteIndex+48,second,0);}
        }else{
            Word(PaletteIndex+12,3);Word(PaletteIndex+20,base);
            for(unsigned i=0;i<3;++i)Reference(PaletteIndex+32+i*8,Changes[i],base);
        }
        unsigned pc=(options&DmlPaletteSuper)?PaletteChild:PaletteIndex;
        unsigned vi=(options&DmlVideoSuper)?VideoChild:VideoIndex;
        if(options&DmlBadReference)Word(pc+32,Read(pc+32)+1);
        if(options&DmlBadChild)Word(PaletteIndex+32,PaletteIndex);
        if(options&DmlRepeated)Reference(pc+40,Changes[0],base);
        if(options&DmlBadOwner)std::memcpy(Data+PaletteIndex+16,"01pc",4);
        if(options&DmlBadVideo)Word(vi+32,0xfffffff0u);
        if(options&DmlFieldPalette){Word(pc+10,1,1);Word(pc+8,3,2);}
        if(options&DmlEmptyPalette)Word(PaletteIndex+12,0);
        if(options&DmlWrongType)Word(pc+11,2,1);
        if(options&DmlTruncated)Word(pc+12,100);
        if(options&DmlNoPalette)std::memcpy(Data+PaletteIndex,"JUNK",4);
        if(options&DmlNoVideo)std::memcpy(Data+VideoIndex,"JUNK",4);
        if(options&DmlClassic){unsigned at=Begin("idx1");for(unsigned i=0;i<Entries;++i){Bytes(Index[i].Id,4);unsigned p=Size;Size+=12;Word(p,Index[i].Flags);Word(p+4,Index[i].Offset);Word(p+8,Index[i].Size);}End(at);}
        Word(FrameCountAt,LogicalCount);Word(StreamCountAt,LogicalCount);Word(44,(options&DmlNoVideo)?0x10:0x30);End(Root);
    }
    void TwoStreams() {
        Make();const unsigned insertion=Movi,clone=Movi-88;
        std::memmove(Data+insertion+clone,Data+insertion,Size-insertion);
        std::memcpy(Data+insertion,Data+88,clone);Size+=clone;Movi+=clone;
        Word(16,Read(16)+clone);Word(56,2);
        for(unsigned i=0;i<Entries;++i)Index[i].Offset+=clone;
        for(unsigned i=0;i<3;++i){Reference(VideoIndex+32+i*8,Pictures[i]);Reference(PaletteIndex+32+i*8,Changes[i]);}
        unsigned video=VideoIndex+clone,palette=PaletteIndex+clone;
        std::memcpy(Data+video+16,"01db",4);std::memcpy(Data+palette+16,"01pc",4);
        const uint8_t yellow[]={255,255,0},green[]={0,255,0},red[]={255,0,0},raw[]={1,2,0,0,2,1,0,0};
        for(unsigned i=0;i<3;++i){
            unsigned p=Entries;Palette(i==1?2:1,1,i==0?yellow:i==1?green:red,"01pc");Reference(palette+32+i*8,p);
            unsigned f=Entries;Add("01db",raw,8,0x10);Reference(video+32+i*8,f);
        }
        End(Movi);End(0);
    }
};
#endif
