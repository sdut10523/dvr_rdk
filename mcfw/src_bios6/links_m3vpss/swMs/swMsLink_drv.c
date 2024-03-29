/*******************************************************************************
 *                                                                             *
 * Copyright (c) 2009 Texas Instruments Incorporated - http://www.ti.com/      *
 *                        ALL RIGHTS RESERVED                                  *
 *                                                                             *
 ******************************************************************************/

#include "swMsLink_priv.h"
#include "mcfw/src_bios6/links_m3vpss/system/system_priv_m3vpss.h"
#include <mcfw/src_bios6/links_m3vpss/avsync/avsync_m3vpss.h>
#include <mcfw/interfaces/common_def/ti_vsys_common_def.h>


UInt_64 input_timestamp[300];
#if AVSYNC_COMP_ENABLE
static UInt32 inputTimestampCount;
#endif

#define SWMSLINK_DEBUG_BLANK_OUTPUT_BUFFER                              (FALSE)

static
Int32 SwMsLink_fillDataPattern(Utils_DmaChObj *swMsDmaChObj,
                               FVID2_Format * pFormat,
                               FVID2_Frame * pFrame,
                               UInt16 numFrames,
                               Bool isTiled)
{

    Int32 status;
    FVID2_Frame tempFrame;


#if 0
    UInt32  frameId;
    UInt8 * bufBaseAddrY, *bufBaseAddrCbCr;
    UInt32  cbcrHeight,cbcrPitch;

    cbcrHeight = pFormat->height;
    if ((pFormat->dataFormat == FVID2_DF_YUV420SP_UV)
        ||
        (pFormat->dataFormat == FVID2_DF_YUV420SP_UV)
        ||
        (pFormat->dataFormat == FVID2_DF_YUV420SP_VU))
    {
        cbcrHeight /= 2;
    }
    switch (pFormat->dataFormat)
    {
        case FVID2_DF_YUV422I_YUYV:
        case FVID2_DF_YUV422I_YVYU:
        case FVID2_DF_YUV422I_VYUY:
            UTILS_assert(FALSE == isTiled);
            bufBaseAddrY = (UInt8 *)pFrame->addr[0][0];
            bufBaseAddrCbCr = ((UInt8 *)pFrame->addr[0][0]) + 1;
            cbcrPitch   = pFormat->pitch[0];
            break;
        case FVID2_DF_YUV422SP_UV:
        case FVID2_DF_YUV420SP_UV:
            bufBaseAddrY = (UInt8 *)pFrame->addr[0][0];
            if (isTiled)
                bufBaseAddrY =
                      (UInt8 *)(Utils_tilerAddr2CpuAddr((UInt32)bufBaseAddrY));
            bufBaseAddrCbCr = (UInt8 *)pFrame->addr[0][1];
            if (isTiled)
                bufBaseAddrCbCr =
                    (UInt8 *)(Utils_tilerAddr2CpuAddr((UInt32)bufBaseAddrCbCr));
            cbcrPitch   = pFormat->pitch[1];
            break;
        default:
            Vps_printf ("SWMSLINK:Warning. Unknown data format. WIll not blank");
            return (FVID2_SOK);
    }

    for (frameId = 0; frameId < numFrames; frameId++)
    {
        Utils_DmaFill2D blankFrameInfo[SWMS_LINK_DMA_MAX_TRANSFERS];
        UInt16  numTx;
        Int32 status;

        numTx=0;

        blankFrameInfo[numTx].destAddr[0] = bufBaseAddrY;
        blankFrameInfo[numTx].destPitch[0] = pFormat->pitch[0];
        blankFrameInfo[numTx].destAddr[1] = bufBaseAddrCbCr;
        blankFrameInfo[numTx].destPitch[1] = cbcrPitch;

        blankFrameInfo[numTx].dataFormat  = pFormat->dataFormat;
        blankFrameInfo[numTx].width = pFormat->width;
        blankFrameInfo[numTx].height = pFormat->height;
        blankFrameInfo[numTx].fillColorYUYV =
                             UTILS_DMA_GENERATE_FILL_PATTERN(
                                                0x80,
                                                0x80,
                                                0x80);
        blankFrameInfo[numTx].startX = 0;
        blankFrameInfo[numTx].startY = 0;
        #ifdef SWMSLINK_DEBUG_BLANK_OUTPUT_BUFFER
            Vps_printf("%d:SWMS: Start Blanking of output buffer: "
                       "BufAddr:[%p] Width:[%d] Height:[%d] Pitch:[%d] Pixel:[0x%X]",
                       Utils_getCurTimeInMsec(),
                       blankFrameInfo[numTx].destAddr[0],
                       blankFrameInfo[numTx].width,
                       blankFrameInfo[numTx].height,
                       blankFrameInfo[numTx].destPitch[0],
                       blankFrameInfo[numTx].fillColorYUYV);
        #endif
        numTx++;

        UTILS_assert(numTx<=SWMS_LINK_DMA_MAX_TRANSFERS);

        status = Utils_dmaFill2D(swMsDmaChObj, blankFrameInfo, numTx);

        if (UTILS_ISERROR(status))
        {
            Vps_printf("SWMS: Utils_dmaFill2D for output buffer failed!!");
        }
        else
        {
            #ifdef SWMSLINK_DEBUG_BLANK_OUTPUT_BUFFER
                Vps_printf("%d:SWMS: End Blanking of output buffer: "
                           "BufAddr:[%p]",
                           Utils_getCurTimeInMsec(),
                           blankFrameInfo[0].destAddr[0]);
            #endif
        }

    }
#else

    if (isTiled)
    {
        /* Creating a temporary frame */
        status = Utils_memFrameAlloc(pFormat,
                                     &tempFrame,
                                     numFrames);
        UTILS_assert(status == FVID2_SOK);

        tempFrame.addr[0][1] = tempFrame.addr[0][0];

        /* Assigning grey value to temporary frame */
        memset(tempFrame.addr[0][0], 0x80, ((pFormat->width)*(pFormat->height)));

        /* Copying it to Y buffer  */
        status = Utils_tilerCopy(
                    UTILS_TILER_COPY_FROM_DDR,
                    (UInt32)pFrame->addr[0][0],
                    pFormat->width,
                    pFormat->height,
                    tempFrame.addr[0][0],
                    pFormat->pitch[0]);
        UTILS_assert(status==FVID2_SOK);

        /* Copying it to CbCr buffer  */
        status = Utils_tilerCopy(
                    UTILS_TILER_COPY_FROM_DDR,
                    (UInt32)pFrame->addr[0][1],
                    pFormat->width,
                    (pFormat->height/2),
                    tempFrame.addr[0][0],
                    pFormat->pitch[1]);
        UTILS_assert(status==FVID2_SOK);

        status = Utils_memFrameFree(pFormat,
                                    &tempFrame,
                                    numFrames);
    }
	else
	{
	    memset(pFrame->addr[0][0], 0x80, ((pFormat->width)*(pFormat->height)));
		memset(pFrame->addr[0][1], 0x80, ((pFormat->width)*(pFormat->height)));
	}


#endif
    return FVID2_SOK;
}

static
Void  SwMsLink_drvSetBlankOutputFlag(SwMsLink_Obj * pObj)
{
    Int frameId;

    for (frameId = 0; frameId < UTILS_ARRAYSIZE(pObj->outFrameInfo);
         frameId++)
    {
        pObj->outFrameInfo[frameId].swMsBlankOutBuf =
            SWMS_LINK_DO_OUTBUF_BLANKING;
    }
}

static
Int32 SwMsLink_drvDmaCreate(SwMsLink_Obj * pObj)
{
    Int32 status;

    status = Utils_dmaCreateCh(&pObj->dmaObj,
                               UTILS_DMA_DEFAULT_EVENT_Q,
                               SWMS_LINK_DMA_MAX_TRANSFERS);
    UTILS_assert(status==FVID2_SOK);

    if(pObj->createArgs.enableLayoutGridDraw == TRUE)
    {
        status = Utils_dmaCreateCh(&pObj->gridDmaObj,
                                   UTILS_DMA_DEFAULT_EVENT_Q,
                                   SWMS_LINK_DMA_GRID_MAX_TRANSFERS);
        UTILS_assert(status==FVID2_SOK);
    }

    return status;
}

static
Int32 SwMsLink_drvDmaDelete(SwMsLink_Obj * pObj)
{
    Int32 status;

    status = Utils_dmaDeleteCh(&pObj->dmaObj);
    UTILS_assert(status==FVID2_SOK);

    if(pObj->createArgs.enableLayoutGridDraw == TRUE)
    {
        status = Utils_dmaDeleteCh(&pObj->gridDmaObj);
        UTILS_assert(status==FVID2_SOK);
    }

    return status;
}

Int32 SwMsLink_drvDoDma(SwMsLink_Obj * pObj, FVID2_Frame *pFrame)
{
    Int32 status = 0,i;
    Utils_DmaFill2D lineInfo[SWMS_LINK_DMA_GRID_MAX_TRANSFERS];

    UInt16 thickness, numTx;

    UInt32 pitch;

    thickness = 4;

    pitch = pObj->outFrameFormat.pitch[0];

    numTx=0;

    for(i=0;i<pObj->layoutParams.numWin;i++)
    {

    /*Horizontal Top line*/
    lineInfo[numTx].destAddr[0] = pFrame->addr[0][0];
    lineInfo[numTx].destPitch[0] = pitch;
    lineInfo[numTx].dataFormat  = FVID2_DF_YUV422I_YUYV;
    lineInfo[numTx].width = pObj->layoutParams.winInfo[i].width;
    lineInfo[numTx].height = thickness;
    lineInfo[numTx].fillColorYUYV =
        UTILS_DMA_GENERATE_FILL_PATTERN(SW_MS_GRID_FILL_PIXEL_LUMA ,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA);

    lineInfo[numTx].startX = pObj->layoutParams.winInfo[i].startX;
    lineInfo[numTx].startY = pObj->layoutParams.winInfo[i].startY;
    numTx++;

    /*Horizontal Bottom line*/
    lineInfo[numTx].destAddr[0] = pFrame->addr[0][0];
    lineInfo[numTx].destPitch[0] = pitch;
    lineInfo[numTx].dataFormat  = FVID2_DF_YUV422I_YUYV;
    lineInfo[numTx].width = pObj->layoutParams.winInfo[i].width;
    lineInfo[numTx].height = thickness;
    lineInfo[numTx].fillColorYUYV =
        UTILS_DMA_GENERATE_FILL_PATTERN(SW_MS_GRID_FILL_PIXEL_LUMA ,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA);

    lineInfo[numTx].startX = pObj->layoutParams.winInfo[i].startX;
    lineInfo[numTx].startY = pObj->layoutParams.winInfo[i].startY +
    pObj->layoutParams.winInfo[i].height - thickness;
    numTx++;

    /*Vertical Left side*/
    lineInfo[numTx].destAddr[0] = pFrame->addr[0][0];
    lineInfo[numTx].destPitch[0] = pitch;
    lineInfo[numTx].dataFormat  = FVID2_DF_YUV422I_YUYV;
    lineInfo[numTx].width = thickness;
    lineInfo[numTx].height = pObj->layoutParams.winInfo[i].height;
    lineInfo[numTx].fillColorYUYV =
        UTILS_DMA_GENERATE_FILL_PATTERN(SW_MS_GRID_FILL_PIXEL_LUMA ,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA);

    lineInfo[numTx].startX = pObj->layoutParams.winInfo[i].startX +
    pObj->layoutParams.winInfo[i].width - thickness;
    lineInfo[numTx].startY = pObj->layoutParams.winInfo[i].startY;
    numTx++;

    /*Vertical right side*/
    lineInfo[numTx].destAddr[0] = pFrame->addr[0][0];
    lineInfo[numTx].destPitch[0] = pitch;
    lineInfo[numTx].dataFormat  = FVID2_DF_YUV422I_YUYV;
    lineInfo[numTx].width = thickness;
    lineInfo[numTx].height =  pObj->layoutParams.winInfo[i].height;
    lineInfo[numTx].fillColorYUYV =
        UTILS_DMA_GENERATE_FILL_PATTERN(SW_MS_GRID_FILL_PIXEL_LUMA ,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA,
                                        SW_MS_GRID_FILL_PIXEL_CHROMA);

    lineInfo[numTx].startX = pObj->layoutParams.winInfo[i].startX;
    lineInfo[numTx].startY = pObj->layoutParams.winInfo[i].startY;
    numTx++;

    UTILS_assert(numTx<=SWMS_LINK_DMA_GRID_MAX_TRANSFERS);

    status = Utils_dmaFill2D(&pObj->gridDmaObj, lineInfo, numTx);
    numTx = 0;
    }
    return status;
}


