/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

/**
    \file demo_vcap_venc_vdis_ipc_frames_exch.c
    \brief
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <osa_que.h>
#include <osa_mutex.h>
#include <osa_thr.h>
#include <osa_sem.h>
#include "demo_vcap_venc_vdis.h"
#include "mcfw/interfaces/ti_vcap.h"
#include "mcfw/interfaces/ti_vdis.h"


#define MCFW_IPC_FRAMES_NONOTIFYMODE_FRAMESIN                          (TRUE)
#define MCFW_IPC_FRAMES_NONOTIFYMODE_FRAMESOUT                         (TRUE)

#define MCFW_IPCFRAMES_SENDRECVFXN_TSK_PRI                             (2)
#define MCFW_IPCFRAMES_SENDRECVFXN_TSK_STACK_SIZE                      (0) /* 0 means system default will be used */
#define MCFW_IPCFRAMES_SENDRECVFXN_PERIOD_MS                           (16)


#define MCFW_IPCFRAMES_INFO_PRINT_INTERVAL                             (8192)

#define MCFW_IPCFRAMES_MAX_PENDING_RECV_SEM_COUNT                      (10)
#define MCFW_IPC_FRAMES_TRACE_ENABLE_FXN_ENTRY_EXIT                    (1)
#define MCFW_IPC_FRAMES_TRACE_INFO_PRINT_INTERVAL                      (8192)


#if MCFW_IPC_FRAMES_TRACE_ENABLE_FXN_ENTRY_EXIT
#define MCFW_IPC_FRAMES_TRACE_FXN(str,...)         do {                           \
                                                     static Int printInterval = 0;\
                                                     if ((printInterval % MCFW_IPC_FRAMES_TRACE_INFO_PRINT_INTERVAL) == 0) \
                                                     {                                                          \
                                                         OSA_printf("MCFW_IPCFRAMES:%s function:%s",str,__func__);     \
                                                         OSA_printf(__VA_ARGS__);                               \
                                                     }                                                          \
                                                     printInterval++;                                           \
                                                   } while (0)
#define MCFW_IPC_FRAMES_TRACE_FXN_ENTRY(...)                  MCFW_IPC_FRAMES_TRACE_FXN("Entered",__VA_ARGS__)
#define MCFW_IPC_FRAMES_TRACE_FXN_EXIT(...)                   MCFW_IPC_FRAMES_TRACE_FXN("Leaving",__VA_ARGS__)
#else
#define MCFW_IPC_FRAMES_TRACE_FXN_ENTRY(...)
#define MCFW_IPC_FRAMES_TRACE_FXN_EXIT(...)
#endif



static void  VcapVenc_ipcFramesPrintFrameInfo(VIDEO_FRAMEBUF_S *buf)
{
    OSA_printf("MCFW_IPCFRAMES:VIDFRAME_INFO:"
               "chNum:%d\t"
               "fid:%d\t"
               "frameWidth:%d\t"
               "frameHeight:%d\t"
               "timeStamp:%d\t"
               "virtAddr[0][0]:%p\t"
               "phyAddr[0][0]:%p",
                buf->channelNum,
                buf->fid,
                buf->frameWidth,
                buf->frameHeight,
                buf->timeStamp,
                buf->addr[0][0],
                buf->phyAddr[0][0]);

}


static void  VcapVenc_ipcFramesPrintFullFrameListInfo(VIDEO_FRAMEBUF_LIST_S *bufList,
                                                char *listName)
{
    static Int printStatsInterval = 0;
    if ((printStatsInterval % MCFW_IPCFRAMES_INFO_PRINT_INTERVAL) == 0)
    {
        Int i;

        OSA_printf("MCFW_IPCFRAMES:VIDFRAMELIST_INFO:%s\t"
                   "numFullFrames:%d",
                   listName,
                   bufList->numFrames);
        for (i = 0; i < bufList->numFrames; i++)
        {
             VcapVenc_ipcFramesPrintFrameInfo(&bufList->frames[i]);
        }
    }
    printStatsInterval++;
}


