/** *************************************************************************
    \file ADM_tsPatPmt.cpp
    \brief Analyze pat & pmt
    copyright            : (C) 2007 by mean
    email                : fixounet@free.fr
    
      
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "ADM_default.h"
#include "ADM_ts.h"
#include "ADM_demuxerInternal.h"
#include "ADM_tsPatPmt.h"

typedef vector <ADM_TS_TRACK> listOfTsTracks;

/**
    \fn scanPmt
*/
static bool TS_scanPmt(tsPacket *t,uint32_t pid,listOfTsTracks *list);


/**
    \class TrackTypeDescriptor
*/
class TrackTypeDescriptor
{
public:
    int                 type;
    ADM_TS_TRACK_TYPE   trackType;
    const char          *desc;
    static TrackTypeDescriptor *find(int t);
};
TrackTypeDescriptor TrackTypes[]=
{
    {0x002,ADM_TS_MPEG2,     "Mpeg2 Video"},
    {0x003,ADM_TS_MPEG_AUDIO,"Mpeg1 Audio"},
    {0x004,ADM_TS_MPEG_AUDIO,"Mpeg2 Audio"},
    {0x01b,ADM_TS_H264,      "H264 Video"},
    {0x081,ADM_TS_AC3,       "AC3 Audio"},
    {0x006,ADM_TS_UNKNOWN,   "Private Stream"},
    {0xfff,ADM_TS_UNKNOWN,   "Unknown"}   // Last one must be "unknown!"
};