static
Int32 SwMsLink_drvCheckBlankOutputBuffer(SwMsLink_Obj * pObj, FVID2_Frame *pFrame)
{
    Int32 status = 0;
    System_FrameInfo *frameInfo = pFrame->appData;

    if ((NULL != frameInfo)
        &&
        (frameInfo->swMsBlankOutBuf))
    {
        Utils_DmaFill2D blankFrameInfo[SWMS_LINK_DMA_MAX_TRANSFERS];
        UInt16  numTx;
        UInt32 pitch;

        pitch = pObj->outFrameFormat.pitch[0];

        numTx=0;

        blankFrameInfo[numTx].destAddr[0] = pFrame->addr[0][0];
        blankFrameInfo[numTx].destPitch[0] = pitch;
        blankFrameInfo[numTx].dataFormat  = FVID2_DF_YUV422I_YUYV;
        blankFrameInfo[numTx].width = pObj->outFrameFormat.width;
        blankFrameInfo[numTx].height = pObj->outFrameFormat.height;
        blankFrameInfo[numTx].fillColorYUYV =
                UTILS_DMA_GENERATE_FILL_PATTERN(SW_MS_BLANK_FRAME_PIXEL_LUMA ,
                                                SW_MS_BLANK_FRAME_PIXEL_CHROMA,
                                                SW_MS_BLANK_FRAME_PIXEL_CHROMA);
        blankFrameInfo[numTx].startX = 0;
        blankFrameInfo[numTx].startY = 0;
        #if SWMSLINK_DEBUG_BLANK_OUTPUT_BUFFER
            Vps_printf("%d:SWMS: Start Blanking of output buffer: "
                       "BufAddr:[%p] Width:[%d] Height:[%d] Pitch:[%d] Pixel:[0x%X]",
                       Utils_getCurTimeInMsec(),
                       blankFrameInfo[numTx].destAddr[0],
                       blankFrameInfo[numTx].width,
                       blankFrameInfo[numTx].height,
                       blankFrameInfo[numTx].destPitch[0],
                       blankFrameInfo[numTx].fillColorYUYV);
        #endif
        numTx++;

        UTILS_assert(numTx<=SWMS_LINK_DMA_MAX_TRANSFERS);

        status = Utils_dmaFill2D(&pObj->dmaObj, blankFrameInfo, numTx);

        if (UTILS_ISERROR(status))
        {
            Vps_printf("SWMS: Utils_dmaFill2D for output buffer failed!!");
        }
        else
        {
            #if SWMSLINK_DEBUG_BLANK_OUTPUT_BUFFER
                Vps_printf("%d:SWMS: End Blanking of output buffer: "
                       "BufAddr:[%p] ",
                       Utils_getCurTimeInMsec(),
                       blankFrameInfo[0].destAddr[0]);
            #endif
        }
        frameInfo->swMsBlankOutBuf = FALSE;
    }

    return status;
}

Int32 SwMsLink_drvResetStatistics(SwMsLink_Obj * pObj)
{
    UInt32 winId;

    SwMsLink_OutWinObj *pWinObj;

    for(winId=0; winId<SYSTEM_SW_MS_MAX_WIN; winId++)
    {
        pWinObj = &pObj->winObj[winId];

        pWinObj->framesRecvCount = 0;
        pWinObj->framesInvalidChCount = 0;
        pWinObj->framesRejectCount = 0;
        pWinObj->framesQueRejectCount = 0;
        pWinObj->framesQueuedCount = 0;
        pWinObj->framesRepeatCount = 0;
        pWinObj->framesAccEventCount = 0;
        pWinObj->framesAccMax = 0;
        pWinObj->framesAccMin = 0xFF;
        pWinObj->framesDroppedCount = 0;
        pWinObj->framesUsedCount = 0;
        pWinObj->framesFidInvalidCount = 0;

        pWinObj->minLatency = 0xFF;
        pWinObj->maxLatency = 0;
    }
    pObj->framesOutReqCount = 0;
    pObj->framesOutDropCount = 0;
    pObj->framesOutRejectCount = 0;
    pObj->framesOutCount = 0;
    pObj->prevDoScalingTime  = 0;
    pObj->scalingInterval    = 0;
    pObj->scalingIntervalMin = SW_MS_SCALING_INTERVAL_INVALID;
    pObj->scalingIntervalMax = 0;
    pObj->statsStartTime = Utils_getCurTimeInMsec();

    return 0;
}

Int32 SwMsLink_drvPrintStatistics(SwMsLink_Obj * pObj, Bool resetAfterPrint)
{
    UInt32 winId;
    SwMsLink_OutWinObj *pWinObj;
    UInt32 elaspedTime;

    elaspedTime = Utils_getCurTimeInMsec() - pObj->statsStartTime; // in msecs
    elaspedTime /= 1000; // convert to secs

    Vps_printf( " \n"
            " *** [%s] Mosaic Statistics *** \n"
            " \n"
            " Elasped Time: %d secs\n"
            " \n"
            " Output Request FPS   : %d fps (%d frames) \n"
            " Output Actual  FPS   : %d fps (%d frames) \n"
            " Output Drop    FPS   : %d fps (%d frames) \n"
            " Output Reject  FPS   : %d fps (%d frames) \n"
            " Scaling Internal     : %d ms \n"
            " Scaling Internal min : %d ms \n"
            " Scaling Internal max : %d ms \n"
            " \n"
            " Win | Window Repeat Drop Recv Que  FID Invlid Acc Event          Invalid   Que Reject Reject Latency  \n"
            " Num | FPS    FPS    FPS  FPS  FPS  FPS        Count (Max/Min)    CH Frames Frames     Frames Min / Max\n"
            " ------------------------------------------------------------------------------------------------------\n",
            pObj->name,
            elaspedTime,
            pObj->framesOutReqCount/elaspedTime,
            pObj->framesOutReqCount,
            pObj->framesOutCount/elaspedTime,
            pObj->framesOutCount,
            pObj->framesOutDropCount/elaspedTime,
            pObj->framesOutDropCount,
            pObj->framesOutRejectCount/elaspedTime,
            pObj->framesOutRejectCount,
            pObj->scalingInterval/pObj->framesOutReqCount,
            pObj->scalingIntervalMin,
            pObj->scalingIntervalMax
            );


    for (winId = 0; winId < pObj->layoutParams.numWin; winId++)
    {
        pWinObj = &pObj->winObj[winId];

        Vps_printf( " %3d | %6d %6d %4d %4d %4d %10d %8d (%3d/%3d) %9d %10d %6d %3d / %3d\n",
            winId,
            #if 1
            pWinObj->framesUsedCount/elaspedTime,
            pWinObj->framesRepeatCount/elaspedTime,
            pWinObj->framesDroppedCount/elaspedTime,
            pWinObj->framesRecvCount/elaspedTime,
            pWinObj->framesQueuedCount/elaspedTime,
            pWinObj->framesFidInvalidCount/elaspedTime,
            #else
            pWinObj->framesUsedCount,
            pWinObj->framesRepeatCount,
            pWinObj->framesDroppedCount,
            pWinObj->framesRecvCount,
            pWinObj->framesQueuedCount,
            pWinObj->framesFidInvalidCount,
            #endif
            pWinObj->framesAccEventCount,
            pWinObj->framesAccMax,
            pWinObj->framesAccMin,
            pWinObj->framesInvalidChCount,
            pWinObj->framesQueRejectCount,
            pWinObj->framesRejectCount,
            pWinObj->minLatency,
            pWinObj->maxLatency
            );
    }

    Vps_printf( " \n");

    SwMsLink_drvPrintLayoutParams(pObj);

    Vps_printf( " \n");

    if(resetAfterPrint)
    {
        SwMsLink_drvResetStatistics(pObj);
    }

    return 0;
}

Int32 SwMsLink_drvModifyFramePointer(SwMsLink_Obj * pObj, SwMsLink_DrvObj *pDrvObj,
                                         Bool addOffset)
{
    Int32 offset,i;
    System_LinkChInfo *pChInfo;
    System_LinkChInfo *rtChannelInfo;

    for(i = 0; i< pDrvObj->inFrameList.numFrames; i++)
    {
        if (pObj->layoutParams.winInfo[i].channelNum <
            pObj->inQueInfo.numCh)
        {
            pChInfo = &pObj->inQueInfo.
                 chInfo[pObj->layoutParams.winInfo[i].channelNum];
            rtChannelInfo = &pObj->
                 rtChannelInfo[pObj->layoutParams.winInfo[i].channelNum];

            if(addOffset == TRUE)
                offset = pChInfo->startX;
            else
                offset = -(pChInfo->startX);

            if(pChInfo->dataFormat == SYSTEM_DF_YUV422I_YUYV)
                offset *= 2;

            if ((rtChannelInfo->width + pChInfo->startX) > SW_MS_MAX_WIDTH_SUPPORTED)
            {
               pDrvObj->inFrameList.frames[i]->addr[0][0] =
                  (Ptr) ((Int32)pDrvObj->inFrameList.frames[i]->addr[0][0] + offset);
               pDrvObj->inFrameList.frames[i]->addr[0][1] =
                  (Ptr) ((Int32)pDrvObj->inFrameList.frames[i]->addr[0][1] + offset);
            }

        }
    }
    return 0;
}

#include <mcfw/src_bios6/utils/utils_dmtimer.h>
static
Void DmTimerTest()
{
    static  UInt32          last_time_in_msec =0;
    static  UInt32          numUpdates = 1;
    static  UInt32          totalElapsedTime =0;
    UInt32 curTime;

    if (last_time_in_msec == 0)
    {
        last_time_in_msec = Utils_dmTimerGetCurTimeInMsec(SYSTEM_DMTIMER_ID);
    }
    else
    {
        curTime           = Utils_dmTimerGetCurTimeInMsec(SYSTEM_DMTIMER_ID);
        if (curTime > last_time_in_msec)
        {
            totalElapsedTime += curTime
                                -
                                last_time_in_msec;
            last_time_in_msec = curTime;
            numUpdates++;
        }
    }
    if ((numUpdates % 1000) == 0)
    {
        Vps_rprintf("DMTIMER_TEST:TotElapsed:%d:NumUpdates:%d",
                    totalElapsedTime,(numUpdates - 1));
        totalElapsedTime = 0;
        numUpdates       = 1;
    }
}

Void SwMsLink_drvTimerCb(UArg arg)
{
    SwMsLink_Obj *pObj = (SwMsLink_Obj *) arg;

    if (pObj->linkId == SYSTEM_LINK_ID_SW_MS_MULTI_INST_0)
    {
        DmTimerTest();
    }
//    if (pObj->linkId != SYSTEM_LINK_ID_SW_MS_MULTI_INST_1)
       Utils_tskSendCmd(&pObj->tsk, SW_MS_LINK_CMD_DO_SCALING);
}

Int32 SwMsLink_drvFvidCb(FVID2_Handle handle, Ptr appData, Ptr reserved)
{
    SwMsLink_DrvObj *pDrvObj = (SwMsLink_DrvObj *) appData;

    Semaphore_post(pDrvObj->complete);

    return FVID2_SOK;
}

Int32 SwMsLink_drvFvidErrCb(FVID2_Handle handle,
                            Ptr appData, Ptr errList, Ptr reserved)
{
    return FVID2_SOK;
}

Int32 SwMsLink_drvGetLayoutParams(SwMsLink_Obj * pObj, SwMsLink_LayoutPrm * layoutParams)
{
    SwMsLink_drvLock(pObj);

    memcpy(layoutParams, &pObj->layoutParams, sizeof(*layoutParams));

    SwMsLink_drvUnlock(pObj);

    return FVID2_SOK;

}

UInt32 SwMsLink_getDrvInstFromWinId(SwMsLink_Obj *pObj, UInt32 winId)
{
    UInt32 i;
    for(i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        if(winId <= pObj->DrvObj[i].endWin)
            break;
    }
    return i;
}

Int32 SwMsLink_drvPrintLayoutParams(SwMsLink_Obj * pObj)
{
    UInt32 winId, chNum;
    SwMsLink_OutWinObj *pWinObj;
    char strDataFormat[8];

    Vps_printf( " \n"
            " *** [%s] Mosaic Parameters *** \n"
            " \n"
            " Output FPS: %d\n"
            " \n"
            " Win | Ch  | Input      | Input          | Input         | Input       | Output     |  Output         | Output        | Output      | Low Cost | SWMS | Data  |\n"
            " Num | Num | Start X, Y | Width x Height | Pitch Y / C   | Memory Type | Start X, Y |  Width x Height | Pitch Y / C   | Memory Type | ON / OFF | Inst | Format|\n"
            " --------------------------------------------------------------------------------------------------------------------------------------------------------------\n",
            pObj->name,
            pObj->layoutParams.outputFPS
            );


    for (winId = 0; winId < pObj->layoutParams.numWin; winId++)
    {
        pWinObj = &pObj->winObj[winId];

        chNum = pObj->layoutParams.winInfo[winId].channelNum;

        if(pWinObj->scRtInFrmPrm.dataFormat==FVID2_DF_YUV422I_YUYV)
            strcpy(strDataFormat, "422I ");
        else
        if(pWinObj->scRtInFrmPrm.dataFormat==FVID2_DF_YUV420SP_UV)
            strcpy(strDataFormat, "420SP");
        else
            strcpy(strDataFormat, "UNKNWN");

        Vps_printf
            (" %3d | %3d | %4d, %4d | %5d x %6d | %5d / %5d | %s | %4d, %4d | %5d x %6d | %5d / %6d | %s | %8s | %4d | %6s |\n",
                winId,
                chNum,
                pWinObj->scRtCropCfg.cropStartX,
                pWinObj->scRtCropCfg.cropStartY,
                pWinObj->scRtInFrmPrm.width,
                pWinObj->scRtInFrmPrm.height,
                pWinObj->scRtInFrmPrm.pitch[0],
                pWinObj->scRtInFrmPrm.pitch[1],
                gSystem_nameMemoryType[pWinObj->scRtInFrmPrm.memType],
                pObj->layoutParams.winInfo[winId].startX,
                pObj->layoutParams.winInfo[winId].startY,
                pWinObj->scRtOutFrmPrm.width,
                pWinObj->scRtOutFrmPrm.height,
                pWinObj->scRtOutFrmPrm.pitch[0],
                pWinObj->scRtOutFrmPrm.pitch[1],
                gSystem_nameMemoryType[pWinObj->scRtOutFrmPrm.memType],
                gSystem_nameOnOff[pObj->layoutParams.winInfo[winId].bypass],
                SwMsLink_getDrvInstFromWinId(pObj,winId),
                strDataFormat
            );
    }

    Vps_printf( " \n");

    return FVID2_SOK;
}


