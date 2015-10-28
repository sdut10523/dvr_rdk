/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/



#ifndef _SCD_BIT_WR_H_
#define _SCD_BIT_WR_H_

#include <demo.h>

//#define IPC_BITS_DEBUG

typedef struct {

    UInt32 totalDataSize;
    UInt32 numKeyFrames;
    UInt32 numFrames;
    UInt32 maxWidth;
    UInt32 minWidth;
    UInt32 maxHeight;
    UInt32 minHeight;
    UInt32 maxLatency;
    UInt32 minLatency;

} Scd_ChInfo;

typedef struct Scd_IpcFramesCtrlThrObj {
    OSA_ThrHndl thrHandleFramesInOut;
    OSA_SemHndl framesInNotifySem;
    volatile Bool exitFramesInOutThread;
} Scd_IpcFramesCtrlThrObj;

typedef struct Scd_IpcFramesCtrl {
    Bool  noNotifyFramesInHLOS;
    Bool  noNotifyFramesOutHLOS;
    Scd_IpcFramesCtrlThrObj  thrObj;
} Scd_IpcFramesCtrl;

typedef struct {

    OSA_ThrHndl wrThrHndl;
    OSA_SemHndl wrSem;
    Bool exitWrThr;
    Bool isWrThrStopDone;

    Scd_ChInfo chInfo[VENC_CHN_MAX];

    UInt32 statsStartTime;

    Bool fileWriteEnable;
    char fileWriteName[512];
    UInt32  chId;
    UInt32  fileWriteChn;
    Scd_IpcFramesCtrl ipcFrames;

} Scd_Ctrl;


extern Scd_Ctrl gScd_ctrl;


Int32 Scd_resetStatistics();
Int32 Scd_printStatistics(Bool resetStats);

Int32 Scd_bitsWriteCreate();
Int32 Scd_bitsWriteDelete();
Void Scd_bitsWriteStop();

Int32 Scd_ipcFramesCreate();
Int32 Scd_ipcFramesDelete();
Void  Scd_ipcFramesStop(void);
Void  Scd_ipcFramesInSetCbInfo (void);

#endif