/**
    \fn find
*/
TrackTypeDescriptor *TrackTypeDescriptor::find(int t)
{
    int size=sizeof(TrackTypes)/sizeof(TrackTypeDescriptor);
    for(int i=0;i<size;i++)
    {
        TrackTypeDescriptor *tp=TrackTypes+i;
        if(tp->type==t) return tp;
    }
    return TrackTypes+size-1;
}
/**
    \fn scanForPrograms
    \brief Lookup PAT & PMT to get tracks
*/
bool TS_scanForPrograms(const char *file,uint32_t *nbTracks, ADM_TS_TRACK **outTracks)
{
    uint32_t len;
    TS_PSIpacketInfo psi;
    listOfTsTracks   list;
    vector <uint32_t>listOfPmt;
    ADM_TS_TRACK *tracks=NULL;
    *outTracks=NULL;
    *nbTracks=0;
    uint32_t nb=0;

    tsPacket *t=new tsPacket();
    t->open(file,FP_PROBE);
    // 1 search the pat...
    if(t->getNextPSI(0,&psi)==true)
    {
        uint8_t *r=psi.payload;
        len=psi.payloadSize;
        while(len>=4)
        {
            uint32_t prg=((0x1F&r[0])<<8)+r[1];
            uint32_t pid=((0x1F&r[2])<<8)+r[3];
            r+=4;
            len-=4;
            printf("[TsDemuxer] Pat : Prg:%d Pid: 0x%04x\n",prg,pid);
            if(prg) // if prg==0, it is network Pid, dont need it
                listOfPmt.push_back(pid);
        }
        int size=listOfPmt.size();
        if(size)
        {
            for(int i=0;i<size;i++) // First PMT is PCR lock ?
            {
                printf("<<< PMT : %d/%d>>>\n",i,size);
                uint32_t pid=listOfPmt[i];
                t->setPos(0); // Make sure we can have the very first one
                TS_scanPmt(t,pid,&list);
            }

        }
    }
    // List now contains a list of elementary tracks

    // TODO: Look deeper to see if there is actual content
    // Some channels advertsise languages that they may have
    // but dont actually exists in the file
    // Allocate output files, max size=list.size
    
    printf("[TS Demuxer] Found %u interesting tracks\n",(unsigned int)list.size());
    
    // Search the video track...
    bool result=false;
    int videoIndex=-1;
    for(int i=0;i<list.size();i++)
    {
        ADM_TS_TRACK_TYPE type=list[i].trackType;
        if(type==ADM_TS_MPEG2 || type==ADM_TS_H264)
        {
            videoIndex=i;
            break;
        }
    }
    if(videoIndex==-1)
    {
        printf("[Ts Demuxer] Cannot find a video track\n");
        goto _failTs;
    }
    {
        //
 
        // After here we cannot fail (normally...)
        tracks=new ADM_TS_TRACK[list.size()];
        *outTracks=tracks;
        // Copy video track
        tracks[0]=list[videoIndex];
        // and remove it from the list
        list.erase(list.begin()+videoIndex);
        nb++;
        // Also add audio tracks we know of
        for(int i=0;i<list.size();i++)
        {
            ADM_TS_TRACK_TYPE type=list[i].trackType;
            if(type==ADM_TS_MPEG_AUDIO || type==ADM_TS_AC3 || type==ADM_TS_AAC)
            {
                TSpacketInfo pkt;
                t->setPos(0);
                if(true==t->getNextPacket_NoHeader(list[i].trackPid,&pkt,false))
                    tracks[nb++]=list[i];
                else        
                    printf("[TS Demuxer] Track %i pid 0x%x does not seem to be there\n",i,list[i].trackPid);
            }
        }
        *nbTracks=nb;
        result=true;
    }
_failTs:
    delete t;
    // Delete the list
    if(list.size())
        list.erase(list.begin(),list.end()-1);
    if(result==true)
    {
        printf("[T Demuxer] Kept %d tracks\n",*nbTracks);
    }
    printf("[TS Demuxer] Probed...\n");
    
    return result;
}
/**
    \fn TS_scanPmt
    \brief Lookup one PMT and returns content
*/
bool TS_scanPmt(tsPacket *t,uint32_t pid,listOfTsTracks *list)
{
    ADM_TS_TRACK trk;
    uint32_t len;
    TS_PSIpacketInfo psi;
    uint8_t *r=psi.payload;
    trk.trackType=ADM_TS_UNKNOWN;
    trk.trackPid=pid;
    printf("[TsDemuxer] Looking for PMT : 0x%x\n",pid);
    if(t->getNextPSI(pid,&psi)==true)
     {
        
        
        len=psi.payloadSize;
        // We should be protected by CRC here
        int packLen=len;
        printf("[TsDemuxer] PCR 0x%x, len=%d\n",(r[0]<<8)+r[1],packLen);
        r+=2;  
        int programInfoLength=(r[0]<<8)+r[1];
        programInfoLength&=0xff;
        r+=2;
        printf("[PMT] PMT :%02x Program Info Len: %d\n",pid,programInfoLength);    
        packLen-=(2+4);
        while(packLen>4)
        {
                int streamType,streamPid,esInfoLength;
                streamType=r[0];
                streamPid=(r[1]<<8)+r[2]&0x1fff;
                esInfoLength=((r[3]<<8)+r[4])&0xfff;
                r+=5;
                packLen-=5;
                packLen-=esInfoLength;
                TrackTypeDescriptor *td=TrackTypeDescriptor::find(streamType);
                printf("[PMT] PMT :%02x StreamType: 0x%x,<<%s>>\n",pid,streamType,td->desc);    
                printf("[PMT]           Pid:        0x%x\n",streamPid);
                printf("[PMT]           Es Info Length: %d (0x%x)\n",esInfoLength,esInfoLength);
                
                trk.trackType=td->trackType;
                trk.trackPid=streamPid;
                uint8_t *p=r,*pend=r+esInfoLength;
                r+=esInfoLength;
                while(p<pend)
                {
                    uint32_t tag,taglen;
                    tag=p[0];
                    taglen=p[1];
                    printf("[PMT]           Tag :%x len:%d =",tag,taglen);
                    switch(tag) //http://www.coolstf.com/tsreader/descriptors.html
                    {
                        
                        case 0x05: printf("Registration Descriptor :%c%c%c%c",p[2],p[3],p[4],p[5]);break;
                        case 0x0a: printf("Language descriptor :%c%c%c",p[2],p[3],p[4]);break;
                        case 0x11: printf("STD\n");break;
                        case 0x1e: printf("SL descriptor (H264 AAC ?)");break;
                        case 0x45: printf("VBI data\n");break;
                        case 0x52: printf("Stream identifier");break;
                        case 0x56: printf("Teletext");break;
                        case 0x59: printf("DVB subtitles");break;
                        case 0x7b: printf("DTS Descriptor");break;
                        case 0x7a:
                        case 0x6a: printf("AC3 Descriptor");break;
                        default : printf("unknown");break;
                    }
                    printf("\n");
                    p+=2+taglen;
                }
                // 
                if(trk.trackType!=ADM_TS_UNKNOWN) list->push_back(trk);
        }
        return true;
     }
    return false;
}
//EOF