Int32 SwMsLink_drvUpdateRtChannelInfo(SwMsLink_Obj * pObj)
{
    UInt32 chId;
    System_LinkChInfo *pChInfo;

    SwMsLink_drvLock(pObj);

   for (chId = 0; chId < pObj->inQueInfo.numCh; chId++)
   {
       UTILS_assert (chId < SYSTEM_SW_MS_MAX_CH_ID);
       pChInfo = &pObj->inQueInfo.chInfo[chId];

       pObj->rtChannelInfo[chId].startX = VpsUtils_align(pChInfo->startX, 2);
       pObj->rtChannelInfo[chId].startY = VpsUtils_align(pChInfo->startY, 2);
       pObj->rtChannelInfo[chId].width = VpsUtils_align(pChInfo->width, 2);
       pObj->rtChannelInfo[chId].height = VpsUtils_align(pChInfo->height, 2);
       pObj->rtChannelInfo[chId].pitch[0] = pChInfo->pitch[0];
       pObj->rtChannelInfo[chId].pitch[1] = pChInfo->pitch[1];
       pObj->rtChannelInfo[chId].pitch[2] = pChInfo->pitch[2];
       pObj->rtChannelInfo[chId].memType = pChInfo->memType;
       pObj->rtChannelInfo[chId].dataFormat = pChInfo->dataFormat;
   }

   SwMsLink_drvUnlock(pObj);

   return FVID2_SOK;
}

Int32 SwMsLink_drvGetInputChInfoFromWinId(SwMsLink_Obj * pObj,SwMsLink_WinInfo * pCropInfo)
{
    UInt32  chnl;

    SwMsLink_drvLock(pObj);

    chnl = pObj->layoutParams.winInfo[pCropInfo->winId].channelNum;

    pCropInfo->startX   = pObj->inTskInfo.queInfo[0].chInfo[chnl].startX;
    pCropInfo->startY   = pObj->inTskInfo.queInfo[0].chInfo[chnl].startY;
    pCropInfo->width    = pObj->inTskInfo.queInfo[0].chInfo[chnl].width;
    pCropInfo->height   = pObj->inTskInfo.queInfo[0].chInfo[chnl].height;

    if ((FVID2_SF_PROGRESSIVE == pObj->inQueInfo.chInfo[chnl].scanFormat) &&
        pObj->layoutParams.winInfo[pCropInfo->winId].bypass &&
        VPS_VPDMA_MT_NONTILEDMEM == pObj->inQueInfo.chInfo[chnl].memType
        )
    {
        pCropInfo->height   /= 2;
    }

    SwMsLink_drvUnlock(pObj);
    return FVID2_SOK;
}

Int32 SwMsLink_drvSetCropParam(SwMsLink_Obj * pObj,SwMsLink_WinInfo * pCropInfo)
{
    SwMsLink_OutWinObj *pWinObj;

    SwMsLink_drvLock(pObj);

    pWinObj = &pObj->winObj[pCropInfo->winId];

    pWinObj->scRtCropCfg.cropStartX =
        VpsUtils_align(pCropInfo->startX, 2);
    pWinObj->scRtCropCfg.cropStartY =
        VpsUtils_align(pCropInfo->startY, 2);
    pWinObj->scRtCropCfg.cropWidth = VpsUtils_align(pCropInfo->width, 2);
    pWinObj->scRtCropCfg.cropHeight =
        VpsUtils_align(pCropInfo->height, 2);

    pWinObj->scRtInFrmPrm.width =
        pWinObj->scRtCropCfg.cropStartX +
        pWinObj->scRtCropCfg.cropWidth;
    pWinObj->scRtInFrmPrm.height =
        pWinObj->scRtCropCfg.cropStartY +
        pWinObj->scRtCropCfg.cropHeight;

    pWinObj->applyRtPrm = TRUE;



    SwMsLink_drvUnlock(pObj);
    return FVID2_SOK;
}


Int32 SwMsLink_drvSwitchLayout(SwMsLink_Obj * pObj,
                               SwMsLink_LayoutPrm * layoutParams,
                               Bool isLockAlredayTaken)
{
    UInt32 winId, chNum;
    SwMsLink_OutWinObj *pWinObj;
    System_LinkChInfo *pChInfo;
    UInt32 drvInst;

    if (isLockAlredayTaken == FALSE)
    {
        SwMsLink_drvLock(pObj);
    }

    if(layoutParams->onlyCh2WinMapChanged == FALSE)
    {
          SwMsLink_drvResetStatistics(pObj);

          pObj->switchLayout = TRUE;
          pObj->skipProcessing = SW_MS_SKIP_PROCESSING;
          SwMsLink_drvSetBlankOutputFlag(pObj);
    }

    memcpy(&pObj->layoutParams, layoutParams, sizeof(*layoutParams));

    SwMsLink_updateLayoutParams(&pObj->layoutParams, pObj->outFrameFormat.pitch[0]);

    SwMsLink_drvGetTimerPeriod(pObj, layoutParams);
    SwMsLink_drvClockPeriodReconfigure(pObj);

    for (winId = 0; winId < pObj->layoutParams.numWin; winId++)
    {
        drvInst = SwMsLink_getDrvInstFromWinId(pObj,winId);

        pWinObj = &pObj->winObj[winId];

        pWinObj->scRtOutFrmPrm.width = pObj->layoutParams.winInfo[winId].width;
        pWinObj->scRtOutFrmPrm.height = pObj->layoutParams.winInfo[winId].height;
        pWinObj->scRtOutFrmPrm.pitch[0] = pObj->outFrameFormat.pitch[0];
        pWinObj->scRtOutFrmPrm.pitch[1] = pObj->outFrameFormat.pitch[1];
        pWinObj->scRtOutFrmPrm.pitch[2] = pObj->outFrameFormat.pitch[2];
        pWinObj->scRtOutFrmPrm.memType =
            pObj->info.queInfo[0].chInfo[0].memType;

        pWinObj->scRtOutFrmPrm.dataFormat = pObj->outFrameFormat.dataFormat;

        chNum = pObj->layoutParams.winInfo[winId].channelNum;

        UTILS_assert((chNum == SYSTEM_SW_MS_INVALID_ID || chNum < pObj->inQueInfo.numCh));

        if (chNum == SYSTEM_SW_MS_INVALID_ID || chNum >= pObj->inQueInfo.numCh)
        {
            /* if channel not mapped to window, then set input width x
             * height == 320 x 240 and take properties from CH0 */

            pChInfo = &pObj->rtChannelInfo[0];

            pWinObj->scRtCropCfg.cropStartX = 0;
            pWinObj->scRtCropCfg.cropStartY = 0;
            pWinObj->scRtCropCfg.cropWidth = 320;
            pWinObj->scRtCropCfg.cropHeight = 240;

            pWinObj->scRtInFrmPrm.width =
                pWinObj->scRtCropCfg.cropStartX +
                pWinObj->scRtCropCfg.cropWidth;
            pWinObj->scRtInFrmPrm.height =
                pWinObj->scRtCropCfg.cropStartY +
                pWinObj->scRtCropCfg.cropHeight;

            pWinObj->scRtInFrmPrm.pitch[0] = pChInfo->pitch[0];
            pWinObj->scRtInFrmPrm.pitch[1] = pChInfo->pitch[1];
            pWinObj->scRtInFrmPrm.pitch[2] = pChInfo->pitch[2];
            pWinObj->scRtInFrmPrm.memType = pChInfo->memType;

            pWinObj->scRtInFrmPrm.dataFormat = pChInfo->dataFormat;

        }
        else
        {
            pChInfo = &pObj->rtChannelInfo[chNum];

            pWinObj->scRtCropCfg.cropStartX =
                VpsUtils_align(pChInfo->startX, 2);
            pWinObj->scRtCropCfg.cropStartY =
                VpsUtils_align(pChInfo->startY, 2);
            pWinObj->scRtCropCfg.cropWidth = VpsUtils_align(pChInfo->width, 2);
            pWinObj->scRtCropCfg.cropHeight =
                VpsUtils_align(pChInfo->height, 2);

            pWinObj->scRtInFrmPrm.width =
                pWinObj->scRtCropCfg.cropStartX +
                pWinObj->scRtCropCfg.cropWidth;
            pWinObj->scRtInFrmPrm.height =
                pWinObj->scRtCropCfg.cropStartY +
                pWinObj->scRtCropCfg.cropHeight;
            pWinObj->scRtInFrmPrm.pitch[0] = pChInfo->pitch[0];
            pWinObj->scRtInFrmPrm.pitch[1] = pChInfo->pitch[1];
            pWinObj->scRtInFrmPrm.pitch[2] = pChInfo->pitch[2];
            pWinObj->scRtInFrmPrm.memType = pChInfo->memType;

            pWinObj->scRtInFrmPrm.dataFormat = pChInfo->dataFormat;
        }

        if((pWinObj->scRtInFrmPrm.width + pWinObj->scRtCropCfg.cropStartX) >
                                          SW_MS_MAX_WIDTH_SUPPORTED)
        {
            pWinObj->scRtInFrmPrm.width = pWinObj->scRtCropCfg.cropWidth;
            pWinObj->scRtCropCfg.cropStartX = 0;
        }

        if (pObj->DrvObj[drvInst].isDeiDrv)
        {
            memset(&pWinObj->deiRtPrm, 0, sizeof(pWinObj->deiRtPrm));

            pWinObj->deiRtPrm.deiOutFrmPrms = &pWinObj->scRtOutFrmPrm;
            pWinObj->deiRtPrm.deiInFrmPrms = &pWinObj->scRtInFrmPrm;
            pWinObj->deiRtPrm.deiScCropCfg = &pWinObj->scRtCropCfg;
            pWinObj->deiRtPrm.deiRtCfg = &pWinObj->deiRtCfg;

            pWinObj->deiRtCfg.resetDei = FALSE;
            pWinObj->deiRtCfg.fldRepeat = FALSE;

            if(pObj->DrvObj[drvInst].forceBypassDei == TRUE)
            {
                /* In case captured data is progressive and window is bypass
                * (don't care quality so much), SC takes only even lines and
                * make S/W mosaic. This is sometimes needed due to SC
                * performance. */
                if ((FVID2_SF_PROGRESSIVE ==
                     pObj->inQueInfo.chInfo[chNum].scanFormat)
                            &&
                    pObj->layoutParams.winInfo[winId].bypass
                            &&
                    VPS_VPDMA_MT_NONTILEDMEM ==
                            pObj->inQueInfo.chInfo[chNum].memType
                    )
                {
                    pWinObj->scRtCropCfg.cropStartY /= 2;
                    pWinObj->scRtCropCfg.cropHeight /= 2;
                    pWinObj->scRtInFrmPrm.height /= 2;
                    pWinObj->scRtInFrmPrm.pitch[0] *= 2;
                    pWinObj->scRtInFrmPrm.pitch[1] *= 2;
                    pWinObj->scRtInFrmPrm.pitch[2] *= 2;
                }
            }
        }
        else
        {
            memset(&pWinObj->scRtPrm, 0, sizeof(pWinObj->scRtPrm));

            pWinObj->scRtPrm.outFrmPrms = &pWinObj->scRtOutFrmPrm;
            pWinObj->scRtPrm.inFrmPrms = &pWinObj->scRtInFrmPrm;
            pWinObj->scRtPrm.srcCropCfg = &pWinObj->scRtCropCfg;
            pWinObj->scRtPrm.scCfg = NULL;

            /* In case captured data is progressive and window is bypass
             * (don't care quality so much), SC takes only even lines and
             * make S/W mosaic. This is sometimes needed due to SC
             * performance. */
            if ((FVID2_SF_PROGRESSIVE ==
                 pObj->inQueInfo.chInfo[chNum].scanFormat) &&
                pObj->layoutParams.winInfo[winId].bypass   &&
                VPS_VPDMA_MT_NONTILEDMEM ==
                            pObj->inQueInfo.chInfo[chNum].memType)
            {
                pWinObj->scRtCropCfg.cropStartY /= 2;
                pWinObj->scRtCropCfg.cropHeight /= 2;
                pWinObj->scRtInFrmPrm.height /= 2;
                pWinObj->scRtInFrmPrm.pitch[0] *= 2;
                pWinObj->scRtInFrmPrm.pitch[1] *= 2;
                pWinObj->scRtInFrmPrm.pitch[2] *= 2;
            }
        }

        pWinObj->applyRtPrm = TRUE;

        pWinObj->blankFrame.channelNum = winId - pObj->DrvObj[drvInst].startWin;

        pWinObj->curOutFrame.channelNum = winId - pObj->DrvObj[drvInst].startWin;


    }

    SwMsLink_drvPrintLayoutParams(pObj);

    if (isLockAlredayTaken == FALSE)
    {
        SwMsLink_drvUnlock(pObj);
    }
    return FVID2_SOK;
}