static Void * VcapVenc_ipcFramesSendRecvFxn(Void * prm)
{
     VcapVenc_IpcFramesCtrlThrObj *thrObj = ( VcapVenc_IpcFramesCtrlThrObj *) prm;
    static Int printStatsInterval = 0;
    VIDEO_FRAMEBUF_LIST_S bufList;
    Int status;

    OSA_printf("MCFW_IPCFRAMES:%s:Entered...",__func__);
    OSA_semWait(&thrObj->framesInNotifySem,OSA_TIMEOUT_FOREVER);
    OSA_printf("MCFW_IPCFRAMES:Received first frame notify...");
    while (FALSE == thrObj->exitFramesInOutThread)
    {
        status =  Vcap_getFullVideoFrames(&bufList,0);
        OSA_assert(0 == status);
        if (bufList.numFrames)
        {
             VcapVenc_ipcFramesPrintFullFrameListInfo(&bufList,"FullFrameList");
            status = Vdis_putFullVideoFrames(&bufList);
            OSA_assert(0 == status);
        }
        status =  Vdis_getEmptyVideoFrames(&bufList,0);
        OSA_assert(0 == status);

        if (bufList.numFrames)
        {
             VcapVenc_ipcFramesPrintFullFrameListInfo(&bufList,"EmptyFrameList");
            status = Vcap_putEmptyVideoFrames(&bufList);
            OSA_assert(0 == status);
        }
        #ifdef IPC_BITS_DEBUG
        if ((printStatsInterval % MCFW_IPCFRAMES_INFO_PRINT_INTERVAL) == 0)
        {
            OSA_printf("MCFW_IPCFRAMES:%s:INFO: periodic print..",__func__);
        }
        #endif
        printStatsInterval++;
        OSA_waitMsecs(MCFW_IPCFRAMES_SENDRECVFXN_PERIOD_MS);
    }
    OSA_printf("MCFW_IPCFRAMES:%s:Leaving...",__func__);
    return NULL;
}


static Void  VcapVenc_ipcFramesInitThrObj( VcapVenc_IpcFramesCtrlThrObj *thrObj)
{

    OSA_semCreate(&thrObj->framesInNotifySem,
                  MCFW_IPCFRAMES_MAX_PENDING_RECV_SEM_COUNT,0);
    thrObj->exitFramesInOutThread = FALSE;
    OSA_thrCreate(&thrObj->thrHandleFramesInOut,
                   VcapVenc_ipcFramesSendRecvFxn,
                  MCFW_IPCFRAMES_SENDRECVFXN_TSK_PRI,
                  MCFW_IPCFRAMES_SENDRECVFXN_TSK_STACK_SIZE,
                  thrObj);

}

static Void  VcapVenc_ipcFramesDeInitThrObj( VcapVenc_IpcFramesCtrlThrObj *thrObj)
{
    thrObj->exitFramesInOutThread = TRUE;
    OSA_thrDelete(&thrObj->thrHandleFramesInOut);
    OSA_semDelete(&thrObj->framesInNotifySem);
}


static
Void  VcapVenc_ipcFramesInCbFxn (Ptr cbCtx)
{
     VcapVenc_IpcFramesCtrl *ipcFramesCtrl;
    static Int printInterval;

    OSA_assert(cbCtx = &gVcapVenc_ctrl.ipcFrames);
    ipcFramesCtrl = cbCtx;
    OSA_semSignal(&ipcFramesCtrl->thrObj.framesInNotifySem);
    if ((printInterval % MCFW_IPCFRAMES_INFO_PRINT_INTERVAL) == 0)
    {
        OSA_printf("MCFW_IPCFRAMES: Callback function:%s",__func__);
    }
    printInterval++;
}


Void  VcapVenc_ipcFramesInSetCbInfo ()
{
    VCAP_CALLBACK_S vcapCallback;

    vcapCallback.newDataAvailableCb =  VcapVenc_ipcFramesInCbFxn;

    Vcap_registerCallback(&vcapCallback, &gVcapVenc_ctrl.ipcFrames);
}

Int32  VcapVenc_ipcFramesCreate()
{
     VcapVenc_ipcFramesInitThrObj(&gVcapVenc_ctrl.ipcFrames.thrObj);
    return OSA_SOK;
}

Void  VcapVenc_ipcFramesStop(void)
{
    gVcapVenc_ctrl.ipcFrames.thrObj.exitFramesInOutThread = TRUE;
}

Int32  VcapVenc_ipcFramesDelete()
{
    OSA_printf("Entered:%s...",__func__);
    VcapVenc_ipcFramesDeInitThrObj(&gVcapVenc_ctrl.ipcFrames.thrObj);
    OSA_printf("Leaving:%s...",__func__);
    return OSA_SOK;
}