Int32 SwMsLink_drvCreateOutInfo(SwMsLink_Obj * pObj, UInt32 outRes)
{
    Int32 status;
    System_LinkChInfo *pChInfo;
    UInt32 frameId, bufferPitch;
    UInt32 bufferWidth, bufferHeight;

    memset(&pObj->outFrameDrop, 0, sizeof(pObj->outFrameDrop));

    pObj->info.numQue = 1;
    pObj->info.queInfo[0].numCh = 1;

    pChInfo = &pObj->info.queInfo[0].chInfo[0];

    pChInfo->dataFormat = FVID2_DF_YUV422I_YUYV;
    pChInfo->memType = VPS_VPDMA_MT_NONTILEDMEM;

    System_getOutSize(pObj->createArgs.maxOutRes, &bufferWidth, &bufferHeight);

    bufferPitch = VpsUtils_align(bufferWidth, VPS_BUFFER_ALIGNMENT * 2) * 2;

    System_getOutSize(outRes, &pChInfo->width, &pChInfo->height);

    pChInfo->pitch[0] = bufferPitch;
    pChInfo->pitch[1] = pChInfo->pitch[2] = 0;

    pChInfo->scanFormat = FVID2_SF_PROGRESSIVE;

    pObj->bufferFrameFormat.channelNum = 0;
    pObj->bufferFrameFormat.width = bufferWidth;
    pObj->bufferFrameFormat.height = bufferHeight;
    pObj->bufferFrameFormat.pitch[0] = pChInfo->pitch[0];
    pObj->bufferFrameFormat.pitch[1] = pChInfo->pitch[1];
    pObj->bufferFrameFormat.pitch[2] = pChInfo->pitch[2];
    pObj->bufferFrameFormat.fieldMerged[0] = FALSE;
    pObj->bufferFrameFormat.fieldMerged[1] = FALSE;
    pObj->bufferFrameFormat.fieldMerged[2] = FALSE;
    pObj->bufferFrameFormat.dataFormat = pChInfo->dataFormat;
    pObj->bufferFrameFormat.scanFormat = pChInfo->scanFormat;
    pObj->bufferFrameFormat.bpp = FVID2_BPP_BITS16;
    pObj->bufferFrameFormat.reserved = NULL;

    status = Utils_bufCreate(&pObj->bufOutQue, TRUE, FALSE);
    UTILS_assert(status == FVID2_SOK);

    /* alloc buffer of max possible size but use only what is needed for a
     * given resolution */
    status = Utils_memFrameAlloc(&pObj->bufferFrameFormat,
                                 pObj->outFrames, pObj->createArgs.numOutBuf);
    UTILS_assert(status == FVID2_SOK);

    /* set actual required width x height */
    pObj->outFrameFormat = pObj->bufferFrameFormat;
    pObj->outFrameFormat.width = pChInfo->width;
    pObj->outFrameFormat.height = pChInfo->height;

    for (frameId = 0; frameId < pObj->createArgs.numOutBuf; frameId++)
    {
        status = Utils_bufPutEmptyFrame(&pObj->bufOutQue,
                                        &pObj->outFrames[frameId]);
        UTILS_assert(status == FVID2_SOK);
        pObj->outFrameInfo[frameId].swMsBlankOutBuf = SWMS_LINK_DO_OUTBUF_BLANKING;
        pObj->outFrames[frameId].appData = &pObj->outFrameInfo[frameId];
    }

    return FVID2_SOK;
}

Int32 SwMsLink_drvCreateWinObj(SwMsLink_Obj * pObj, UInt32 winId)
{
    SwMsLink_OutWinObj *pWinObj;
    Int32 status;
    System_MemoryType memType;

    pWinObj = &pObj->winObj[winId];

    /* assume all CHs are of same input size, format, pitch */
    pWinObj->scanFormat =
        (FVID2_ScanFormat) pObj->inQueInfo.chInfo[0].scanFormat;
    pWinObj->expectedFid = 0;
    pWinObj->applyRtPrm = FALSE;

    memType = (System_MemoryType)pObj->inQueInfo.chInfo[0].memType;
    pObj->blankBufferFormat.channelNum = 0;
    pObj->blankBufferFormat.width = pObj->inQueInfo.chInfo[0].width + pObj->inQueInfo.chInfo[0].startX;
    pObj->blankBufferFormat.height = pObj->inQueInfo.chInfo[0].height + pObj->inQueInfo.chInfo[0].startY;
    pObj->blankBufferFormat.fieldMerged[0] = FALSE;
    pObj->blankBufferFormat.fieldMerged[1] = FALSE;
    pObj->blankBufferFormat.fieldMerged[2] = FALSE;
    pObj->blankBufferFormat.pitch[0]  = pObj->inQueInfo.chInfo[0].pitch[0];
    pObj->blankBufferFormat.pitch[1]  = pObj->inQueInfo.chInfo[0].pitch[1];
    pObj->blankBufferFormat.pitch[2]  = pObj->inQueInfo.chInfo[0].pitch[2];
    pObj->blankBufferFormat.dataFormat= pObj->inQueInfo.chInfo[0].dataFormat;
    pObj->blankBufferFormat.scanFormat= pObj->inQueInfo.chInfo[0].scanFormat;
    pObj->blankBufferFormat.bpp = FVID2_BPP_BITS12;
    pObj->blankBufferFormat.reserved = NULL;

    if (winId == 0)
    {
         if(memType==SYSTEM_MT_NONTILEDMEM)
         {
             /* alloc buffer of max possible size as input blank buffer */
             status = System_getBlankFrame(&pWinObj->blankFrame);
             UTILS_assert(status == FVID2_SOK);
             pWinObj->blankFrame.addr[0][1] = pWinObj->blankFrame.addr[0][0];
             SwMsLink_fillDataPattern(&pObj->dmaObj,
                                      &pObj->blankBufferFormat,
                                      &pWinObj->blankFrame,
                                      1,
                                      FALSE);
         }
         else
         {
             status = Utils_tilerFrameAlloc(&pObj->blankBufferFormat,
                                            &pWinObj->blankFrame,
                                            1);

             UTILS_assert(status==FVID2_SOK);
             #ifndef TI816X_2G_DDR
             SwMsLink_fillDataPattern(&pObj->dmaObj,
                                      &pObj->blankBufferFormat,
                                      &pWinObj->blankFrame,
                                      1,
                                      TRUE);
             #endif
         }
    }
    else
    {
        pWinObj->blankFrame = pObj->winObj[0].blankFrame;
    }

    pWinObj->blankFrame.channelNum = winId;

    memset(&pWinObj->curOutFrame, 0, sizeof(pWinObj->curOutFrame));
    pWinObj->curOutFrame.addr[0][0] = NULL;
    pWinObj->curOutFrame.channelNum = winId;

    pWinObj->pCurInFrame = NULL;

    status = Utils_bufCreate(&pWinObj->bufInQue, FALSE, FALSE);
    UTILS_assert(status == FVID2_SOK);

    return FVID2_SOK;
}

Int32 SwMsLink_drvAllocCtxMem(SwMsLink_DrvObj * pObj)
{
    Int32 retVal = FVID2_SOK;
    Vps_DeiCtxInfo deiCtxInfo;
    Vps_DeiCtxBuf deiCtxBuf;
    UInt32 chCnt, bCnt;

    for (chCnt = 0u; chCnt < pObj->cfg.dei.deiCreateParams.numCh; chCnt++)
    {
        /* Get the number of buffers to allocate */
        deiCtxInfo.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_INFO, &deiCtxInfo, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Allocate the buffers as requested by the driver */
        for (bCnt = 0u; bCnt < deiCtxInfo.numFld; bCnt++)
        {
            deiCtxBuf.fldBuf[bCnt] = Utils_memAlloc(deiCtxInfo.fldBufSize,
                                                    VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.fldBuf[bCnt]);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMv; bCnt++)
        {
            deiCtxBuf.mvBuf[bCnt] = Utils_memAlloc(deiCtxInfo.mvBufSize,
                                                   VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.mvBuf[bCnt]);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMvstm; bCnt++)
        {
            deiCtxBuf.mvstmBuf[bCnt] = Utils_memAlloc(deiCtxInfo.mvstmBufSize,
                                                      VPS_BUFFER_ALIGNMENT);
            UTILS_assert(NULL != deiCtxBuf.mvstmBuf[bCnt]);
        }

        /* Provided the allocated buffer to driver */
        deiCtxBuf.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_SET_DEI_CTX_BUF, &deiCtxBuf, NULL);
        UTILS_assert(FVID2_SOK == retVal);
    }

    return (retVal);
}

Int32 SwMsLink_drvFreeCtxMem(SwMsLink_DrvObj * pObj)
{
    Int32 retVal = FVID2_SOK;
    Vps_DeiCtxInfo deiCtxInfo;
    Vps_DeiCtxBuf deiCtxBuf;
    UInt32 chCnt, bCnt;

    for (chCnt = 0u; chCnt < pObj->cfg.dei.deiCreateParams.numCh; chCnt++)
    {
        /* Get the number of buffers to allocate */
        deiCtxInfo.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_INFO, &deiCtxInfo, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Get the allocated buffer back from the driver */
        deiCtxBuf.channelNum = chCnt;
        retVal = FVID2_control(pObj->fvidHandle,
                               IOCTL_VPS_GET_DEI_CTX_BUF, &deiCtxBuf, NULL);
        UTILS_assert(FVID2_SOK == retVal);

        /* Free the buffers */
        for (bCnt = 0u; bCnt < deiCtxInfo.numFld; bCnt++)
        {
            Utils_memFree(deiCtxBuf.fldBuf[bCnt], deiCtxInfo.fldBufSize);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMv; bCnt++)
        {
            Utils_memFree(deiCtxBuf.mvBuf[bCnt], deiCtxInfo.mvBufSize);
        }
        for (bCnt = 0u; bCnt < deiCtxInfo.numMvstm; bCnt++)
        {
            Utils_memFree(deiCtxBuf.mvstmBuf[bCnt], deiCtxInfo.mvstmBufSize);
        }
    }

    return (retVal);
}

Int32 SwMsLink_drvSetScCoeffs
    (FVID2_Handle fvidHandle, Bool loadUpsampleCoeffs, Bool isDei) {
    Int32 retVal = FVID2_SOK;
    Vps_ScCoeffParams coeffPrms;

    if (loadUpsampleCoeffs)
    {
        Vps_rprintf(" %d: SWMS    : Loading Up-scaling Co-effs ... \n",
                    Utils_getCurTimeInMsec());

        coeffPrms.hScalingSet = VPS_SC_US_SET;
        coeffPrms.vScalingSet = VPS_SC_US_SET;
    }
    else
    {
        Vps_rprintf(" %d: SWMS    : Loading Down-scaling Co-effs ... \n",
                    Utils_getCurTimeInMsec());

        coeffPrms.hScalingSet = VPS_SC_DS_SET_0;
        coeffPrms.vScalingSet = VPS_SC_DS_SET_0;
    }
    coeffPrms.coeffPtr = NULL;
    coeffPrms.scalarId = isDei
        ? VPS_M2M_DEI_SCALAR_ID_DEI_SC : VPS_M2M_SC_SCALAR_ID_DEFAULT;

    /* Program DEI scalar coefficient - Always used */
    retVal = FVID2_control(fvidHandle, IOCTL_VPS_SET_COEFFS, &coeffPrms, NULL);
    UTILS_assert(FVID2_SOK == retVal);

    Vps_rprintf(" %d: SWMS    : Co-effs Loading ... DONE !!!\n",
                Utils_getCurTimeInMsec());

    return (retVal);
}

Int32 SwMsLink_drvCreateDeiDrv(SwMsLink_Obj * pObj, SwMsLink_DrvObj *pDrvObj)
{
    Semaphore_Params semParams;
    Vps_M2mDeiChParams *pDrvChPrm;
    UInt32 winId;
    System_LinkChInfo *pChInfo;
    FVID2_CbParams cbParams;

    Semaphore_Params_init(&semParams);

    semParams.mode = Semaphore_Mode_BINARY;

    pDrvObj->complete = Semaphore_create(0u, &semParams, NULL);
    UTILS_assert(pDrvObj->complete != NULL);

    pDrvObj->cfg.dei.deiCreateParams.mode = VPS_M2M_CONFIG_PER_CHANNEL;
    if (pDrvObj->bypassDei)
    {
        pDrvObj->cfg.dei.deiCreateParams.numCh = pDrvObj->endWin - pDrvObj->startWin + 1;
    }
    else
    {
        pDrvObj->cfg.dei.deiCreateParams.numCh = SW_MS_MAX_DEI_CH;
    }
    pDrvObj->cfg.dei.deiCreateParams.deiHqCtxMode = VPS_DEIHQ_CTXMODE_DRIVER_ALL;
    pDrvObj->cfg.dei.deiCreateParams.chParams =
        (const Vps_M2mDeiChParams *) pDrvObj->cfg.dei.deiChParams;

    Vps_rprintf(" %d: SWMS    : VipScReq is %s!!!\n",
                Utils_getCurTimeInMsec(), pDrvObj->cfg.dei.deiCreateParams.isVipScReq == TRUE ? "TRUE" : "FALSE" );

    for (winId = 0; winId < pDrvObj->cfg.dei.deiCreateParams.numCh; winId++)
    {
        pDrvChPrm = &pDrvObj->cfg.dei.deiChParams[winId];

        /* assume all CHs are of same input size, format, pitch */
        pChInfo = &pObj->inQueInfo.chInfo[0];

        pDrvChPrm->inFmt.channelNum = winId;
        pDrvChPrm->inFmt.width = pChInfo->width;
        pDrvChPrm->inFmt.height = pChInfo->height;
        pDrvChPrm->inFmt.pitch[0] = pChInfo->pitch[0];
        pDrvChPrm->inFmt.pitch[1] = pChInfo->pitch[1];
        pDrvChPrm->inFmt.pitch[2] = pChInfo->pitch[2];
        pDrvChPrm->inFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->inFmt.fieldMerged[1] = FALSE;
        pDrvChPrm->inFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->inFmt.dataFormat = pChInfo->dataFormat;
        if (pDrvObj->bypassDei)
        {
            pDrvChPrm->inFmt.scanFormat = FVID2_SF_PROGRESSIVE;
        }
        else
        {
            pDrvChPrm->inFmt.scanFormat = FVID2_SF_INTERLACED;
        }
        pDrvChPrm->inFmt.bpp = FVID2_BPP_BITS16;

        pDrvChPrm->outFmtDei = &pDrvObj->drvOutFormat[winId];
        pDrvChPrm->outFmtDei->channelNum = winId;
        pDrvChPrm->outFmtDei->width = pChInfo->width;
        pDrvChPrm->outFmtDei->height = pChInfo->height;
        pDrvChPrm->outFmtDei->pitch[0] = pObj->outFrameFormat.pitch[0];
        pDrvChPrm->outFmtDei->pitch[1] = pObj->outFrameFormat.pitch[1];
        pDrvChPrm->outFmtDei->pitch[2] = pObj->outFrameFormat.pitch[2];
        pDrvChPrm->outFmtDei->fieldMerged[0] = FALSE;
        pDrvChPrm->outFmtDei->fieldMerged[1] = FALSE;
        pDrvChPrm->outFmtDei->fieldMerged[0] = FALSE;
        pDrvChPrm->outFmtDei->dataFormat = pObj->outFrameFormat.dataFormat;
        pDrvChPrm->outFmtDei->scanFormat = FVID2_SF_PROGRESSIVE;
        pDrvChPrm->outFmtDei->bpp = pObj->outFrameFormat.bpp;

        pDrvChPrm->inMemType = pChInfo->memType;
        pDrvChPrm->outMemTypeDei = VPS_VPDMA_MT_NONTILEDMEM;
        pDrvChPrm->outMemTypeVip = VPS_VPDMA_MT_NONTILEDMEM;
        pDrvChPrm->drnEnable = FALSE;
        pDrvChPrm->comprEnable = FALSE;

        pDrvChPrm->deiHqCfg = &pDrvObj->cfg.dei.deiHqCfg;
        pDrvChPrm->deiCfg = &pDrvObj->cfg.dei.deiCfg;

        pDrvChPrm->scCfg = &pDrvObj->scCfg;
        pDrvChPrm->deiCropCfg = &pDrvObj->scCropCfg[winId];

        pDrvChPrm->deiHqCfg->bypass = pDrvObj->bypassDei;
        pDrvChPrm->deiHqCfg->inpMode = VPS_DEIHQ_EDIMODE_EDI_LARGE_WINDOW;
        pDrvChPrm->deiHqCfg->tempInpEnable = TRUE;
        pDrvChPrm->deiHqCfg->tempInpChromaEnable = TRUE;
        pDrvChPrm->deiHqCfg->spatMaxBypass = FALSE;
        pDrvChPrm->deiHqCfg->tempMaxBypass = FALSE;
        pDrvChPrm->deiHqCfg->fldMode = VPS_DEIHQ_FLDMODE_5FLD;
        pDrvChPrm->deiHqCfg->lcModeEnable = TRUE;
        pDrvChPrm->deiHqCfg->mvstmEnable = TRUE;
        if (pDrvObj->bypassDei)
        {
            pDrvChPrm->deiHqCfg->tnrEnable = FALSE;
        }
        else
        {
            pDrvChPrm->deiHqCfg->tnrEnable = TRUE;
        }
        pDrvChPrm->deiHqCfg->snrEnable = TRUE;
        pDrvChPrm->deiHqCfg->sktEnable = FALSE;
        pDrvChPrm->deiHqCfg->chromaEdiEnable = TRUE;

        pDrvChPrm->deiCfg->bypass = pDrvObj->bypassDei;
        pDrvChPrm->deiCfg->inpMode = VPS_DEIHQ_EDIMODE_EDI_LARGE_WINDOW;
        pDrvChPrm->deiCfg->tempInpEnable = TRUE;
        pDrvChPrm->deiCfg->tempInpChromaEnable = TRUE;
        pDrvChPrm->deiCfg->spatMaxBypass = FALSE;
        pDrvChPrm->deiCfg->tempMaxBypass = FALSE;

        pDrvChPrm->scCfg->bypass = FALSE;
        pDrvChPrm->scCfg->nonLinear = FALSE;
        pDrvChPrm->scCfg->stripSize = 0;
        pDrvChPrm->scCfg->vsType = VPS_SC_VST_POLYPHASE;

        pDrvChPrm->deiCropCfg->cropStartX = 0;
        pDrvChPrm->deiCropCfg->cropStartY = 0;
        pDrvChPrm->deiCropCfg->cropWidth = pDrvChPrm->inFmt.width;
        if (pDrvObj->bypassDei)
        {
            pDrvChPrm->deiCropCfg->cropHeight = pDrvChPrm->inFmt.height;
        }
        else
        {
            pDrvChPrm->deiCropCfg->cropHeight = pDrvChPrm->inFmt.height * 2;
        }

        /* VIP-SC settings */
        if (pDrvObj->cfg.dei.deiCreateParams.isVipScReq == TRUE)
        {
            pDrvChPrm->outFmtVip = &pDrvObj->drvVipOutFormat[winId];
            pDrvChPrm->outFmtVip->channelNum = winId;
            pDrvChPrm->outFmtVip->width = 352;
            pDrvChPrm->outFmtVip->height = 288;
            pDrvChPrm->outFmtVip->pitch[0] = pDrvChPrm->outFmtVip->width*2;
            pDrvChPrm->outFmtVip->pitch[1] = pDrvChPrm->outFmtVip->pitch[0];
            pDrvChPrm->outFmtVip->pitch[2] = 0;
            pDrvChPrm->outFmtVip->fieldMerged[0] = FALSE;
            pDrvChPrm->outFmtVip->fieldMerged[1] = FALSE;
            pDrvChPrm->outFmtVip->fieldMerged[0] = FALSE;
            pDrvChPrm->outFmtVip->dataFormat = pObj->outFrameFormat.dataFormat;
            pDrvChPrm->outFmtVip->scanFormat = FVID2_SF_PROGRESSIVE;
            pDrvChPrm->outFmtVip->bpp = pObj->outFrameFormat.bpp;

            pDrvChPrm->vipScCfg = &pDrvObj->vipScCfg;
            pDrvChPrm->vipCropCfg = &pDrvObj->vipScCropCfg[winId];

            pDrvChPrm->vipScCfg->bypass = TRUE;
            pDrvChPrm->vipScCfg->nonLinear = FALSE;
            pDrvChPrm->vipScCfg->stripSize = 0;
            pDrvChPrm->vipScCfg->vsType = VPS_SC_VST_POLYPHASE;

            pDrvChPrm->vipCropCfg->cropStartX = 0;
            pDrvChPrm->vipCropCfg->cropStartY = 0;
            pDrvChPrm->vipCropCfg->cropWidth = pDrvChPrm->inFmt.width;
            if (pDrvObj->bypassDei)
            {
                pDrvChPrm->vipCropCfg->cropHeight = pDrvChPrm->inFmt.height;
            }
            else
            {
                pDrvChPrm->vipCropCfg->cropHeight = pDrvChPrm->inFmt.height * 2;
            }
        }

        #ifdef TI_814X_BUILD

        pDrvChPrm->deiHqCfg = NULL;

        /* for SC2 driver deiHqCfg and deiCfg MUST be NULL */
        if ((pDrvObj->drvInstId == VPS_M2M_INST_AUX_SC2_WB1) ||
            (pDrvObj->drvInstId == VPS_M2M_INST_AUX_SC4_VIP1) ||
            (pDrvObj->drvInstId == VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1))
        {
            pDrvChPrm->deiCfg = NULL;
        }
        #endif

        pDrvChPrm->subFrameParams = NULL;
    }

    memset(&cbParams, 0, sizeof(cbParams));

    cbParams.cbFxn = SwMsLink_drvFvidCb;
    cbParams.errCbFxn = SwMsLink_drvFvidErrCb;
    cbParams.errList = &pDrvObj->errCbProcessList;
    cbParams.appData = pDrvObj;

    pDrvObj->fvidHandle = FVID2_create(FVID2_VPS_M2M_DEI_DRV,
                                       pDrvObj->drvInstId,
                                       &pDrvObj->cfg.dei.deiCreateParams,
                                       &pDrvObj->cfg.dei.deiCreateStatus, &cbParams);
    UTILS_assert(pDrvObj->fvidHandle != NULL);

    if(pDrvObj->bypassDei == FALSE)
        SwMsLink_drvAllocCtxMem(pDrvObj);

    /* load co-effs only once */
    if (
        pDrvObj->bypassDei == FALSE
        ||
        ( /* if DEI is in force bypass mode,
            then DEI in DEI mode driver is not created hence need load co-effs here
            */
            pDrvObj->bypassDei == TRUE
            &&
            pDrvObj->forceBypassDei == TRUE
        )
        )
    {
        SwMsLink_drvSetScCoeffs(pDrvObj->fvidHandle, TRUE, TRUE);
    }

    pDrvObj->processList.numInLists = 1;
    pDrvObj->processList.numOutLists = 1;
    pDrvObj->processList.inFrameList[0] = &pDrvObj->inFrameList;
    pDrvObj->processList.outFrameList[0] = &pDrvObj->outFrameList;

    /* If in dual out mode, initialize VIP-SC outs to dummy frames */
    if (pDrvObj->cfg.dei.deiCreateParams.isVipScReq == TRUE)
    {
        Int32 i;

        pDrvObj->processList.numOutLists = 2;
        pDrvObj->processList.outFrameList[1] = &pObj->outFrameDropList;

        for (i=0; i<FVID2_MAX_FVID_FRAME_PTR; i++)
            pObj->outFrameDropList.frames[i] = &pObj->outFrameDrop;
        Vps_rprintf(" %d: SWMS    : OutFrames List -> %d !!!!!!!\n", Utils_getCurTimeInMsec(), pDrvObj->processList.numOutLists);
    }
    return FVID2_SOK;
}

Int32 SwMsLink_drvCreateScDrv(SwMsLink_Obj * pObj, SwMsLink_DrvObj *pDrvObj)
{
    Semaphore_Params semParams;
    Vps_M2mScChParams *pDrvChPrm;
    UInt32 winId;
    System_LinkChInfo *pChInfo;
    FVID2_CbParams cbParams;

    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    pDrvObj->complete = Semaphore_create(0u, &semParams, NULL);
    UTILS_assert(pDrvObj->complete != NULL);

    pDrvObj->cfg.sc.scCreateParams.mode = VPS_M2M_CONFIG_PER_CHANNEL;
    pDrvObj->cfg.sc.scCreateParams.numChannels = SYSTEM_SW_MS_MAX_WIN_PER_SC;
    pDrvObj->cfg.sc.scCreateParams.chParams
        = (Vps_M2mScChParams *) pDrvObj->cfg.sc.scChParams;

    for (winId = 0; winId < pDrvObj->cfg.sc.scCreateParams.numChannels; winId++)
    {
        pDrvChPrm = &pDrvObj->cfg.sc.scChParams[winId];

        /* assume all CHs are of same input size, format, pitch */
        pChInfo = &pObj->inQueInfo.chInfo[0];

        pDrvChPrm->inFmt.channelNum = winId;
        pDrvChPrm->inFmt.width = pChInfo->width;
        pDrvChPrm->inFmt.height = pChInfo->height;
        pDrvChPrm->inFmt.pitch[0] = pChInfo->pitch[0];
        pDrvChPrm->inFmt.pitch[1] = pChInfo->pitch[1];
        pDrvChPrm->inFmt.pitch[2] = pChInfo->pitch[2];
        pDrvChPrm->inFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->inFmt.fieldMerged[1] = FALSE;
        pDrvChPrm->inFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->inFmt.dataFormat = pChInfo->dataFormat;
        pDrvChPrm->inFmt.scanFormat = FVID2_SF_PROGRESSIVE;
        pDrvChPrm->inFmt.bpp = FVID2_BPP_BITS16;

        pDrvChPrm->outFmt.channelNum = winId;
        pDrvChPrm->outFmt.width = pChInfo->width;
        pDrvChPrm->outFmt.height = pChInfo->height;
        pDrvChPrm->outFmt.pitch[0] = pObj->outFrameFormat.pitch[0];
        pDrvChPrm->outFmt.pitch[1] = pObj->outFrameFormat.pitch[1];
        pDrvChPrm->outFmt.pitch[2] = pObj->outFrameFormat.pitch[2];
        pDrvChPrm->outFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->outFmt.fieldMerged[1] = FALSE;
        pDrvChPrm->outFmt.fieldMerged[0] = FALSE;
        pDrvChPrm->outFmt.dataFormat = pObj->outFrameFormat.dataFormat;
        pDrvChPrm->outFmt.scanFormat = FVID2_SF_PROGRESSIVE;
        pDrvChPrm->outFmt.bpp = pObj->outFrameFormat.bpp;

        pDrvChPrm->inMemType = pChInfo->memType;
        pDrvChPrm->outMemType = VPS_VPDMA_MT_NONTILEDMEM;

        pDrvChPrm->scCfg = &pDrvObj->scCfg;
        pDrvChPrm->srcCropCfg = &pDrvObj->scCropCfg[winId];

        pDrvChPrm->scCfg->bypass = FALSE;
        pDrvChPrm->scCfg->nonLinear = FALSE;
        pDrvChPrm->scCfg->stripSize = 0;
        pDrvChPrm->scCfg->vsType = VPS_SC_VST_POLYPHASE;

        pDrvChPrm->srcCropCfg->cropStartX = 0;
        pDrvChPrm->srcCropCfg->cropStartY = 0;
        pDrvChPrm->srcCropCfg->cropWidth = pDrvChPrm->inFmt.width;
        pDrvChPrm->srcCropCfg->cropHeight = pDrvChPrm->inFmt.height;
    }

    memset(&cbParams, 0, sizeof(cbParams));
    cbParams.cbFxn = SwMsLink_drvFvidCb;
    cbParams.errCbFxn = SwMsLink_drvFvidErrCb;
    cbParams.errList = &pDrvObj->errCbProcessList;
    cbParams.appData = pDrvObj;

    pDrvObj->fvidHandle = FVID2_create(FVID2_VPS_M2M_SC_DRV,
                                       pDrvObj->drvInstId,
                                       &pDrvObj->cfg.sc.scCreateParams,
                                       &pDrvObj->cfg.sc.scCreateStatus, &cbParams);
    UTILS_assert(pDrvObj->fvidHandle != NULL);

    SwMsLink_drvSetScCoeffs(pDrvObj->fvidHandle, TRUE, TRUE);   /* TODO:
                                                                 * change
                                                                 * based on
                                                                 * scaling
                                                                 * ratio */

    pDrvObj->processList.numInLists = 1;
    pDrvObj->processList.numOutLists = 1;
    pDrvObj->processList.inFrameList[0] = &pDrvObj->inFrameList;
    pDrvObj->processList.outFrameList[0] = &pDrvObj->outFrameList;

    return FVID2_SOK;
}

Int32 SwMsLink_drvCreate(SwMsLink_Obj * pObj, SwMsLink_CreateParams * pPrm)
{
    Semaphore_Params semParams;
    Clock_Params clockParams;
    UInt32 winId;
    Int32 status;
    UInt32 i;
    Bool vip0Exist = FALSE;
    Bool sc5Exist  = FALSE;

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Create in progress !!!\n", Utils_getCurTimeInMsec());
#endif

    pObj->inFramePutCount = 0;
    pObj->inFrameGetCount = 0;

    pObj->frameCount = 0;
    pObj->totalTime = 0;

    pObj->skipProcessing = 0;
    pObj->switchLayout = FALSE;
    pObj->rtParamUpdate = FALSE;
    pObj->vipLockRequired = FALSE;

    memcpy(&pObj->createArgs, pPrm, sizeof(*pPrm));

    if(pObj->createArgs.numOutBuf==0)
        pObj->createArgs.numOutBuf = SW_MS_LINK_MAX_OUT_FRAMES_DEFAULT;

    if(pObj->createArgs.numOutBuf>SW_MS_LINK_MAX_OUT_FRAMES)
        pObj->createArgs.numOutBuf = SW_MS_LINK_MAX_OUT_FRAMES;

    memset(pObj->winObj, 0, sizeof(pObj->winObj));

    for(i = 0; i < (2 * SYSTEM_SW_MS_MAX_INST); i++)
    {
        memset(&pObj->DrvObj[i], 0, sizeof(pObj->DrvObj[i]));
    }

    status = System_linkGetInfo(pPrm->inQueParams.prevLinkId, &pObj->inTskInfo);
    UTILS_assert(status == FVID2_SOK);
    UTILS_assert(pPrm->inQueParams.prevLinkQueId < pObj->inTskInfo.numQue);

    memcpy(&pObj->inQueInfo,
           &pObj->inTskInfo.queInfo[pPrm->inQueParams.prevLinkQueId],
           sizeof(pObj->inQueInfo));

    SwMsLink_drvGetTimerPeriod(pObj, &pPrm->layoutPrm);

    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    pObj->lock = Semaphore_create(1u, &semParams, NULL);
    UTILS_assert(pObj->lock != NULL);

    Clock_Params_init(&clockParams);
    clockParams.period = pObj->timerPeriod;
    clockParams.arg = (UArg) pObj;

    pObj->timer = Clock_create(SwMsLink_drvTimerCb,
                               pObj->timerPeriod, &clockParams, NULL);
    UTILS_assert(pObj->timer != NULL);

    SwMsLink_drvCreateOutInfo(pObj, pPrm->maxOutRes);

    status = SwMsLink_drvDmaCreate(pObj);
    UTILS_assert(0 == status);

    for (winId = 0; winId < SYSTEM_SW_MS_MAX_WIN; winId++)
    {
        SwMsLink_drvCreateWinObj(pObj, winId);
    }

    for (i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        if (pObj->createArgs.swMsInstId[i] == SYSTEM_SW_MS_SC_INST_VIP0_SC)
            vip0Exist = TRUE;
        if (pObj->createArgs.swMsInstId[i] == SYSTEM_SW_MS_SC_INST_SC5)
            sc5Exist = TRUE;
        if(vip0Exist && sc5Exist)
        {
            Vps_printf("Didn't support SC5 and VIP0 both exist now !!!\n");
            UTILS_assert(0);
        }
    }

    pObj->maxWinId = 0;
    for (i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        pObj->DrvObj[i].forceBypassDei = FALSE;

        if (pObj->createArgs.numSwMsInst != 1)
            pObj->DrvObj[i].startWin = pObj->createArgs.swMsInstStartWin[i];
        else
            pObj->DrvObj[i].startWin = 0;

        if( i != (pObj->createArgs.numSwMsInst - 1))
            pObj->DrvObj[i].endWin = pObj->createArgs.swMsInstStartWin[i+1] - 1;
        else
        {
            if (SYSTEM_SW_MS_MAX_WIN > pObj->DrvObj[i].startWin)
                pObj->DrvObj[i].endWin = SYSTEM_SW_MS_MAX_WIN - 1;
            else
                pObj->DrvObj[i].endWin = pObj->DrvObj[i].startWin + 1;
        }

        if ((pObj->DrvObj[i].startWin + SYSTEM_SW_MS_MAX_WIN_PER_SC - 1) < pObj->DrvObj[i].endWin)
        {
            pObj->DrvObj[i].endWin = pObj->DrvObj[i].startWin + SYSTEM_SW_MS_MAX_WIN_PER_SC - 1;
        }

        pObj->maxWinId = pObj->DrvObj[i].endWin;

        Vps_printf("SWMS: instance %d, sc id %d, start win %d end win %d\n", i,
            pObj->createArgs.swMsInstId[i], pObj->DrvObj[i].startWin, pObj->DrvObj[i].endWin);
        switch (pObj->createArgs.swMsInstId[i])
        {
            case SYSTEM_SW_MS_SC_INST_DEIHQ_SC_NO_DEI:
                pObj->DrvObj[i].forceBypassDei = TRUE;
                /* rest of DrvObj setup would be same as DEI SW MS setup, hence no 'break' */

            case SYSTEM_SW_MS_SC_INST_DEIHQ_SC:
                pObj->DrvObj[i].isDeiDrv = TRUE;
                pObj->DrvObj[i].bypassDei = FALSE;
#ifdef TI_814X_BUILD
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_MAIN_DEI_SC1_WB0;
#else
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_MAIN_DEIH_SC1_WB0;
#endif
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].cfg.dei.deiCreateParams.isVipScReq = FALSE;
                if(pObj->DrvObj[i].forceBypassDei==FALSE)
                {
                    /* create DEI driver in DEI mode only if force DEI bypass is FALSE */
                    SwMsLink_drvCreateDeiDrv(pObj, &pObj->DrvObj[i]);
                }
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].startWin = pObj->DrvObj[i].startWin;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].endWin = pObj->DrvObj[i].endWin;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].isDeiDrv = TRUE;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].bypassDei = TRUE;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].forceBypassDei = pObj->DrvObj[i].forceBypassDei;
#ifdef TI_814X_BUILD
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].drvInstId = VPS_M2M_INST_MAIN_DEI_SC1_WB0;
#else
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].drvInstId = VPS_M2M_INST_MAIN_DEIH_SC1_WB0;
#endif
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].cfg.dei.deiCreateParams.isVipScReq = FALSE;
                SwMsLink_drvCreateDeiDrv(pObj, &pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST]);
                break;

            case SYSTEM_SW_MS_SC_INST_DEI_SC_NO_DEI:
                pObj->DrvObj[i].forceBypassDei = TRUE;
                /* rest of DrvObj setup would be same as DEI SW MS setup, hence no 'break' */

            case SYSTEM_SW_MS_SC_INST_DEI_SC:
                pObj->DrvObj[i].isDeiDrv = TRUE;
                pObj->DrvObj[i].bypassDei = FALSE;
#ifdef TI_814X_BUILD
                /* Use DEI-SC + VIP0-SC dual out mode only for 814x
                    * Mainly used in some Ce usecases where DEI link as well swMSLink opens same
                    * DrvInstances. 2 different driver instances - 1 sing both single out mode & other in dual mode>
                    * sharing same SC intance is not supported, we open in dual out mode in swMS as well.
                    * We use VIP outframes as null frames <memset to 0> in order to not generate any output for VIP-SC
                    * So, effectively we get single output
                    */
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1;
                pObj->DrvObj[i].cfg.dei.deiCreateParams.isVipScReq = TRUE;
                pObj->vipLockRequired = TRUE;
                pObj->vipInstId = SYSTEM_VIP_1;
#else
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_AUX_DEI_SC2_WB1;
                pObj->DrvObj[i].cfg.dei.deiCreateParams.isVipScReq = FALSE;
#endif
                if(pObj->DrvObj[i].forceBypassDei==FALSE)
                {
                    SwMsLink_drvCreateDeiDrv(pObj, &pObj->DrvObj[i]);
                }
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].startWin = pObj->DrvObj[i].startWin;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].endWin = pObj->DrvObj[i].endWin;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].isDeiDrv = TRUE;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].bypassDei = TRUE;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].forceBypassDei = pObj->DrvObj[i].forceBypassDei;
#ifdef TI_814X_BUILD
                /* Use DEI-SC + VIP0-SC dual out mode only for 814x */
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].drvInstId = VPS_M2M_INST_AUX_SC2_SC4_WB1_VIP1;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].cfg.dei.deiCreateParams.isVipScReq = TRUE;
                pObj->vipLockRequired = TRUE;
                pObj->vipInstId = SYSTEM_VIP_1;
#else
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].drvInstId = VPS_M2M_INST_AUX_DEI_SC2_WB1;
                pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].cfg.dei.deiCreateParams.isVipScReq = FALSE;
#endif
                SwMsLink_drvCreateDeiDrv(pObj, &pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST]);
                break;

            case SYSTEM_SW_MS_SC_INST_VIP0_SC:
                pObj->DrvObj[i].isDeiDrv = FALSE;
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_SEC0_SC3_VIP0;
                SwMsLink_drvCreateScDrv(pObj,&pObj->DrvObj[i]);
                break;
            case SYSTEM_SW_MS_SC_INST_VIP1_SC:
                pObj->DrvObj[i].isDeiDrv = FALSE;
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_SEC1_SC4_VIP1;
                SwMsLink_drvCreateScDrv(pObj,&pObj->DrvObj[i]);
                break;
            case SYSTEM_SW_MS_SC_INST_SC5:
                pObj->DrvObj[i].isDeiDrv = FALSE;
                pObj->DrvObj[i].drvInstId = VPS_M2M_INST_SEC0_SC5_WB2;
                SwMsLink_drvCreateScDrv(pObj,&pObj->DrvObj[i]);
                break;
            default:
                UTILS_assert(0);
                break;
        }
    }
    SwMsLink_drvUpdateRtChannelInfo(pObj);
    SwMsLink_drvSwitchLayout(pObj, &pObj->createArgs.layoutPrm, FALSE);

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Create Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 SwMsLink_freeFrame(System_LinkInQueParams * pInQueParams,
                         FVID2_FrameList * frameList, FVID2_Frame * pFrame)
{
    frameList->frames[frameList->numFrames] = pFrame;
    frameList->numFrames++;

    UTILS_assert (frameList->numFrames < FVID2_MAX_FVID_FRAME_PTR);
    if (frameList->numFrames >= (FVID2_MAX_FVID_FRAME_PTR/4))
    {
        System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                                   pInQueParams->prevLinkQueId, frameList);

        frameList->numFrames = 0;
    }

    return FVID2_SOK;
}

Int32 SwMsLink_drvProcessData(SwMsLink_Obj * pObj)
{
    UInt32 frameId, winId, chId,drvInst;
    FVID2_Frame *pFrame;
    System_LinkInQueParams *pInQueParams;
    FVID2_FrameList frameList;
    SwMsLink_LayoutWinInfo *pWinInfo;
    SwMsLink_OutWinObj *pWinObj;
    Int32 status;

    pInQueParams = &pObj->createArgs.inQueParams;

    System_getLinksFullFrames(pInQueParams->prevLinkId,
                              pInQueParams->prevLinkQueId, &frameList);

    pObj->freeFrameList.numFrames = 0;

    if (frameList.numFrames)
    {
        pObj->inFrameGetCount += frameList.numFrames;

        SwMsLink_drvLock(pObj);

        for (frameId = 0; frameId < frameList.numFrames; frameId++)
        {
            pFrame = frameList.frames[frameId];

            if (pFrame == NULL)
            {
#ifdef SWMS_DEBUG_FRAME_REJECT
                Vps_printf(" SWMS: Frame Reject: NULL frame\n");
#endif

                continue;
            }

            /* valid frame */
            chId = pFrame->channelNum;
            if (chId >= SYSTEM_SW_MS_MAX_CH_ID)
            {
#ifdef SWMS_DEBUG_FRAME_REJECT
                Vps_printf(" SWMS: Frame Reject: Invalid CH ID (%d)\n", chId);
#endif
                /* invalid ch ID */
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pFrame);
                continue;
            }

            /* valid chId */
            winId = pObj->layoutParams.ch2WinMap[chId];
            if (winId == SYSTEM_SW_MS_INVALID_ID)
            {
#ifdef SWMS_DEBUG_FRAME_REJECT
                Vps_printf(" SWMS: Frame Reject: Invalid Win ID (%d)\n", winId);
#endif

                /* ch not mapped to any window */
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pFrame);
                continue;
            }

            /* ch mapped to window */
            pWinObj  = &pObj->winObj[winId];
            pWinInfo = &pObj->layoutParams.winInfo[winId];

            pWinObj->framesRecvCount++;

            if (pWinInfo->channelNum != chId)
            {
#ifdef SWMS_DEBUG_FRAME_REJECT
                Vps_printf
                    (" SWMS: Frame Reject: Win not mapped to this channel (%d)\n",
                     chId);
#endif

                pWinObj->framesInvalidChCount++;

                /* win is not assigned to this ch, normally this condition
                 * wont happen */
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pFrame);
                continue;
            }

            drvInst = SwMsLink_getDrvInstFromWinId(pObj,winId);

            /* valid win and ch */
            /* [TODO] For the moment, SC drv takes only even fields and make
             * S/W moasic. It throw away odd fields. Later, it can take both
             * fields and make S/W mosaic in merged frame format. */
            if (pWinInfo->bypass || !pObj->DrvObj[drvInst].isDeiDrv || pObj->DrvObj[drvInst].forceBypassDei)
            {
                /* window shows channel in bypass mode, then drop odd fields,
                 * i.e always expect even fields */
                pWinObj->expectedFid = 0;
            }

            if (pFrame->fid != pWinObj->expectedFid)
            {
                pWinObj->framesFidInvalidCount++;

                /* incoming frame fid does not match required fid */
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pFrame);
                continue;
            }

            /* queue the frame */
            status = Utils_bufPutFullFrame(&pWinObj->bufInQue, pFrame);
            if (status != FVID2_SOK)
            {
                pWinObj->framesQueRejectCount++;

                /* Q full, release frame */
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pFrame);
                continue;
            }

            pWinObj->framesQueuedCount++;

            /* toggle next expected fid */
            if (pObj->DrvObj[drvInst].isDeiDrv && pObj->DrvObj[drvInst].forceBypassDei == FALSE)
            {
                pWinObj->expectedFid ^= 1;
            }
        }

        SwMsLink_drvUnlock(pObj);

        if (pObj->freeFrameList.numFrames)
        {
            pObj->inFramePutCount += pObj->freeFrameList.numFrames;
            System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                                       pInQueParams->prevLinkQueId,
                                       &pObj->freeFrameList);
        }
    }

    return FVID2_SOK;
}

Int32 SwMsLink_drvMakeFrameLists(SwMsLink_Obj * pObj, FVID2_Frame * pOutFrame)
{
    UInt32 winId;
    SwMsLink_LayoutWinInfo *pWinInfo;
    SwMsLink_OutWinObj *pWinObj;
    SwMsLink_DrvObj *pDrvObj;
    FVID2_Frame *pInFrame, *pFrame;
    Int32 status;
    Bool repeatFld;
    UInt32 nextFid;
    System_LinkInQueParams *pInQueParams;
    UInt32                  queCnt,i, drvInst;
    UInt32                  skipInputFrames;
    Bool rtParamUpdatePerFrame;
    System_FrameInfo *pFrameInfo;
    System_LinkChInfo *rtChannelInfo;
    UInt32  maxChnl;

#if AVSYNC_COMP_ENABLE
    AVSYNC_FRAME_RENDER_FLAG rendr_flag;
#endif

    pInQueParams = &pObj->createArgs.inQueParams;

    for (i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        pObj->DrvObj[i].inFrameList.numFrames = 0;
        pObj->DrvObj[i].outFrameList.numFrames = 0;

        if (pObj->DrvObj[i].isDeiDrv)
        {
            pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].inFrameList.numFrames = 0;
            pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].outFrameList.numFrames = 0;
        }
    }

    pObj->rtParamUpdate = FALSE;
    pObj->freeFrameList.numFrames = 0;

    SwMsLink_drvCheckBlankOutputBuffer(pObj,pOutFrame);
#if 0//AVSYNC_COMP_ENABLE

    if (( Utils_getCurTimeInMsec() - framecounter ) >= 3000) {
        framecounter = Utils_getCurTimeInMsec();
        int i;
            Vps_printf (" \n\n**************************\n");
        for (i = 0; i<32;i++) {

            Vps_printf (" CH [%d] ===> Dq [%6d] Ren [%6d] Rep [%6d], Skip [%6d], Inv [%6d], LstPTS %10d\n",i,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].nVidFramesDequeued,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].nVidFramesRendered,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].nVidFramesRepeated,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].nVidFramesSkipped,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].nVidFramesInvalid,
                                    pAvSyncInfo_obj->StreamStats.StreamChStats[i].lastVidPTS
                                    );
        }
    }
#endif

    maxChnl = pObj->maxWinId + 1;

    if (maxChnl > SYSTEM_SW_MS_MAX_WIN)
        maxChnl = SYSTEM_SW_MS_MAX_WIN;

    for (winId = 0; winId < maxChnl; winId++)
    {
        pWinObj = &pObj->winObj[winId];
        pWinInfo = &pObj->layoutParams.winInfo[winId];
        drvInst = SwMsLink_getDrvInstFromWinId(pObj, winId);

        UTILS_assert(drvInst < pObj->createArgs.numSwMsInst );

        repeatFld = FALSE;

#if AVSYNC_COMP_ENABLE
        if (pAvSyncInfo_obj->avSyncCompEnable == TRUE){ pInFrame =  NULL; } // ***
#endif

        if (winId >= pObj->layoutParams.numWin)
        {
            /* empty input queue */
            status = Utils_bufGetFullFrame(&pWinObj->bufInQue,
                                           &pInFrame, BIOS_NO_WAIT);
                //pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesDequeued++;
            if (status == FVID2_SOK)
            {
                pWinObj->framesRejectCount++ ;

                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                   pInFrame);
            }

            /* free any frame which is being held */
            if (pWinObj->pCurInFrame != NULL)
            {
                pWinObj->framesRejectCount++ ;

                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                   pWinObj->pCurInFrame);
                pWinObj->pCurInFrame = NULL;
            }
            continue;
        }

        if(pWinInfo->channelNum == SYSTEM_SW_MS_INVALID_ID)
        {
            // No channel mapped to this window, free all frames queue for this window
            do
            {
                status = Utils_bufGetFullFrame(&pWinObj->bufInQue,
                                           &pInFrame, BIOS_NO_WAIT);
                //pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesDequeued++;
                if (status == FVID2_SOK)
                {
                    pWinObj->framesRejectCount++ ;

                    SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                   pInFrame);
                }
            } while(status==FVID2_SOK);

            /* free any frame which is being held */
            if (pWinObj->pCurInFrame != NULL)
            {
                pWinObj->framesRejectCount++ ;

                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                   pWinObj->pCurInFrame);
                pWinObj->pCurInFrame = NULL;
            }
        }

        /* Free all the accumulated input frames except the last one,
         * if multiple frames are present in the input side of SwMsLink
         */

            pInFrame = NULL;

#if AVSYNC_COMP_ENABLE
        if (pAvSyncInfo_obj->avSyncCompEnable == FALSE) // ***
#endif
        {
           // pInFrame = NULL;
            queCnt = Utils_queGetQueuedCount (&pWinObj->bufInQue.fullQue);
                if (SYSTEM_SW_MS_DEFAULT_INPUT_QUE_LEN == pObj->createArgs.maxInputQueLen)
                {
                    pObj->createArgs.maxInputQueLen = SW_MS_SKIP_INPUT_FRAMES_SC;
                    if(pObj->DrvObj[drvInst].isDeiDrv && pObj->DrvObj[drvInst].forceBypassDei == FALSE)
                    {
                        pObj->createArgs.maxInputQueLen = SW_MS_SKIP_INPUT_FRAMES_DEI;
                    }
                }
                else
                {
                    if (SYSTEM_SW_MS_INVALID_INPUT_QUE_LEN != pObj->createArgs.maxInputQueLen)
                    {
                        if(pObj->DrvObj[drvInst].isDeiDrv && pObj->DrvObj[drvInst].forceBypassDei == FALSE)
                        {
                            /* Make sure that if we use DEI we drop even number of fields */
                            if ((pObj->createArgs.maxInputQueLen & 0x1) == 0)
                            {
                                pObj->createArgs.maxInputQueLen++;
                            }
                        }
                    }
                }
            skipInputFrames = pObj->createArgs.maxInputQueLen;
            if (queCnt > skipInputFrames)
            {
                pWinObj->framesAccEventCount++;

                if(queCnt>pWinObj->framesAccMax)
                    pWinObj->framesAccMax = queCnt;

                if(queCnt<pWinObj->framesAccMin)
                    pWinObj->framesAccMin = queCnt;

                queCnt = skipInputFrames;
            }
            else
                queCnt = 1;

            while (queCnt)
            {
            /* valid window for processing */
                status = Utils_bufGetFullFrame(
                            &pWinObj->bufInQue,
                            &pInFrame,
                            BIOS_NO_WAIT);
                //pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesDequeued++;

                if ((status == FVID2_SOK) && (queCnt > 1))
                {
                    pWinObj->framesDroppedCount++;

                    SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pInFrame);
                }
                queCnt--;
            };

            if (pObj->switchLayout && pInFrame)
            {
                /* when switching layout get newest frame, in other situations get first available frame */

                nextFid = pInFrame->fid;

                /* get latest frame */
                while (status == FVID2_SOK)
                {
                    /* valid window for processing */
                    status = Utils_bufGetFullFrame(&pWinObj->bufInQue,
                                                   &pFrame, BIOS_NO_WAIT);
                //pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesDequeued++;
                    if (status == FVID2_SOK)
                    {
                        pWinObj->framesRejectCount++;

                        SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                           pInFrame);
                        pInFrame = pFrame;
                    }
                }

                if ((pInFrame->fid != nextFid) && pObj->DrvObj[drvInst].isDeiDrv && pObj->DrvObj[drvInst].forceBypassDei == FALSE)
                {
                    repeatFld = TRUE;
                }
            }
        }

#if AVSYNC_COMP_ENABLE
        if ( pAvSyncInfo_obj->avSyncCompEnable == TRUE
                        && (Utils_queGetQueuedCount (&pWinObj->bufInQue.fullQue))) {        // ***
            AVSYNC_FRAME_RENDER_FLAG RenderStatus;
            FVID2_Frame *pSkippableFrame = NULL;

            do
            {
                if(Frame_Dequeued[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)][pWinInfo->channelNum] == NULL)   // do dequeue
                {
                    /* valid window for processing */
                    status = Utils_bufGetFullFrame(
                                &pWinObj->bufInQue,
                                &pInFrame,
                                BIOS_NO_WAIT);

                    if(pAvSyncInfo_obj->DisplayEnable.nActiveDisplayFlag[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)] & (1 << pInFrame->channelNum)) {
                        pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesDequeued++;
                    }
                }
                else {
                        pInFrame = Frame_Dequeued[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)][pWinInfo->channelNum];
                        Frame_Dequeued[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)][pWinInfo->channelNum] = NULL;
                }

                if(pSkippableFrame != NULL) {
                    if(pInFrame != NULL) {
                        pWinObj->framesRejectCount++;
                        SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList, pSkippableFrame);
                        pSkippableFrame = NULL;
                        pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesSkipped++;
                    }
                }

                // AvSync frame render control Part
                // for enabled avsync, valid frame, channel enabled  case : // ***
        if ( pInFrame && (pAvSyncInfo_obj->DisplayEnable.nActiveDisplayFlag[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)] & (1 << pInFrame->channelNum)) )
                                                                        // consider channels [16-31]
                {
                    if (pInFrame->channelNum == 16){    // test case
                            input_timestamp[inputTimestampCount++] = Utils_getCurTimeInMsec();
                            if(inputTimestampCount > 300) inputTimestampCount = 0;
                    }
                    rendr_flag = VideoSyncFn (pInFrame->timeStamp, pInFrame->channelNum, AVSYNC_BUFFERFLAG_STARTTIME); //*((UInt32*)(pInFrame->reserved)));

                    switch (rendr_flag) {
                        case AVSYNC_DropFrame :
                        {
                            pSkippableFrame = pInFrame;
                            RenderStatus = AVSYNC_DropFrame;

                            break;
                        }
                        case AVSYNC_RepeatFrame :
                        {
                            Frame_Dequeued[AVSYNC_MAP_SC_INS_ID_2_SC_NUM(pObj->linkId)][pInFrame->channelNum] = pInFrame;
                            pInFrame = NULL;
                            RenderStatus = AVSYNC_RepeatFrame;
                            pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesRepeated++;

                            break;
                        }
                        case AVSYNC_RenderFrame :
                        {
                            RenderStatus = AVSYNC_RenderFrame;
                            pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesRendered++;
                            pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].lastVidPTS = pInFrame->timeStamp;
                            break;  // by default, it will render.
                        }
                        default:
                            printf("AVSYNC_DropFrame by default switch option\n");
                    }
                }
                else {
                    if((pSkippableFrame != NULL) && (pInFrame == NULL)){
                        pInFrame = pSkippableFrame;         // getting the latest frame for the case drop frame
                        pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].nVidFramesRendered++;
                        pAvSyncInfo_obj->StreamStats.StreamChStats[pWinInfo->channelNum].lastVidPTS = pInFrame->timeStamp;
                    }
                    break;
                }
            }while(RenderStatus == AVSYNC_DropFrame);
        }
        // End - AvSync frame render control Part - End //
#endif

        if (pInFrame)
        {
            UInt32 latency;

            /* got new frame, free any frame which is being held */
            if (pWinObj->pCurInFrame != NULL)
            {
                SwMsLink_freeFrame(pInQueParams, &pObj->freeFrameList,
                                   pWinObj->pCurInFrame);
            }

            pWinObj->pCurInFrame = pInFrame;

            pWinObj->framesUsedCount++;

            latency = Utils_getCurTimeInMsec() - pInFrame->timeStamp;

            if(latency>pWinObj->maxLatency)
                pWinObj->maxLatency = latency;
            if(latency<pWinObj->minLatency)
                pWinObj->minLatency = latency;
        }
        else
        {
            /* no new frame, repeat previous frame */

            pWinObj->framesRepeatCount++;

            repeatFld = TRUE;
        }

        if (pWinObj->pCurInFrame == NULL)
        {
            /* use blank frame */
            pInFrame = &pWinObj->blankFrame;
        }
        else
        {
            /* use actual frame */
            pInFrame = pWinObj->pCurInFrame;
        }

        pDrvObj = &pObj->DrvObj[drvInst];
        if (pObj->DrvObj[drvInst].isDeiDrv)
        {
            if (pWinInfo->bypass || pObj->DrvObj[drvInst].forceBypassDei == TRUE)
            {

                pDrvObj = &pObj->DrvObj[drvInst + SYSTEM_SW_MS_MAX_INST];
                /* [TODO] This is for avoiding bypass DEI driver getting FID
                 * = 1. Not clear why FID = 1 is passed to bypass DEI without
                 * this workaround. */
                if (FVID2_FID_TOP != pInFrame->fid)
                {
                    pInFrame->fid = FVID2_FID_TOP;
                }
            }
        }

        pDrvObj->inFrameList.frames[pDrvObj->inFrameList.numFrames]
            = pInFrame;
        pDrvObj->inFrameList.numFrames++;
        pDrvObj->outFrameList.frames[pDrvObj->outFrameList.numFrames]
            = &pWinObj->curOutFrame;
        pDrvObj->outFrameList.numFrames++;

        rtParamUpdatePerFrame = FALSE;
        pFrameInfo = (System_FrameInfo *) pInFrame->appData;
        if (pFrameInfo != NULL)
        {
            if (pFrameInfo->rtChInfoUpdate == TRUE)
            {
                UTILS_assert(pInFrame->channelNum<SYSTEM_SW_MS_MAX_CH_ID);

                rtChannelInfo = &pObj->rtChannelInfo[pInFrame->channelNum];
                if (pFrameInfo->rtChInfo.height != rtChannelInfo->height)
                {
                    rtChannelInfo->height = pFrameInfo->rtChInfo.height;
                    rtParamUpdatePerFrame = TRUE;
                }
                if (pFrameInfo->rtChInfo.width != rtChannelInfo->width)
                {
                    rtChannelInfo->width = pFrameInfo->rtChInfo.width;
                    rtParamUpdatePerFrame = TRUE;
                }
                if (pFrameInfo->rtChInfo.pitch[0] != rtChannelInfo->pitch[0])
                {
                    rtChannelInfo->pitch[0] = pFrameInfo->rtChInfo.pitch[0];
                    rtParamUpdatePerFrame = TRUE;
                }
                if (pFrameInfo->rtChInfo.pitch[1] != rtChannelInfo->pitch[1])
                {
                    rtChannelInfo->pitch[1] = pFrameInfo->rtChInfo.pitch[1];
                    rtParamUpdatePerFrame = TRUE;
                }
                pFrameInfo->rtChInfoUpdate = FALSE;
            }
        }
        pInFrame->channelNum = winId - pDrvObj->startWin;
        pInFrame->perFrameCfg = NULL;
        pWinObj->curOutFrame.perFrameCfg = NULL;

        pWinObj->curOutFrame.addr[0][0] =
            (Ptr) ((UInt32) pOutFrame->addr[0][0] + pWinInfo->bufAddrOffset);

        if (pWinObj->applyRtPrm || repeatFld || rtParamUpdatePerFrame)
        {
            if (pObj->DrvObj[drvInst].isDeiDrv)
            {
                pInFrame->perFrameCfg = &pWinObj->deiRtPrm;
                pWinObj->curOutFrame.perFrameCfg = &pWinObj->deiRtPrm;
            }
            else
            {
                pInFrame->perFrameCfg = &pWinObj->scRtPrm;
                pWinObj->curOutFrame.perFrameCfg = &pWinObj->scRtPrm;
            }
            pWinObj->applyRtPrm = FALSE;

            if (repeatFld && pObj->DrvObj[drvInst].isDeiDrv && pObj->DrvObj[drvInst].forceBypassDei == FALSE)
            {
                pWinObj->deiRtCfg.fldRepeat = TRUE;
            }
            if (rtParamUpdatePerFrame == TRUE)
            {
                pObj->rtParamUpdate = TRUE;
            }
        }
    }

    if (pObj->rtParamUpdate == TRUE)
    {
        Vps_printf(" SWMS: *** UPDATING RT Params ***\n");
        pObj->layoutParams.onlyCh2WinMapChanged = TRUE;
        SwMsLink_drvSwitchLayout(pObj, &pObj->layoutParams, TRUE);
    }
    pObj->rtParamUpdate = FALSE;
    pObj->switchLayout = FALSE;

    if (pObj->freeFrameList.numFrames)
    {
        pObj->inFramePutCount += pObj->freeFrameList.numFrames;
        System_putLinksEmptyFrames(pInQueParams->prevLinkId,
                                   pInQueParams->prevLinkQueId,
                                   &pObj->freeFrameList);
    }

    return FVID2_SOK;
}

Int32 SwMsLink_DrvProcessFrames(SwMsLink_Obj * pObj)
{
    Int32 status = FVID2_SOK;
    UInt32 curTime;
    SwMsLink_DrvObj *pDrvObj[2*SYSTEM_SW_MS_MAX_INST];
    UInt32 i,drvId;

    for (i = 0; i < SYSTEM_SW_MS_MAX_INST; i++)
    {
        pDrvObj[i] = NULL;
        pDrvObj[i + SYSTEM_SW_MS_MAX_INST] = NULL;
    }

    drvId = 0;
    for (i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        if (pObj->DrvObj[i].isDeiDrv)
        {
            if((pObj->DrvObj[i].forceBypassDei == FALSE) &&(pObj->DrvObj[i].inFrameList.numFrames))
            {
                pDrvObj[drvId++] = &pObj->DrvObj[i];
            }

            if (pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST].inFrameList.numFrames)
            {
                pDrvObj[drvId++] = &pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST];
            }
        }
        else
        {
            if(pObj->DrvObj[i].inFrameList.numFrames)
                pDrvObj[drvId++] = &pObj->DrvObj[i];
        }
    }

    //now we'll begin process framelist in parallel
    if (drvId)
    {
        curTime = Utils_getCurTimeInMsec();
    }
    else
    {
        return status;
    }

    /* VIP locking should be done in dual out mode as
       *  VIP access happens at capture during reset & DEI processing
       */
    if (pObj->vipLockRequired == TRUE)
    {
        System_lockVip(pObj->vipInstId);
    }

    for (i = 0; i < drvId; i++)
    {
        /* If in dual out mode; initialize VIP-SC numOutFrames */
        if (pDrvObj[i]->processList.numOutLists == 2)
        {
            pObj->outFrameDropList.numFrames = pDrvObj[i]->outFrameList.numFrames;
        }

        SwMsLink_drvModifyFramePointer(pObj, pDrvObj[i], 1);
        status = FVID2_processFrames(pDrvObj[i]->fvidHandle, &pDrvObj[i]->processList);
        UTILS_assert(status == FVID2_SOK);
    }

    for (i = 0; i < drvId; i++)
    {
        Semaphore_pend(pDrvObj[i]->complete, BIOS_WAIT_FOREVER);
        status = FVID2_getProcessedFrames(pDrvObj[i]->fvidHandle,
                                          &pDrvObj[i]->processList, BIOS_NO_WAIT);
        UTILS_assert(status == FVID2_SOK);

        SwMsLink_drvModifyFramePointer(pObj, pDrvObj[i], 0);
        pObj->frameCount += pDrvObj[i]->inFrameList.numFrames;
    }
    if (pObj->vipLockRequired == TRUE)
    {
        System_unlockVip(pObj->vipInstId);
    }

    curTime = Utils_getCurTimeInMsec() - curTime;
    pObj->totalTime += curTime;


    return status;
}

Int32 SwMsLink_drvDoScaling(SwMsLink_Obj * pObj)
{
    FVID2_Frame *pOutFrame;
    Int32 status;
    UInt32 curTime = Utils_getCurTimeInMsec();

    if (pObj->skipProcessing)
    {
        pObj->skipProcessing--;
        return FVID2_SOK;
    }

    if (pObj->prevDoScalingTime != 0)
    {
        if (curTime > pObj->prevDoScalingTime)
        {
            UInt32 curScalingInterval;

            curScalingInterval    = curTime - pObj->prevDoScalingTime;
            pObj->scalingInterval += curScalingInterval;
            if (curScalingInterval < pObj->scalingIntervalMin)
            {
                pObj->scalingIntervalMin = curScalingInterval;
            }
            if (curScalingInterval > pObj->scalingIntervalMax)
            {
                pObj->scalingIntervalMax = curScalingInterval;
            }
        }
    }
    pObj->prevDoScalingTime = curTime;
    pObj->framesOutReqCount++;

    status = Utils_bufGetEmptyFrame(&pObj->bufOutQue, &pOutFrame, BIOS_NO_WAIT);
    if (status != FVID2_SOK)
    {
        pObj->framesOutDropCount++;
        return status;
    }

    pOutFrame->timeStamp = Utils_getCurTimeInMsec();

    SwMsLink_drvLock(pObj);

    SwMsLink_drvMakeFrameLists(pObj, pOutFrame);

    SwMsLink_DrvProcessFrames(pObj);

    SwMsLink_drvUnlock(pObj);

    if(pObj->createArgs.enableLayoutGridDraw == TRUE)
    {
        SwMsLink_drvDoDma(pObj,pOutFrame);
    }

    status = Utils_bufPutFullFrame(&pObj->bufOutQue, pOutFrame);
    if (status != FVID2_SOK)
    {
        pObj->framesOutRejectCount++;

        // return back frame to empty que
        Utils_bufPutEmptyFrame(&pObj->bufOutQue, pOutFrame);
    }
    else
    {
        pObj->framesOutCount++;
    }

    System_sendLinkCmd(pObj->createArgs.outQueParams.nextLink,
                       SYSTEM_CMD_NEW_DATA);

    return status;
}

Int32 SwMsLink_drvDeleteDrv(SwMsLink_Obj * pObj, SwMsLink_DrvObj *pDrvObj)
{

    if (pDrvObj->isDeiDrv && pDrvObj->bypassDei == FALSE)
    {
        SwMsLink_drvFreeCtxMem(pDrvObj);
    }

    FVID2_delete(pDrvObj->fvidHandle, NULL);
    Semaphore_delete(&pDrvObj->complete);

    return FVID2_SOK;
}

Int32 SwMsLink_drvDelete(SwMsLink_Obj * pObj)
{
    UInt32 winId,i,maxChnl;
    System_MemoryType memType;
    Int32 status;

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Frames = %d (fps = %d) !!!\n",
               Utils_getCurTimeInMsec(),
               pObj->frameCount,
               pObj->frameCount * 100 / (pObj->totalTime / 10));
#endif

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Delete in progress !!!\n", Utils_getCurTimeInMsec());
#endif

    memType = (System_MemoryType)pObj->inQueInfo.chInfo[0].memType;


    for (i = 0; i < pObj->createArgs.numSwMsInst; i++)
    {
        if(pObj->DrvObj[i].isDeiDrv)
        {
            if(pObj->DrvObj[i].forceBypassDei==FALSE)
            {   /* DEI driver in DEI mode is deleted only force DEI bypass is FALSE */
                SwMsLink_drvDeleteDrv(pObj, &pObj->DrvObj[i]);
            }
            SwMsLink_drvDeleteDrv(pObj, &pObj->DrvObj[i + SYSTEM_SW_MS_MAX_INST]);
        }
        else
        {
            SwMsLink_drvDeleteDrv(pObj, &pObj->DrvObj[i]);
        }
    }

    status = SwMsLink_drvDmaDelete(pObj);
    UTILS_assert(0 == status);

    Utils_bufDelete(&pObj->bufOutQue);

    maxChnl = SYSTEM_SW_MS_MAX_WIN;

    for (winId = 0; winId < maxChnl; winId++)
    {
        Utils_bufDelete(&pObj->winObj[winId].bufInQue);
    }

    Semaphore_delete(&pObj->lock);
    Clock_delete(&pObj->timer);

    Utils_memFrameFree(&pObj->bufferFrameFormat,
                       pObj->outFrames, pObj->createArgs.numOutBuf);

    if(memType==SYSTEM_MT_NONTILEDMEM)
    {
        /* blank frame need not be freed, its freed at system de-init */
        }
    else
    {
        // free tiler buffer
        SystemTiler_freeAll();
    }

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Delete Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 SwMsLink_drvStart(SwMsLink_Obj * pObj)
{
#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Start in Progress !!!\n", Utils_getCurTimeInMsec());
#endif

    SwMsLink_drvResetStatistics(pObj);
    Clock_start(pObj->timer);

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Start Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 SwMsLink_drvStop(SwMsLink_Obj * pObj)
{
#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Stop in Progress !!!\n", Utils_getCurTimeInMsec());
#endif

    Clock_stop(pObj->timer);

#ifdef SYSTEM_DEBUG_SWMS
    Vps_printf(" %d: SWMS: Stop Done !!!\n", Utils_getCurTimeInMsec());
#endif

    return FVID2_SOK;
}

Int32 SwMsLink_drvLock(SwMsLink_Obj * pObj)
{
    return Semaphore_pend(pObj->lock, BIOS_WAIT_FOREVER);
}

Int32 SwMsLink_drvUnlock(SwMsLink_Obj * pObj)
{
    Semaphore_post(pObj->lock);

    return FVID2_SOK;
}

Int32 SwMsLink_drvClockPeriodReconfigure(SwMsLink_Obj * pObj)
{
    UInt32 timerPeriod;

    Vps_rprintf(" %d: SWMS    : ******* Configuring clock %d secs... \n",
                Utils_getCurTimeInMsec(), pObj->timerPeriod);

    timerPeriod = pObj->timerPeriod;

    Clock_stop(pObj->timer);
    Clock_setPeriod(pObj->timer, timerPeriod);
    Clock_setTimeout(pObj->timer, timerPeriod);
    Clock_start(pObj->timer);

    return FVID2_SOK;
}

Int32 SwMsLink_drvGetTimerPeriod(SwMsLink_Obj * pObj,
                                 SwMsLink_LayoutPrm * layoutParams)
{
    if (layoutParams->outputFPS == 0 || layoutParams->outputFPS > 200)
    {
        pObj->timerPeriod = SW_MS_LINK_TIMER_DEFAULT_PERIOD;
    }
    else
    {
        pObj->timerPeriod =
              (1000/(layoutParams->outputFPS+(layoutParams->outputFPS/10)));
    }
    return FVID2_SOK;
}

Int32 SwMsLink_printBufferStatus (SwMsLink_Obj * pObj)
{
    Uint8 str[256];

    Vps_rprintf
        (" \n"
          " *** [%s] Mosaic Statistics *** \n"
          "%d: SWMS: Rcvd from prev = %d, Returned to prev = %d\r\n",
          pObj->name,
          Utils_getCurTimeInMsec(), pObj->inFrameGetCount, pObj->inFramePutCount);

    sprintf ((char *)str, "SWMS Out ");
    Utils_bufPrintStatus(str, &pObj->bufOutQue);
    return 0;
}

